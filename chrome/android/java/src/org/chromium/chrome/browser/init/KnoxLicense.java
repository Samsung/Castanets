// Copyright 2020 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Context;

import org.chromium.base.Log;

import com.samsung.android.knox.license.EnterpriseLicenseManager;
import com.samsung.android.knox.license.KnoxEnterpriseLicenseManager;

public class KnoxLicense {
    private static final String TAG = "KnoxLicense";
    public boolean UseKnoxSdk() {
        return true;
    }

    public void ActivateLicense(Context context) {
        Log.i(TAG, "ActivateLicense");
        KnoxEnterpriseLicenseManager klmManager =
                KnoxEnterpriseLicenseManager.getInstance(context);
        klmManager.activateLicense("ACTIVATION-NUMBER-KNOX"); //NOTE: Activation key needs to be modified by one's own
    }
}
