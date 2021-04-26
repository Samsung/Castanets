// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_AMBIENT_CONTROLLER_H_
#define ASH_AMBIENT_AMBIENT_CONTROLLER_H_

#include <memory>

#include "ash/ambient/ambient_access_token_controller.h"
#include "ash/ambient/ambient_photo_controller.h"
#include "ash/ambient/ambient_view_delegate_impl.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/system/power/power_status.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;

namespace ash {

class AmbientBackendController;
class AmbientContainerView;
class AmbientPhotoController;
class AmbientViewDelegateObserver;

// Class to handle all ambient mode functionalities.
class ASH_EXPORT AmbientController
    : public AmbientUiModelObserver,
      public SessionObserver,
      public PowerStatus::Observer,
      public chromeos::PowerManagerClient::Observer {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AmbientController();
  ~AmbientController() override;

  // AmbientUiModelObserver:
  void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) override;

  // SessionObserver:
  void OnLockStateChanged(bool locked) override;

  // PowerStatus::Observer:
  void OnPowerStatusChanged() override;

  // chromeos::PowerManagerClient::Observer:
  void LidEventReceived(chromeos::PowerManagerClient::LidState state,
                        const base::TimeTicks& timestamp) override;
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;
  void ScreenIdleStateChanged(
      const power_manager::ScreenIdleState& idle_state) override;

  void AddAmbientViewDelegateObserver(AmbientViewDelegateObserver* observer);
  void RemoveAmbientViewDelegateObserver(AmbientViewDelegateObserver* observer);

  // Initializes the |container_view_|. Called in |CreateWidget()| to create the
  // contents view.
  std::unique_ptr<AmbientContainerView> CreateContainerView();

  // Invoked to show/close ambient UI in |mode|.
  void ShowUi(AmbientUiMode mode);
  void CloseUi();

  void HideLockScreenUi();

  void ToggleInSessionUi();

  // Returns true if the |container_view_| is currently visible.
  bool IsShown() const;

  // Handles events on the background photo.
  void OnBackgroundPhotoEvents();

  void UpdateUiMode(AmbientUiMode ui_mode);

  void RequestAccessToken(
      AmbientAccessTokenController::AccessTokenCallback callback);

  AmbientBackendModel* GetAmbientBackendModel();

  AmbientBackendController* ambient_backend_controller() {
    return ambient_backend_controller_.get();
  }

  AmbientUiModel* ambient_ui_model() { return &ambient_ui_model_; }

 private:
  class InactivityMonitor;
  friend class AmbientAshTestBase;

  // TODO(meilinw): reuses the lock-screen widget: b/156531168, b/157175030.
  // Creates and shows a full-screen widget responsible for showing
  // the ambient UI.
  void CreateWidget();

  void StartRefreshingImages();
  void StopRefreshingImages();

  // Invoked when the auto-show timer in |InactivityMonitor| gets fired after
  // device being inactive for a specific amount of time.
  void OnAutoShowTimeOut();

  void CleanUpOnClosed();

  void set_backend_controller_for_testing(
      std::unique_ptr<AmbientBackendController> photo_client);

  // Creates (if not created) and acquires |wake_lock_|. Unbalanced call
  // without subsequently |ReleaseWakeLock| will have no effect.
  void AcquireWakeLock();

  // Release |wake_lock_|. Unbalanced release call will have no effect.
  void ReleaseWakeLock();

  // Updates |autoshow_enabled_| flag based on the lid state.
  void OnReceiveSwitchStates(
      base::Optional<chromeos::PowerManagerClient::SwitchStates> switch_states);

  // Invoked to dismiss ambient when the device is suspending.
  void HandleOnSuspend();

  // Invoked to restart the auto-show timer when the device is resuming.
  void HandleOnResume();

  AmbientPhotoController* get_ambient_photo_controller_for_testing() {
    return &ambient_photo_controller_;
  }

  AmbientContainerView* get_container_view_for_testing() {
    return container_view_;
  }

  // Owned by |RootView| of its parent widget.
  AmbientContainerView* container_view_ = nullptr;

  AmbientViewDelegateImpl delegate_{this};
  AmbientUiModel ambient_ui_model_;

  AmbientAccessTokenController access_token_controller_;
  std::unique_ptr<AmbientBackendController> ambient_backend_controller_;
  AmbientPhotoController ambient_photo_controller_;

  // Monitors the device inactivity and controls the auto-show of ambient.
  std::unique_ptr<InactivityMonitor> inactivity_monitor_;

  // Lazily initialized on the first call of |AcquireWakeLock|.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  ScopedObserver<AmbientUiModel, AmbientUiModelObserver>
      ambient_ui_model_observer_{this};
  ScopedObserver<SessionControllerImpl, SessionObserver> session_observer_{
      this};
  ScopedObserver<PowerStatus, PowerStatus::Observer> power_status_observer_{
      this};
  ScopedObserver<chromeos::PowerManagerClient,
                 chromeos::PowerManagerClient::Observer>
      power_manager_client_observer_{this};

  // Whether ambient screen will be brought up from hidden after long device
  // inactivity.
  bool autoshow_enabled_ = false;

  base::WeakPtrFactory<AmbientController> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AmbientController);
};

}  // namespace ash

#endif  // ASH_AMBIENT_AMBIENT_CONTROLLER_H_
