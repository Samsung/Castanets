/*
 *  Copyright 2019 Samsung Electronics. All rights reserved.
 *  Copyright 2013 The Chromium Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can be
 *  found in the LICENSE file.
 *
 *  This file defines utility functions for working with strings.
 */

#include <stdio.h>

namespace mmBase {

// BSD-style safe and consistent string copy functions.
// Copies |src| to |dst|, where |dst_size| is the total allocated size of |dst|.
// Copies at most |dst_size|-1 characters, and always NULL terminates |dst|, as
// long as |dst_size| is not 0.  Returns the length of |src| in characters.
// If the return value is >= dst_size, then the output was truncated.
// NOTE: All sizes are in number of characters, NOT in bytes.
size_t strlcpy(char* dst, const char* src, size_t dst_size);

}  // namespace mmBase