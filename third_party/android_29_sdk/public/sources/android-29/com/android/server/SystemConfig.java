/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.server;

import static com.android.internal.util.ArrayUtils.appendInt;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Environment;
import android.os.Process;
import android.os.SystemProperties;
import android.os.storage.StorageManager;
import android.permission.PermissionManager.SplitPermissionInfo;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Slog;
import android.util.SparseArray;
import android.util.Xml;

import com.android.internal.util.XmlUtils;

import libcore.io.IoUtils;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;

/**
 * Loads global system configuration info.
 */
public class SystemConfig {
    static final String TAG = "SystemConfig";

    static SystemConfig sInstance;

    // permission flag, determines which types of configuration are allowed to be read
    private static final int ALLOW_FEATURES = 0x01;
    private static final int ALLOW_LIBS = 0x02;
    private static final int ALLOW_PERMISSIONS = 0x04;
    private static final int ALLOW_APP_CONFIGS = 0x08;
    private static final int ALLOW_PRIVAPP_PERMISSIONS = 0x10;
    private static final int ALLOW_OEM_PERMISSIONS = 0x20;
    private static final int ALLOW_HIDDENAPI_WHITELISTING = 0x40;
    private static final int ALLOW_ASSOCIATIONS = 0x80;
    private static final int ALLOW_ALL = ~0;

    // property for runtime configuration differentiation
    private static final String SKU_PROPERTY = "ro.boot.product.hardware.sku";

    // Group-ids that are given to all packages as read from etc/permissions/*.xml.
    int[] mGlobalGids;

    // These are the built-in uid -> permission mappings that were read from the
    // system configuration files.
    final SparseArray<ArraySet<String>> mSystemPermissions = new SparseArray<>();

    final ArrayList<SplitPermissionInfo> mSplitPermissions = new ArrayList<>();

    public static final class SharedLibraryEntry {
        public final String name;
        public final String filename;
        public final String[] dependencies;

        SharedLibraryEntry(String name, String filename, String[] dependencies) {
            this.name = name;
            this.filename = filename;
            this.dependencies = dependencies;
        }
    }

    // These are the built-in shared libraries that were read from the
    // system configuration files. Keys are the library names; values are
    // the individual entries that contain information such as filename
    // and dependencies.
    final ArrayMap<String, SharedLibraryEntry> mSharedLibraries = new ArrayMap<>();

    // These are the features this devices supports that were read from the
    // system configuration files.
    final ArrayMap<String, FeatureInfo> mAvailableFeatures = new ArrayMap<>();

    // These are the features which this device doesn't support; the OEM
    // partition uses these to opt-out of features from the system image.
    final ArraySet<String> mUnavailableFeatures = new ArraySet<>();

    public static final class PermissionEntry {
        public final String name;
        public int[] gids;
        public boolean perUser;

        PermissionEntry(String name, boolean perUser) {
            this.name = name;
            this.perUser = perUser;
        }
    }

    // These are the permission -> gid mappings that were read from the
    // system configuration files.
    final ArrayMap<String, PermissionEntry> mPermissions = new ArrayMap<>();

    // These are the packages that are white-listed to be able to run in the
    // background while in power save mode (but not whitelisted from device idle modes),
    // as read from the configuration files.
    final ArraySet<String> mAllowInPowerSaveExceptIdle = new ArraySet<>();

    // These are the packages that are white-listed to be able to run in the
    // background while in power save mode, as read from the configuration files.
    final ArraySet<String> mAllowInPowerSave = new ArraySet<>();

    // These are the packages that are white-listed to be able to run in the
    // background while in data-usage save mode, as read from the configuration files.
    final ArraySet<String> mAllowInDataUsageSave = new ArraySet<>();

    // These are the packages that are white-listed to be able to run background location
    // without throttling, as read from the configuration files.
    final ArraySet<String> mAllowUnthrottledLocation = new ArraySet<>();

    // These are the packages that are white-listed to be able to retrieve location even when user
    // location settings are off, for emergency purposes, as read from the configuration files.
    final ArraySet<String> mAllowIgnoreLocationSettings = new ArraySet<>();

    // These are the action strings of broadcasts which are whitelisted to
    // be delivered anonymously even to apps which target O+.
    final ArraySet<String> mAllowImplicitBroadcasts = new ArraySet<>();

    // These are the package names of apps which should be in the 'always'
    // URL-handling state upon factory reset.
    final ArraySet<String> mLinkedApps = new ArraySet<>();

    // These are the packages that are whitelisted to be able to run as system user
    final ArraySet<String> mSystemUserWhitelistedApps = new ArraySet<>();

    // These are the packages that should not run under system user
    final ArraySet<String> mSystemUserBlacklistedApps = new ArraySet<>();

    // These are the components that are enabled by default as VR mode listener services.
    final ArraySet<ComponentName> mDefaultVrComponents = new ArraySet<>();

    // These are the permitted backup transport service components
    final ArraySet<ComponentName> mBackupTransportWhitelist = new ArraySet<>();

    // Package names that are exempted from private API blacklisting
    final ArraySet<String> mHiddenApiPackageWhitelist = new ArraySet<>();

    // The list of carrier applications which should be disabled until used.
    // This function suppresses update notifications for these pre-installed apps.
    // In SubscriptionInfoUpdater, the listed applications are disabled until used when all of the
    // following conditions are met.
    // 1. Not currently carrier-privileged according to the inserted SIM
    // 2. Pre-installed
    // 3. In the default state (enabled but not explicitly)
    // And SubscriptionInfoUpdater undoes this and marks the app enabled when a SIM is inserted
    // that marks the app as carrier privileged. It also grants the app default permissions
    // for Phone and Location. As such, apps MUST only ever be added to this list if they
    // obtain user consent to access their location through other means.
    final ArraySet<String> mDisabledUntilUsedPreinstalledCarrierApps = new ArraySet<>();

    // These are the packages of carrier-associated apps which should be disabled until used until
    // a SIM is inserted which grants carrier privileges to that carrier app.
    final ArrayMap<String, List<String>> mDisabledUntilUsedPreinstalledCarrierAssociatedApps =
            new ArrayMap<>();

    final ArrayMap<String, ArraySet<String>> mPrivAppPermissions = new ArrayMap<>();
    final ArrayMap<String, ArraySet<String>> mPrivAppDenyPermissions = new ArrayMap<>();

    final ArrayMap<String, ArraySet<String>> mVendorPrivAppPermissions = new ArrayMap<>();
    final ArrayMap<String, ArraySet<String>> mVendorPrivAppDenyPermissions = new ArrayMap<>();

    final ArrayMap<String, ArraySet<String>> mProductPrivAppPermissions = new ArrayMap<>();
    final ArrayMap<String, ArraySet<String>> mProductPrivAppDenyPermissions = new ArrayMap<>();

    final ArrayMap<String, ArraySet<String>> mProductServicesPrivAppPermissions = new ArrayMap<>();
    final ArrayMap<String, ArraySet<String>> mProductServicesPrivAppDenyPermissions =
            new ArrayMap<>();

    final ArrayMap<String, ArrayMap<String, Boolean>> mOemPermissions = new ArrayMap<>();

    // Allowed associations between applications.  If there are any entries
    // for an app, those are the only associations allowed; otherwise, all associations
    // are allowed.  Allowing an association from app A to app B means app A can not
    // associate with any other apps, but does not limit what apps B can associate with.
    final ArrayMap<String, ArraySet<String>> mAllowedAssociations = new ArrayMap<>();

    private final ArraySet<String> mBugreportWhitelistedPackages = new ArraySet<>();

    public static SystemConfig getInstance() {
        synchronized (SystemConfig.class) {
            if (sInstance == null) {
                sInstance = new SystemConfig();
            }
            return sInstance;
        }
    }

    public int[] getGlobalGids() {
        return mGlobalGids;
    }

    public SparseArray<ArraySet<String>> getSystemPermissions() {
        return mSystemPermissions;
    }

    public ArrayList<SplitPermissionInfo> getSplitPermissions() {
        return mSplitPermissions;
    }

    public ArrayMap<String, SharedLibraryEntry> getSharedLibraries() {
        return mSharedLibraries;
    }

    public ArrayMap<String, FeatureInfo> getAvailableFeatures() {
        return mAvailableFeatures;
    }

    public ArrayMap<String, PermissionEntry> getPermissions() {
        return mPermissions;
    }

    public ArraySet<String> getAllowImplicitBroadcasts() {
        return mAllowImplicitBroadcasts;
    }

    public ArraySet<String> getAllowInPowerSaveExceptIdle() {
        return mAllowInPowerSaveExceptIdle;
    }

    public ArraySet<String> getAllowInPowerSave() {
        return mAllowInPowerSave;
    }

    public ArraySet<String> getAllowInDataUsageSave() {
        return mAllowInDataUsageSave;
    }

    public ArraySet<String> getAllowUnthrottledLocation() {
        return mAllowUnthrottledLocation;
    }

    public ArraySet<String> getAllowIgnoreLocationSettings() {
        return mAllowIgnoreLocationSettings;
    }

    public ArraySet<String> getLinkedApps() {
        return mLinkedApps;
    }

    public ArraySet<String> getSystemUserWhitelistedApps() {
        return mSystemUserWhitelistedApps;
    }

    public ArraySet<String> getSystemUserBlacklistedApps() {
        return mSystemUserBlacklistedApps;
    }

    public ArraySet<String> getHiddenApiWhitelistedApps() {
        return mHiddenApiPackageWhitelist;
    }

    public ArraySet<ComponentName> getDefaultVrComponents() {
        return mDefaultVrComponents;
    }

    public ArraySet<ComponentName> getBackupTransportWhitelist() {
        return mBackupTransportWhitelist;
    }

    public ArraySet<String> getDisabledUntilUsedPreinstalledCarrierApps() {
        return mDisabledUntilUsedPreinstalledCarrierApps;
    }

    public ArrayMap<String, List<String>> getDisabledUntilUsedPreinstalledCarrierAssociatedApps() {
        return mDisabledUntilUsedPreinstalledCarrierAssociatedApps;
    }

    public ArraySet<String> getPrivAppPermissions(String packageName) {
        return mPrivAppPermissions.get(packageName);
    }

    public ArraySet<String> getPrivAppDenyPermissions(String packageName) {
        return mPrivAppDenyPermissions.get(packageName);
    }

    public ArraySet<String> getVendorPrivAppPermissions(String packageName) {
        return mVendorPrivAppPermissions.get(packageName);
    }

    public ArraySet<String> getVendorPrivAppDenyPermissions(String packageName) {
        return mVendorPrivAppDenyPermissions.get(packageName);
    }

    public ArraySet<String> getProductPrivAppPermissions(String packageName) {
        return mProductPrivAppPermissions.get(packageName);
    }

    public ArraySet<String> getProductPrivAppDenyPermissions(String packageName) {
        return mProductPrivAppDenyPermissions.get(packageName);
    }

    public ArraySet<String> getProductServicesPrivAppPermissions(String packageName) {
        return mProductServicesPrivAppPermissions.get(packageName);
    }

    public ArraySet<String> getProductServicesPrivAppDenyPermissions(String packageName) {
        return mProductServicesPrivAppDenyPermissions.get(packageName);
    }

    public Map<String, Boolean> getOemPermissions(String packageName) {
        final Map<String, Boolean> oemPermissions = mOemPermissions.get(packageName);
        if (oemPermissions != null) {
            return oemPermissions;
        }
        return Collections.emptyMap();
    }

    public ArrayMap<String, ArraySet<String>> getAllowedAssociations() {
        return mAllowedAssociations;
    }

    public ArraySet<String> getBugreportWhitelistedPackages() {
        return mBugreportWhitelistedPackages;
    }

    SystemConfig() {
        // Read configuration from system
        readPermissions(Environment.buildPath(
                Environment.getRootDirectory(), "etc", "sysconfig"), ALLOW_ALL);

        // Read configuration from the old permissions dir
        readPermissions(Environment.buildPath(
                Environment.getRootDirectory(), "etc", "permissions"), ALLOW_ALL);

        // Vendors are only allowed to customize these
        int vendorPermissionFlag = ALLOW_LIBS | ALLOW_FEATURES | ALLOW_PRIVAPP_PERMISSIONS
                | ALLOW_ASSOCIATIONS;
        if (Build.VERSION.FIRST_SDK_INT <= Build.VERSION_CODES.O_MR1) {
            // For backward compatibility
            vendorPermissionFlag |= (ALLOW_PERMISSIONS | ALLOW_APP_CONFIGS);
        }
        readPermissions(Environment.buildPath(
                Environment.getVendorDirectory(), "etc", "sysconfig"), vendorPermissionFlag);
        readPermissions(Environment.buildPath(
                Environment.getVendorDirectory(), "etc", "permissions"), vendorPermissionFlag);

        // Allow ODM to customize system configs as much as Vendor, because /odm is another
        // vendor partition other than /vendor.
        int odmPermissionFlag = vendorPermissionFlag;
        readPermissions(Environment.buildPath(
                Environment.getOdmDirectory(), "etc", "sysconfig"), odmPermissionFlag);
        readPermissions(Environment.buildPath(
                Environment.getOdmDirectory(), "etc", "permissions"), odmPermissionFlag);

        String skuProperty = SystemProperties.get(SKU_PROPERTY, "");
        if (!skuProperty.isEmpty()) {
            String skuDir = "sku_" + skuProperty;

            readPermissions(Environment.buildPath(
                    Environment.getOdmDirectory(), "etc", "sysconfig", skuDir), odmPermissionFlag);
            readPermissions(Environment.buildPath(
                    Environment.getOdmDirectory(), "etc", "permissions", skuDir),
                    odmPermissionFlag);
        }

        // Allow OEM to customize these
        int oemPermissionFlag = ALLOW_FEATURES | ALLOW_OEM_PERMISSIONS | ALLOW_ASSOCIATIONS;
        readPermissions(Environment.buildPath(
                Environment.getOemDirectory(), "etc", "sysconfig"), oemPermissionFlag);
        readPermissions(Environment.buildPath(
                Environment.getOemDirectory(), "etc", "permissions"), oemPermissionFlag);

        // Allow Product to customize all system configs
        readPermissions(Environment.buildPath(
                Environment.getProductDirectory(), "etc", "sysconfig"), ALLOW_ALL);
        readPermissions(Environment.buildPath(
                Environment.getProductDirectory(), "etc", "permissions"), ALLOW_ALL);

        // Allow /product_services to customize all system configs
        readPermissions(Environment.buildPath(
                Environment.getProductServicesDirectory(), "etc", "sysconfig"), ALLOW_ALL);
        readPermissions(Environment.buildPath(
                Environment.getProductServicesDirectory(), "etc", "permissions"), ALLOW_ALL);
    }

    void readPermissions(File libraryDir, int permissionFlag) {
        // Read permissions from given directory.
        if (!libraryDir.exists() || !libraryDir.isDirectory()) {
            if (permissionFlag == ALLOW_ALL) {
                Slog.w(TAG, "No directory " + libraryDir + ", skipping");
            }
            return;
        }
        if (!libraryDir.canRead()) {
            Slog.w(TAG, "Directory " + libraryDir + " cannot be read");
            return;
        }

        // Iterate over the files in the directory and scan .xml files
        File platformFile = null;
        for (File f : libraryDir.listFiles()) {
            if (!f.isFile()) {
                continue;
            }

            // We'll read platform.xml last
            if (f.getPath().endsWith("etc/permissions/platform.xml")) {
                platformFile = f;
                continue;
            }

            if (!f.getPath().endsWith(".xml")) {
                Slog.i(TAG, "Non-xml file " + f + " in " + libraryDir + " directory, ignoring");
                continue;
            }
            if (!f.canRead()) {
                Slog.w(TAG, "Permissions library file " + f + " cannot be read");
                continue;
            }

            readPermissionsFromXml(f, permissionFlag);
        }

        // Read platform permissions last so it will take precedence
        if (platformFile != null) {
            readPermissionsFromXml(platformFile, permissionFlag);
        }
    }

    private void logNotAllowedInPartition(String name, File permFile, XmlPullParser parser) {
        Slog.w(TAG, "<" + name + "> not allowed in partition of "
                + permFile + " at " + parser.getPositionDescription());
    }

    private void readPermissionsFromXml(File permFile, int permissionFlag) {
        FileReader permReader = null;
        try {
            permReader = new FileReader(permFile);
        } catch (FileNotFoundException e) {
            Slog.w(TAG, "Couldn't find or open permissions file " + permFile);
            return;
        }

        final boolean lowRam = ActivityManager.isLowRamDeviceStatic();

        try {
            XmlPullParser parser = Xml.newPullParser();
            parser.setInput(permReader);

            int type;
            while ((type=parser.next()) != parser.START_TAG
                       && type != parser.END_DOCUMENT) {
                ;
            }

            if (type != parser.START_TAG) {
                throw new XmlPullParserException("No start tag found");
            }

            if (!parser.getName().equals("permissions") && !parser.getName().equals("config")) {
                throw new XmlPullParserException("Unexpected start tag in " + permFile
                        + ": found " + parser.getName() + ", expected 'permissions' or 'config'");
            }

            final boolean allowAll = permissionFlag == ALLOW_ALL;
            final boolean allowLibs = (permissionFlag & ALLOW_LIBS) != 0;
            final boolean allowFeatures = (permissionFlag & ALLOW_FEATURES) != 0;
            final boolean allowPermissions = (permissionFlag & ALLOW_PERMISSIONS) != 0;
            final boolean allowAppConfigs = (permissionFlag & ALLOW_APP_CONFIGS) != 0;
            final boolean allowPrivappPermissions = (permissionFlag & ALLOW_PRIVAPP_PERMISSIONS)
                    != 0;
            final boolean allowOemPermissions = (permissionFlag & ALLOW_OEM_PERMISSIONS) != 0;
            final boolean allowApiWhitelisting = (permissionFlag & ALLOW_HIDDENAPI_WHITELISTING)
                    != 0;
            final boolean allowAssociations = (permissionFlag & ALLOW_ASSOCIATIONS) != 0;
            while (true) {
                XmlUtils.nextElement(parser);
                if (parser.getEventType() == XmlPullParser.END_DOCUMENT) {
                    break;
                }

                String name = parser.getName();
                if (name == null) {
                    XmlUtils.skipCurrentTag(parser);
                    continue;
                }
                switch (name) {
                    case "group": {
                        if (allowAll) {
                            String gidStr = parser.getAttributeValue(null, "gid");
                            if (gidStr != null) {
                                int gid = android.os.Process.getGidForName(gidStr);
                                mGlobalGids = appendInt(mGlobalGids, gid);
                            } else {
                                Slog.w(TAG, "<" + name + "> without gid in " + permFile + " at "
                                        + parser.getPositionDescription());
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "permission": {
                        if (allowPermissions) {
                            String perm = parser.getAttributeValue(null, "name");
                            if (perm == null) {
                                Slog.w(TAG, "<" + name + "> without name in " + permFile + " at "
                                        + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            perm = perm.intern();
                            readPermission(parser, perm);
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                            XmlUtils.skipCurrentTag(parser);
                        }
                    } break;
                    case "assign-permission": {
                        if (allowPermissions) {
                            String perm = parser.getAttributeValue(null, "name");
                            if (perm == null) {
                                Slog.w(TAG, "<" + name + "> without name in " + permFile
                                        + " at " + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            String uidStr = parser.getAttributeValue(null, "uid");
                            if (uidStr == null) {
                                Slog.w(TAG, "<" + name + "> without uid in " + permFile
                                        + " at " + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            int uid = Process.getUidForName(uidStr);
                            if (uid < 0) {
                                Slog.w(TAG, "<" + name + "> with unknown uid \""
                                        + uidStr + "  in " + permFile + " at "
                                        + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            perm = perm.intern();
                            ArraySet<String> perms = mSystemPermissions.get(uid);
                            if (perms == null) {
                                perms = new ArraySet<String>();
                                mSystemPermissions.put(uid, perms);
                            }
                            perms.add(perm);
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "split-permission": {
                        if (allowPermissions) {
                            readSplitPermission(parser, permFile);
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                            XmlUtils.skipCurrentTag(parser);
                        }
                    } break;
                    case "library": {
                        if (allowLibs) {
                            String lname = parser.getAttributeValue(null, "name");
                            String lfile = parser.getAttributeValue(null, "file");
                            String ldependency = parser.getAttributeValue(null, "dependency");
                            if (lname == null) {
                                Slog.w(TAG, "<" + name + "> without name in " + permFile + " at "
                                        + parser.getPositionDescription());
                            } else if (lfile == null) {
                                Slog.w(TAG, "<" + name + "> without file in " + permFile + " at "
                                        + parser.getPositionDescription());
                            } else {
                                //Log.i(TAG, "Got library " + lname + " in " + lfile);
                                SharedLibraryEntry entry = new SharedLibraryEntry(lname, lfile,
                                        ldependency == null ? new String[0] : ldependency.split(":"));
                                mSharedLibraries.put(lname, entry);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "feature": {
                        if (allowFeatures) {
                            String fname = parser.getAttributeValue(null, "name");
                            int fversion = XmlUtils.readIntAttribute(parser, "version", 0);
                            boolean allowed;
                            if (!lowRam) {
                                allowed = true;
                            } else {
                                String notLowRam = parser.getAttributeValue(null, "notLowRam");
                                allowed = !"true".equals(notLowRam);
                            }
                            if (fname == null) {
                                Slog.w(TAG, "<" + name + "> without name in " + permFile + " at "
                                        + parser.getPositionDescription());
                            } else if (allowed) {
                                addFeature(fname, fversion);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "unavailable-feature": {
                        if (allowFeatures) {
                            String fname = parser.getAttributeValue(null, "name");
                            if (fname == null) {
                                Slog.w(TAG, "<" + name + "> without name in " + permFile
                                        + " at " + parser.getPositionDescription());
                            } else {
                                mUnavailableFeatures.add(fname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-in-power-save-except-idle": {
                        if (allowAll) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowInPowerSaveExceptIdle.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-in-power-save": {
                        if (allowAll) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowInPowerSave.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-in-data-usage-save": {
                        if (allowAll) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowInDataUsageSave.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-unthrottled-location": {
                        if (allowAll) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowUnthrottledLocation.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-ignore-location-settings": {
                        if (allowAll) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowIgnoreLocationSettings.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-implicit-broadcast": {
                        if (allowAll) {
                            String action = parser.getAttributeValue(null, "action");
                            if (action == null) {
                                Slog.w(TAG, "<" + name + "> without action in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mAllowImplicitBroadcasts.add(action);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "app-link": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in " + permFile
                                        + " at " + parser.getPositionDescription());
                            } else {
                                mLinkedApps.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "system-user-whitelisted-app": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mSystemUserWhitelistedApps.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "system-user-blacklisted-app": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mSystemUserBlacklistedApps.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "default-enabled-vr-app": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            String clsname = parser.getAttributeValue(null, "class");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else if (clsname == null) {
                                Slog.w(TAG, "<" + name + "> without class in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mDefaultVrComponents.add(new ComponentName(pkgname, clsname));
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "backup-transport-whitelisted-service": {
                        if (allowFeatures) {
                            String serviceName = parser.getAttributeValue(null, "service");
                            if (serviceName == null) {
                                Slog.w(TAG, "<" + name + "> without service in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                ComponentName cn = ComponentName.unflattenFromString(serviceName);
                                if (cn == null) {
                                    Slog.w(TAG, "<" + name + "> with invalid service name "
                                            + serviceName + " in " + permFile
                                            + " at " + parser.getPositionDescription());
                                } else {
                                    mBackupTransportWhitelist.add(cn);
                                }
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "disabled-until-used-preinstalled-carrier-associated-app": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            String carrierPkgname = parser.getAttributeValue(null,
                                    "carrierAppPackage");
                            if (pkgname == null || carrierPkgname == null) {
                                Slog.w(TAG, "<" + name
                                        + "> without package or carrierAppPackage in " + permFile
                                        + " at " + parser.getPositionDescription());
                            } else {
                                List<String> associatedPkgs =
                                        mDisabledUntilUsedPreinstalledCarrierAssociatedApps.get(
                                                carrierPkgname);
                                if (associatedPkgs == null) {
                                    associatedPkgs = new ArrayList<>();
                                    mDisabledUntilUsedPreinstalledCarrierAssociatedApps.put(
                                            carrierPkgname, associatedPkgs);
                                }
                                associatedPkgs.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "disabled-until-used-preinstalled-carrier-app": {
                        if (allowAppConfigs) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG,
                                        "<" + name + "> without "
                                                + "package in " + permFile + " at "
                                                + parser.getPositionDescription());
                            } else {
                                mDisabledUntilUsedPreinstalledCarrierApps.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "privapp-permissions": {
                        if (allowPrivappPermissions) {
                            // privapp permissions from system, vendor, product and product_services
                            // partitions are stored separately. This is to prevent xml files in
                            // the vendor partition from granting permissions to priv apps in the
                            // system partition and vice versa.
                            boolean vendor = permFile.toPath().startsWith(
                                    Environment.getVendorDirectory().toPath() + "/")
                                    || permFile.toPath().startsWith(
                                    Environment.getOdmDirectory().toPath() + "/");
                            boolean product = permFile.toPath().startsWith(
                                    Environment.getProductDirectory().toPath() + "/");
                            boolean productServices = permFile.toPath().startsWith(
                                    Environment.getProductServicesDirectory().toPath() + "/");
                            if (vendor) {
                                readPrivAppPermissions(parser, mVendorPrivAppPermissions,
                                        mVendorPrivAppDenyPermissions);
                            } else if (product) {
                                readPrivAppPermissions(parser, mProductPrivAppPermissions,
                                        mProductPrivAppDenyPermissions);
                            } else if (productServices) {
                                readPrivAppPermissions(parser, mProductServicesPrivAppPermissions,
                                        mProductServicesPrivAppDenyPermissions);
                            } else {
                                readPrivAppPermissions(parser, mPrivAppPermissions,
                                        mPrivAppDenyPermissions);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                            XmlUtils.skipCurrentTag(parser);
                        }
                    } break;
                    case "oem-permissions": {
                        if (allowOemPermissions) {
                            readOemPermissions(parser);
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                            XmlUtils.skipCurrentTag(parser);
                        }
                    } break;
                    case "hidden-api-whitelisted-app": {
                        if (allowApiWhitelisting) {
                            String pkgname = parser.getAttributeValue(null, "package");
                            if (pkgname == null) {
                                Slog.w(TAG, "<" + name + "> without package in "
                                        + permFile + " at " + parser.getPositionDescription());
                            } else {
                                mHiddenApiPackageWhitelist.add(pkgname);
                            }
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "allow-association": {
                        if (allowAssociations) {
                            String target = parser.getAttributeValue(null, "target");
                            if (target == null) {
                                Slog.w(TAG, "<" + name + "> without target in " + permFile
                                        + " at " + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            String allowed = parser.getAttributeValue(null, "allowed");
                            if (allowed == null) {
                                Slog.w(TAG, "<" + name + "> without allowed in " + permFile
                                        + " at " + parser.getPositionDescription());
                                XmlUtils.skipCurrentTag(parser);
                                break;
                            }
                            target = target.intern();
                            allowed = allowed.intern();
                            ArraySet<String> associations = mAllowedAssociations.get(target);
                            if (associations == null) {
                                associations = new ArraySet<>();
                                mAllowedAssociations.put(target, associations);
                            }
                            Slog.i(TAG, "Adding association: " + target + " <- " + allowed);
                            associations.add(allowed);
                        } else {
                            logNotAllowedInPartition(name, permFile, parser);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    case "bugreport-whitelisted": {
                        String pkgname = parser.getAttributeValue(null, "package");
                        if (pkgname == null) {
                            Slog.w(TAG, "<" + name + "> without package in " + permFile
                                    + " at " + parser.getPositionDescription());
                        } else {
                            mBugreportWhitelistedPackages.add(pkgname);
                        }
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                    default: {
                        Slog.w(TAG, "Tag " + name + " is unknown in "
                                + permFile + " at " + parser.getPositionDescription());
                        XmlUtils.skipCurrentTag(parser);
                    } break;
                }
            }
        } catch (XmlPullParserException e) {
            Slog.w(TAG, "Got exception parsing permissions.", e);
        } catch (IOException e) {
            Slog.w(TAG, "Got exception parsing permissions.", e);
        } finally {
            IoUtils.closeQuietly(permReader);
        }

        // Some devices can be field-converted to FBE, so offer to splice in
        // those features if not already defined by the static config
        if (StorageManager.isFileEncryptedNativeOnly()) {
            addFeature(PackageManager.FEATURE_FILE_BASED_ENCRYPTION, 0);
            addFeature(PackageManager.FEATURE_SECURELY_REMOVES_USERS, 0);
        }

        // Help legacy devices that may not have updated their static config
        if (StorageManager.hasAdoptable()) {
            addFeature(PackageManager.FEATURE_ADOPTABLE_STORAGE, 0);
        }

        if (ActivityManager.isLowRamDeviceStatic()) {
            addFeature(PackageManager.FEATURE_RAM_LOW, 0);
        } else {
            addFeature(PackageManager.FEATURE_RAM_NORMAL, 0);
        }

        for (String featureName : mUnavailableFeatures) {
            removeFeature(featureName);
        }
    }

    private void addFeature(String name, int version) {
        FeatureInfo fi = mAvailableFeatures.get(name);
        if (fi == null) {
            fi = new FeatureInfo();
            fi.name = name;
            fi.version = version;
            mAvailableFeatures.put(name, fi);
        } else {
            fi.version = Math.max(fi.version, version);
        }
    }

    private void removeFeature(String name) {
        if (mAvailableFeatures.remove(name) != null) {
            Slog.d(TAG, "Removed unavailable feature " + name);
        }
    }

    void readPermission(XmlPullParser parser, String name)
            throws IOException, XmlPullParserException {
        if (mPermissions.containsKey(name)) {
            throw new IllegalStateException("Duplicate permission definition for " + name);
        }

        final boolean perUser = XmlUtils.readBooleanAttribute(parser, "perUser", false);
        final PermissionEntry perm = new PermissionEntry(name, perUser);
        mPermissions.put(name, perm);

        int outerDepth = parser.getDepth();
        int type;
        while ((type=parser.next()) != XmlPullParser.END_DOCUMENT
               && (type != XmlPullParser.END_TAG
                       || parser.getDepth() > outerDepth)) {
            if (type == XmlPullParser.END_TAG
                    || type == XmlPullParser.TEXT) {
                continue;
            }

            String tagName = parser.getName();
            if ("group".equals(tagName)) {
                String gidStr = parser.getAttributeValue(null, "gid");
                if (gidStr != null) {
                    int gid = Process.getGidForName(gidStr);
                    perm.gids = appendInt(perm.gids, gid);
                } else {
                    Slog.w(TAG, "<group> without gid at "
                            + parser.getPositionDescription());
                }
            }
            XmlUtils.skipCurrentTag(parser);
        }
    }

    private void readPrivAppPermissions(XmlPullParser parser,
            ArrayMap<String, ArraySet<String>> grantMap,
            ArrayMap<String, ArraySet<String>> denyMap)
            throws IOException, XmlPullParserException {
        String packageName = parser.getAttributeValue(null, "package");
        if (TextUtils.isEmpty(packageName)) {
            Slog.w(TAG, "package is required for <privapp-permissions> in "
                    + parser.getPositionDescription());
            return;
        }

        ArraySet<String> permissions = grantMap.get(packageName);
        if (permissions == null) {
            permissions = new ArraySet<>();
        }
        ArraySet<String> denyPermissions = denyMap.get(packageName);
        int depth = parser.getDepth();
        while (XmlUtils.nextElementWithin(parser, depth)) {
            String name = parser.getName();
            if ("permission".equals(name)) {
                String permName = parser.getAttributeValue(null, "name");
                if (TextUtils.isEmpty(permName)) {
                    Slog.w(TAG, "name is required for <permission> in "
                            + parser.getPositionDescription());
                    continue;
                }
                permissions.add(permName);
            } else if ("deny-permission".equals(name)) {
                String permName = parser.getAttributeValue(null, "name");
                if (TextUtils.isEmpty(permName)) {
                    Slog.w(TAG, "name is required for <deny-permission> in "
                            + parser.getPositionDescription());
                    continue;
                }
                if (denyPermissions == null) {
                    denyPermissions = new ArraySet<>();
                }
                denyPermissions.add(permName);
            }
        }
        grantMap.put(packageName, permissions);
        if (denyPermissions != null) {
            denyMap.put(packageName, denyPermissions);
        }
    }

    void readOemPermissions(XmlPullParser parser) throws IOException, XmlPullParserException {
        final String packageName = parser.getAttributeValue(null, "package");
        if (TextUtils.isEmpty(packageName)) {
            Slog.w(TAG, "package is required for <oem-permissions> in "
                    + parser.getPositionDescription());
            return;
        }

        ArrayMap<String, Boolean> permissions = mOemPermissions.get(packageName);
        if (permissions == null) {
            permissions = new ArrayMap<>();
        }
        final int depth = parser.getDepth();
        while (XmlUtils.nextElementWithin(parser, depth)) {
            final String name = parser.getName();
            if ("permission".equals(name)) {
                final String permName = parser.getAttributeValue(null, "name");
                if (TextUtils.isEmpty(permName)) {
                    Slog.w(TAG, "name is required for <permission> in "
                            + parser.getPositionDescription());
                    continue;
                }
                permissions.put(permName, Boolean.TRUE);
            } else if ("deny-permission".equals(name)) {
                String permName = parser.getAttributeValue(null, "name");
                if (TextUtils.isEmpty(permName)) {
                    Slog.w(TAG, "name is required for <deny-permission> in "
                            + parser.getPositionDescription());
                    continue;
                }
                permissions.put(permName, Boolean.FALSE);
            }
        }
        mOemPermissions.put(packageName, permissions);
    }

    private void readSplitPermission(XmlPullParser parser, File permFile)
            throws IOException, XmlPullParserException {
        String splitPerm = parser.getAttributeValue(null, "name");
        if (splitPerm == null) {
            Slog.w(TAG, "<split-permission> without name in " + permFile + " at "
                    + parser.getPositionDescription());
            XmlUtils.skipCurrentTag(parser);
            return;
        }
        String targetSdkStr = parser.getAttributeValue(null, "targetSdk");
        int targetSdk = Build.VERSION_CODES.CUR_DEVELOPMENT + 1;
        if (!TextUtils.isEmpty(targetSdkStr)) {
            try {
                targetSdk = Integer.parseInt(targetSdkStr);
            } catch (NumberFormatException e) {
                Slog.w(TAG, "<split-permission> targetSdk not an integer in " + permFile + " at "
                        + parser.getPositionDescription());
                XmlUtils.skipCurrentTag(parser);
                return;
            }
        }
        final int depth = parser.getDepth();
        List<String> newPermissions = new ArrayList<>();
        while (XmlUtils.nextElementWithin(parser, depth)) {
            String name = parser.getName();
            if ("new-permission".equals(name)) {
                final String newName = parser.getAttributeValue(null, "name");
                if (TextUtils.isEmpty(newName)) {
                    Slog.w(TAG, "name is required for <new-permission> in "
                            + parser.getPositionDescription());
                    continue;
                }
                newPermissions.add(newName);
            } else {
                XmlUtils.skipCurrentTag(parser);
            }
        }
        if (!newPermissions.isEmpty()) {
            mSplitPermissions.add(new SplitPermissionInfo(splitPerm, newPermissions, targetSdk));
        }
    }
}
