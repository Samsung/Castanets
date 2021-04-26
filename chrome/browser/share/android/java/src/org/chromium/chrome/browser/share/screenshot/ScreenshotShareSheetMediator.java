// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;

import org.chromium.chrome.browser.share.BitmapUriRequest;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.screenshot.ScreenshotShareSheetViewProperties.NoArgOperation;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * ScreenshotShareSheetMediator is in charge of calculating and setting values for
 * ScreenshotShareSheetViewProperties.
 */
class ScreenshotShareSheetMediator {
    private final PropertyModel mModel;
    private final Context mContext;
    private final Runnable mSaveRunnable;
    private final Runnable mDeleteRunnable;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private Tab mTab;

    /**
     * The ScreenshotShareSheetMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param deleteRunnable The action to take when cancel or delete is called.
     */
    ScreenshotShareSheetMediator(Context context, PropertyModel propertyModel,
            Runnable deleteRunnable, Runnable saveRunnable, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback) {
        mDeleteRunnable = deleteRunnable;
        mSaveRunnable = saveRunnable;
        mContext = context;
        mModel = propertyModel;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mModel.set(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER,
                operation -> { performNoArgOperation(operation); });
    }

    /**
     * Performs the operation passed in.
     *
     * @param operation The operation to perform.
     */
    public void performNoArgOperation(
            @ScreenshotShareSheetViewProperties.NoArgOperation int operation) {
        if (NoArgOperation.SHARE == operation) {
            share();
        } else if (NoArgOperation.SAVE == operation) {
            mSaveRunnable.run();
        } else if (NoArgOperation.DELETE == operation) {
            mDeleteRunnable.run();
        }
    }

    /**
     * Sends the current image to the share target.
     */
    private void share() {
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);
        final Uri bitmapUri = Uri.parse(BitmapUriRequest.bitmapUri(bitmap));
        // TODO(crbug.com/1093386) Add Metrics for tracking size or Uri and performance of
        // UriRequest.

        WindowAndroid window = mTab.getWindowAndroid();
        String title = mTab.getTitle();
        String visibleUrl = mTab.getUrlString();

        ShareParams params = new ShareParams.Builder(window, title, visibleUrl)
                                     .setScreenshotUri(bitmapUri)
                                     .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder()
                                                      .setSaveLastUsed(false)
                                                      .setShareDirectly(false)
                                                      .setIsUrlOfVisiblePage(false)
                                                      .build();
        mChromeOptionShareCallback.showThirdPartyShareSheet(
                params, chromeShareExtras, System.currentTimeMillis());
        mDeleteRunnable.run();
    }
}
