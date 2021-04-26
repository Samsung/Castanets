// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_ASSISTIVE_WINDOW_HANDLER_INTERFACE_H_
#define UI_BASE_IME_IME_ASSISTIVE_WINDOW_HANDLER_INTERFACE_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/strings/string16.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
namespace ime {
struct AssistiveWindowButton;
struct SuggestionDetails;
}  // namespace ime
}  // namespace ui

namespace chromeos {

struct AssistiveWindowProperties;

// A interface to handle the assistive windows related method call.
class COMPONENT_EXPORT(UI_BASE_IME) IMEAssistiveWindowHandlerInterface {
 public:
  virtual ~IMEAssistiveWindowHandlerInterface() {}

  // Called when showing/hiding assistive window.
  virtual void SetAssistiveWindowProperties(
      const AssistiveWindowProperties& window) {}

  virtual void ShowSuggestion(const ui::ime::SuggestionDetails& details) {}

  virtual void ShowMultipleSuggestions(
      const std::vector<base::string16>& suggestions) {}

  // Highlights or unhighlights a given assistive button based on the given
  // parameters.
  virtual void SetButtonHighlighted(
      const ui::ime::AssistiveWindowButton& button,
      bool highlighted) {}

  virtual void AcceptSuggestion(const base::string16& suggestion) {}

  virtual void HideSuggestion() {}

  // Called to get the current suggestion text.
  virtual base::string16 GetSuggestionText() const = 0;

  // Called to get length of the confirmed part of suggestion text.
  virtual size_t GetConfirmedLength() const = 0;

  // Called when the application changes its caret bounds.
  virtual void SetBounds(const gfx::Rect& cursor_bounds) = 0;

  // Called when the text field's focus state is changed.
  virtual void FocusStateChanged() {}

 protected:
  IMEAssistiveWindowHandlerInterface() {}
};

}  // namespace chromeos

#endif  // UI_BASE_IME_IME_ASSISTIVE_WINDOW_HANDLER_INTERFACE_H_
