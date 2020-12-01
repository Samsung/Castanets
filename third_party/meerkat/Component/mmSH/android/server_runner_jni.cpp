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

#include "server_runner.h"

static JavaVM* g_jvm = nullptr;
static jobject g_class_loader = nullptr;
static jmethodID g_find_class_method_id = 0;
static ServerRunner* g_server_runner = nullptr;

static const char* const kLogTag = "MeerkatServer_JNI";
static const char* const kMeerkatServerServiceName = "com/samsung/android/meerkat/MeerkatServerService";
static const char* const kMulticastAddress = "224.1.1.11";
static const int kMulticastPort = 9901;
static const int kServicePort = 9902;
static const int kMonitorPort = 9903;

static bool GetEnv(JNIEnv** env) {
  if (g_jvm->GetEnv(reinterpret_cast<void**>(env), JNI_VERSION_1_6) != JNI_OK) {
    if (g_jvm->AttachCurrentThread(env, nullptr) != JNI_OK)
      return false;
  }
  return true;
}

static jclass GetClass(JNIEnv* env, const char* class_name) {
  jclass clazz =
    static_cast<jclass>(env->CallObjectMethod(g_class_loader,
                                              g_find_class_method_id,
                                              env->NewStringUTF(class_name)));
  return clazz;
}

std::string Java_getIdToken() {
  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return std::string();
  }

  JNIEnv* env;
  if (!GetEnv(&env)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
    return std::string();
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return std::string();
  }

  auto mid = env->GetStaticMethodID(clazz, "getIdToken", "()Ljava/lang/String;");
  if (!mid) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetMethodID failed");
    return std::string();
  }

  std::string ret;
  auto j_token = static_cast<jstring>(env->CallStaticObjectMethod(clazz, mid));
  if (j_token) {
    const char* token = env->GetStringUTFChars(j_token, nullptr);
    ret = token;
    env->ReleaseStringUTFChars(j_token, token);
  }

  g_jvm->DetachCurrentThread();

  return ret;
}

bool Java_verifyIdToken(const char* token) {
  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return false;
  }

  JNIEnv* env;
  if (!GetEnv(&env)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
    return false;
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return false;
  }

  auto mid = env->GetStaticMethodID(clazz, "verifyIdToken", "(Ljava/lang/String;)Z");
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

std::string Java_getCapability() {
  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return std::string();
  }

  JNIEnv* env;
  if (!GetEnv(&env)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
    return std::string();
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return std::string();
  }

  auto mid = env->GetStaticMethodID(clazz, "getCapability", "()Ljava/lang/String;");
  if (!mid) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetMethodID failed");
    return std::string();
  }

  std::string ret;
  auto j_capability = static_cast<jstring>(env->CallStaticObjectMethod(clazz, mid));
  if (j_capability) {
    const char* capability = env->GetStringUTFChars(j_capability, nullptr);
    ret = capability;
    env->ReleaseStringUTFChars(j_capability, capability);
  }

  g_jvm->DetachCurrentThread();

  return ret;
}

bool Java_startCastanetsRenderer(std::vector<char*>& argv) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start Chrome as renderer");

  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return false;
  }

  JNIEnv* env;
  if (!GetEnv(&env)) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
    return false;
  }

  auto clazz = GetClass(env, kMeerkatServerServiceName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetClass failed");
    return false;
  }

  auto mid = env->GetStaticMethodID(clazz, "startCastanetsRenderer", "(Ljava/lang/String;)Z");
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

jint Native_startServer(JNIEnv* env, jobject /* this */, jstring j_ini_path) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start server runner");

  if (g_server_runner) {
      __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Server runner is already running");
      return 0;
  }

  ServerRunner::ServerRunnerParams params;

  if (j_ini_path && env->GetStringUTFLength(j_ini_path) > 0) {
    auto* ini_path = env->GetStringUTFChars(j_ini_path, nullptr);
    __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Build params from %s", ini_path);
    if (!ServerRunner::BuildParams(ini_path, params)) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Unable to build params from ini file.");
      env->ReleaseStringUTFChars(j_ini_path, ini_path);
      return 1;
    }
    env->ReleaseStringUTFChars(j_ini_path, ini_path);
  } else {
    params.multicast_addr = kMulticastAddress;
    params.multicast_port = kMulticastPort;
    params.service_port = kServicePort;
    params.monitor_port = kMonitorPort;
    params.get_token = &Java_getIdToken;
    params.verify_token = &Java_verifyIdToken;
    params.get_capability = &Java_getCapability;
  }

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
  {"nativeStartServer", "(Ljava/lang/String;)I", reinterpret_cast<void*>(&Native_startServer)},
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
