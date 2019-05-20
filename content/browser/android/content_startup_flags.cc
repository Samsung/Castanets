// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_startup_flags.h"

#include "base/android/build_info.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "cc/base/switches.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(CASTANETS)
#include "base/distributed_chromium_util.h"
#include "ui/gl/gl_switches.h"
#endif

namespace content {

void SetContentCommandLineFlags(bool single_process,
                                const std::string& plugin_descriptor) {
  // May be called multiple times, to cover all possible program entry points.
  static bool already_initialized = false;
  if (already_initialized)
    return;
  already_initialized = true;

  base::CommandLine* parsed_command_line =
      base::CommandLine::ForCurrentProcess();

#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoSandbox);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoZygote);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kInProcessGPU);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kDisableGpuVsync);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableUseZoomForDSF);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kNumRasterThreads,"4");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kRendererClientId,"2");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kLang,"en-US");
  }
#endif

  if (single_process) {
    // Need to ensure the command line flag is consistent as a lot of chrome
    // internal code checks this directly, but it wouldn't normally get set when
    // we are implementing an embedded WebView.
    parsed_command_line->AppendSwitch(switches::kSingleProcess);
  }

  parsed_command_line->AppendSwitch(switches::kEnablePinch);
  parsed_command_line->AppendSwitch(switches::kEnableViewport);
  parsed_command_line->AppendSwitch(switches::kValidateInputEventStream);

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_MARSHMALLOW) {
    parsed_command_line->AppendSwitch(switches::kEnableLongpressDragSelection);
    parsed_command_line->AppendSwitchASCII(
        switches::kTouchTextSelectionStrategy, "direction");
  }

  // There is no software fallback on Android, so don't limit GPU crashes.
  parsed_command_line->AppendSwitch(switches::kDisableGpuProcessCrashLimit);

  // On legacy low-memory devices the behavior has not been studied with regard
  // to having an extra process with similar priority as the foreground renderer
  // and given that the system will often be looking for a process to be killed
  // on such systems.
  if (base::SysInfo::IsLowEndDevice())
    parsed_command_line->AppendSwitch(switches::kInProcessGPU);

  parsed_command_line->AppendSwitch(
      switches::kMainFrameResizesAreOrientationChanges);

  // Disable anti-aliasing.
  parsed_command_line->AppendSwitch(
      cc::switches::kDisableCompositedAntialiasing);

  parsed_command_line->AppendSwitch(switches::kUIPrioritizeInGpuProcess);

  if (!plugin_descriptor.empty()) {
    parsed_command_line->AppendSwitchNative(
      switches::kRegisterPepperPlugins, plugin_descriptor);
  }
}

}  // namespace content
