// Copyright 2015 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_message_start.h"

// Ideally we should add our own unique ID for EWK messages. However, in order
// to minimize chromium changes let's hijack ID used by content_shell app.
#define EwkMsgStart ShellMsgStart

