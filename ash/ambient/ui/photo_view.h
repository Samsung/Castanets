// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_PHOTO_VIEW_H_
#define ASH_AMBIENT_UI_PHOTO_VIEW_H_

#include <memory>

#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class AnimationMetricsReporter;
}  // namespace ui

namespace ash {

class AmbientBackgroundImageView;
class AmbientViewDelegate;

// View to display photos in ambient mode.
class ASH_EXPORT PhotoView : public views::View,
                             public AmbientBackendModelObserver,
                             public ui::ImplicitAnimationObserver {
 public:
  explicit PhotoView(AmbientViewDelegate* delegate);
  PhotoView(const PhotoView&) = delete;
  PhotoView& operator=(PhotoView&) = delete;
  ~PhotoView() override;

  // views::View:
  const char* GetClassName() const override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // AmbientBackendModelObserver:
  void OnImagesChanged() override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  friend class AmbientAshTestBase;

  void Init();
  void UpdateImages();
  void StartTransitionAnimation();

  // Return if can start transition animation.
  bool NeedToAnimateTransition() const;

  const gfx::ImageSkia& GetCurrentImagesForTesting();

  // Note that we should be careful when using |delegate_|, as there is no
  // strong guarantee on the life cycle.
  AmbientViewDelegate* const delegate_ = nullptr;

  std::unique_ptr<ui::AnimationMetricsReporter> metrics_reporter_;

  // Image containers used for animation. Owned by view hierarchy.
  AmbientBackgroundImageView* image_views_[2]{nullptr, nullptr};

  // The unscaled images used for scaling and displaying in different bounds.
  gfx::ImageSkia images_unscaled_[2];

  // The index of |image_views_| to update the next image.
  int image_index_ = 0;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_PHOTO_VIEW_H_
