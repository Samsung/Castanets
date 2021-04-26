// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestSpec;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.RenderFrameHost;

/**
 * Native bridge for finding payment apps.
 */
public class PaymentAppServiceBridge implements PaymentAppFactoryInterface {
    private static boolean sCanMakePaymentForTesting;

    /* package */ PaymentAppServiceBridge() {}

    /**
     * Make canMakePayment() return true always for testing purpose.
     *
     * @param canMakePayment Indicates whether a SW payment app can make payment.
     */
    @VisibleForTesting
    public static void setCanMakePaymentForTesting(boolean canMakePayment) {
        sCanMakePaymentForTesting = canMakePayment;
    }

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        assert delegate.getParams().getPaymentRequestOrigin().equals(
                UrlFormatter.formatUrlForSecurityDisplay(
                        delegate.getParams().getRenderFrameHost().getLastCommittedURL()));

        PaymentAppServiceCallback callback = new PaymentAppServiceCallback(delegate);

        PaymentAppServiceBridgeJni.get().create(delegate.getParams().getRenderFrameHost(),
                delegate.getParams().getTopLevelOrigin(), delegate.getParams().getSpec(),
                delegate.getParams().getMayCrawl(), callback);
    }

    /** Handles callbacks from native PaymentAppService. */
    public class PaymentAppServiceCallback {
        private final PaymentAppFactoryDelegate mDelegate;

        private PaymentAppServiceCallback(PaymentAppFactoryDelegate delegate) {
            mDelegate = delegate;
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onCanMakePaymentCalculated(boolean canMakePayment) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onCanMakePaymentCalculated(canMakePayment || sCanMakePaymentForTesting);
        }

        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreated(PaymentApp paymentApp) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreated(paymentApp);
        }

        /**
         * Called when an error has occurred.
         * @param errorMessage Developer facing error message.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onPaymentAppCreationError(String errorMessage) {
            ThreadUtils.assertOnUiThread();
            mDelegate.onPaymentAppCreationError(errorMessage);
        }

        /**
         * Called when the factory is finished creating payment apps. Expects to be called exactly
         * once and after all onPaymentAppCreated() calls.
         */
        @CalledByNative("PaymentAppServiceCallback")
        private void onDoneCreatingPaymentApps() {
            ThreadUtils.assertOnUiThread();
            mDelegate.onDoneCreatingPaymentApps(PaymentAppServiceBridge.this);
        }
    }

    @NativeMethods
    /* package */ interface Natives {
        void create(RenderFrameHost initiatorRenderFrameHost, String topOrigin,
                PaymentRequestSpec spec, boolean mayCrawlForInstallablePaymentApps,
                PaymentAppServiceCallback callback);
    }
}
