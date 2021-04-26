// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_

#include <vector>

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/model/ambient_backend_model_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

// Class to handle photos in ambient mode.
class ASH_EXPORT AmbientPhotoController : public AmbientBackendModelObserver {
 public:
  // Start fetching next |ScreenUpdate| from the backdrop server. The specified
  // download callback will be run upon completion and returns a null image
  // if: 1. the response did not have the desired fields or urls or, 2. the
  // download attempt from that url failed. The |icon_callback| also returns
  // the weather temperature in Fahrenheit together with the image.
  using TopicsDownloadCallback =
      base::OnceCallback<void(const std::vector<AmbientModeTopic>& topics)>;
  using WeatherIconDownloadCallback =
      base::OnceCallback<void(base::Optional<float>, const gfx::ImageSkia&)>;

  using PhotoDownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  AmbientPhotoController();
  ~AmbientPhotoController() override;

  // Start/stop updating the screen contents.
  // We need different logics to update photos and weather info because they
  // have different refreshing intervals. Currently we only update weather info
  // one time when entering ambient mode. Photos will be refreshed every
  // |kPhotoRefreshInterval|.
  void StartScreenUpdate();
  void StopScreenUpdate();

  AmbientBackendModel* ambient_backend_model() {
    return &ambient_backend_model_;
  }

  const base::OneShotTimer& photo_refresh_timer_for_testing() const {
    return photo_refresh_timer_;
  }

  // AmbientBackendModelObserver:
  void OnTopicsChanged() override;

 private:
  friend class AmbientAshTestBase;

  void FetchTopics();

  void ScheduleFetchTopics();

  void ScheduleRefreshImage();

  void GetScreenUpdateInfo();

  // Return a topic to download the image.
  const AmbientModeTopic& GetNextTopic();

  void GetNextImage();

  void OnScreenUpdateInfoFetched(const ash::ScreenUpdate& screen_update);

  void StartDownloadingWeatherConditionIcon(
      const ash::ScreenUpdate& screen_update);

  void OnPhotoDownloaded(const gfx::ImageSkia& image);

  // Invoked upon completion of the weather icon download, |icon| can be a null
  // image if the download attempt from the url failed.
  void OnWeatherConditionIconDownloaded(base::Optional<float> temp_f,
                                        const gfx::ImageSkia& icon);

  AmbientBackendModel ambient_backend_model_;

  // The timer to refresh photos.
  base::OneShotTimer photo_refresh_timer_;

  // The index of a topic to download.
  size_t topic_index_ = 0;

  // Tracking how many batches of topics have been fetched.
  int topics_batch_fetched_ = 0;

  ScopedObserver<AmbientBackendModel, AmbientBackendModelObserver>
      ambient_backedn_model_observer_{this};

  base::WeakPtrFactory<AmbientPhotoController> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AmbientPhotoController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_PHOTO_CONTROLLER_H_
