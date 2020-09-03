/*
 * Copyright (C) 2007 The Android Open Source Project
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

package android.app;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.annotation.SystemService;
import android.annotation.TestApi;
import android.annotation.UnsupportedAppUsage;
import android.content.Context;
import android.os.Binder;
import android.os.IBinder;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Pair;
import android.util.Slog;
import android.view.View;

import com.android.internal.statusbar.IStatusBarService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Allows an app to control the status bar.
 */
@SystemService(Context.STATUS_BAR_SERVICE)
public class StatusBarManager {

    /** @hide */
    public static final int DISABLE_EXPAND = View.STATUS_BAR_DISABLE_EXPAND;
    /** @hide */
    public static final int DISABLE_NOTIFICATION_ICONS = View.STATUS_BAR_DISABLE_NOTIFICATION_ICONS;
    /** @hide */
    public static final int DISABLE_NOTIFICATION_ALERTS
            = View.STATUS_BAR_DISABLE_NOTIFICATION_ALERTS;

    /** @hide */
    @Deprecated
    @UnsupportedAppUsage
    public static final int DISABLE_NOTIFICATION_TICKER
            = View.STATUS_BAR_DISABLE_NOTIFICATION_TICKER;
    /** @hide */
    public static final int DISABLE_SYSTEM_INFO = View.STATUS_BAR_DISABLE_SYSTEM_INFO;
    /** @hide */
    public static final int DISABLE_HOME = View.STATUS_BAR_DISABLE_HOME;
    /** @hide */
    public static final int DISABLE_RECENT = View.STATUS_BAR_DISABLE_RECENT;
    /** @hide */
    public static final int DISABLE_BACK = View.STATUS_BAR_DISABLE_BACK;
    /** @hide */
    public static final int DISABLE_CLOCK = View.STATUS_BAR_DISABLE_CLOCK;
    /** @hide */
    public static final int DISABLE_SEARCH = View.STATUS_BAR_DISABLE_SEARCH;

    /** @hide */
    @Deprecated
    public static final int DISABLE_NAVIGATION =
            View.STATUS_BAR_DISABLE_HOME | View.STATUS_BAR_DISABLE_RECENT;

    /** @hide */
    public static final int DISABLE_NONE = 0x00000000;

    /** @hide */
    public static final int DISABLE_MASK = DISABLE_EXPAND | DISABLE_NOTIFICATION_ICONS
            | DISABLE_NOTIFICATION_ALERTS | DISABLE_NOTIFICATION_TICKER
            | DISABLE_SYSTEM_INFO | DISABLE_RECENT | DISABLE_HOME | DISABLE_BACK | DISABLE_CLOCK
            | DISABLE_SEARCH;

    /** @hide */
    @IntDef(flag = true, prefix = {"DISABLE_"}, value = {
            DISABLE_NONE,
            DISABLE_EXPAND,
            DISABLE_NOTIFICATION_ICONS,
            DISABLE_NOTIFICATION_ALERTS,
            DISABLE_NOTIFICATION_TICKER,
            DISABLE_SYSTEM_INFO,
            DISABLE_HOME,
            DISABLE_RECENT,
            DISABLE_BACK,
            DISABLE_CLOCK,
            DISABLE_SEARCH
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface DisableFlags {}

    /**
     * Flag to disable quick settings.
     *
     * Setting this flag disables quick settings completely, but does not disable expanding the
     * notification shade.
     */
    /** @hide */
    public static final int DISABLE2_QUICK_SETTINGS = 1;
    /** @hide */
    public static final int DISABLE2_SYSTEM_ICONS = 1 << 1;
    /** @hide */
    public static final int DISABLE2_NOTIFICATION_SHADE = 1 << 2;
    /** @hide */
    public static final int DISABLE2_GLOBAL_ACTIONS = 1 << 3;
    /** @hide */
    public static final int DISABLE2_ROTATE_SUGGESTIONS = 1 << 4;

    /** @hide */
    public static final int DISABLE2_NONE = 0x00000000;

    /** @hide */
    public static final int DISABLE2_MASK = DISABLE2_QUICK_SETTINGS | DISABLE2_SYSTEM_ICONS
            | DISABLE2_NOTIFICATION_SHADE | DISABLE2_GLOBAL_ACTIONS | DISABLE2_ROTATE_SUGGESTIONS;

    /** @hide */
    @IntDef(flag = true, prefix = { "DISABLE2_" }, value = {
            DISABLE2_NONE,
            DISABLE2_MASK,
            DISABLE2_QUICK_SETTINGS,
            DISABLE2_SYSTEM_ICONS,
            DISABLE2_NOTIFICATION_SHADE,
            DISABLE2_GLOBAL_ACTIONS,
            DISABLE2_ROTATE_SUGGESTIONS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Disable2Flags {}

    /**
     * Default disable flags for setup
     *
     * @hide
     */
    public static final int DEFAULT_SETUP_DISABLE_FLAGS = DISABLE_NOTIFICATION_ALERTS
            | DISABLE_HOME | DISABLE_EXPAND | DISABLE_RECENT | DISABLE_CLOCK | DISABLE_SEARCH;

    /**
     * Default disable2 flags for setup
     *
     * @hide
     */
    public static final int DEFAULT_SETUP_DISABLE2_FLAGS = DISABLE2_ROTATE_SUGGESTIONS;

    /** @hide */
    public static final int NAVIGATION_HINT_BACK_ALT      = 1 << 0;
    /** @hide */
    public static final int NAVIGATION_HINT_IME_SHOWN     = 1 << 1;

    /** @hide */
    public static final int WINDOW_STATUS_BAR = 1;
    /** @hide */
    public static final int WINDOW_NAVIGATION_BAR = 2;

    /** @hide */
    @IntDef(flag = true, prefix = { "WINDOW_" }, value = {
        WINDOW_STATUS_BAR,
        WINDOW_NAVIGATION_BAR
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WindowType {}

    /** @hide */
    public static final int WINDOW_STATE_SHOWING = 0;
    /** @hide */
    public static final int WINDOW_STATE_HIDING = 1;
    /** @hide */
    public static final int WINDOW_STATE_HIDDEN = 2;

    /** @hide */
    @IntDef(flag = true, prefix = { "WINDOW_STATE_" }, value = {
            WINDOW_STATE_SHOWING,
            WINDOW_STATE_HIDING,
            WINDOW_STATE_HIDDEN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface WindowVisibleState {}

    /** @hide */
    public static final int CAMERA_LAUNCH_SOURCE_WIGGLE = 0;
    /** @hide */
    public static final int CAMERA_LAUNCH_SOURCE_POWER_DOUBLE_TAP = 1;
    /** @hide */
    public static final int CAMERA_LAUNCH_SOURCE_LIFT_TRIGGER = 2;

    @UnsupportedAppUsage
    private Context mContext;
    private IStatusBarService mService;
    @UnsupportedAppUsage
    private IBinder mToken = new Binder();

    @UnsupportedAppUsage
    StatusBarManager(Context context) {
        mContext = context;
    }

    @UnsupportedAppUsage
    private synchronized IStatusBarService getService() {
        if (mService == null) {
            mService = IStatusBarService.Stub.asInterface(
                    ServiceManager.getService(Context.STATUS_BAR_SERVICE));
            if (mService == null) {
                Slog.w("StatusBarManager", "warning: no STATUS_BAR_SERVICE");
            }
        }
        return mService;
    }

    /**
     * Disable some features in the status bar.  Pass the bitwise-or of the DISABLE_* flags.
     * To re-enable everything, pass {@link #DISABLE_NONE}.
     *
     * @hide
     */
    @UnsupportedAppUsage
    public void disable(int what) {
        try {
            final int userId = Binder.getCallingUserHandle().getIdentifier();
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.disableForUser(what, mToken, mContext.getPackageName(), userId);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Disable additional status bar features. Pass the bitwise-or of the DISABLE2_* flags.
     * To re-enable everything, pass {@link #DISABLE_NONE}.
     *
     * Warning: Only pass DISABLE2_* flags into this function, do not use DISABLE_* flags.
     *
     * @hide
     */
    public void disable2(@Disable2Flags int what) {
        try {
            final int userId = Binder.getCallingUserHandle().getIdentifier();
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.disable2ForUser(what, mToken, mContext.getPackageName(), userId);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Expand the notifications panel.
     *
     * @hide
     */
    @UnsupportedAppUsage
    public void expandNotificationsPanel() {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.expandNotificationsPanel();
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Collapse the notifications and settings panels.
     *
     * @hide
     */
    @UnsupportedAppUsage
    public void collapsePanels() {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.collapsePanels();
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Expand the settings panel.
     *
     * @hide
     */
    @UnsupportedAppUsage
    public void expandSettingsPanel() {
        expandSettingsPanel(null);
    }

    /**
     * Expand the settings panel and open a subPanel. If the subpanel is null or does not have a
     * corresponding tile, the QS panel is simply expanded
     *
     * @hide
     */
    @UnsupportedAppUsage
    public void expandSettingsPanel(@Nullable String subPanel) {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.expandSettingsPanel(subPanel);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /** @hide */
    @UnsupportedAppUsage
    public void setIcon(String slot, int iconId, int iconLevel, String contentDescription) {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.setIcon(slot, mContext.getPackageName(), iconId, iconLevel,
                    contentDescription);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /** @hide */
    @UnsupportedAppUsage
    public void removeIcon(String slot) {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.removeIcon(slot);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /** @hide */
    @UnsupportedAppUsage
    public void setIconVisibility(String slot, boolean visible) {
        try {
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.setIconVisibility(slot, visible);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Enable or disable status bar elements (notifications, clock) which are inappropriate during
     * device setup.
     *
     * @param disabled whether to apply or remove the disabled flags
     *
     * @hide
     */
    @SystemApi
    @TestApi
    @RequiresPermission(android.Manifest.permission.STATUS_BAR)
    public void setDisabledForSetup(boolean disabled) {
        try {
            final int userId = Binder.getCallingUserHandle().getIdentifier();
            final IStatusBarService svc = getService();
            if (svc != null) {
                svc.disableForUser(disabled ? DEFAULT_SETUP_DISABLE_FLAGS : DISABLE_NONE,
                        mToken, mContext.getPackageName(), userId);
                svc.disable2ForUser(disabled ? DEFAULT_SETUP_DISABLE2_FLAGS : DISABLE2_NONE,
                        mToken, mContext.getPackageName(), userId);
            }
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /**
     * Get this app's currently requested disabled components
     *
     * @return a new DisableInfo
     *
     * @hide
     */
    @SystemApi
    @TestApi
    @RequiresPermission(android.Manifest.permission.STATUS_BAR)
    @NonNull
    public DisableInfo getDisableInfo() {
        try {
            final int userId = Binder.getCallingUserHandle().getIdentifier();
            final IStatusBarService svc = getService();
            int[] flags = new int[] {0, 0};
            if (svc != null) {
                flags = svc.getDisableFlags(mToken, userId);
            }

            return new DisableInfo(flags[0], flags[1]);
        } catch (RemoteException ex) {
            throw ex.rethrowFromSystemServer();
        }
    }

    /** @hide */
    public static String windowStateToString(int state) {
        if (state == WINDOW_STATE_HIDING) return "WINDOW_STATE_HIDING";
        if (state == WINDOW_STATE_HIDDEN) return "WINDOW_STATE_HIDDEN";
        if (state == WINDOW_STATE_SHOWING) return "WINDOW_STATE_SHOWING";
        return "WINDOW_STATE_UNKNOWN";
    }

    /**
     * DisableInfo describes this app's requested state of the StatusBar with regards to which
     * components are enabled/disabled
     *
     * @hide
     */
    @SystemApi
    @TestApi
    public static final class DisableInfo {

        private boolean mStatusBarExpansion;
        private boolean mNavigateHome;
        private boolean mNotificationPeeking;
        private boolean mRecents;
        private boolean mSearch;

        /** @hide */
        public DisableInfo(int flags1, int flags2) {
            mStatusBarExpansion = (flags1 & DISABLE_EXPAND) != 0;
            mNavigateHome = (flags1 & DISABLE_HOME) != 0;
            mNotificationPeeking = (flags1 & DISABLE_NOTIFICATION_ALERTS) != 0;
            mRecents = (flags1 & DISABLE_RECENT) != 0;
            mSearch = (flags1 & DISABLE_SEARCH) != 0;
        }

        /** @hide */
        public DisableInfo() {}

        /**
         * @return {@code true} if expanding the notification shade is disabled
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean isStatusBarExpansionDisabled() {
            return mStatusBarExpansion;
        }

        /** * @hide */
        public void setStatusBarExpansionDisabled(boolean disabled) {
            mStatusBarExpansion = disabled;
        }

        /**
         * @return {@code true} if navigation home is disabled
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean isNavigateToHomeDisabled() {
            return mNavigateHome;
        }

        /** * @hide */
        public void setNagivationHomeDisabled(boolean disabled) {
            mNavigateHome = disabled;
        }

        /**
         * @return {@code true} if notification peeking (heads-up notification) is disabled
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean isNotificationPeekingDisabled() {
            return mNotificationPeeking;
        }

        /** @hide */
        public void setNotificationPeekingDisabled(boolean disabled) {
            mNotificationPeeking = disabled;
        }

        /**
         * @return {@code true} if mRecents/overview is disabled
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean isRecentsDisabled() {
            return mRecents;
        }

        /**  @hide */
        public void setRecentsDisabled(boolean disabled) {
            mRecents = disabled;
        }

        /**
         * @return {@code true} if mSearch is disabled
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean isSearchDisabled() {
            return mSearch;
        }

        /** @hide */
        public void setSearchDisabled(boolean disabled) {
            mSearch = disabled;
        }

        /**
         * @return {@code true} if no components are disabled (default state)
         *
         * @hide
         */
        @SystemApi
        @TestApi
        public boolean areAllComponentsEnabled() {
            return !mStatusBarExpansion && !mNavigateHome && !mNotificationPeeking && !mRecents
                    && !mSearch;
        }

        /** @hide */
        public void setEnableAll() {
            mStatusBarExpansion = false;
            mNavigateHome = false;
            mNotificationPeeking = false;
            mRecents = false;
            mSearch = false;
        }

        /**
         * @return {@code true} if all status bar components are disabled
         *
         * @hide
         */
        public boolean areAllComponentsDisabled() {
            return mStatusBarExpansion && mNavigateHome && mNotificationPeeking
                    && mRecents && mSearch;
        }

        /** @hide */
        public void setDisableAll() {
            mStatusBarExpansion = true;
            mNavigateHome = true;
            mNotificationPeeking = true;
            mRecents = true;
            mSearch = true;
        }

        @Override
        public String toString() {
            StringBuilder sb = new StringBuilder();
            sb.append("DisableInfo: ");
            sb.append(" mStatusBarExpansion=").append(mStatusBarExpansion ? "disabled" : "enabled");
            sb.append(" mNavigateHome=").append(mNavigateHome ? "disabled" : "enabled");
            sb.append(" mNotificationPeeking=")
                    .append(mNotificationPeeking ? "disabled" : "enabled");
            sb.append(" mRecents=").append(mRecents ? "disabled" : "enabled");
            sb.append(" mSearch=").append(mSearch ? "disabled" : "enabled");

            return sb.toString();

        }

        /**
         * Convert a DisableInfo to equivalent flags
         * @return a pair of equivalent disable flags
         *
         * @hide
         */
        public Pair<Integer, Integer> toFlags() {
            int disable1 = DISABLE_NONE;
            int disable2 = DISABLE2_NONE;

            if (mStatusBarExpansion) disable1 |= DISABLE_EXPAND;
            if (mNavigateHome) disable1 |= DISABLE_HOME;
            if (mNotificationPeeking) disable1 |= DISABLE_NOTIFICATION_ALERTS;
            if (mRecents) disable1 |= DISABLE_RECENT;
            if (mSearch) disable1 |= DISABLE_SEARCH;

            return new Pair<Integer, Integer>(disable1, disable2);
        }
    }
}
