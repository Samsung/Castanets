// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/shell/android/browsertests_apk/metrics_test_helper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "weblayer/test/weblayer_browsertests_jni/MetricsTestHelper_jni.h"

namespace weblayer {

namespace {

OnLogsMetricsCallback& GetOnLogMetricsCallback() {
  static base::NoDestructor<OnLogsMetricsCallback> s_callback;
  return *s_callback;
}

}  // namespace

void InstallTestGmsBridge(bool has_user_consent,
                          const OnLogsMetricsCallback on_log_metrics) {
  GetOnLogMetricsCallback() = on_log_metrics;
  Java_MetricsTestHelper_installTestGmsBridge(
      base::android::AttachCurrentThread(), has_user_consent);
}

void RemoveTestGmsBridge() {
  Java_MetricsTestHelper_removeTestGmsBridge(
      base::android::AttachCurrentThread());
  GetOnLogMetricsCallback().Reset();
}

void CreateProfile(const std::string& name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MetricsTestHelper_createProfile(
      env, base::android::ConvertUTF8ToJavaString(env, name));
}
void DestroyProfile(const std::string& name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_MetricsTestHelper_destroyProfile(
      env, base::android::ConvertUTF8ToJavaString(env, name));
}

void JNI_MetricsTestHelper_OnLogMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jbyteArray>& data) {
  auto& callback = GetOnLogMetricsCallback();
  if (!callback)
    return;

  metrics::ChromeUserMetricsExtension proto;
  jbyte* src_bytes = env->GetByteArrayElements(data, nullptr);
  proto.ParseFromArray(src_bytes, env->GetArrayLength(data.obj()));
  env->ReleaseByteArrayElements(data, src_bytes, JNI_ABORT);
  callback.Run(proto);
}

}  // namespace weblayer
