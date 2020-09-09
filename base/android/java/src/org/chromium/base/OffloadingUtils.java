// Copyright 2020 Samsung Electronics. All rights reserved.

package org.chromium.base;

import android.Manifest;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

public class OffloadingUtils {
    private static final String TAG = "OffloadingUtils";

    public static boolean IsCastanets() {
        return IsEnabled("enable_castanets");
    }

    public static boolean IsServiceOffloading() {
        return IsEnabled("enable_service_offloading");
    }

    public static boolean IsServiceOffloadingKnox() {
        return IsEnabled("enable_service_offloading_knox");
    }

    private static boolean IsEnabled(String feature) {
        Context context = ContextUtils.getApplicationContext();
        try {
            ApplicationInfo info = context.getPackageManager().getApplicationInfo(
                    context.getPackageName(), PackageManager.GET_META_DATA);
            return (Boolean) info.metaData.get(feature);
        } catch (NameNotFoundException ex) {
            // NameNotFoundExceptions occurs.
            Log.e("TAG", "Error: " + ex);
        }
        return false;
    }
}
