// Copyright 2020 Samsung Electronics. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.content.Context;

import org.chromium.base.Log;

import java.lang.reflect.Method;
import java.lang.reflect.Constructor;

public class LicenseAdapter {
    private static final String TAG = "LicenseAdapter";

    Class mClass;
    Object mObj;

    LicenseAdapter() {
        try {
            mClass = Class.forName("org.chromium.chrome.browser.init.KnoxLicense");
            mObj = mClass.getConstructor(new Class[] {}).newInstance();
        } catch (Exception e) {
            Log.e(TAG, e.toString());
        }
    }

    public boolean UseKnoxSdk() {
        try {
            if (mClass != null) {
                Method method = mClass.getMethod("UseKnoxSdk");
                return (boolean) method.invoke(mObj);
            }
        } catch (Throwable e) {
            Log.e(TAG, e.toString());
        }
        return false;
    }

    public void ActivateLicense(Context context) {
        try {
            if (mClass != null) {
                Method method = mClass.getMethod("ActivateLicense", Context.class);
                method.invoke(mObj, context);
            }
        } catch (Throwable e) {
            Log.e(TAG, e.toString());
        }
    }
}
