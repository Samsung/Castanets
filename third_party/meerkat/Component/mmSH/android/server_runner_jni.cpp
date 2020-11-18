/*
 * Copyright 2019 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "server_runner_jni.h"

#include <android/log.h>
#include <jni.h>

#if defined(SERVICE_OFFLOADING)
#include "base/distributed_chromium_util.h"
#endif
#include "server_runner.h"

static JavaVM* g_jvm = nullptr;
static jobject g_class_loader = nullptr;
static jmethodID g_find_class_method_id = 0;
static ServerRunner* g_server_runner = nullptr;

static const char* const kLogTag = "MeerkatServer_JNI";
static const char* const kMeerkatServerServiceName = "com/samsung/android/meerkat/MeerkatServerService";

static jclass GetClass(JNIEnv* env, const char* class_name) {
  jclass clazz =
    static_cast<jclass>(env->CallObjectMethod(g_class_loader,
                                              g_find_class_method_id,
                                              env->NewStringUTF(class_name)));
  return clazz;
}

static jmethodID GetMethodID(JNIEnv* env,
                             jclass clazz,
                             const char* method_name,
                             const char* jni_signature) {
  jmethodID id = env->GetStaticMethodID(clazz, method_name, jni_signature);
  return id;
}

std::string Java_getIdToken() {
  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return std::string();
  }

  JNIEnv* env;
  if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
      return std::string();
    }
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return std::string();
  }

  auto mid = GetMethodID(env, clazz, "getIdToken", "()Ljava/lang/String;");
  if (!mid) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetMethodID failed");
    return std::string();
  }

  auto j_token = static_cast<jstring>(env->CallStaticObjectMethod(clazz, mid));
  const char* token =env->GetStringUTFChars(j_token, nullptr);
  std::string ret(token);
  env->ReleaseStringUTFChars(j_token, token);

  g_jvm->DetachCurrentThread();

  return ret;
}

bool Java_verifyIdToken(const char* token) {
  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return false;
  }

  JNIEnv* env;
  if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
      return false;
    }
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return false;
  }

  auto mid = GetMethodID(env, clazz, "verifyIdToken", "(Ljava/lang/String;)Z");
  if (!mid) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetMethodID failed");
    return false;
  }

  auto j_token = env->NewStringUTF(token);
  auto ret = env->CallStaticBooleanMethod(clazz, mid, j_token);
  env->DeleteLocalRef(j_token);

  g_jvm->DetachCurrentThread();

  return ret == JNI_TRUE;
}

bool Java_startCastanetsRenderer(std::vector<char*>& argv) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start Chrome as renderer");

  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return false;
  }

  JNIEnv* env;
  if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
        return false;
    }
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return false;
  }

  auto mid = GetMethodID(env, clazz, "startCastanetsRenderer", "(Ljava/lang/String;)Z");
  if (!mid) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetStaticMethodID failed");
    return false;
  }

  int argc = argv.size();
  std::string argv_str(argv[0]);
  for (int i = 1; i < argc; i++) {
    argv_str += " ";
    argv_str += argv[i];
  }
  auto j_argv = env->NewStringUTF(argv_str.c_str());
  auto ret = env->CallStaticBooleanMethod(clazz, mid, j_argv);
  env->DeleteLocalRef(j_argv);

  g_jvm->DetachCurrentThread();

  return ret == JNI_TRUE;
}

jint Native_startServer(JNIEnv* env, jobject /* this */) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start server runner");

  if (g_server_runner) {
      __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Server runner is already running");
      return 0;
  }

  ServerRunner::ServerRunnerParams params;
  // TODO(yh106.jung): Read from configuration file
#if defined(SERVICE_OFFLOADING)
  if (base::ServiceOffloading::IsEnabled()) {
    params.multicast_addr = "224.1.1.7";
  } else {
#endif
    params.multicast_addr = "224.1.1.11";
#if defined(SERVICE_OFFLOADING)
  }
#endif
  params.multicast_port = 9901;
  params.service_port = 9902;
  params.exec_path = "com.samsung.android.castanets";
  params.monitor_port = 9903;
  params.is_daemon = params.with_presence = false;
  params.get_token = &Java_getIdToken;
  params.verify_token = &Java_verifyIdToken;

  g_server_runner = new ServerRunner(params);
  int exit_code = g_server_runner->Initialize();
  if (exit_code > 0) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Initialization failed: exit code(%d)", exit_code);
      return exit_code;
  }
  exit_code = g_server_runner->Run();
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Server runner stopped: exit code(%d)", exit_code);

  delete g_server_runner;
  g_server_runner = nullptr;

  return exit_code;
}

void Native_stopServer(JNIEnv* env, jobject /* this */) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Stop server runner");
  if (g_server_runner) {
      g_server_runner->Stop();
  }
}

static JNINativeMethod kNativeMethods[] = {
  {"nativeStartServer", "()I", reinterpret_cast<void*>(&Native_startServer)},
  {"nativeStopServer", "()V", reinterpret_cast<void*>(&Native_stopServer)},
};

extern "C" {

JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void* /* reserved */) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "JNI_OnLoad");

  // Cache Java VM
  g_jvm = vm;

  JNIEnv* env;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
    return -1;
  }

  auto clazz = env->FindClass(kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "FindClass failed");
    return -1;
  }

  if (env->RegisterNatives(clazz, kNativeMethods, sizeof(kNativeMethods) / sizeof(kNativeMethods[0])) < 0) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "RegisterNatives faild");
    return -1;
  }

  // Cache ClassLoader
  auto class_class = env->GetObjectClass(clazz);
  auto get_class_loader_method = env->GetMethodID(class_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
  auto class_loader = env->CallObjectMethod(clazz, get_class_loader_method);
  // TODO(yh106.jung): DeleteGlobalRef should be called when service is finished.
  g_class_loader = env->NewGlobalRef(class_loader);

  // Cache findClass method ID
  auto class_loader_class = env->FindClass("java/lang/ClassLoader");
  g_find_class_method_id = env->GetMethodID(class_loader_class, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");

  return JNI_VERSION_1_6;
}

}  // extern "C"
