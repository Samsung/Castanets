// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ewk_wrt_private.h"

#include "common/render_messages_ewk.h"

const char* Ewk_Wrt_Message_Data::GetType() const {
  return type.c_str();
}

void Ewk_Wrt_Message_Data::SetType(const char* val) {
  type = val;
}

const char* Ewk_Wrt_Message_Data::GetValue() const {
  return value.c_str();
}

void Ewk_Wrt_Message_Data::SetValue(const char* val) {
  value = val;
}

const char* Ewk_Wrt_Message_Data::GetId() const {
  return id.c_str();
}

void Ewk_Wrt_Message_Data::SetId(const char* val) {
  id = val;
}

const char* Ewk_Wrt_Message_Data::GetReferenceId() const {
  return reference_id.c_str();
}

void Ewk_Wrt_Message_Data::SetReferenceId(const char* val) {
  reference_id = val;
}
