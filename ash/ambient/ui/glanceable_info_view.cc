// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/glanceable_info_view.h"

#include <memory>
#include <string>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/util/ambient_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/time_view.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Appearance.
constexpr int kWeatherIconSizeDip = 32;
constexpr int kWeatherIconLeftPaddingDip = 24;
constexpr int kWeatherIconRightPaddingDip = 8;

// Typography.
constexpr SkColor kTextColor = SK_ColorWHITE;
constexpr int kDefaultFontSizeDip = 64;
constexpr int kWeatherTemperatureFontSizeDip = 32;

// Returns the fontlist used for the time text.
const gfx::FontList& GetTimeFontList() {
  return ambient::util::GetDefaultFontlist();
}

// Returns the fontlist used for the temperature text.
gfx::FontList GetWeatherTemperatureFontList() {
  int temperature_font_size_delta =
      kWeatherTemperatureFontSizeDip - kDefaultFontSizeDip;
  return ambient::util::GetDefaultFontlist().DeriveWithSizeDelta(
      temperature_font_size_delta);
}

// The condition icon should be baseline-aligned to the time text.
int CalculateIconBottomPadding() {
  return GetTimeFontList().GetHeight() - GetTimeFontList().GetBaseline();
}

// The temperature text should be baseline-aligned to the time text.
int CalculateTemperatureTextBottomPadding() {
  int time_font_descent =
      GetTimeFontList().GetHeight() - GetTimeFontList().GetBaseline();
  int temperature_font_descent = GetWeatherTemperatureFontList().GetHeight() -
                                 GetWeatherTemperatureFontList().GetBaseline();
  return time_font_descent - temperature_font_descent;
}

}  // namespace

GlanceableInfoView::GlanceableInfoView(AmbientViewDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
  delegate_->GetAmbientBackendModel()->AddObserver(this);

  InitLayout();
}

GlanceableInfoView::~GlanceableInfoView() {
  delegate_->GetAmbientBackendModel()->RemoveObserver(this);
}

const char* GlanceableInfoView::GetClassName() const {
  return "GlanceableInfoView";
}

void GlanceableInfoView::OnWeatherInfoUpdated() {
  Show();
}

void GlanceableInfoView::Show() {
  weather_condition_icon_->SetImage(
      delegate_->GetAmbientBackendModel()->weather_condition_icon());

  float temperature = delegate_->GetAmbientBackendModel()->temperature();
  // TODO(b/154046129): handle Celsius format.
  temperature_->SetText(l10n_util::GetStringFUTF16(
      IDS_ASH_AMBIENT_MODE_WEATHER_TEMPERATURE_IN_FAHRENHEIT,
      base::FormatNumber(static_cast<int>(temperature))));
}

void GlanceableInfoView::InitLayout() {
  // The children of |GlanceableInfoView| will be drawn on their own
  // layer instead of the layer of |PhotoView| with a solid black background.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(views::BoxLayout::CrossAxisAlignment::kEnd);

  // Init and layout time view.
  time_view_ = AddChildView(std::make_unique<tray::TimeView>(
      ash::tray::TimeView::ClockLayout::HORIZONTAL_CLOCK,
      Shell::Get()->system_tray_model()->clock()));
  time_view_->SetTextFont(GetTimeFontList());
  time_view_->SetTextColor(kTextColor,
                           /*auto_color_readability_enabled=*/false);

  // Init and layout condition icon. It is baseline-aligned to the time view.
  weather_condition_icon_ = AddChildView(std::make_unique<views::ImageView>());
  const gfx::Size size = gfx::Size(kWeatherIconSizeDip, kWeatherIconSizeDip);
  weather_condition_icon_->SetSize(size);
  weather_condition_icon_->SetImageSize(size);
  weather_condition_icon_->SetBorder(views::CreateEmptyBorder(
      0, kWeatherIconLeftPaddingDip, CalculateIconBottomPadding(),
      kWeatherIconRightPaddingDip));

  // Init and layout temperature view. It is baseline-aligned to the time view.
  temperature_ = AddChildView(std::make_unique<views::Label>());
  temperature_->SetAutoColorReadabilityEnabled(false);
  temperature_->SetEnabledColor(kTextColor);
  temperature_->SetFontList(GetWeatherTemperatureFontList());
  temperature_->SetBorder(views::CreateEmptyBorder(
      0, 0, CalculateTemperatureTextBottomPadding(), 0));
}

}  // namespace ash
