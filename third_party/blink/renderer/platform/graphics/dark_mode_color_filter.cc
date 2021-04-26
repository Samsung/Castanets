// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_color_filter.h"

#include "base/check.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/lab_color_space.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkHighContrastFilter.h"
#include "third_party/skia/include/effects/SkTableColorFilter.h"

namespace blink {
namespace {

// SkColorFilterWrapper implementation.
class SkColorFilterWrapper : public DarkModeColorFilter {
 public:
  static std::unique_ptr<SkColorFilterWrapper> Create(
      sk_sp<SkColorFilter> color_filter) {
    return std::unique_ptr<SkColorFilterWrapper>(
        new SkColorFilterWrapper(color_filter));
  }

  static std::unique_ptr<SkColorFilterWrapper> Create(
      SkHighContrastConfig::InvertStyle invert_style,
      const DarkModeSettings& settings) {
    SkHighContrastConfig config;
    config.fInvertStyle = invert_style;
    config.fGrayscale = settings.grayscale;
    config.fContrast = settings.contrast;

    return std::unique_ptr<SkColorFilterWrapper>(
        new SkColorFilterWrapper(SkHighContrastFilter::Make(config)));
  }

  SkColor InvertColor(SkColor color) const override {
    return filter_->filterColor(color);
  }

  sk_sp<SkColorFilter> ToSkColorFilter() const override { return filter_; }

 private:
  explicit SkColorFilterWrapper(sk_sp<SkColorFilter> filter)
      : filter_(filter) {}

  sk_sp<SkColorFilter> filter_;
};

// LabColorFilter implementation.
class LabColorFilter : public DarkModeColorFilter {
 public:
  LabColorFilter() : transformer_(LabColorSpace::RGBLABTransformer()) {
    SkHighContrastConfig config;
    config.fInvertStyle = SkHighContrastConfig::InvertStyle::kInvertLightness;
    config.fGrayscale = false;
    config.fContrast = 0.0;
    filter_ = SkHighContrastFilter::Make(config);
  }

  SkColor InvertColor(SkColor color) const override {
    blink::FloatPoint3D rgb = {SkColorGetR(color) / 255.0f,
                               SkColorGetG(color) / 255.0f,
                               SkColorGetB(color) / 255.0f};
    blink::FloatPoint3D lab = transformer_.sRGBToLab(rgb);
    float invertedL = std::min(110.0f - lab.X(), 100.0f);
    lab.SetX(invertedL);
    rgb = transformer_.LabToSRGB(lab);

    SkColor inverted_color = SkColorSetARGB(
        SkColorGetA(color), static_cast<unsigned int>(rgb.X() * 255 + 0.5),
        static_cast<unsigned int>(rgb.Y() * 255 + 0.5),
        static_cast<unsigned int>(rgb.Z() * 255 + 0.5));
    return AdjustGray(inverted_color);
  }

  sk_sp<SkColorFilter> ToSkColorFilter() const override { return filter_; }

 private:
  // Further darken dark grays to match the primary surface color recommended by
  // the material design guidelines:
  //   https://material.io/design/color/dark-theme.html#properties
  //
  // TODO(gilmanmh): Consider adding a more general way to adjust colors after
  // applying the main filter.
  SkColor AdjustGray(SkColor color) const {
    static const uint8_t kBrightnessThreshold = 32;
    static const uint8_t kAdjustedBrightness = 18;

    uint8_t r = SkColorGetR(color);
    uint8_t g = SkColorGetG(color);
    uint8_t b = SkColorGetB(color);

    if (r == b && r == g && r < kBrightnessThreshold &&
        r > kAdjustedBrightness) {
      return SkColorSetRGB(kAdjustedBrightness, kAdjustedBrightness,
                           kAdjustedBrightness);
    }

    return color;
  }

  const LabColorSpace::RGBLABTransformer transformer_;
  sk_sp<SkColorFilter> filter_;
};

}  // namespace

std::unique_ptr<DarkModeColorFilter> DarkModeColorFilter::FromSettings(
    const DarkModeSettings& settings) {
  switch (settings.mode) {
    case DarkModeInversionAlgorithm::kOff:
      return nullptr;

    case DarkModeInversionAlgorithm::kSimpleInvertForTesting:
      uint8_t identity[256], invert[256];
      for (int i = 0; i < 256; ++i) {
        identity[i] = i;
        invert[i] = 255 - i;
      }
      return SkColorFilterWrapper::Create(
          SkTableColorFilter::MakeARGB(identity, invert, invert, invert));

    case DarkModeInversionAlgorithm::kInvertBrightness:
      return SkColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertBrightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightness:
      return SkColorFilterWrapper::Create(
          SkHighContrastConfig::InvertStyle::kInvertLightness, settings);

    case DarkModeInversionAlgorithm::kInvertLightnessLAB:
      return std::make_unique<LabColorFilter>();
  }
  NOTREACHED();
}

DarkModeColorFilter::~DarkModeColorFilter() {}

}  // namespace blink
