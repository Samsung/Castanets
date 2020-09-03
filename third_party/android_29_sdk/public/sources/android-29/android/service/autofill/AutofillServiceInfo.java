/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.service.autofill;

import android.Manifest;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.AppGlobals;
import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.metrics.LogMaker;
import android.os.RemoteException;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Xml;

import com.android.internal.R;
import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.internal.util.XmlUtils;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.IOException;
import java.io.PrintWriter;

/**
 * {@link ServiceInfo} and meta-data about an {@link AutofillService}.
 *
 * @hide
 */
public final class AutofillServiceInfo {
    private static final String TAG = "AutofillServiceInfo";

    private static final String TAG_AUTOFILL_SERVICE = "autofill-service";
    private static final String TAG_COMPATIBILITY_PACKAGE = "compatibility-package";

    private static ServiceInfo getServiceInfoOrThrow(ComponentName comp, int userHandle)
            throws PackageManager.NameNotFoundException {
        try {
            ServiceInfo si = AppGlobals.getPackageManager().getServiceInfo(
                    comp,
                    PackageManager.GET_META_DATA,
                    userHandle);
            if (si != null) {
                return si;
            }
        } catch (RemoteException e) {
        }
        throw new PackageManager.NameNotFoundException(comp.toString());
    }

    @NonNull
    private final ServiceInfo mServiceInfo;

    @Nullable
    private final String mSettingsActivity;

    @Nullable
    private final ArrayMap<String, Long> mCompatibilityPackages;

    public AutofillServiceInfo(Context context, ComponentName comp, int userHandle)
            throws PackageManager.NameNotFoundException {
        this(context, getServiceInfoOrThrow(comp, userHandle));
    }

    public AutofillServiceInfo(Context context, ServiceInfo si) {
        // Check for permissions.
        if (!Manifest.permission.BIND_AUTOFILL_SERVICE.equals(si.permission)) {
            if (Manifest.permission.BIND_AUTOFILL.equals(si.permission)) {
                // Let it go for now...
                Log.w(TAG, "AutofillService from '" + si.packageName + "' uses unsupported "
                        + "permission " + Manifest.permission.BIND_AUTOFILL + ". It works for "
                        + "now, but might not be supported on future releases");
                new MetricsLogger().write(new LogMaker(MetricsEvent.AUTOFILL_INVALID_PERMISSION)
                        .setPackageName(si.packageName));
            } else {
                Log.w(TAG, "AutofillService from '" + si.packageName
                        + "' does not require permission "
                        + Manifest.permission.BIND_AUTOFILL_SERVICE);
                throw new SecurityException("Service does not require permission "
                        + Manifest.permission.BIND_AUTOFILL_SERVICE);
            }
        }

        mServiceInfo = si;

        // Get the AutoFill metadata, if declared.
        final XmlResourceParser parser = si.loadXmlMetaData(context.getPackageManager(),
                AutofillService.SERVICE_META_DATA);
        if (parser == null) {
            mSettingsActivity = null;
            mCompatibilityPackages = null;
            return;
        }

        String settingsActivity = null;
        ArrayMap<String, Long> compatibilityPackages = null;

        try {
            final Resources resources = context.getPackageManager().getResourcesForApplication(
                    si.applicationInfo);

            int type = 0;
            while (type != XmlPullParser.END_DOCUMENT && type != XmlPullParser.START_TAG) {
                type = parser.next();
            }

            if (TAG_AUTOFILL_SERVICE.equals(parser.getName())) {
                final AttributeSet allAttributes = Xml.asAttributeSet(parser);
                TypedArray afsAttributes = null;
                try {
                    afsAttributes = resources.obtainAttributes(allAttributes,
                            com.android.internal.R.styleable.AutofillService);
                    settingsActivity = afsAttributes.getString(
                            R.styleable.AutofillService_settingsActivity);
                } finally {
                    if (afsAttributes != null) {
                        afsAttributes.recycle();
                    }
                }
                compatibilityPackages = parseCompatibilityPackages(parser, resources);
            } else {
                Log.e(TAG, "Meta-data does not start with autofill-service tag");
            }
        } catch (PackageManager.NameNotFoundException | IOException | XmlPullParserException e) {
            Log.e(TAG, "Error parsing auto fill service meta-data", e);
        }

        mSettingsActivity = settingsActivity;
        mCompatibilityPackages = compatibilityPackages;
    }

    private ArrayMap<String, Long> parseCompatibilityPackages(XmlPullParser parser,
            Resources resources) throws IOException, XmlPullParserException {
        ArrayMap<String, Long> compatibilityPackages = null;

        final int outerDepth = parser.getDepth();
        int type;
        while ((type = parser.next()) != XmlPullParser.END_DOCUMENT
                && (type != XmlPullParser.END_TAG || parser.getDepth() > outerDepth)) {
            if (type == XmlPullParser.END_TAG || type == XmlPullParser.TEXT) {
                continue;
            }

            if (TAG_COMPATIBILITY_PACKAGE.equals(parser.getName())) {
                TypedArray cpAttributes = null;
                try {
                    final AttributeSet allAttributes = Xml.asAttributeSet(parser);

                    cpAttributes = resources.obtainAttributes(allAttributes,
                           R.styleable.AutofillService_CompatibilityPackage);

                    final String name = cpAttributes.getString(
                            R.styleable.AutofillService_CompatibilityPackage_name);
                    if (TextUtils.isEmpty(name)) {
                        Log.e(TAG, "Invalid compatibility package:" + name);
                        break;
                    }

                    final String maxVersionCodeStr = cpAttributes.getString(
                            R.styleable.AutofillService_CompatibilityPackage_maxLongVersionCode);
                    final Long maxVersionCode;
                    if (maxVersionCodeStr != null) {
                        try {
                            maxVersionCode = Long.parseLong(maxVersionCodeStr);
                        } catch (NumberFormatException e) {
                            Log.e(TAG, "Invalid compatibility max version code:"
                                    + maxVersionCodeStr);
                            break;
                        }
                        if (maxVersionCode < 0) {
                            Log.e(TAG, "Invalid compatibility max version code:"
                                    + maxVersionCode);
                            break;
                        }
                    } else {
                        maxVersionCode = Long.MAX_VALUE;
                    }
                    if (compatibilityPackages == null) {
                        compatibilityPackages = new ArrayMap<>();
                    }
                    compatibilityPackages.put(name, maxVersionCode);
                } finally {
                    XmlUtils.skipCurrentTag(parser);
                    if (cpAttributes != null) {
                        cpAttributes.recycle();
                    }
                }
            }
        }

        return compatibilityPackages;
    }

    public ServiceInfo getServiceInfo() {
        return mServiceInfo;
    }

    @Nullable
    public String getSettingsActivity() {
        return mSettingsActivity;
    }

    public ArrayMap<String, Long> getCompatibilityPackages() {
        return mCompatibilityPackages;
    }

    @Override
    public String toString() {
        final StringBuilder builder = new StringBuilder();
        builder.append(getClass().getSimpleName());
        builder.append("[").append(mServiceInfo);
        builder.append(", settings:").append(mSettingsActivity);
        builder.append(", hasCompatPckgs:").append(mCompatibilityPackages != null
                && !mCompatibilityPackages.isEmpty()).append("]");
        return builder.toString();
    }

    /**
     * Dumps it!
     */
    public void dump(String prefix, PrintWriter pw) {
        pw.print(prefix); pw.print("Component: "); pw.println(getServiceInfo().getComponentName());
        pw.print(prefix); pw.print("Settings: "); pw.println(mSettingsActivity);
        pw.print(prefix); pw.print("Compat packages: "); pw.println(mCompatibilityPackages);
    }
}
