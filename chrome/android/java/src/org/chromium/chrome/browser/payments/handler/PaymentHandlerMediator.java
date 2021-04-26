// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.os.Handler;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.chrome.browser.payments.SslValidityChecker;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarCoordinator.PaymentHandlerToolbarObserver;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * PaymentHandler mediator, which is responsible for receiving events from the view and notifies the
 * backend (the coordinator).
 */
/* package */ class PaymentHandlerMediator extends WebContentsObserver
        implements BottomSheetObserver, PaymentHandlerToolbarObserver, View.OnLayoutChangeListener {
    // The value is picked in order to allow users to see the tab behind this UI.
    /* package */ static final float FULL_HEIGHT_RATIO = 0.9f;
    /* package */ static final float HALF_HEIGHT_RATIO = 0.5f;

    private final PropertyModel mModel;
    // Whenever invoked, invoked outside of the WebContentsObserver callbacks.
    private final Runnable mHider;
    // Postfixes with "Ref" to distinguish from mWebContent in WebContentsObserver. Although
    // referencing the same object, mWebContentsRef is preferable to WebContents here because
    // mWebContents (a weak ref) requires null checks, while mWebContentsRef is guaranteed to be not
    // null.
    private final WebContents mWebContentsRef;
    private final PaymentHandlerUiObserver mPaymentHandlerUiObserver;
    // Used to postpone execution of a callback to avoid destroy objects (e.g., WebContents) in
    // their own methods.
    private final Handler mHandler = new Handler();
    private final Destroyable mActivityDestroyListener;
    private final ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final View mTabView;
    private final int mToolbarViewHeightPx;
    private final int mContainerTopPaddingPx;
    private @CloseReason int mCloseReason = CloseReason.OTHERS;

    /** A token held while the payment sheet is obscuring all visible tabs. */
    private int mTabObscuringToken = TokenHolder.INVALID_TOKEN;

    @IntDef({CloseReason.OTHERS, CloseReason.USER, CloseReason.ACTIVITY_DIED,
            CloseReason.INSECURE_NAVIGATION, CloseReason.FAIL_LOAD})
    @Retention(RetentionPolicy.SOURCE)
    public @interface CloseReason {
        int OTHERS = 0;
        int USER = 1;
        int ACTIVITY_DIED = 2;
        int INSECURE_NAVIGATION = 3;
        int FAIL_LOAD = 4;
    }

    /**
     * Build a new mediator that handle events from outside the payment handler component.
     * @param model The {@link PaymentHandlerProperties} that holds all the view state for the
     *         payment handler component.
     * @param hider The callback to clean up {@link PaymentHandlerCoordinator} when the sheet is
     *         hidden.
     * @param webContents The web-contents that loads the payment app.
     * @param observer The {@link PaymentHandlerUiObserver} that observes this Payment Handler UI.
     * @param tabView The view of the main tab.
     * @param toolbarViewHeightPx The height of the toolbar view in px.
     * @param containerTopPaddingPx The padding top of bottom_sheet_toolbar_container in px
     * @param activityLifeCycleDispatcher The lifecycle dispatcher of the activity where this UI
     *         lives.
     */
    /* package */ PaymentHandlerMediator(PropertyModel model, Runnable hider,
            WebContents webContents, PaymentHandlerUiObserver observer, View tabView,
            int toolbarViewHeightPx, int containerTopPaddingPx,
            ActivityLifecycleDispatcher activityLifeCycleDispatcher) {
        super(webContents);
        assert webContents != null;
        mTabView = tabView;
        mWebContentsRef = webContents;
        mToolbarViewHeightPx = toolbarViewHeightPx;
        mModel = model;
        mModel.set(PaymentHandlerProperties.BACK_PRESS_CALLBACK, this::onSystemBackButtonClicked);
        mHider = hider;
        mPaymentHandlerUiObserver = observer;
        mContainerTopPaddingPx = containerTopPaddingPx;
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());

        mActivityLifecycleDispatcher = activityLifeCycleDispatcher;
        mActivityDestroyListener = new Destroyable() {
            @Override
            public void destroy() {
                mCloseReason = CloseReason.ACTIVITY_DIED;
                mHandler.post(mHider);
            }
        };
        mActivityLifecycleDispatcher.register(mActivityDestroyListener);
    }

    // Implement View.OnLayoutChangeListener:
    // This is the Tab View's layout change listener, invoked in response to phone rotation.
    // TODO(crbug.com/1057825): It should listen to the BottomSheet container's layout change
    // instead of the Tab View layout change for better encapsulation.
    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        mModel.set(PaymentHandlerProperties.CONTENT_VISIBLE_HEIGHT_PX, contentVisibleHeight());
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetStateChanged(@SheetState int newState) {
        switch (newState) {
            case BottomSheetController.SheetState.HIDDEN:
                mCloseReason = CloseReason.USER;
                mHandler.post(mHider);
                break;
        }
    }

    /** @return The height of visible area of the bottom sheet's content part. */
    private int contentVisibleHeight() {
        return (int) (mTabView.getHeight() * FULL_HEIGHT_RATIO) - mToolbarViewHeightPx
                - mContainerTopPaddingPx;
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    /**
     * Set whether PaymentHandler UI is obscuring all tabs.
     * @param activity The ChromeActivity of the tab.
     * @param isObscuring Whether PaymentHandler UI is considered to be obscuring.
     */
    private void setIsObscuringAllTabs(ChromeActivity activity, boolean isObscuring) {
        TabObscuringHandler obscuringHandler = activity.getTabObscuringHandler();
        if (obscuringHandler == null) return;
        if (isObscuring) {
            assert mTabObscuringToken == TokenHolder.INVALID_TOKEN;
            mTabObscuringToken = obscuringHandler.obscureAllTabs();
        } else {
            obscuringHandler.unobscureAllTabs(mTabObscuringToken);
            mTabObscuringToken = TokenHolder.INVALID_TOKEN;
        }
    }

    private void showScrim() {
        // Using an empty scrim observer is to avoid the dismissal of the bottom-sheet on tapping.
        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContentsRef);
        assert activity != null;

        BottomSheetController controller = activity.getBottomSheetController();
        PropertyModel params = controller.createScrimParams();
        ScrimCoordinator coordinator = controller.getScrimCoordinator();
        coordinator.showScrim(params);

        setIsObscuringAllTabs(activity, true);
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        mPaymentHandlerUiObserver.onPaymentHandlerUiShown();
        showScrim();
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This is invoked when the sheet returns to the peek state, but Payment Handler doesn't
        // have a peek state.
    }

    // Implement BottomSheetObserver:
    @Override
    public void onSheetFullyPeeked() {}

    // Implement BottomSheetObserver:
    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}

    // Implement WebContentsObserver:
    @Override
    public void destroy() {
        mActivityLifecycleDispatcher.unregister(mActivityDestroyListener);

        switch (mCloseReason) {
            case CloseReason.INSECURE_NAVIGATION:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindowForInsecureNavigation(
                        mWebContentsRef);
                break;
            case CloseReason.USER:
                // Intentional fallthrough.
            case CloseReason.FAIL_LOAD:
                // Intentional fallthrough.
                // TODO(crbug.com/1017926): Respond to service worker with the net error.
            case CloseReason.ACTIVITY_DIED:
                ServiceWorkerPaymentAppBridge.onClosingPaymentAppWindow(mWebContentsRef);
                break;
            case CloseReason.OTHERS:
                // No need to notify ServiceWorkerPaymentAppBridge when merchant aborts the
                // payment request (and thus {@link PaymentRequestImpl} closes
                // PaymentHandlerMediator). "OTHERS" category includes this cases.
                // TODO(crbug.com/1091957): we should explicitly list merchant aborting payment
                // request as a {@link CloseReason}, renames "OTHERS" as "UNKNOWN" and asserts
                // that PaymentHandler wouldn't be closed for unknown reason.
        }
        mHandler.removeCallbacksAndMessages(null);
        hideScrim();
        super.destroy(); // Stops observing the web contents and cleans up associated references.
    }

    private void hideScrim() {
        ChromeActivity activity = ChromeActivity.fromWebContents(mWebContentsRef);
        // activity would be null when this method is triggered by activity being destroyed.
        if (activity == null) return;

        setIsObscuringAllTabs(activity, false);

        ScrimCoordinator coordinator = activity.getBottomSheetController().getScrimCoordinator();
        if (coordinator == null) return;
        coordinator.hideScrim(/*animate=*/true);
    }

    // Implement WebContentsObserver:
    @Override
    public void didFinishNavigation(NavigationHandle navigationHandle) {
        if (navigationHandle.isSameDocument()) return;
        closeIfInsecure();
    }

    // Implement WebContentsObserver:
    @Override
    public void didChangeVisibleSecurityState() {
        closeIfInsecure();
    }

    private void closeIfInsecure() {
        if (!SslValidityChecker.isValidPageInPaymentHandlerWindow(mWebContentsRef)) {
            closeUIForInsecureNavigation();
        }
    }

    private void closeUIForInsecureNavigation() {
        mHandler.post(() -> {
            mCloseReason = CloseReason.INSECURE_NAVIGATION;
            mHider.run();
        });
    }

    // Implement WebContentsObserver:
    @Override
    public void didFailLoad(boolean isMainFrame, int errorCode, String failingUrl) {
        mHandler.post(() -> {
            mCloseReason = CloseReason.FAIL_LOAD;
            mHider.run();
        });
    }

    // Implement PaymentHandlerToolbarObserver:
    @Override
    public void onToolbarCloseButtonClicked() {
        mCloseReason = CloseReason.USER;
        mHandler.post(mHider);
    }

    private void onSystemBackButtonClicked() {
        NavigationController navigation = mWebContentsRef.getNavigationController();
        if (navigation.canGoBack()) navigation.goBack();
    }
}
