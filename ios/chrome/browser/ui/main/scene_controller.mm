// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/scene_controller.h"

#include "base/bind_helpers.h"
#import "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/post_task.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/url_formatter/url_formatter.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#include "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/application_delegate/url_opener.h"
#import "ios/chrome/app/application_delegate/url_opener_params.h"
#import "ios/chrome/app/application_delegate/user_activity_handler.h"
#include "ios/chrome/app/application_mode.h"
#import "ios/chrome/app/blocking_scene_commands.h"
#import "ios/chrome/app/chrome_overlay_window.h"
#import "ios/chrome/app/deferred_initialization_runner.h"
#import "ios/chrome/app/main_controller_guts.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_browser_agent.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/features.h"
#include "ios/chrome/browser/crash_report/crash_keys_helper.h"
#include "ios/chrome/browser/crash_report/crash_report_helper.h"
#import "ios/chrome/browser/first_run/first_run.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#include "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp_snippets/content_suggestions_scheduler_notifications.h"
#include "ios/chrome/browser/signin/constants.h"
#include "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/ui/authentication/signed_in_accounts_view_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_utils.h"
#import "ios/chrome/browser/ui/blocking_overlay/blocking_overlay_view_controller.h"
#import "ios/chrome/browser/ui/browser_view/browser_view_controller.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/omnibox_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/show_signin_command.h"
#import "ios/chrome/browser/ui/first_run/first_run_util.h"
#import "ios/chrome/browser/ui/first_run/orientation_limiting_navigation_controller.h"
#import "ios/chrome/browser/ui/first_run/welcome_to_chrome_view_controller.h"
#include "ios/chrome/browser/ui/history/history_coordinator.h"
#import "ios/chrome/browser/ui/main/browser_interface_provider.h"
#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#include "ios/chrome/browser/ui/tab_grid/tab_grid_coordinator.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/ui/util/top_view_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/url_loading/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/mailto/mailto_handler_provider.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"
#include "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A rough estimate of the expected duration of a view controller transition
// animation. It's used to temporarily disable mutally exclusive chrome
// commands that trigger a view controller presentation.
const int64_t kExpectedTransitionDurationInNanoSeconds = 0.2 * NSEC_PER_SEC;

// Possible results of snapshotting at the moment the user enters the tab
// switcher. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class EnterTabSwitcherSnapshotResult {
  // Page was loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageLoadingAndSnapshotFailed = 0,
  // Page was loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageLoadingAndSnapshotSucceeded = 1,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // failed.
  kPageNotLoadingAndSnapshotFailed = 2,
  // Page was not loading at the time of the snapshot request, and the snapshot
  // succeeded.
  kPageNotLoadingAndSnapshotSucceeded = 3,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kPageNotLoadingAndSnapshotSucceeded,
};

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dismissed.  This is different than ApplicationMode in
// that it can be set to |NONE| when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// Constants for deferred promo display.
const NSTimeInterval kDisplayPromoDelay = 0.1;

// Key of the UMA IOS.MultiWindow.OpenInNewWindow histogram.
const char kMultiWindowOpenInNewWindowHistogram[] =
    "IOS.MultiWindow.OpenInNewWindow";

}  // namespace

@interface SceneController () <AppStateObserver,
                               UserFeedbackDataSource,
                               SettingsNavigationControllerDelegate,
                               SceneURLLoadingServiceDelegate,
                               WebStateListObserving> {
  std::unique_ptr<WebStateListObserverBridge> _webStateListForwardingObserver;
}

// Navigation View controller for the settings.
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The scene level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) SceneUrlLoadingService* sceneURLLoadingService;

// A flag that keeps track of the UI initialization for the controlled scene.
@property(nonatomic, assign) BOOL hasInitializedUI;

// Returns YES if the settings are presented, either from
// self.settingsNavigationController or from SigninCoordinator.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;

// The tab switcher command and the voice search commands can be sent by views
// that reside in a different UIWindow leading to the fact that the exclusive
// touch property will be ineffective and a command for processing both
// commands may be sent in the same run of the runloop leading to
// inconsistencies. Those two boolean indicate if one of those commands have
// been processed in the last 200ms in order to only allow processing one at
// a time.
// TODO(crbug.com/560296):  Provide a general solution for handling mutually
// exclusive chrome commands sent at nearly the same time.
@property(nonatomic, assign) BOOL isProcessingTabSwitcherCommand;
@property(nonatomic, assign) BOOL isProcessingVoiceSearchCommand;

// If not NONE, the current BVC should be switched to this BVC on completion
// of tab switcher dismissal.
@property(nonatomic, assign)
    TabSwitcherDismissalMode modeToDisplayOnTabSwitcherDismissal;

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
@property(nonatomic, readwrite)
    NTPTabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// TabSwitcher object -- the tab grid.
@property(nonatomic, strong) id<TabSwitcher> tabSwitcher;

// The main coordinator, lazily created the first time it is accessed. Manages
// the main view controller. This property should not be accessed before the
// browser has started up to the FOREGROUND stage.
@property(nonatomic, strong) TabGridCoordinator* mainCoordinator;
// If YES, the tab switcher is currently active.
@property(nonatomic, assign, getter=isTabSwitcherActive)
    BOOL tabSwitcherIsActive;
// YES while animating the dismissal of tab switcher.
@property(nonatomic, assign) BOOL dismissingTabSwitcher;

// Wrangler to handle BVC and tab model creation, access, and related logic.
// Implements faetures exposed from this object through the
// BrowserViewInformation protocol.
@property(nonatomic, strong) BrowserViewWrangler* browserViewWrangler;
// The coordinator used to control sign-in UI flows. Lazily created the first
// time it is accessed.
@property(nonatomic, strong) SigninCoordinator* signinCoordinator;

// The view controller that blocks all interactions with the scene.
@property(nonatomic, strong)
    BlockingOverlayViewController* blockingOverlayViewController;

@end

@implementation SceneController
@synthesize startupParameters = _startupParameters;

- (instancetype)initWithSceneState:(SceneState*)sceneState {
  self = [super init];
  if (self) {
    _sceneState = sceneState;
    [_sceneState addObserver:self];
    [_sceneState.appState addObserver:self];
    // The window is necessary very early in the app/scene lifecycle, so it
    // should be created right away.
    // When multiwindow is supported, the window is created by SceneDelegate,
    // and fetched by SceneState from UIScene's windows.
    if (!IsSceneStartupSupported() && !self.sceneState.window) {
      self.sceneState.window = [[ChromeOverlayWindow alloc]
          initWithFrame:[[UIScreen mainScreen] bounds]];
    }
    _sceneURLLoadingService = new SceneUrlLoadingService();
    _sceneURLLoadingService->SetDelegate(self);

    _webStateListForwardingObserver =
        std::make_unique<WebStateListObserverBridge>(self);
  }
  return self;
}

#pragma mark - Setters and getters

- (TabGridCoordinator*)mainCoordinator {
  if (!_mainCoordinator) {
    // Lazily create the main coordinator.
    TabGridCoordinator* tabGridCoordinator =
        [[TabGridCoordinator alloc] initWithWindow:self.sceneState.window
                        applicationCommandEndpoint:self
                       browsingDataCommandEndpoint:self.mainController];
    tabGridCoordinator.regularBrowser = self.mainInterface.browser;
    tabGridCoordinator.incognitoBrowser = self.incognitoInterface.browser;
    _mainCoordinator = tabGridCoordinator;
  }
  return _mainCoordinator;
}

- (id<BrowserInterface>)mainInterface {
  return self.browserViewWrangler.mainInterface;
}

- (id<BrowserInterface>)currentInterface {
  return self.browserViewWrangler.currentInterface;
}

- (id<BrowserInterface>)incognitoInterface {
  return self.browserViewWrangler.incognitoInterface;
}

- (id<BrowserInterfaceProvider>)interfaceProvider {
  return self.browserViewWrangler;
}

- (BOOL)isSettingsViewPresented {
  return self.settingsNavigationController ||
         self.signinCoordinator.isSettingsViewPresented;
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  AppState* appState = self.sceneState.appState;
  if (appState.isInSafeMode) {
    // Nothing at all should happen in safe mode. Code in
    // appStateDidExitSafeMode will ensure the updates happen once safe mode
    // ends.
    return;
  }
  BOOL initializingUIInColdStart =
      level > SceneActivationLevelBackground && !self.hasInitializedUI;
  if (initializingUIInColdStart) {
    [self initializeUI];
  }

  if (level == SceneActivationLevelForegroundActive) {
    [self presentSignInAccountsViewControllerIfNecessary];
    // Mitigation for crbug.com/1092326, where a nil browser state is passed
    // (presumably because mainInterface is nil as well).
    // TODO(crbug.com/1094916): Handle this more cleanly.
    if (self.mainInterface.browserState) {
      [ContentSuggestionsSchedulerNotifications
          notifyForeground:self.mainInterface.browserState];
    }
    if (IsSceneStartupSupported()) {
      if (@available(iOS 13, *)) {
        // Handle URL opening from
        // |UIWindowSceneDelegate scene:willConnectToSession:options:|.
        for (UIOpenURLContext* context in self.sceneState.connectionOptions
                 .URLContexts) {
          URLOpenerParams* params =
              [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
          [self openTabFromLaunchWithParams:params
                         startupInformation:self.mainController
                                   appState:self.mainController.appState];
        }
        if (self.sceneState.connectionOptions.shortcutItem) {
          [UserActivityHandler
              performActionForShortcutItem:self.sceneState.connectionOptions
                                               .shortcutItem
                         completionHandler:nil
                                 tabOpener:self
                     connectionInformation:self
                        startupInformation:self.mainController
                         interfaceProvider:self.interfaceProvider];
        }

        // See if this scene launched as part of a multiwindow URL opening.
        // If so, load that URL (this also creates a new tab to load the URL
        // in). No other UI will show in this case.
        NSUserActivity* activityWithCompletion;
        for (NSUserActivity* activity in self.sceneState.connectionOptions
                 .userActivities) {
          if (ActivityIsURLLoad(activity)) {
            UrlLoadParams params = LoadParamsFromActivity(activity);
            UrlLoadingBrowserAgent::FromBrowser(self.mainInterface.browser)
                ->Load(params);
          } else if (!activityWithCompletion) {
            // Completion involves user interaction.
            // Only one can be triggered.
            activityWithCompletion = activity;
          }
        }
        if (activityWithCompletion) {
          [UserActivityHandler continueUserActivity:activityWithCompletion
                                applicationIsActive:YES
                                          tabOpener:self
                              connectionInformation:self
                                 startupInformation:self.mainController];
        }
        self.sceneState.connectionOptions = nil;

        // Handle URL opening from
        // |UIWindowSceneDelegate scene:openURLContexts:|.
        if (self.sceneState.URLContextsToOpen) {
          // When multiwindow is supported we already pass the external URLs
          // through the scene state, therefore we do not need to rely on
          // startup parameters.
          [self openURLContexts:self.sceneState.URLContextsToOpen];
          self.sceneState.URLContextsToOpen = nil;
        }
      }
    } else {
      NSDictionary* launchOptions = self.mainController.launchOptions;
      URLOpenerParams* params =
          [[URLOpenerParams alloc] initWithLaunchOptions:launchOptions];
      [self openTabFromLaunchWithParams:params
                     startupInformation:self.mainController
                               appState:self.mainController.appState];
    }

    if (!initializingUIInColdStart && self.tabSwitcherIsActive &&
        [self shouldOpenNTPTabOnActivationOfBrowser:self.currentInterface
                                                        .browser]) {
      DCHECK(!self.dismissingTabSwitcher);
      [self beginDismissingTabSwitcherWithCurrentBrowser:self.mainInterface
                                                             .browser
                                            focusOmnibox:NO];

      OpenNewTabCommand* command = [OpenNewTabCommand commandWithIncognito:NO];
      command.userInitiated = NO;
      Browser* browser = self.currentInterface.browser;
      id<ApplicationCommands> applicationHandler = HandlerForProtocol(
          browser->GetCommandDispatcher(), ApplicationCommands);
      [applicationHandler openURLInNewTab:command];
      [self finishDismissingTabSwitcher];
    }
  }

  if (sceneState.currentOrigin != WindowActivityRestoredOrigin) {
    if (IsMultiwindowSupported()) {
      if (@available(iOS 13, *)) {
        base::UmaHistogramEnumeration(kMultiWindowOpenInNewWindowHistogram,
                                      sceneState.currentOrigin);
      }
    }
  }

  if (self.hasInitializedUI && level == SceneActivationLevelUnattached) {
    [self teardownUI];
  }
}

- (void)sceneStateWillShowModalOverlay:(SceneState*)sceneState {
  [self displayBlockingOverlay];
}

- (void)sceneStateWillHideModalOverlay:(SceneState*)sceneState {
  if (!self.blockingOverlayViewController) {
    return;
  }

  [self.blockingOverlayViewController.view removeFromSuperview];
  self.blockingOverlayViewController = nil;

  // When the scene has displayed the blocking overlay and isn't in foreground
  // when it exits it, the cached app switcher snapshot will have the overlay on
  // it, and therefore needs updating.
  if (sceneState.activationLevel < SceneActivationLevelForegroundInactive) {
    if (@available(iOS 13, *)) {
      if (IsMultiwindowSupported()) {
        DCHECK(sceneState.scene.session);
        [[UIApplication sharedApplication]
            requestSceneSessionRefresh:sceneState.scene.session];
      }
    }
  }
}

// TODO(crbug.com/1072408): factor out into a new class.
- (void)displayBlockingOverlay {
  if (self.blockingOverlayViewController) {
    // The overlay is already displayed, nothing to do.
    return;
  }

  // Make the window visible. This is because in safe mode it's not visible yet.
  if (self.sceneState.window.hidden) {
    [self.sceneState.window makeKeyAndVisible];
  }

  self.blockingOverlayViewController =
      [[BlockingOverlayViewController alloc] init];
  self.blockingOverlayViewController.blockingSceneCommandHandler =
      HandlerForProtocol(self.mainController.appState.appCommandDispatcher,
                         BlockingSceneCommands);
  UIView* overlayView = self.blockingOverlayViewController.view;
  [self.sceneState.window addSubview:overlayView];
  overlayView.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.sceneState.window, overlayView);
}

- (void)presentSignInAccountsViewControllerIfNecessary {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  DCHECK(browserState);
  if ([SignedInAccountsViewController
          shouldBePresentedForBrowserState:browserState]) {
    [self presentSignedInAccountsViewControllerForBrowserState:browserState];
  }
}

- (void)sceneState:(SceneState*)sceneState
    hasPendingURLs:(NSSet<UIOpenURLContext*>*)URLContexts
    API_AVAILABLE(ios(13)) {
  if (URLContexts &&
      sceneState.activationLevel == SceneActivationLevelForegroundActive) {
    // It is necessary to reset the URLContextsToOpen after opening them.
    // Handle the opening asynchronously to avoid interfering with potential
    // other observers.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self openURLContexts:sceneState.URLContextsToOpen];
      self.sceneState.URLContextsToOpen = nil;
    });
  }
}

- (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
    API_AVAILABLE(ios(13)) {
  [UserActivityHandler performActionForShortcutItem:shortcutItem
                                  completionHandler:completionHandler
                                          tabOpener:self
                              connectionInformation:self
                                 startupInformation:self.mainController
                                  interfaceProvider:self.interfaceProvider];
}

- (void)sceneState:(SceneState*)sceneState
    receivedUserActivity:(NSUserActivity*)userActivity {
  if (self.mainController.appState.isInSafeMode || !userActivity) {
    return;
  }
  BOOL sceneIsActive =
      self.sceneState.activationLevel >= SceneActivationLevelForegroundActive;
  [UserActivityHandler continueUserActivity:userActivity
                        applicationIsActive:sceneIsActive
                                  tabOpener:self
                      connectionInformation:self
                         startupInformation:self.mainController];
  // It is necessary to reset the pendingUserActivity after handling it.
  // Handle the reset asynchronously to avoid interfering with other observers.
  dispatch_async(dispatch_get_main_queue(), ^{
    self.sceneState.pendingUserActivity = nil;
  });
}

#pragma mark - AppStateObserver

- (void)appStateDidExitSafeMode:(AppState*)appState {
  // All events were postponed in safe mode. Resend them.
  [self sceneState:self.sceneState
      transitionedToActivationLevel:self.sceneState.activationLevel];
}

#pragma mark - SceneControllerGuts
- (void)initializeUI {
  if (self.hasInitializedUI) {
    return;
  }

  DCHECK(self.mainController);
  if (IsSceneStartupSupported()) {
    // TODO(crbug.com/1012697): This should probably be the only code path for
    // UIScene and non-UIScene cases.
    [self startUpChromeUIPostCrash:NO needRestoration:NO];
  }

  self.hasInitializedUI = YES;
}

#pragma mark - private

// Starts up a single chrome window and its UI.
- (void)startUpChromeUIPostCrash:(BOOL)isPostCrashLaunch
                 needRestoration:(BOOL)needsRestoration {
  DCHECK(!self.browserViewWrangler);
  DCHECK(self.sceneURLLoadingService);
  DCHECK(self.mainController);
  DCHECK(self.mainController.mainBrowserState);

  self.browserViewWrangler = [[BrowserViewWrangler alloc]
             initWithBrowserState:self.mainController.mainBrowserState
       applicationCommandEndpoint:self
      browsingDataCommandEndpoint:self.mainController];

  if (IsMultiwindowSupported()) {
    if (@available(iOS 13, *)) {
      self.browserViewWrangler.sessionID =
          self.sceneState.scene.session.persistentIdentifier;
    }
  }

  // Ensure the main browser is created. This also creates the BVC.
  [self.browserViewWrangler createMainBrowser];

  // Only create the restoration helper if the browser state was backed up
  // successfully.
  if (needsRestoration) {
    self.mainController.restoreHelper =
        [[CrashRestoreHelper alloc] initWithBrowser:self.mainInterface.browser];
  }

  // Before bringing up the UI, make sure the launch mode is correct, and
  // check for previous crashes.
  BOOL startInIncognito =
      [[NSUserDefaults standardUserDefaults] boolForKey:kIncognitoCurrentKey];
  BOOL switchFromIncognito =
      startInIncognito && ![self.mainController canLaunchInIncognito];

  if (isPostCrashLaunch || switchFromIncognito) {
    [self clearIOSSpecificIncognitoData];
    if (switchFromIncognito)
      [self.browserViewWrangler
          switchGlobalStateToMode:ApplicationMode::NORMAL];
  }
  if (switchFromIncognito)
    startInIncognito = NO;

  [self createInitialUI:(startInIncognito ? ApplicationMode::INCOGNITO
                                          : ApplicationMode::NORMAL)];

  if (!self.startupParameters) {
    // The startup parameters may create new tabs or navigations. If the restore
    // infobar is displayed now, it may be dismissed immediately and the user
    // will never be able to restore the session.
    [self.mainController.restoreHelper showRestorePrompt];
    self.mainController.restoreHelper = nil;
  }
}

// Determines which UI should be shown on startup, and shows it.
- (void)createInitialUI:(ApplicationMode)launchMode {
  DCHECK(self.mainController.mainBrowserState);

  // Set the Scene application URL loader on the URL loading browser interface
  // for the regular and incognito interfaces. This will lazily instantiate the
  // incognito interface if it isn't already created.
  UrlLoadingBrowserAgent::FromBrowser(self.mainInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  // Observe the web state lists for both browsers.
  self.mainInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  // Enables UI initializations to query the keyWindow's size.
  [self.sceneState.window makeKeyAndVisible];

  // Lazy init of mainCoordinator.
  [self.mainCoordinator start];

  self.tabSwitcher = self.mainCoordinator.tabSwitcher;
  // Call -restoreInternalState so that the grid shows the correct panel.
  [_tabSwitcher
      restoreInternalStateWithMainBrowser:self.mainInterface.browser
                               otrBrowser:self.incognitoInterface.browser
                            activeBrowser:self.currentInterface.browser];

  // Decide if the First Run UI needs to run.
  const bool firstRun = ShouldPresentFirstRunExperience();

  [self.browserViewWrangler switchGlobalStateToMode:launchMode];

  Browser* browser;
  if (launchMode == ApplicationMode::INCOGNITO) {
    browser = self.incognitoInterface.browser;
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  } else {
    browser = self.mainInterface.browser;
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }

  // Figure out what UI to show initially.

  if (self.tabSwitcherIsActive) {
    DCHECK(!self.dismissingTabSwitcher);
    [self
        beginDismissingTabSwitcherWithCurrentBrowser:self.mainInterface.browser
                                        focusOmnibox:NO];
    [self finishDismissingTabSwitcher];
  }

  // If this is first run, or if this web state list should have an NTP created
  // when it activates, then create that tab.
  if (firstRun || [self shouldOpenNTPTabOnActivationOfBrowser:browser]) {
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithIncognito:self.currentInterface.incognito];
    command.userInitiated = NO;
    Browser* browser = self.currentInterface.browser;
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];
  }

  // If this is first run, show the first run UI on top of the new tab.
  // If this isn't first run, check if the sign-in promo needs to display.
  if (firstRun) {
    if (self.mainController.isPresentingFirstRunUI) {
      [self displayBlockingOverlay];
    } else {
      [self.mainController prepareForFirstRunUI:self.sceneState];
      [self showFirstRunUI];
      // Do not ever show the 'restore' infobar during first run.
      self.mainController.restoreHelper = nil;
    }

  } else {
    [self scheduleShowPromo];
  }
}

// This method completely destroys all of the UI. It should be called when the
// scene is disconnected.
- (void)teardownUI {
  if (!self.hasInitializedUI) {
    return;  // Nothing to do.
  }

  // The UI should be stopped before the models they observe are stopped.
  [self.signinCoordinator
      interruptWithAction:SigninCoordinatorInterruptActionNoDismiss
               completion:nil];
  self.signinCoordinator = nil;

  [self.historyCoordinator stop];
  self.historyCoordinator = nil;

  [_mainCoordinator stop];
  _mainCoordinator = nil;

  // Stop observing web state list changes before tearing down the web state
  // lists.
  self.mainInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());

  [self.browserViewWrangler shutdown];
  self.browserViewWrangler = nil;

  self.hasInitializedUI = NO;
}

#pragma mark - First Run

// Initializes the first run UI and presents it to the user.
- (void)showFirstRunUI {
  DCHECK(!self.signinCoordinator);
  // Register for the first run dismissal notification to reset
  // |sceneState.presentingFirstRunUI| flag;
  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(handleFirstRunUIWillFinish)
             name:kChromeFirstRunUIWillFinishNotification
           object:nil];
  Browser* browser = self.mainInterface.browser;
  id<ApplicationCommands, BrowsingDataCommands> welcomeHandler =
      static_cast<id<ApplicationCommands, BrowsingDataCommands>>(
          browser->GetCommandDispatcher());

  WelcomeToChromeViewController* welcomeToChrome =
      [[WelcomeToChromeViewController alloc]
          initWithBrowser:browser
                presenter:self.mainInterface.bvc
               dispatcher:welcomeHandler];
  UINavigationController* navController =
      [[OrientationLimitingNavigationController alloc]
          initWithRootViewController:welcomeToChrome];
  [navController setModalTransitionStyle:UIModalTransitionStyleCrossDissolve];
  navController.modalPresentationStyle = UIModalPresentationFullScreen;
  CGRect appFrame = [[UIScreen mainScreen] bounds];
  [[navController view] setFrame:appFrame];
  self.sceneState.presentingFirstRunUI = YES;
  [self.mainInterface.viewController presentViewController:navController
                                                  animated:NO
                                                completion:nil];
}

- (void)handleFirstRunUIWillFinish {
  DCHECK(self.sceneState.presentingFirstRunUI);
  self.sceneState.presentingFirstRunUI = NO;
  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:kChromeFirstRunUIWillFinishNotification
              object:nil];
  if (self.startupParameters) {
    UrlLoadParams params =
        UrlLoadParams::InNewTab(self.startupParameters.externalURL);
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                    NORMAL
                              withUrlLoadParams:params
                                 dismissOmnibox:YES
                                     completion:^{
                                       [self setStartupParameters:nil];
                                     }];
  }
}

#pragma mark - Promo support

// Schedules presentation of the first eligible promo found, if any.
- (void)scheduleShowPromo {
  // Don't show promos if first run is shown.  (Note:  This flag is only YES
  // while the first run UI is visible.  However, as this function is called
  // immediately after the UI is shown, it's a safe check.)
  if (self.sceneState.presentingFirstRunUI)
    return;
  // Don't show promos in Incognito mode.
  if (self.currentInterface == self.incognitoInterface)
    return;
  // Don't show promos if the app was launched from a URL.
  if (self.startupParameters)
    return;

  // Show the sign-in promo if needed
  if (SigninShouldPresentUserSigninUpgrade(
          self.mainController.mainBrowserState)) {
    Browser* browser = self.mainInterface.browser;
    self.signinCoordinator = [SigninCoordinator
        upgradeSigninPromoCoordinatorWithBaseViewController:self.mainInterface
                                                                .viewController
                                                    browser:browser];

    __weak SceneController* weakSelf = self;
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      static_cast<int64_t>(kDisplayPromoDelay * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          [weakSelf startSigninCoordinatorWithCompletion:nil];
        });
  }
}

#pragma mark - ApplicationCommands

- (void)dismissModalDialogs {
  [self dismissModalDialogsWithCompletion:nil dismissOmnibox:YES];
}

- (void)showHistory {
  self.historyCoordinator = [[HistoryCoordinator alloc]
      initWithBaseViewController:self.currentInterface.viewController
                         browser:self.mainInterface.browser];
  self.historyCoordinator.loadStrategy =
      self.currentInterface.incognito ? UrlLoadStrategy::ALWAYS_IN_INCOGNITO
                                      : UrlLoadStrategy::NORMAL;
  [self.historyCoordinator start];
}

// Opens an url from a link in the settings UI.
- (void)closeSettingsUIAndOpenURL:(OpenNewTabCommand*)command {
  [self openUrlFromSettings:command];
}

- (void)closeSettingsUI {
  [self closeSettingsAnimated:YES completion:nullptr];
}

- (void)prepareTabSwitcher {
  web::WebState* currentWebState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (currentWebState) {
    BOOL loading = currentWebState->IsLoading();
    SnapshotTabHelper::FromWebState(currentWebState)
        ->UpdateSnapshotWithCallback(^(UIImage* snapshot) {
          EnterTabSwitcherSnapshotResult snapshotResult;
          if (loading && !snapshot) {
            snapshotResult =
                EnterTabSwitcherSnapshotResult::kPageLoadingAndSnapshotFailed;
          } else if (loading && snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageLoadingAndSnapshotSucceeded;
          } else if (!loading && !snapshot) {
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotFailed;
          } else {
            DCHECK(!loading && snapshot);
            snapshotResult = EnterTabSwitcherSnapshotResult::
                kPageNotLoadingAndSnapshotSucceeded;
          }
          UMA_HISTOGRAM_ENUMERATION("IOS.EnterTabSwitcherSnapshotResult",
                                    snapshotResult);
        });
  }
  [self.mainCoordinator prepareToShowTabSwitcher:self.tabSwitcher];
}

- (void)displayTabSwitcher {
  DCHECK(!self.tabSwitcherIsActive);
  if (!self.isProcessingVoiceSearchCommand) {
    [self.currentInterface.bvc userEnteredTabSwitcher];
    [self showTabSwitcher];
    self.isProcessingTabSwitcherCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingTabSwitcherCommand = NO;
                   });
  }
}

// TODO(crbug.com/779791) : Remove showing settings from MainController.
- (void)showAutofillSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController)
    return;

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)showReportAnIssueFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(baseViewController);
  // This dispatch is necessary to give enough time for the tools menu to
  // disappear before taking a screenshot.
  dispatch_async(dispatch_get_main_queue(), ^{
    DCHECK(!self.signinCoordinator);
    if (self.settingsNavigationController)
      return;
    Browser* browser = self.mainInterface.browser;
    self.settingsNavigationController =
        [SettingsNavigationController userFeedbackControllerForBrowser:browser
                                                              delegate:self
                                                    feedbackDataSource:self
                                                            dispatcher:self];
    [baseViewController presentViewController:self.settingsNavigationController
                                     animated:YES
                                   completion:nil];
  });
}

- (void)openURLInNewTab:(OpenNewTabCommand*)command {
  UrlLoadParams params =
      UrlLoadParams::InNewTab(command.URL, command.virtualURL);
  params.SetInBackground(command.inBackground);
  params.web_params.referrer = command.referrer;
  params.in_incognito = command.inIncognito;
  params.append_to = command.appendTo;
  params.origin_point = command.originPoint;
  params.from_chrome = command.fromChrome;
  params.user_initiated = command.userInitiated;
  params.should_focus_omnibox = command.shouldFocusOmnibox;
  self.sceneURLLoadingService->LoadUrlInNewTab(params);
}

// TODO(crbug.com/779791) : Do not pass |baseViewController| through dispatcher.
- (void)showSignin:(ShowSigninCommand*)command
    baseViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  Browser* mainBrowser = self.mainInterface.browser;

  switch (command.operation) {
    case AUTHENTICATION_OPERATION_REAUTHENTICATE:
      self.signinCoordinator = [SigninCoordinator
          reAuthenticationCoordinatorWithBaseViewController:baseViewController
                                                    browser:mainBrowser
                                                accessPoint:command.accessPoint
                                                promoAction:command
                                                                .promoAction];
      break;
    case AUTHENTICATION_OPERATION_SIGNIN:
      self.signinCoordinator = [SigninCoordinator
          userSigninCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                             identity:command.identity
                                          accessPoint:command.accessPoint
                                          promoAction:command.promoAction];
      break;
    case AUTHENTICATION_OPERATION_ADD_ACCOUNT:
      self.signinCoordinator = [SigninCoordinator
          addAccountCoordinatorWithBaseViewController:baseViewController
                                              browser:mainBrowser
                                          accessPoint:command.accessPoint];
      break;
  }
  [self startSigninCoordinatorWithCompletion:command.callback];
}

- (void)showAdvancedSigninSettingsFromViewController:
    (UIViewController*)baseViewController {
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:baseViewController
                                                      browser:mainBrowser];
  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)
    showTrustedVaultReauthenticationFromViewController:
        (UIViewController*)baseViewController
                                      retrievalTrigger:
                                          (syncer::KeyRetrievalTriggerForUMA)
                                              retrievalTrigger {
  Browser* mainBrowser = self.mainInterface.browser;
  self.signinCoordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordiantorWithBaseViewController:
          baseViewController
                                                            browser:mainBrowser
                                                   retrievalTrigger:
                                                       retrievalTrigger];
  [self startSigninCoordinatorWithCompletion:nil];
}

// TODO(crbug.com/779791) : Remove settings commands from MainController.
- (void)showAddAccountFromViewController:(UIViewController*)baseViewController {
  self.signinCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:baseViewController
                                          browser:self.mainInterface.browser
                                      accessPoint:signin_metrics::AccessPoint::
                                                      ACCESS_POINT_UNKNOWN];

  [self startSigninCoordinatorWithCompletion:nil];
}

- (void)setIncognitoContentVisible:(BOOL)incognitoContentVisible {
  _incognitoContentVisible = incognitoContentVisible;
}

- (void)startVoiceSearch {
  if (!self.isProcessingTabSwitcherCommand) {
    [self startVoiceSearchInCurrentBVC];
    self.isProcessingVoiceSearchCommand = YES;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 kExpectedTransitionDurationInNanoSeconds),
                   dispatch_get_main_queue(), ^{
                     self.isProcessingVoiceSearchCommand = NO;
                   });
  }
}

- (void)showSettingsFromViewController:(UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController)
    return;
  [[DeferredInitializationRunner sharedInstance]
      runBlockIfNecessary:kPrefObserverInit];

  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController mainSettingsControllerForBrowser:browser
                                                            delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

- (void)openNewWindowWithActivity:(NSUserActivity*)userActivity {
  if (!IsMultiwindowSupported())
    return;  // silent no-op.

  if (@available(iOS 13, *)) {
    UISceneActivationRequestOptions* options =
        [[UISceneActivationRequestOptions alloc] init];
    options.requestingScene = self.sceneState.scene;

    [UIApplication.sharedApplication
        requestSceneSessionActivation:nil /* make a new scene */
                         userActivity:userActivity
                              options:options
                         errorHandler:nil];
  }
}

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Remove show settings from MainController.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.currentInterface.incognito) {
    NOTREACHED();
    return;
  }
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showAccountsSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController accountsControllerForBrowser:browser
                                                        delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove Google services settings from MainController.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (!baseViewController) {
    DCHECK_EQ(self.currentInterface.viewController,
              self.mainCoordinator.activeViewController);
    baseViewController = self.currentInterface.viewController;
  }

  if (self.settingsNavigationController) {
    // Navigate to the Google services settings if the settings dialog is
    // already opened.
    [self.settingsNavigationController
        showGoogleServicesSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController googleServicesControllerForBrowser:browser
                                                              delegate:self];

  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSyncPassphraseSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController syncPassphraseControllerForBrowser:browser
                                                              delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showSavedPasswordsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if (!baseViewController) {
    // TODO(crbug.com/779791): Don't pass base view controller through
    // dispatched command.
    baseViewController = self.currentInterface.viewController;
  }
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showSavedPasswordsSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController =
      [SettingsNavigationController savePasswordsControllerForBrowser:browser
                                                             delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showProfileSettingsFromViewController:baseViewController];
    return;
  }
  Browser* browser = self.mainInterface.browser;

  self.settingsNavigationController =
      [SettingsNavigationController autofillProfileControllerForBrowser:browser
                                                               delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

// TODO(crbug.com/779791) : Remove show settings commands from MainController.
- (void)showCreditCardSettingsFromViewController:
    (UIViewController*)baseViewController {
  DCHECK(!self.signinCoordinator);
  if (self.settingsNavigationController) {
    [self.settingsNavigationController
        showCreditCardSettingsFromViewController:baseViewController];
    return;
  }

  Browser* browser = self.mainInterface.browser;
  self.settingsNavigationController = [SettingsNavigationController
      autofillCreditCardControllerForBrowser:browser
                                    delegate:self];
  [baseViewController presentViewController:self.settingsNavigationController
                                   animated:YES
                                 completion:nil];
}

#pragma mark - ApplicationCommandsHelpers

- (void)openUrlFromSettings:(OpenNewTabCommand*)command {
  DCHECK([command fromChrome]);
  UrlLoadParams params = UrlLoadParams::InNewTab([command URL]);
  params.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;
  ProceduralBlock completion = ^{
    [self dismissModalsAndOpenSelectedTabInMode:ApplicationModeForTabOpening::
                                                    NORMAL
                              withUrlLoadParams:params
                                 dismissOmnibox:YES
                                     completion:nil];
  };
  [self closeSettingsAnimated:YES completion:completion];
}

#pragma mark - UserFeedbackDataSource

- (BOOL)currentPageIsIncognito {
  return self.currentInterface.incognito;
}

- (NSString*)currentPageDisplayURL {
  if (self.tabSwitcherIsActive)
    return nil;
  web::WebState* webState =
      self.currentInterface.browser->GetWebStateList()->GetActiveWebState();
  if (!webState)
    return nil;
  // Returns URL of browser tab that is currently showing.
  GURL url = webState->GetVisibleURL();
  base::string16 urlText = url_formatter::FormatUrl(url);
  return base::SysUTF16ToNSString(urlText);
}

- (UIImage*)currentPageScreenshot {
  UIView* lastView = self.mainCoordinator.activeViewController.view;
  DCHECK(lastView);
  CGFloat scale = 0.0;
  // For screenshots of the tab switcher we need to use a scale of 1.0 to avoid
  // spending too much time since the tab switcher can have lots of subviews.
  if (self.tabSwitcherIsActive)
    scale = 1.0;
  return CaptureView(lastView, scale);
}

- (NSString*)currentPageSyncedUserName {
  ChromeBrowserState* browserState = self.currentInterface.browserState;
  if (browserState->IsOffTheRecord())
    return nil;
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForBrowserState(browserState);
  std::string username = identity_manager->GetPrimaryAccountInfo().email;
  return username.empty() ? nil : base::SysUTF8ToNSString(username);
}

#pragma mark - SettingsNavigationControllerDelegate

- (void)closeSettings {
  [self closeSettingsUI];
}

- (void)settingsWasDismissed {
  [self.settingsNavigationController cleanUpSettings];
  self.settingsNavigationController = nil;
}

- (id<ApplicationCommands, BrowserCommands>)handlerForSettings {
  // Assume that settings always wants the dispatcher from the main BVC.
  return static_cast<id<ApplicationCommands, BrowserCommands>>(
      self.mainInterface.browser->GetCommandDispatcher());
}

#pragma mark - TabSwitcherDelegate

- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    shouldFinishWithBrowser:(Browser*)browser
               focusOmnibox:(BOOL)focusOmnibox {
  [self beginDismissingTabSwitcherWithCurrentBrowser:browser
                                        focusOmnibox:focusOmnibox];
}

- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  [self finishDismissingTabSwitcher];
}

// Begins the process of dismissing the tab switcher with the given current
// model, switching which BVC is suspended if necessary, but not updating the
// UI.  The omnibox will be focused after the tab switcher dismissal is
// completed if |focusOmnibox| is YES.
- (void)beginDismissingTabSwitcherWithCurrentBrowser:(Browser*)browser
                                        focusOmnibox:(BOOL)focusOmnibox {
  DCHECK(browser == self.mainInterface.browser ||
         browser == self.incognitoInterface.browser);

  self.dismissingTabSwitcher = YES;
  ApplicationMode mode = (browser == self.mainInterface.browser)
                             ? ApplicationMode::NORMAL
                             : ApplicationMode::INCOGNITO;
  [self setCurrentInterfaceForMode:mode];

  // The call to set currentBVC above does not actually display the BVC, because
  // _dismissingTabSwitcher is YES.  So: Force the BVC transition to start.
  [self displayCurrentBVCAndFocusOmnibox:focusOmnibox];
}

// Completes the process of dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishDismissingTabSwitcher {
  // In real world devices, it is possible to have an empty tab model at the
  // finishing block of a BVC presentation animation. This can happen when the
  // following occur: a) There is JS that closes the last incognito tab, b) that
  // JS was paused while the user was in the tab switcher, c) the user enters
  // the tab, activating the JS while the tab is being presented. Effectively,
  // the BVC finishes the presentation animation, but there are no tabs to
  // display. The only appropriate action is to dismiss the BVC and return the
  // user to the tab switcher.
  if (self.currentInterface.browser &&
      self.currentInterface.browser->GetWebStateList() &&
      self.currentInterface.browser->GetWebStateList()->count() == 0U) {
    self.tabSwitcherIsActive = NO;
    self.dismissingTabSwitcher = NO;
    self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;
    self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
    [self showTabSwitcher];
    return;
  }

  // The tab switcher dismissal animation runs
  // as part of the BVC presentation process.  The BVC is presented before the
  // animations begin, so it should be the current active VC at this point.
  DCHECK_EQ(self.mainCoordinator.activeViewController,
            self.currentInterface.viewController);

  if (self.modeToDisplayOnTabSwitcherDismissal ==
      TabSwitcherDismissalMode::NORMAL) {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  } else if (self.modeToDisplayOnTabSwitcherDismissal ==
             TabSwitcherDismissalMode::INCOGNITO) {
    [self setCurrentInterfaceForMode:ApplicationMode::INCOGNITO];
  }

  self.modeToDisplayOnTabSwitcherDismissal = TabSwitcherDismissalMode::NONE;

  ProceduralBlock action = [self completionBlockForTriggeringAction:
                                     self.NTPActionAfterTabSwitcherDismissal];
  self.NTPActionAfterTabSwitcherDismissal = NO_ACTION;
  if (action) {
    action();
  }

  self.tabSwitcherIsActive = NO;
  self.dismissingTabSwitcher = NO;
}

#pragma mark Tab opening utility methods.

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action {
  switch (action) {
    case START_VOICE_SEARCH:
      return ^{
        [self startVoiceSearchInCurrentBVC];
      };
    case START_QR_CODE_SCANNER:
      return ^{
        id<QRScannerCommands> QRHandler = HandlerForProtocol(
            self.currentInterface.browser->GetCommandDispatcher(),
            QRScannerCommands);
        [QRHandler showQRScanner];
      };
    case FOCUS_OMNIBOX:
      return ^{
        id<OmniboxCommands> focusHandler = HandlerForProtocol(
            self.currentInterface.browser->GetCommandDispatcher(),
            OmniboxCommands);
        [focusHandler focusOmnibox];
      };
    default:
      return nil;
  }
}

// Starts a voice search on the current BVC.
- (void)startVoiceSearchInCurrentBVC {
  // If the background (non-current) BVC is playing TTS audio, call
  // -startVoiceSearch on it to stop the TTS.
  BrowserViewController* backgroundBVC =
      self.mainInterface == self.currentInterface ? self.incognitoInterface.bvc
                                                  : self.mainInterface.bvc;
  if (backgroundBVC.playingTTS)
    [backgroundBVC startVoiceSearch];
  else
    [self.currentInterface.bvc startVoiceSearch];
}

#pragma mark - TabSwitching

- (BOOL)openNewTabFromTabSwitcher {
  if (!self.tabSwitcher)
    return NO;

  UrlLoadParams urlLoadParams =
      UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL));
  urlLoadParams.web_params.transition_type = ui::PAGE_TRANSITION_TYPED;

  Browser* mainBrowser = self.mainInterface.browser;
  WebStateList* webStateList = mainBrowser->GetWebStateList();
  [self.tabSwitcher dismissWithNewTabAnimationToBrowser:mainBrowser
                                      withUrlLoadParams:urlLoadParams
                                                atIndex:webStateList->count()];
  return YES;
}

#pragma mark - TabOpening implementation.

- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion {
  UrlLoadParams copyOfUrlLoadParams = urlLoadParams;
  [self
      dismissModalDialogsWithCompletion:^{
        [self openSelectedTabInMode:targetMode
                  withUrlLoadParams:copyOfUrlLoadParams
                         completion:completion];
      }
                         dismissOmnibox:dismissOmnibox];
}

- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation
                           appState:(AppState*)appState {
  if (params) {
    BOOL sceneIsActive =
        self.sceneState.activationLevel >= SceneActivationLevelForegroundActive;

    [URLOpener handleLaunchOptions:params
                 applicationActive:sceneIsActive
                         tabOpener:self
             connectionInformation:self
                startupInformation:startupInformation
                          appState:appState];
  }
}

- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL {
  WebStateList* webStateList = self.mainInterface.browser->GetWebStateList();
  return webStateList && webStateList->GetIndexOfWebStateWithURL(URL) !=
                             WebStateList::kInvalidIndex;
}

- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser {
  if (self.tabSwitcherIsActive) {
    Browser* mainBrowser = self.mainInterface.browser;
    Browser* otrBrowser = self.incognitoInterface.browser;
    // Only attempt to dismiss the tab switcher and open a new tab if:
    // - there are no tabs open in either tab model, and
    // - the tab switcher controller is not directly or indirectly presenting
    // another view controller.
    if (!(mainBrowser->GetWebStateList()->empty()) ||
        !(otrBrowser->GetWebStateList()->empty()))
      return NO;

    // If the tabSwitcher is contained, check if the parent container is
    // presenting another view controller.
    if ([[self.tabSwitcher viewController]
                .parentViewController presentedViewController]) {
      return NO;
    }

    // Check if the tabSwitcher is directly presenting another view controller.
    if ([self.tabSwitcher viewController].presentedViewController) {
      return NO;
    }

    return YES;
  }
  return browser->GetWebStateList()->empty() &&
         !(browser->GetBrowserState()->IsOffTheRecord());
}

#pragma mark - SceneURLLoadingServiceDelegate

// Note that the current tab of |browserCoordinator|'s BVC will normally be
// reloaded by this method. If a new tab is about to be added, call
// expectNewForegroundTab on the BVC first to avoid extra work and possible page
// load side-effects for the tab being replaced.
- (void)setCurrentInterfaceForMode:(ApplicationMode)mode {
  DCHECK(self.interfaceProvider);
  BOOL incognitio = mode == ApplicationMode::INCOGNITO;
  id<BrowserInterface> currentInterface =
      self.interfaceProvider.currentInterface;
  id<BrowserInterface> newInterface =
      incognitio ? self.interfaceProvider.incognitoInterface
                 : self.interfaceProvider.mainInterface;
  if (currentInterface && currentInterface == newInterface)
    return;

  // Update the snapshot before switching another application mode.  This
  // ensures that the snapshot is correct when links are opened in a different
  // application mode.
  [self updateActiveWebStateSnapshot];

  self.interfaceProvider.currentInterface = newInterface;

  if (!self.dismissingTabSwitcher)
    [self displayCurrentBVCAndFocusOmnibox:NO];

  // Tell the BVC that was made current that it can use the web.
  [self activateBVCAndMakeCurrentBVCPrimary];
}

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox {
  // Immediately hide modals from the provider (alert views, action sheets,
  // popovers). They will be ultimately dismissed by their owners, but at least,
  // they are not visible.
  ios::GetChromeBrowserProvider()->HideModalViewStack();

  // ChromeIdentityService is responsible for the dialogs displayed by the
  // services it wraps.
  ios::GetChromeBrowserProvider()->GetChromeIdentityService()->DismissDialogs();

  // MailtoHandlerProvider is responsible for the dialogs displayed by the
  // services it wraps.
  ios::GetChromeBrowserProvider()
      ->GetMailtoHandlerProvider()
      ->DismissAllMailtoHandlerInterfaces();

  // Then, depending on what the SSO view controller is presented on, dismiss
  // it.
  ProceduralBlock completionWithBVC = ^{
    DCHECK(self.currentInterface.viewController);
    DCHECK(!self.tabSwitcherIsActive);
    DCHECK(!self.signinCoordinator);
    // This will dismiss the SSO view controller.
    [self.interfaceProvider.currentInterface
        clearPresentedStateWithCompletion:completion
                           dismissOmnibox:dismissOmnibox];
  };
  ProceduralBlock completionWithoutBVC = ^{
    // |self.currentInterface.bvc| may exist but tab switcher should be
    // active.
    DCHECK(self.tabSwitcherIsActive);
    DCHECK(!self.signinCoordinator);
    // History coordinator can be started on top of the tab grid.
    // This is not true of the other tab switchers.
    DCHECK(self.mainCoordinator);
    [self.mainCoordinator stopChildCoordinatorsWithCompletion:completion];
  };

  // Select a completion based on whether the BVC is shown.
  ProceduralBlock chosenCompletion =
      self.tabSwitcherIsActive ? completionWithoutBVC : completionWithBVC;

  if (self.isSettingsViewPresented) {
    // In this case, the settings are up and the BVC is showing. Close the
    // settings then call the chosen completion.
    [self closeSettingsAnimated:NO completion:chosenCompletion];
  } else if (self.signinCoordinator) {
    // The sign-in screen is showing, interrupt it and call the chosen
    // completion.
    [self interruptSigninCoordinatorAnimated:NO completion:chosenCompletion];
  } else {
    // Does not require a special case. Run the chosen completion.
    chosenCompletion();
  }

  // Verify that no modal views are left presented.
  ios::GetChromeBrowserProvider()->LogIfModalViewsArePresented();
}

// Opens a tab in the target BVC, and switches to it in a way that's appropriate
// to the current UI, based on the |dismissModals| flag:
// - If a modal dialog is showing and |dismissModals| is NO, the selected tab of
// the main tab model will change in the background, but the view won't change.
// - Otherwise, any modal view will be dismissed, the tab switcher will animate
// out if it is showing, the target BVC will become active, and the new tab will
// be shown.
// If the current tab in |targetMode| is a NTP, it can be reused to open URL.
// |completion| is executed after the tab is opened. After Tab is open the
// virtual URL is set to the pending navigation item.
- (void)openSelectedTabInMode:(ApplicationModeForTabOpening)tabOpeningTargetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion {
  // Update the snapshot before opening a new tab. This ensures that the
  // snapshot is correct when tabs are openned via the dispatcher.
  [self updateActiveWebStateSnapshot];

  ApplicationMode targetMode;

  if (tabOpeningTargetMode == ApplicationModeForTabOpening::CURRENT) {
    targetMode = self.interfaceProvider.currentInterface.incognito
                     ? ApplicationMode::INCOGNITO
                     : ApplicationMode::NORMAL;
  } else if (tabOpeningTargetMode == ApplicationModeForTabOpening::NORMAL) {
    targetMode = ApplicationMode::NORMAL;
  } else {
    targetMode = ApplicationMode::INCOGNITO;
  }

  id<BrowserInterface> targetInterface =
      targetMode == ApplicationMode::NORMAL
          ? self.interfaceProvider.mainInterface
          : self.interfaceProvider.incognitoInterface;
  NSUInteger tabIndex = NSNotFound;
  ProceduralBlock startupCompletion =
      [self completionBlockForTriggeringAction:[self.startupParameters
                                                       postOpeningAction]];

  // Commands are only allowed on NTP.
  DCHECK(IsURLNtp(urlLoadParams.web_params.url) || !startupCompletion);

  ProceduralBlock tabOpenedCompletion = nil;
  if (startupCompletion && completion) {
    tabOpenedCompletion = ^{
      // Order is important here. |completion| may do cleaning tasks that will
      // invalidate |startupCompletion|.
      startupCompletion();
      completion();
    };
  } else if (startupCompletion) {
    tabOpenedCompletion = startupCompletion;
  } else {
    tabOpenedCompletion = completion;
  }

  if (self.tabSwitcherIsActive) {
    // If the tab switcher is already being dismissed, simply add the tab and
    // note that when the tab switcher finishes dismissing, the current BVC
    // should be switched to be the main BVC if necessary.
    if (self.dismissingTabSwitcher) {
      self.modeToDisplayOnTabSwitcherDismissal =
          targetMode == ApplicationMode::NORMAL
              ? TabSwitcherDismissalMode::NORMAL
              : TabSwitcherDismissalMode::INCOGNITO;
      [targetInterface.bvc appendTabAddedCompletion:tabOpenedCompletion];
      UrlLoadParams savedParams = urlLoadParams;
      savedParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
      UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
          ->Load(savedParams);
    } else {
      // Voice search, QRScanner and the omnibox are presented by the BVC.
      // They must be started after the BVC view is added in the hierarchy.
      self.NTPActionAfterTabSwitcherDismissal =
          [self.startupParameters postOpeningAction];
      [self setStartupParameters:nil];
      [self.tabSwitcher
          dismissWithNewTabAnimationToBrowser:targetInterface.browser
                            withUrlLoadParams:urlLoadParams
                                      atIndex:tabIndex];
    }
  } else {
    if (!self.currentInterface.viewController.presentedViewController) {
      [targetInterface.bvc expectNewForegroundTab];
    }
    [self setCurrentInterfaceForMode:targetMode];
    [self openOrReuseTabInMode:targetMode
             withUrlLoadParams:urlLoadParams
           tabOpenedCompletion:tabOpenedCompletion];
  }

  if (self.mainController.restoreHelper) {
    // Now that all the operations on the tabs have been done, display the
    // restore infobar if needed.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self.mainController.restoreHelper showRestorePrompt];
      self.mainController.restoreHelper = nil;
    });
  }
}

- (void)openURLInNewTabWithCommand:(OpenNewTabCommand*)command {
  [self openURLInNewTab:command];
}

- (void)expectNewForegroundTabForMode:(ApplicationMode)targetMode {
  id<BrowserInterface> interface =
      targetMode == ApplicationMode::INCOGNITO
          ? self.interfaceProvider.incognitoInterface
          : self.interfaceProvider.mainInterface;
  DCHECK(interface);
  [interface.bvc expectNewForegroundTab];
}

- (void)openNewTabFromOriginPoint:(CGPoint)originPoint
                     focusOmnibox:(BOOL)focusOmnibox {
  [self.currentInterface.bvc openNewTabFromOriginPoint:originPoint
                                          focusOmnibox:focusOmnibox];
}

- (Browser*)currentBrowserForURLLoading {
  return self.currentInterface.browser;
}

// Asks the respective Snapshot helper to update the snapshot for the active
// WebState.
- (void)updateActiveWebStateSnapshot {
  // Durinhg startup, there may be no current interface. Do nothing in that
  // case.
  if (!self.currentInterface)
    return;

  WebStateList* webStateList = self.currentInterface.browser->GetWebStateList();
  web::WebState* webState = webStateList->GetActiveWebState();
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->UpdateSnapshotWithCallback(nil);
  }
}


// Checks the target BVC's current tab's URL. If this URL is chrome://newtab,
// loads |urlLoadParams| in this tab. Otherwise, open |urlLoadParams| in a new
// tab in the target BVC. |tabDisplayedCompletion| will be called on the new tab
// (if not nil).
- (void)openOrReuseTabInMode:(ApplicationMode)targetMode
           withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
         tabOpenedCompletion:(ProceduralBlock)tabOpenedCompletion {
  id<BrowserInterface> targetInterface = targetMode == ApplicationMode::NORMAL
                                             ? self.mainInterface
                                             : self.incognitoInterface;

  BrowserViewController* targetBVC = targetInterface.bvc;
  web::WebState* currentWebState =
      targetInterface.browser->GetWebStateList()->GetActiveWebState();

  // Don't call loadWithParams for chrome://newtab when it's already loaded.
  // Note that it's safe to use -GetVisibleURL here, as it doesn't matter if the
  // NTP hasn't finished loading.
  if (currentWebState && IsURLNtp(currentWebState->GetVisibleURL()) &&
      IsURLNtp(urlLoadParams.web_params.url)) {
    if (tabOpenedCompletion) {
      tabOpenedCompletion();
    }
    return;
  }

  // With kBrowserContainerContainsNTP enabled paired with a restored NTP
  // session, the NTP may appear committed when it is still loading.  For the
  // time being, always load within a new tab when this feature is enabled.
  // TODO(crbug.com/931284): Revert this change when fixed.
  BOOL alwaysInsertNewTab =
      base::FeatureList::IsEnabled(kBlockNewTabPagePendingLoad);
  // If the current tab isn't an NTP, open a new tab.  Be sure to use
  // -GetLastCommittedURL incase the NTP is still loading.
  if (alwaysInsertNewTab ||
      !(currentWebState && IsURLNtp(currentWebState->GetVisibleURL()))) {
    [targetBVC appendTabAddedCompletion:tabOpenedCompletion];
    UrlLoadParams newTabParams = urlLoadParams;
    newTabParams.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    newTabParams.in_incognito = targetMode == ApplicationMode::INCOGNITO;
    UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
        ->Load(newTabParams);
    return;
  }

  // Otherwise, load |urlLoadParams| in the current tab.
  UrlLoadParams sameTabParams = urlLoadParams;
  sameTabParams.disposition = WindowOpenDisposition::CURRENT_TAB;
  UrlLoadingBrowserAgent::FromBrowser(targetInterface.browser)
      ->Load(sameTabParams);
  if (tabOpenedCompletion) {
    tabOpenedCompletion();
  }
}

// Displays current (incognito/normal) BVC and optionally focuses the omnibox.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox {
  ProceduralBlock completion = nil;
  if (focusOmnibox) {
    id<OmniboxCommands> omniboxHandler = HandlerForProtocol(
        self.currentInterface.browser->GetCommandDispatcher(), OmniboxCommands);
    completion = ^{
      [omniboxHandler focusOmnibox];
    };
  }
  [self.mainCoordinator
      showTabViewController:self.currentInterface.viewController
                 completion:completion];
  [HandlerForProtocol(self.currentInterface.browser->GetCommandDispatcher(),
                      ApplicationCommands)
      setIncognitoContentVisible:self.currentInterface.incognito];
}

#pragma mark - Sign In UI presentation

- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ChromeBrowserState*)browserState {
  UMA_HISTOGRAM_BOOLEAN("Signin.SignedInAccountsViewImpression", true);
  id<ApplicationSettingsCommands> settingsHandler =
      HandlerForProtocol(self.mainInterface.browser->GetCommandDispatcher(),
                         ApplicationSettingsCommands);
  UIViewController* accountsViewController =
      [[SignedInAccountsViewController alloc]
          initWithBrowserState:browserState
                    dispatcher:settingsHandler];
  [[self topPresentedViewController]
      presentViewController:accountsViewController
                   animated:YES
                 completion:nil];
}

- (void)closeSettingsAnimated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  if (self.settingsNavigationController) {
    ProceduralBlock dismissSettings = ^() {
      [self.settingsNavigationController cleanUpSettings];
      UIViewController* presentingViewController =
          [self.settingsNavigationController presentingViewController];
      // If presentingViewController is nil it means the VC was already
      // dismissed by some other action like swiping down.
      DCHECK(presentingViewController);
      [presentingViewController dismissViewControllerAnimated:animated
                                                   completion:completion];
      self.settingsNavigationController = nil;
    };
    // |self.signinCoordinator| can be presented on top of the settings, to
    // present the Trusted Vault reauthentication |self.signinCoordinator| has
    // to be closed first.
    if (self.signinCoordinator) {
      [self interruptSigninCoordinatorAnimated:animated
                                    completion:dismissSettings];
    } else if (dismissSettings) {
      dismissSettings();
    }
    return;
  }
  // |self.signinCoordinator| can also present settings, like
  // the advanced sign-in settings navigation controller. If the settings has
  // to be closed, it is thus the responsibility of the main controller to
  // dismiss the advanced sign-in settings by dismssing the settings
  // presented by |self.signinCoordinator|.
  // To reproduce this case:
  //  - open Bookmark view
  //  - start sign-in
  //  - tap on "Settings" to open the advanced sign-in settings
  //  - tap on "Manage Your Google Account"
  DCHECK(self.signinCoordinator);
  [self interruptSigninCoordinatorAnimated:animated completion:completion];
}

- (UIViewController*)topPresentedViewController {
  // TODO(crbug.com/754642): Implement TopPresentedViewControllerFrom()
  // privately.
  return top_view_controller::TopPresentedViewControllerFrom(
      self.mainCoordinator.viewController);
}

// Interrupts the sign-in coordinator actions and dismisses its views either
// with or without animation.
- (void)interruptSigninCoordinatorAnimated:(BOOL)animated
                                completion:(ProceduralBlock)completion {
  SigninCoordinatorInterruptAction action =
      animated ? SigninCoordinatorInterruptActionDismissWithAnimation
               : SigninCoordinatorInterruptActionDismissWithoutAnimation;
  [self.signinCoordinator interruptWithAction:action completion:completion];
}

// Starts the sign-in coordinator with a default cleanup completion.
- (void)startSigninCoordinatorWithCompletion:
    (signin_ui::CompletionCallback)completion {
  self.mainController.appState.sceneShowingBlockingUI = self.sceneState;
  DCHECK(self.signinCoordinator);
  __weak SceneController* weakSelf = self;
  self.signinCoordinator.signinCompletion =
      ^(SigninCoordinatorResult result, SigninCompletionInfo*) {
        [weakSelf.signinCoordinator stop];
        weakSelf.signinCoordinator = nil;
        weakSelf.mainController.appState.sceneShowingBlockingUI = nil;

        if (completion) {
          completion(result == SigninCoordinatorResultSuccess);
        }
      };

  [self.signinCoordinator start];
}

#pragma mark - WebStateListObserving

// Called when a WebState is removed. Triggers the switcher view when the last
// WebState is closed on a device that uses the switcher.
- (void)webStateList:(WebStateList*)notifiedWebStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  // Do nothing on initialization.
  if (!self.currentInterface.browser)
    return;

  if (notifiedWebStateList->empty()) {
    if (webState->GetBrowserState()->IsOffTheRecord()) {
      [self lastIncognitoTabClosed];
    } else {
      [self lastRegularTabClosed];
    }
  }
}

#pragma mark - Helpers for web state list events

// Called when the last incognito tab was closed.
- (void)lastIncognitoTabClosed {
  // If no other window has incognito tab, then destroy and rebuild the
  // BrowserState. Otherwise, just do the state transition animation.
  if ([self shouldDestroyAndRebuildIncognitoBrowserState]) {
    // Incognito browser state cannot be deleted before all the requests are
    // deleted. Queue empty task on IO thread and destroy the BrowserState
    // when the task has executed.
    base::PostTaskAndReply(FROM_HERE, {web::WebThread::IO}, base::DoNothing(),
                           base::BindRepeating(^{
                             [self destroyAndRebuildIncognitoBrowserState];
                           }));
  }

  // a) The first condition can happen when the last incognito tab is closed
  // from the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the otr tab model when it's not current.
  // Nothing to do here. The next user action (like clicking on an existing
  // regular tab or creating a new incognito tab from the settings menu) will
  // take care of the logic to mode switch.
  if (self.tabSwitcherIsActive || !self.currentInterface.incognito) {
    return;
  }

  WebStateList* currentWebStateList =
      self.currentInterface.browser->GetWebStateList();
  if (currentWebStateList->empty()) {
    [self showTabSwitcher];
  } else {
    [self setCurrentInterfaceForMode:ApplicationMode::NORMAL];
  }
}

// Called when the last regular tab was closed.
- (void)lastRegularTabClosed {
  // a) The first condition can happen when the last regular tab is closed from
  // the tab switcher.
  // b) The second condition can happen if some other code (like JS) triggers
  // closure of tabs from the main tab model when the main tab model is not
  // current.
  // Nothing to do here.
  if (self.tabSwitcherIsActive || self.currentInterface.incognito) {
    return;
  }

  [self showTabSwitcher];
}

// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData {
  DCHECK(self.mainController.mainBrowserState
             ->HasOffTheRecordChromeBrowserState());
  ChromeBrowserState* otrBrowserState =
      self.mainController.mainBrowserState->GetOffTheRecordChromeBrowserState();
  [self.mainController
      removeBrowsingDataForBrowserState:otrBrowserState
                             timePeriod:browsing_data::TimePeriod::ALL_TIME
                             removeMask:BrowsingDataRemoveMask::REMOVE_ALL
                        completionBlock:^{
                          [self activateBVCAndMakeCurrentBVCPrimary];
                        }];
}

- (void)activateBVCAndMakeCurrentBVCPrimary {
  // If there are pending removal operations, the activation will be deferred
  // until the callback is received.
  BrowsingDataRemover* browsingDataRemover =
      BrowsingDataRemoverFactory::GetForBrowserStateIfExists(
          self.currentInterface.browser->GetBrowserState());
  if (browsingDataRemover && browsingDataRemover->IsRemoving())
    return;

  self.interfaceProvider.mainInterface.userInteractionEnabled = YES;
  self.interfaceProvider.incognitoInterface.userInteractionEnabled = YES;
  [self.currentInterface setPrimary:YES];
}

- (void)showTabSwitcher {
  DCHECK(self.tabSwitcher);
  // Tab switcher implementations may need to rebuild state before being
  // displayed.
  [self.tabSwitcher
      restoreInternalStateWithMainBrowser:self.mainInterface.browser
                               otrBrowser:self.incognitoInterface.browser
                            activeBrowser:self.currentInterface.browser];
  self.tabSwitcherIsActive = YES;
  [self.tabSwitcher setDelegate:self];

  [self.mainCoordinator showTabSwitcher:self.tabSwitcher];
}

- (void)openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
    API_AVAILABLE(ios(13)) {
  if (self.mainController.appState.isInSafeMode) {
    return;
  }

  NSMutableSet<URLOpenerParams*>* URLsToOpen = [[NSMutableSet alloc] init];
  for (UIOpenURLContext* context : URLContexts) {
    URLOpenerParams* options =
        [[URLOpenerParams alloc] initWithUIOpenURLContext:context];
    if (!ios::GetChromeBrowserProvider()
             ->GetChromeIdentityService()
             ->HandleApplicationOpenURL([UIApplication sharedApplication],
                                        context.URL,
                                        [options toLaunchOptions])) {
      [URLsToOpen addObject:options];
    }
  }
  // When opening with URLs for GetChromeIdentityService, it is expected that a
  // single URL is passed.
  DCHECK(URLsToOpen.count == URLContexts.count || URLContexts.count == 1);
  BOOL active =
      _sceneState.activationLevel >= SceneActivationLevelForegroundActive;
  for (URLOpenerParams* options : URLsToOpen) {
    [URLOpener openURL:options
            applicationActive:active
                    tabOpener:self
        connectionInformation:self
           startupInformation:self.mainController];
  }
}

#pragma mark - Handling of destroying the incognito BrowserState

// The incognito BrowserState should be closed when the last incognito tab is
// closed (i.e. if there are other incognito tabs open in another Scene, the
// BrowserState must not be destroyed).
- (BOOL)shouldDestroyAndRebuildIncognitoBrowserState {
  ChromeBrowserState* mainBrowserState = self.mainController.mainBrowserState;
  if (!mainBrowserState->HasOffTheRecordChromeBrowserState())
    return NO;

  ChromeBrowserState* otrBrowserState =
      mainBrowserState->GetOffTheRecordChromeBrowserState();
  DCHECK(otrBrowserState);

  BrowserList* browserList =
      BrowserListFactory::GetForBrowserState(otrBrowserState);
  for (Browser* browser : browserList->AllIncognitoBrowsers()) {
    WebStateList* webStateList = browser->GetWebStateList();
    if (!webStateList->empty())
      return NO;
  }

  return YES;
}

// Destroys and rebuilds the incognito BrowserState. This will inform all the
// other SceneController to destroy state tied to the BrowserState and to
// recreate it.
- (void)destroyAndRebuildIncognitoBrowserState {
  // This seems the best place to mark the start of destroying the incognito
  // browser state.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/true);

  [self clearIOSSpecificIncognitoData];

  ChromeBrowserState* mainBrowserState = self.mainController.mainBrowserState;
  DCHECK(mainBrowserState->HasOffTheRecordChromeBrowserState());

  NSMutableArray<SceneController*>* sceneControllers =
      [[NSMutableArray alloc] init];
  for (SceneState* sceneState in self.mainController.appState.connectedScenes) {
    SceneController* sceneController = sceneState.controller;
    if (sceneController.mainController.mainBrowserState == mainBrowserState) {
      [sceneControllers addObject:sceneController];
    }
  }

  for (SceneController* sceneController in sceneControllers) {
    [sceneController willDestroyIncognitoBrowserState];
  }

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    breakpad::StopMonitoringBreadcrumbManagerService(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            mainBrowserState->GetOffTheRecordChromeBrowserState()));
  }

  // Destroy and recreate the off-the-record BrowserState.
  mainBrowserState->DestroyOffTheRecordChromeBrowserState();
  mainBrowserState->GetOffTheRecordChromeBrowserState();

  for (SceneController* sceneController in sceneControllers) {
    [sceneController incognitoBrowserStateCreated];
  }

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    breakpad::MonitorBreadcrumbManagerService(
        BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
            mainBrowserState->GetOffTheRecordChromeBrowserState()));
  }

  // This seems the best place to deem the destroying and rebuilding the
  // incognito browser state to be completed.
  crash_keys::SetDestroyingAndRebuildingIncognitoBrowserState(
      /*in_progress=*/false);
}

- (void)willDestroyIncognitoBrowserState {
  // Clear the Incognito Browser and notify the _tabSwitcher that its otrBrowser
  // will be destroyed.
  [self.tabSwitcher setOtrBrowser:nil];

  if (base::FeatureList::IsEnabled(kLogBreadcrumbs)) {
    BreadcrumbManagerBrowserAgent::FromBrowser(self.incognitoInterface.browser)
        ->SetLoggingEnabled(false);
  }

  self.incognitoInterface.browser->GetWebStateList()->RemoveObserver(
      _webStateListForwardingObserver.get());
  [self.browserViewWrangler willDestroyIncognitoBrowserState];
}

- (void)incognitoBrowserStateCreated {
  [self.browserViewWrangler incognitoBrowserStateCreated];

  // There should be a new URL loading browser agent for the incognito browser,
  // so set the scene URL loading service on it.
  UrlLoadingBrowserAgent::FromBrowser(self.incognitoInterface.browser)
      ->SetSceneService(self.sceneURLLoadingService);
  self.incognitoInterface.browser->GetWebStateList()->AddObserver(
      _webStateListForwardingObserver.get());

  if (self.currentInterface.incognito) {
    [self activateBVCAndMakeCurrentBVCPrimary];
  }

  // Always set the new otr Browser for the tablet or grid switcher.
  // Notify the _tabSwitcher with the new Incognito Browser.
  [self.tabSwitcher setOtrBrowser:self.incognitoInterface.browser];
}

@end
