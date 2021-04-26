// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/content_settings/core/common/features.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/content_settings/host_content_settings_map_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_mediator.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface NewTabPageCoordinator ()

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for incognito.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;
@property(nonatomic, strong) PrivacyCookiesMediator* mediator;

@end

@implementation NewTabPageCoordinator

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  return [super initWithBaseViewController:nil browser:browser];
}

- (void)start {
  if (self.started)
    return;

  DCHECK(self.browser);
  DCHECK(self.webState);
  DCHECK(self.toolbarDelegate);

  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    if (base::FeatureList::IsEnabled(
            content_settings::kImprovedCookieControls)) {
      ChromeBrowserState* originalBrowser =
          self.browser->GetBrowserState()->GetOriginalChromeBrowserState();
      self.mediator = [[PrivacyCookiesMediator alloc]
          initWithPrefService:originalBrowser->GetPrefs()
                  settingsMap:ios::HostContentSettingsMapFactory::
                                  GetForBrowserState(originalBrowser)];
      self.mediator.consumer = self.incognitoViewController;
      self.incognitoViewController.handler = self.mediator;
    }
  } else {
    DCHECK(!self.contentSuggestionsCoordinator);
    self.contentSuggestionsCoordinator = [[ContentSuggestionsCoordinator alloc]
        initWithBaseViewController:nil
                           browser:self.browser];
    self.contentSuggestionsCoordinator.webState = self.webState;
    self.contentSuggestionsCoordinator.toolbarDelegate = self.toolbarDelegate;
    [self.contentSuggestionsCoordinator start];
    base::RecordAction(base::UserMetricsAction("MobileNTPShowMostVisited"));
  }
  self.started = YES;
}

- (void)stop {
  if (!self.started)
    return;
  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.incognitoViewController = nil;
  self.mediator = nil;
  self.started = NO;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  [self start];
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.contentSuggestionsCoordinator.viewController;
  }
}

#pragma mark - Public Methods

- (void)dismissModals {
  [self.contentSuggestionsCoordinator dismissModals];
}

- (UIEdgeInsets)contentInset {
  return [self.contentSuggestionsCoordinator contentInset];
}

- (CGPoint)contentOffset {
  return [self.contentSuggestionsCoordinator contentOffset];
}

- (void)willUpdateSnapshot {
  [self.contentSuggestionsCoordinator willUpdateSnapshot];
}

- (void)focusFakebox {
  [self.contentSuggestionsCoordinator.headerController focusFakebox];
}

- (void)reload {
  [self.contentSuggestionsCoordinator reload];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidBecomeFirstResponder];
}
- (void)locationBarDidResignFirstResponder {
  [self.contentSuggestionsCoordinator locationBarDidResignFirstResponder];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.contentSuggestionsCoordinator
              .headerController logoAnimationControllerOwner];
}

@end
