// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_startup_flags.h"

#include "base/android/build_info.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/system/sys_info.h"
#include "cc/base/switches.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/base/ui_base_switches.h"

#if defined(CASTANETS)
#include "components/viz/common/switches.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/gl/gl_switches.h"
#endif

#if defined(SERVICE_OFFLOADING)
#include "services/service_manager/sandbox/switches.h"
#endif

#if defined(CASTANETS) || defined(SERVICE_OFFLOADING)
#include "base/distributed_chromium_util.h"
#endif

namespace content {

void SetContentCommandLineFlags(bool single_process) {
  // May be called multiple times, to cover all possible program entry points.
  static bool already_initialized = false;
  if (already_initialized)
    return;
  already_initialized = true;

  base::CommandLine* parsed_command_line =
      base::CommandLine::ForCurrentProcess();

#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        service_manager::switches::kNoSandbox);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kNoZygote);
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kNumRasterThreads, "4");
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(switches::kLang,
                                                              "en-US");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kIgnoreGpuBlacklist);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableGpuDriverBugWorkarounds);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableFrameRateLimit);
    std::string disabled_features =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDisableFeatures);
    std::string features_to_disable =
        "NetworkService,NetworkServiceInProcess,SpareRendererForSitePerProcess,\
        SurfaceSynchronization,VizDisplayCompositor";
    if (!disabled_features.empty())
      disabled_features = disabled_features + "," + features_to_disable;
    else
      disabled_features = features_to_disable;
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kDisableFeatures, disabled_features);
  }
#endif
  if (single_process) {
    // Need to ensure the command line flag is consistent as a lot of chrome
    // internal code checks this directly, but it wouldn't normally get set when
    // we are implementing an embedded WebView.
    parsed_command_line->AppendSwitch(switches::kSingleProcess);
  }

  parsed_command_line->AppendSwitch(switches::kEnableViewport);
  parsed_command_line->AppendSwitch(switches::kValidateInputEventStream);

  if (base::android::BuildInfo::GetInstance()->sdk_int() >=
      base::android::SDK_VERSION_MARSHMALLOW) {
    parsed_command_line->AppendSwitch(switches::kEnableLongpressDragSelection);
    parsed_command_line->AppendSwitchASCII(
        switches::kTouchTextSelectionStrategy, "direction");
  }

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

#if defined(SERVICE_OFFLOADING)
  if (base::ServiceOffloading::IsEnabled()) {
    // Prevents the renderer process from being killed for Service Offloading.
    parsed_command_line->AppendSwitch(
        service_manager::switches::kNoSandbox);
  }
#endif
}

}  // namespace content
