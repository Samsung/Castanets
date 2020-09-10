// Copyright 2020 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/distributed_chromium_util.h"

#include "base/base_switches.h"
#include "base/command_line.h"

namespace base {

bool Castanets::IsEnabled() {
  return (CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kEnableCastanets))
             ? true
             : false;
}

std::string Castanets::ServerAddress() {
  std::string server_address;
  base::CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableCastanets)) {
    server_address =
        command_line->GetSwitchValueASCII(switches::kEnableCastanets);
  }

  return server_address;
}

}  // namespace base
