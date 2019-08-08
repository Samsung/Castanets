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

static const char* const kClassName = "app/samsung/org/servicediscovery/SDServerService";
static const char* const kLogTag = "SERVICE-DISCOVERY";

int Java_startChromeRenderer() {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start Chrome as renderer");

  if (!g_jvm || !g_class_loader || !g_find_class_method_id) {
    __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Not ready to call Java method");
    return -1;
  }

  JNIEnv* env;
  if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    // TODO(yh106.jung): DetachCurrentThread should be call at the end of thread.
    if (g_jvm->AttachCurrentThread(&env, NULL) != JNI_OK) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "GetEnv failed");
        return -1;
    }
  }

  auto clazz = static_cast<jclass>(env->CallObjectMethod(g_class_loader, g_find_class_method_id, env->NewStringUTF(kClassName)));
  if (!clazz) {
    __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "FindClass failed");
    return -1;
  }

  auto mid = env->GetStaticMethodID(clazz, "startChromeRenderer", "()Z");
  if (!mid) {
    __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "GetStaticMethodID failed");
    return -1;
  }

  auto ret = env->CallStaticBooleanMethod(clazz, mid);

  return (ret == JNI_TRUE) ? 0 : -1;
}

jint Native_startServer(JNIEnv* env, jobject /* this */) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Start server runner");

  if (g_server_runner) {
      __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Server runner is already running");
      return 0;
  }

  ServerRunner::ServerRunnerParams params;
  params.multicast_addr = "224.1.1.11";
  params.multicast_port = 9901;
  params.service_port = 9902;
  params.exec_path = "/opt/google/chrome/chrome";
  params.monitor_port = 9903;
  params.is_daemon = params.with_presence = false;

  g_server_runner = new ServerRunner(params);
  int exit_code = g_server_runner->Initialize();
  if (exit_code > 0) {
      __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Initialization failed: exit code(%d)", exit_code);
      return exit_code;
  }
  exit_code = g_server_runner->Run();
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Server runner stopped: exit code(%d)", exit_code);

  return exit_code;
}

void Native_stopServer(JNIEnv* env, jobject /* this */) {
  __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "Stop server runner");
  if (g_server_runner)
      g_server_runner->Stop();
}

static JNINativeMethod kNativeMethods[] = {
  {"startServer", "()I", reinterpret_cast<void*>(&Native_startServer)},
  {"stopServer", "()V", reinterpret_cast<void*>(&Native_stopServer)},
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

  auto clazz = env->FindClass(kClassName);
  if (!clazz) {
    __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "FindClass failed");
    return -1;
  }

  if (env->RegisterNatives(clazz, kNativeMethods, sizeof(kNativeMethods) / sizeof(kNativeMethods[0])) < 0) {
    __android_log_print(ANDROID_LOG_DEBUG, kLogTag, "RegisterNatives faild");
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
