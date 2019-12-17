// Copyright 2014 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EWK_WRT_PRIVATE_H
#define EWK_WRT_PRIVATE_H

#include <string>

struct Ewk_Wrt_Message_Data {
  std::string type;
  std::string value;
  std::string id;
  std::string reference_id;

  const char* GetType() const;
  void SetType(const char* val);
  const char* GetValue() const;
  void SetValue(const char* val);
  const char* GetId() const;
  void SetId(const char* val);
  const char* GetReferenceId() const;
  void SetReferenceId(const char* val);
};

#endif // EWK_WRT_PRIVATE_H
