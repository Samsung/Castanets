// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_display.h"

#include <xf86drmMode.h>
#include <memory>

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/display_features.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/color_space.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"

namespace ui {

namespace {

const char kContentProtection[] = "Content Protection";

const char kPrivacyScreen[] = "privacy-screen";

struct ContentProtectionMapping {
  const char* name;
  display::HDCPState state;
};

const ContentProtectionMapping kContentProtectionStates[] = {
    {"Undesired", display::HDCP_STATE_UNDESIRED},
    {"Desired", display::HDCP_STATE_DESIRED},
    {"Enabled", display::HDCP_STATE_ENABLED}};

// Converts |state| to the DRM value associated with the it.
uint32_t GetContentProtectionValue(drmModePropertyRes* property,
                                   display::HDCPState state) {
  std::string name;
  for (size_t i = 0; i < base::size(kContentProtectionStates); ++i) {
    if (kContentProtectionStates[i].state == state) {
      name = kContentProtectionStates[i].name;
      break;
    }
  }

  for (int i = 0; i < property->count_enums; ++i)
    if (name == property->enums[i].name)
      return i;

  NOTREACHED();
  return 0;
}

std::string GetEnumNameForProperty(drmModeObjectProperties* property_values,
                                   drmModePropertyRes* property) {
  for (uint32_t prop_idx = 0; prop_idx < property_values->count_props;
       ++prop_idx) {
    if (property_values->props[prop_idx] != property->prop_id)
      continue;

    for (int enum_idx = 0; enum_idx < property->count_enums; ++enum_idx) {
      const drm_mode_property_enum& property_enum = property->enums[enum_idx];
      if (property_enum.value == property_values->prop_values[prop_idx])
        return property_enum.name;
    }
  }

  NOTREACHED();
  return std::string();
}

gfx::Size GetDrmModeSize(const drmModeModeInfo& mode) {
  return gfx::Size(mode.hdisplay, mode.vdisplay);
}

std::vector<drmModeModeInfo> GetDrmModeVector(drmModeConnector* connector) {
  std::vector<drmModeModeInfo> modes;
  for (int i = 0; i < connector->count_modes; ++i)
    modes.push_back(connector->modes[i]);

  return modes;
}

void FillLinearValues(std::vector<display::GammaRampRGBEntry>* table,
                      size_t table_size,
                      float max_value) {
  for (size_t i = 0; i < table_size; i++) {
    const uint16_t v =
        max_value * std::numeric_limits<uint16_t>::max() * i / (table_size - 1);
    struct display::GammaRampRGBEntry gamma_entry = {v, v, v};
    table->push_back(gamma_entry);
  }
}

}  // namespace

DrmDisplay::DrmDisplay(ScreenManager* screen_manager,
                       const scoped_refptr<DrmDevice>& drm)
    : screen_manager_(screen_manager),
      drm_(drm),
      current_color_space_(gfx::ColorSpace::CreateSRGB()) {}

DrmDisplay::~DrmDisplay() {
}

uint32_t DrmDisplay::connector() const {
  return connector_->connector_id;
}

std::unique_ptr<display::DisplaySnapshot> DrmDisplay::Update(
    HardwareDisplayControllerInfo* info,
    size_t device_index) {
  std::unique_ptr<display::DisplaySnapshot> params = CreateDisplaySnapshot(
      info, drm_->get_fd(), drm_->device_path(), device_index, origin_);
  crtc_ = info->crtc()->crtc_id;
  // TODO(dcastagna): consider taking ownership of |info->connector()|
  connector_ = ScopedDrmConnectorPtr(
      drm_->GetConnector(info->connector()->connector_id));
  if (!connector_) {
    PLOG(ERROR) << "Failed to get connector "
                << info->connector()->connector_id;
  }

  display_id_ = params->display_id();
  modes_ = GetDrmModeVector(info->connector());
  is_hdr_capable_ =
      params->bits_per_channel() > 8 && params->color_space().IsHDR();
#if defined(OS_CHROMEOS)
  is_hdr_capable_ =
      is_hdr_capable_ &&
      base::FeatureList::IsEnabled(display::features::kUseHDRTransferFunction);
#endif

  return params;
}

bool DrmDisplay::Configure(const drmModeModeInfo* mode,
                           const gfx::Point& origin) {
  VLOG(1) << "DRM configuring: device=" << drm_->device_path().value()
          << " crtc=" << crtc_ << " connector=" << connector_->connector_id
          << " origin=" << origin.ToString()
          << " size=" << (mode ? GetDrmModeSize(*mode).ToString() : "0x0")
          << " refresh_rate=" << (mode ? mode->vrefresh : 0) << "Hz";

  if (mode) {
    if (!screen_manager_->ConfigureDisplayController(
            drm_, crtc_, connector_->connector_id, origin, *mode)) {
      VLOG(1) << "Failed to configure: device=" << drm_->device_path().value()
              << " crtc=" << crtc_ << " connector=" << connector_->connector_id;
      return false;
    }
  } else {
    if (!screen_manager_->DisableDisplayController(drm_, crtc_)) {
      VLOG(1) << "Failed to disable device=" << drm_->device_path().value()
              << " crtc=" << crtc_;
      return false;
    }
  }

  origin_ = origin;
  return true;
}

bool DrmDisplay::GetHDCPState(display::HDCPState* state) {
  if (!connector_)
    return false;

  TRACE_EVENT1("drm", "DrmDisplay::GetHDCPState", "connector",
               connector_->connector_id);
  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector_.get(), kContentProtection));
  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  ScopedDrmObjectPropertyPtr property_values(drm_->GetObjectProperties(
      connector_->connector_id, DRM_MODE_OBJECT_CONNECTOR));
  std::string name =
      GetEnumNameForProperty(property_values.get(), hdcp_property.get());
  for (size_t i = 0; i < base::size(kContentProtectionStates); ++i) {
    if (name == kContentProtectionStates[i].name) {
      *state = kContentProtectionStates[i].state;
      VLOG(3) << "HDCP state: " << *state << " (" << name << ")";
      return true;
    }
  }

  LOG(ERROR) << "Unknown content protection value '" << name << "'";
  return false;
}

bool DrmDisplay::SetHDCPState(display::HDCPState state) {
  if (!connector_) {
    return false;
  }

  ScopedDrmPropertyPtr hdcp_property(
      drm_->GetProperty(connector_.get(), kContentProtection));

  if (!hdcp_property) {
    PLOG(INFO) << "'" << kContentProtection << "' property doesn't exist.";
    return false;
  }

  return drm_->SetProperty(
      connector_->connector_id, hdcp_property->prop_id,
      GetContentProtectionValue(hdcp_property.get(), state));
}

void DrmDisplay::SetColorMatrix(const std::vector<float>& color_matrix) {
  if (!drm_->plane_manager()->SetColorMatrix(crtc_, color_matrix)) {
    LOG(ERROR) << "Failed to set color matrix for display: crtc_id = " << crtc_;
  }
}

void DrmDisplay::SetBackgroundColor(const uint64_t background_color) {
  drm_->plane_manager()->SetBackgroundColor(crtc_, background_color);
}

void DrmDisplay::SetGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  // When both |degamma_lut| and |gamma_lut| are empty they are interpreted as
  // "linear/pass-thru" [1]. If the display |is_hdr_capable_| we have to make
  // sure the |current_color_space_| is considered properly.
  // [1] https://www.kernel.org/doc/html/v4.19/gpu/drm-kms.html#color-management-properties
  if (degamma_lut.empty() && gamma_lut.empty() && is_hdr_capable_)
    SetColorSpace(current_color_space_);
  else
    CommitGammaCorrection(degamma_lut, gamma_lut);
}

// TODO(gildekel): consider reformatting this to use the new DRM API or cache
// |privacy_screen_property| after crrev.com/c/1715751 lands.
void DrmDisplay::SetPrivacyScreen(bool enabled) {
  if (!connector_)
    return;

  ScopedDrmPropertyPtr privacy_screen_property(
      drm_->GetProperty(connector_.get(), kPrivacyScreen));

  if (!privacy_screen_property) {
    LOG(ERROR) << "'" << kPrivacyScreen << "' property doesn't exist.";
    return;
  }

  if (!drm_->SetProperty(connector_->connector_id,
                         privacy_screen_property->prop_id, enabled)) {
    LOG(ERROR) << (enabled ? "Enabling" : "Disabling") << " property '"
               << kPrivacyScreen << "' failed!";
  }
}

void DrmDisplay::SetColorSpace(const gfx::ColorSpace& color_space) {
  // There's only something to do if the display supports HDR.
  if (!is_hdr_capable_)
    return;
  current_color_space_ = color_space;

  // When |color_space| is HDR we can simply leave the gamma tables empty, which
  // is interpreted as "linear/pass-thru", see [1]. However when we have an SDR
  // |color_space|, we need to write a scaled down |gamma| function to prevent
  // the mode change brightness to be visible.
  std::vector<display::GammaRampRGBEntry> degamma;
  std::vector<display::GammaRampRGBEntry> gamma;
  if (current_color_space_.IsHDR())
    return CommitGammaCorrection(degamma, gamma);

  // TODO(mcasas) This should be the same value as in DisplayChangeObservers's
  // FillDisplayColorSpaces, move to a common place.
  constexpr float kHDRLevel = 2.0;
  // TODO(mcasas): Retrieve this from the |drm_| HardwareDisplayPlaneManager.
  constexpr size_t kNumGammaSamples = 16ul;
  FillLinearValues(&gamma, kNumGammaSamples, 1.0 / kHDRLevel);
  CommitGammaCorrection(degamma, gamma);
}

void DrmDisplay::CommitGammaCorrection(
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  if (!drm_->plane_manager()->SetGammaCorrection(crtc_, degamma_lut, gamma_lut))
    LOG(ERROR) << "Failed to set gamma tables for display: crtc_id = " << crtc_;
}

}  // namespace ui
