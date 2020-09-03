/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.systemui.statusbar.notification;

import static com.android.systemui.statusbar.StatusBarState.SHADE;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.database.ContentObserver;
import android.hardware.display.AmbientDisplayConfiguration;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.UserHandle;
import android.provider.Settings;
import android.service.dreams.DreamService;
import android.service.dreams.IDreamManager;
import android.service.notification.StatusBarNotification;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;
import com.android.systemui.Dependency;
import com.android.systemui.plugins.statusbar.StatusBarStateController;
import com.android.systemui.statusbar.NotificationPresenter;
import com.android.systemui.statusbar.notification.collection.NotificationEntry;
import com.android.systemui.statusbar.phone.ShadeController;
import com.android.systemui.statusbar.policy.HeadsUpManager;

/**
 * Provides heads-up and pulsing state for notification entries.
 */
public class NotificationInterruptionStateProvider {

    private static final String TAG = "InterruptionStateProvider";
    private static final boolean DEBUG = false;
    private static final boolean ENABLE_HEADS_UP = true;
    private static final String SETTING_HEADS_UP_TICKER = "ticker_gets_heads_up";

    private final StatusBarStateController mStatusBarStateController =
            Dependency.get(StatusBarStateController.class);
    private final NotificationFilter mNotificationFilter = Dependency.get(NotificationFilter.class);
    private final AmbientDisplayConfiguration mAmbientDisplayConfiguration;

    private final Context mContext;
    private final PowerManager mPowerManager;
    private final IDreamManager mDreamManager;

    private NotificationPresenter mPresenter;
    private ShadeController mShadeController;
    private HeadsUpManager mHeadsUpManager;
    private HeadsUpSuppressor mHeadsUpSuppressor;

    private ContentObserver mHeadsUpObserver;
    @VisibleForTesting
    protected boolean mUseHeadsUp = false;
    private boolean mDisableNotificationAlerts;

    public NotificationInterruptionStateProvider(Context context) {
        this(context,
                (PowerManager) context.getSystemService(Context.POWER_SERVICE),
                IDreamManager.Stub.asInterface(
                        ServiceManager.checkService(DreamService.DREAM_SERVICE)),
                new AmbientDisplayConfiguration(context));
    }

    @VisibleForTesting
    protected NotificationInterruptionStateProvider(
            Context context,
            PowerManager powerManager,
            IDreamManager dreamManager,
            AmbientDisplayConfiguration ambientDisplayConfiguration) {
        mContext = context;
        mPowerManager = powerManager;
        mDreamManager = dreamManager;
        mAmbientDisplayConfiguration = ambientDisplayConfiguration;
    }

    /** Sets up late-binding dependencies for this component. */
    public void setUpWithPresenter(
            NotificationPresenter notificationPresenter,
            HeadsUpManager headsUpManager,
            HeadsUpSuppressor headsUpSuppressor) {
        mPresenter = notificationPresenter;
        mHeadsUpManager = headsUpManager;
        mHeadsUpSuppressor = headsUpSuppressor;

        mHeadsUpObserver = new ContentObserver(Dependency.get(Dependency.MAIN_HANDLER)) {
            @Override
            public void onChange(boolean selfChange) {
                boolean wasUsing = mUseHeadsUp;
                mUseHeadsUp = ENABLE_HEADS_UP && !mDisableNotificationAlerts
                        && Settings.Global.HEADS_UP_OFF != Settings.Global.getInt(
                        mContext.getContentResolver(),
                        Settings.Global.HEADS_UP_NOTIFICATIONS_ENABLED,
                        Settings.Global.HEADS_UP_OFF);
                Log.d(TAG, "heads up is " + (mUseHeadsUp ? "enabled" : "disabled"));
                if (wasUsing != mUseHeadsUp) {
                    if (!mUseHeadsUp) {
                        Log.d(TAG,
                                "dismissing any existing heads up notification on disable event");
                        mHeadsUpManager.releaseAllImmediately();
                    }
                }
            }
        };

        if (ENABLE_HEADS_UP) {
            mContext.getContentResolver().registerContentObserver(
                    Settings.Global.getUriFor(Settings.Global.HEADS_UP_NOTIFICATIONS_ENABLED),
                    true,
                    mHeadsUpObserver);
            mContext.getContentResolver().registerContentObserver(
                    Settings.Global.getUriFor(SETTING_HEADS_UP_TICKER), true,
                    mHeadsUpObserver);
        }
        mHeadsUpObserver.onChange(true); // set up
    }

    private ShadeController getShadeController() {
        if (mShadeController == null) {
            mShadeController = Dependency.get(ShadeController.class);
        }
        return mShadeController;
    }

    /**
     * Whether the notification should appear as a bubble with a fly-out on top of the screen.
     *
     * @param entry the entry to check
     * @return true if the entry should bubble up, false otherwise
     */
    public boolean shouldBubbleUp(NotificationEntry entry) {
        final StatusBarNotification sbn = entry.notification;
        if (!entry.canBubble) {
            if (DEBUG) {
                Log.d(TAG, "No bubble up: not allowed to bubble: " + sbn.getKey());
            }
            return false;
        }

        if (!entry.isBubble()) {
            if (DEBUG) {
                Log.d(TAG, "No bubble up: notification " + sbn.getKey()
                        + " is bubble? " + entry.isBubble());
            }
            return false;
        }

        final Notification n = sbn.getNotification();
        if (n.getBubbleMetadata() == null || n.getBubbleMetadata().getIntent() == null) {
            if (DEBUG) {
                Log.d(TAG, "No bubble up: notification: " + sbn.getKey()
                        + " doesn't have valid metadata");
            }
            return false;
        }

        if (!canHeadsUpCommon(entry)) {
            return false;
        }

        return true;
    }

    /**
     * Whether the notification should peek in from the top and alert the user.
     *
     * @param entry the entry to check
     * @return true if the entry should heads up, false otherwise
     */
    public boolean shouldHeadsUp(NotificationEntry entry) {
        StatusBarNotification sbn = entry.notification;

        if (getShadeController().isDozing()) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: device is dozing: " + sbn.getKey());
            }
            return false;
        }

        boolean inShade = mStatusBarStateController.getState() == SHADE;
        if (entry.isBubble() && inShade) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: in unlocked shade where notification is shown as a "
                        + "bubble: " + sbn.getKey());
            }
            return false;
        }

        if (!canAlertCommon(entry)) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: notification shouldn't alert: " + sbn.getKey());
            }
            return false;
        }

        if (!canHeadsUpCommon(entry)) {
            return false;
        }

        if (entry.importance < NotificationManager.IMPORTANCE_HIGH) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: unimportant notification: " + sbn.getKey());
            }
            return false;
        }

        boolean isDreaming = false;
        try {
            isDreaming = mDreamManager.isDreaming();
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to query dream manager.", e);
        }
        boolean inUse = mPowerManager.isScreenOn() && !isDreaming;

        if (!inUse) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: not in use: " + sbn.getKey());
            }
            return false;
        }

        if (!mHeadsUpSuppressor.canHeadsUp(entry, sbn)) {
            return false;
        }

        return true;
    }

    /**
     * Whether or not the notification should "pulse" on the user's display when the phone is
     * dozing.  This displays the ambient view of the notification.
     *
     * @param entry the entry to check
     * @return true if the entry should ambient pulse, false otherwise
     */
    public boolean shouldPulse(NotificationEntry entry) {
        StatusBarNotification sbn = entry.notification;

        if (!mAmbientDisplayConfiguration.pulseOnNotificationEnabled(UserHandle.USER_CURRENT)) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: disabled by setting: " + sbn.getKey());
            }
            return false;
        }

        if (!getShadeController().isDozing()) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: not dozing: " + sbn.getKey());
            }
            return false;
        }

        if (!canAlertCommon(entry)) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: notification shouldn't alert: " + sbn.getKey());
            }
            return false;
        }

        if (entry.shouldSuppressAmbient()) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: ambient effect suppressed: " + sbn.getKey());
            }
            return false;
        }

        if (entry.importance < NotificationManager.IMPORTANCE_DEFAULT) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: not important enough: " + sbn.getKey());
            }
            return false;
        }

        Bundle extras = sbn.getNotification().extras;
        CharSequence title = extras.getCharSequence(Notification.EXTRA_TITLE);
        CharSequence text = extras.getCharSequence(Notification.EXTRA_TEXT);
        if (TextUtils.isEmpty(title) && TextUtils.isEmpty(text)) {
            if (DEBUG) {
                Log.d(TAG, "No pulsing: title and text are empty: " + sbn.getKey());
            }
            return false;
        }

        return true;
    }

    /**
     * Common checks between heads up alerting and ambient pulse alerting.  See
     * {@link #shouldHeadsUp(NotificationEntry)} and
     * {@link #shouldPulse(NotificationEntry)}.  Notifications that fail any of these checks
     * should not alert at all.
     *
     * @param entry the entry to check
     * @return true if these checks pass, false if the notification should not alert
     */
    protected boolean canAlertCommon(NotificationEntry entry) {
        StatusBarNotification sbn = entry.notification;

        if (mNotificationFilter.shouldFilterOut(entry)) {
            if (DEBUG) {
                Log.d(TAG, "No alerting: filtered notification: " + sbn.getKey());
            }
            return false;
        }

        // Don't alert notifications that are suppressed due to group alert behavior
        if (sbn.isGroup() && sbn.getNotification().suppressAlertingDueToGrouping()) {
            if (DEBUG) {
                Log.d(TAG, "No alerting: suppressed due to group alert behavior");
            }
            return false;
        }

        return true;
    }

    /**
     * Common checks between heads up alerting and bubble fly out alerting. See
     * {@link #shouldHeadsUp(NotificationEntry)} and
     * {@link #shouldBubbleUp(NotificationEntry)}. Notifications that fail any of these
     * checks should not interrupt the user on screen.
     *
     * @param entry the entry to check
     * @return true if these checks pass, false if the notification should not interrupt on screen
     */
    public boolean canHeadsUpCommon(NotificationEntry entry) {
        StatusBarNotification sbn = entry.notification;

        if (!mUseHeadsUp || mPresenter.isDeviceInVrMode()) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: no huns or vr mode");
            }
            return false;
        }

        if (entry.shouldSuppressPeek()) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: suppressed by DND: " + sbn.getKey());
            }
            return false;
        }

        if (isSnoozedPackage(sbn)) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: snoozed package: " + sbn.getKey());
            }
            return false;
        }

        if (entry.hasJustLaunchedFullScreenIntent()) {
            if (DEBUG) {
                Log.d(TAG, "No heads up: recent fullscreen: " + sbn.getKey());
            }
            return false;
        }

        return true;
    }

    private boolean isSnoozedPackage(StatusBarNotification sbn) {
        return mHeadsUpManager.isSnoozed(sbn.getPackageName());
    }

    /** Sets whether to disable all alerts. */
    public void setDisableNotificationAlerts(boolean disableNotificationAlerts) {
        mDisableNotificationAlerts = disableNotificationAlerts;
        mHeadsUpObserver.onChange(true);
    }

    protected NotificationPresenter getPresenter() {
        return mPresenter;
    }

    /** A component which can suppress heads-up notifications due to the overall state of the UI. */
    public interface HeadsUpSuppressor {
        /**
         * Returns false if the provided notification is ineligible for heads-up according to this
         * component.
         *
         * @param entry entry of the notification that might be heads upped
         * @param sbn   notification that might be heads upped
         * @return false if the notification can not be heads upped
         */
        boolean canHeadsUp(NotificationEntry entry, StatusBarNotification sbn);

    }

}
