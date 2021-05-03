/*
 * Copyright (C) 2020 Samsung Electronics Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/input_control/input_control.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "jni/InputControl_jni.h"

#include <string>

namespace blink {
InputControl::InputControl() {
  j_input_control_.Reset(base::android::Java_InputControl_CreateInputControl(
      base::android::AttachCurrentThread()));
}

bool InputControl::sendMouseInput(String type, long x, long y) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::Java_InputControl_SendMouseInput(
      env, j_input_control_,
      base::android::ConvertUTF8ToJavaString(env, type.Utf8().data()), (int)x, (int)y);

  return true;
}

bool InputControl::sendKeyboardInput(String type, long code) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::Java_InputControl_SendKeyboardInput(
      env, j_input_control_,
      base::android::ConvertUTF8ToJavaString(env, type.Utf8().data()), (int)code);

  return true;
}

bool InputControl::sendTouchInput(String type, String json) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::Java_InputControl_SendTouchInput(
      env, j_input_control_,
      base::android::ConvertUTF8ToJavaString(env, type.Utf8().data()), 
      base::android::ConvertUTF8ToJavaString(env, json.Utf8().data()));

  return true;
}

String InputControl::getIPAddr() {
  JNIEnv* env = base::android::AttachCurrentThread();

  std::string strIpAddr = ConvertJavaStringToUTF8(base::android::Java_InputControl_GetIPAddr(env, j_input_control_));
  return String(strIpAddr.c_str());
}

bool InputControl::stopApplication(String pkgName) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::Java_InputControl_StopApplication(
      env, j_input_control_,
      base::android::ConvertUTF8ToJavaString(env, pkgName.Utf8().data()));
  return true;
}

bool InputControl::startApplication(String pkgName) {
  JNIEnv* env = base::android::AttachCurrentThread();

  base::android::Java_InputControl_StartApplication(
      env, j_input_control_,
      base::android::ConvertUTF8ToJavaString(env, pkgName.Utf8().data()));
  return true;
}

}  // namespace blink
