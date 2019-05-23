// Copyright 2018 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/distributed_chromium_util.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"

namespace base {

// static
bool Castanets::IsEnabled() {
  return (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableCastanets)) ? true : false;
}

// static
std::string Castanets::ServerAddress() {
  std::string server_address = "127.0.0.1";
  base::CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableCastanets)) {
    server_address = command_line->GetSwitchValueASCII(
        switches::kEnableCastanets);
  }

  return server_address;
}

// static
void Castanets::SetBrowserOSType() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kBrowserOSType))
    command_line->AppendSwitch(switches::kBrowserOSType);

  command_line->AppendSwitchASCII(switches::kBrowserOSType,
#if defined(OS_ANDROID)
                                  IntToString(ANDROID_OS)
#elif defined(OS_TIZEN)
                                  IntToString(TIZEN_OS)
#elif defined(OS_LINUX)
                                  IntToString(LINUX_OS)
#elif defined(OS_WIN)
                                  IntToString(WINDOWS_OS)
#else
                                  IntToString(OTHERS)
#endif
                                      );
}

static int os_type = -1;

// static
int Castanets::GetBrowserOSType() {
  if (os_type != -1)
    return os_type;

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kBrowserOSType))
    StringToInt(command_line->GetSwitchValueASCII(switches::kBrowserOSType),
                &os_type);
  return os_type;
}

}  // namespace base
