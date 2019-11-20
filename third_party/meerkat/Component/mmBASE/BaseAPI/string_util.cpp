/*
 *  Copyright 2019 Samsung Electronics. All rights reserved.
 *  Copyright 2013 The Chromium Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file.
 *
 *  This file defines utility functions for working with strings.
 */

#include "string_util.h"

namespace mmBase {

template <typename CHAR>
size_t lcpyT(CHAR* dst, const CHAR* src, size_t dst_size) {
  for (size_t i = 0; i < dst_size; ++i) {
    if ((dst[i] = src[i]) == 0)  // We hit and copied the terminating NULL.
      return i;
  }

  // We were left off at dst_size.  We over copied 1 byte.  Null terminate.
  if (dst_size != 0)
    dst[dst_size - 1] = 0;

  // Count the rest of the |src|, and return it's length in characters.
  while (src[dst_size])
    ++dst_size;
  return dst_size;
}

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  return lcpyT<char>(dst, src, dst_size);
}

}  // namespace mmBase
