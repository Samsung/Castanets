// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HUD_DISPLAY_HUD_DISPLAY_H_
#define ASH_HUD_DISPLAY_HUD_DISPLAY_H_

#include "base/sequence_checker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class ImageButton;
}

namespace ash {
namespace hud_display {

class GraphsContainerView;
class HUDSettingsView;

// HUDDisplayView class can be used to display a system monitoring overview.
class HUDDisplayView : public views::WidgetDelegateView,
                       public views::ButtonListener {
 public:
  METADATA_HEADER(HUDDisplayView);

  // Default HUDDisplayView height.
  static constexpr size_t kDefaultHUDHeight = 300;

  // Border width inside the HUDDisplayView rectangle around contents.
  static constexpr size_t kHUDInset = 5;

  HUDDisplayView();
  ~HUDDisplayView() override;

  HUDDisplayView(const HUDDisplayView&) = delete;
  HUDDisplayView& operator=(const HUDDisplayView&) = delete;

  // view::
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  // WidgetDelegate:
  views::ClientView* CreateClientView(views::Widget* widget) override;
  void OnWidgetInitialized() override;

  // views::ButtonListener
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Destroys global instance.
  static void Destroy();

  // Creates/Destroys global singleton.
  static void Toggle();

  // Called from ClientView. Responsible for moving widget when clicked outside
  // of the children.
  int NonClientHitTest(const gfx::Point& point);

 private:
  GraphsContainerView* graphs_container_ = nullptr;        // not owned
  HUDSettingsView* settings_view_ = nullptr;               // not owned
  views::ImageButton* settings_trigger_button_ = nullptr;  // not owned

  SEQUENCE_CHECKER(ui_sequence_checker_);
};

}  // namespace hud_display
}  // namespace ash

#endif  // ASH_HUD_DISPLAY_HUD_DISPLAY_H_
