// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_UI_H_
#define CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_UI_H_

#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"

class KaleidoscopeUI : public ui::MojoWebUIController {
 public:
  explicit KaleidoscopeUI(content::WebUI* web_ui);
  KaleidoscopeUI(const KaleidoscopeUI&) = delete;
  KaleidoscopeUI& operator=(const KaleidoscopeUI&) = delete;
  ~KaleidoscopeUI() override;

  void BindInterface(
      mojo::PendingReceiver<media::mojom::KaleidoscopeDataProvider> provider);

 private:
  std::unique_ptr<media::mojom::KaleidoscopeDataProvider> provider_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_MEDIA_KALEIDOSCOPE_KALEIDOSCOPE_UI_H_
