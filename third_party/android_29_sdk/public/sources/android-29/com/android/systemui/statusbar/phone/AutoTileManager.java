/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.systemui.statusbar.phone;

import android.content.Context;
import android.hardware.display.ColorDisplayManager;
import android.hardware.display.NightDisplayListener;
import android.os.Handler;
import android.provider.Settings.Secure;

import com.android.internal.annotations.VisibleForTesting;
import com.android.systemui.Dependency;
import com.android.systemui.qs.AutoAddTracker;
import com.android.systemui.qs.QSTileHost;
import com.android.systemui.qs.SecureSetting;
import com.android.systemui.statusbar.policy.CastController;
import com.android.systemui.statusbar.policy.CastController.CastDevice;
import com.android.systemui.statusbar.policy.DataSaverController;
import com.android.systemui.statusbar.policy.DataSaverController.Listener;
import com.android.systemui.statusbar.policy.HotspotController;
import com.android.systemui.statusbar.policy.HotspotController.Callback;

import javax.inject.Inject;
import javax.inject.Named;

/**
 * Manages which tiles should be automatically added to QS.
 */
public class AutoTileManager {
    public static final String HOTSPOT = "hotspot";
    public static final String SAVER = "saver";
    public static final String INVERSION = "inversion";
    public static final String WORK = "work";
    public static final String NIGHT = "night";
    public static final String CAST = "cast";

    private final Context mContext;
    private final QSTileHost mHost;
    private final Handler mHandler;
    private final AutoAddTracker mAutoTracker;
    private final HotspotController mHotspotController;
    private final DataSaverController mDataSaverController;
    private final ManagedProfileController mManagedProfileController;
    private final NightDisplayListener mNightDisplayListener;
    private final CastController mCastController;

    @Inject
    public AutoTileManager(Context context, AutoAddTracker autoAddTracker, QSTileHost host,
            @Named(Dependency.BG_HANDLER_NAME) Handler handler,
            HotspotController hotspotController,
            DataSaverController dataSaverController,
            ManagedProfileController managedProfileController,
            NightDisplayListener nightDisplayListener,
            CastController castController) {
        mAutoTracker = autoAddTracker;
        mContext = context;
        mHost = host;
        mHandler = handler;
        mHotspotController = hotspotController;
        mDataSaverController = dataSaverController;
        mManagedProfileController = managedProfileController;
        mNightDisplayListener = nightDisplayListener;
        mCastController = castController;
        if (!mAutoTracker.isAdded(HOTSPOT)) {
            hotspotController.addCallback(mHotspotCallback);
        }
        if (!mAutoTracker.isAdded(SAVER)) {
            dataSaverController.addCallback(mDataSaverListener);
        }
        if (!mAutoTracker.isAdded(INVERSION)) {
            mColorsSetting = new SecureSetting(mContext, mHandler,
                    Secure.ACCESSIBILITY_DISPLAY_INVERSION_ENABLED) {
                @Override
                protected void handleValueChanged(int value, boolean observedChange) {
                    if (mAutoTracker.isAdded(INVERSION)) return;
                    if (value != 0) {
                        mHost.addTile(INVERSION);
                        mAutoTracker.setTileAdded(INVERSION);
                        mHandler.post(() -> mColorsSetting.setListening(false));
                    }
                }
            };
            mColorsSetting.setListening(true);
        }
        if (!mAutoTracker.isAdded(WORK)) {
            managedProfileController.addCallback(mProfileCallback);
        }
        if (!mAutoTracker.isAdded(NIGHT)
                && ColorDisplayManager.isNightDisplayAvailable(mContext)) {
            nightDisplayListener.setCallback(mNightDisplayCallback);
        }
        if (!mAutoTracker.isAdded(CAST)) {
            castController.addCallback(mCastCallback);
        }
    }

    public void destroy() {
        if (mColorsSetting != null) {
            mColorsSetting.setListening(false);
        }
        mAutoTracker.destroy();
        mHotspotController.removeCallback(mHotspotCallback);
        mDataSaverController.removeCallback(mDataSaverListener);
        mManagedProfileController.removeCallback(mProfileCallback);
        if (ColorDisplayManager.isNightDisplayAvailable(mContext)) {
            mNightDisplayListener.setCallback(null);
        }
        mCastController.removeCallback(mCastCallback);
    }

    public void unmarkTileAsAutoAdded(String tabSpec) {
        mAutoTracker.setTileRemoved(tabSpec);
    }

    private final ManagedProfileController.Callback mProfileCallback =
            new ManagedProfileController.Callback() {
                @Override
                public void onManagedProfileChanged() {
                    if (mAutoTracker.isAdded(WORK)) return;
                    if (mManagedProfileController.hasActiveProfile()) {
                        mHost.addTile(WORK);
                        mAutoTracker.setTileAdded(WORK);
                    }
                }

                @Override
                public void onManagedProfileRemoved() {
                }
            };

    private SecureSetting mColorsSetting;

    private final DataSaverController.Listener mDataSaverListener = new Listener() {
        @Override
        public void onDataSaverChanged(boolean isDataSaving) {
            if (mAutoTracker.isAdded(SAVER)) return;
            if (isDataSaving) {
                mHost.addTile(SAVER);
                mAutoTracker.setTileAdded(SAVER);
                mHandler.post(() -> mDataSaverController.removeCallback(mDataSaverListener));
            }
        }
    };

    private final HotspotController.Callback mHotspotCallback = new Callback() {
        @Override
        public void onHotspotChanged(boolean enabled, int numDevices) {
            if (mAutoTracker.isAdded(HOTSPOT)) return;
            if (enabled) {
                mHost.addTile(HOTSPOT);
                mAutoTracker.setTileAdded(HOTSPOT);
                mHandler.post(() -> mHotspotController.removeCallback(mHotspotCallback));
            }
        }
    };

    @VisibleForTesting
    final NightDisplayListener.Callback mNightDisplayCallback =
            new NightDisplayListener.Callback() {
        @Override
        public void onActivated(boolean activated) {
            if (activated) {
                addNightTile();
            }
        }

        @Override
        public void onAutoModeChanged(int autoMode) {
            if (autoMode == ColorDisplayManager.AUTO_MODE_CUSTOM_TIME
                    || autoMode == ColorDisplayManager.AUTO_MODE_TWILIGHT) {
                addNightTile();
            }
        }

        private void addNightTile() {
            if (mAutoTracker.isAdded(NIGHT)) return;
            mHost.addTile(NIGHT);
            mAutoTracker.setTileAdded(NIGHT);
            mHandler.post(() -> mNightDisplayListener.setCallback(null));
        }
    };

    @VisibleForTesting
    final CastController.Callback mCastCallback = new CastController.Callback() {
        @Override
        public void onCastDevicesChanged() {
            if (mAutoTracker.isAdded(CAST)) return;

            boolean isCasting = false;
            for (CastDevice device : mCastController.getCastDevices()) {
                if (device.state == CastDevice.STATE_CONNECTED
                        || device.state == CastDevice.STATE_CONNECTING) {
                    isCasting = true;
                    break;
                }
            }

            if (isCasting) {
                mHost.addTile(CAST);
                mAutoTracker.setTileAdded(CAST);
                mHandler.post(() -> mCastController.removeCallback(mCastCallback));
            }
        }
    };
}
