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

#include "base/strings/string_number_conversions.h"

#include <windows.h>
#include <string>

namespace blink {

InputControl::InputControl() {}

bool InputControl::sendMouseInput(String type, long x, long y, long code) {
  double screenRes_width = GetSystemMetrics(SM_CXSCREEN)-1;
  double screenRes_height = GetSystemMetrics(SM_CYSCREEN)-1;

  INPUT input = {0};
  input.type = INPUT_MOUSE;
  input.mi.dx = x * (65535.0f / screenRes_width);
  input.mi.dy = y * (65535.0f / screenRes_height);

  if (type == String("mousedown"))
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE|MOUSEEVENTF_LEFTDOWN;
  else if (type == String("mousemove"))
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE;
  else if (type == String("mouseup"))
    input.mi.dwFlags = MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE|MOUSEEVENTF_LEFTUP;
  else
    return false;

  return SendInput(1, &input, sizeof(INPUT));
}

bool InputControl::stopApplication(String pkgName){
  //TODO: Implementation
  return false;
}

}  // namespace blink
