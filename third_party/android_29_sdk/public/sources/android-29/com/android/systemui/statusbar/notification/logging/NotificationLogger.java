/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * limitations under the License
 */
package com.android.systemui.statusbar.notification.logging;

import android.content.Context;
import android.os.Handler;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.os.SystemClock;
import android.service.notification.NotificationListenerService;
import android.service.notification.NotificationStats;
import android.service.notification.StatusBarNotification;
import android.util.ArrayMap;
import android.util.ArraySet;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.statusbar.IStatusBarService;
import com.android.internal.statusbar.NotificationVisibility;
import com.android.systemui.UiOffloadThread;
import com.android.systemui.plugins.statusbar.StatusBarStateController;
import com.android.systemui.plugins.statusbar.StatusBarStateController.StateListener;
import com.android.systemui.statusbar.NotificationListener;
import com.android.systemui.statusbar.notification.NotificationEntryListener;
import com.android.systemui.statusbar.notification.NotificationEntryManager;
import com.android.systemui.statusbar.notification.collection.NotificationEntry;
import com.android.systemui.statusbar.notification.stack.ExpandableViewState;
import com.android.systemui.statusbar.notification.stack.NotificationListContainer;
import com.android.systemui.statusbar.policy.HeadsUpManager;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Map;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * Handles notification logging, in particular, logging which notifications are visible and which
 * are not.
 */
@Singleton
public class NotificationLogger implements StateListener {
    private static final String TAG = "NotificationLogger";

    /** The minimum delay in ms between reports of notification visibility. */
    private static final int VISIBILITY_REPORT_MIN_DELAY_MS = 500;

    /** Keys of notifications currently visible to the user. */
    private final ArraySet<NotificationVisibility> mCurrentlyVisibleNotifications =
            new ArraySet<>();

    // Dependencies:
    private final NotificationListenerService mNotificationListener;
    private final UiOffloadThread mUiOffloadThread;
    private final NotificationEntryManager mEntryManager;
    private HeadsUpManager mHeadsUpManager;
    private final ExpansionStateLogger mExpansionStateLogger;

    protected Handler mHandler = new Handler();
    protected IStatusBarService mBarService;
    private long mLastVisibilityReportUptimeMs;
    private NotificationListContainer mListContainer;
    private final Object mDozingLock = new Object();
    private boolean mDozing;

    protected final OnChildLocationsChangedListener mNotificationLocationsChangedListener =
            new OnChildLocationsChangedListener() {
                @Override
                public void onChildLocationsChanged() {
                    if (mHandler.hasCallbacks(mVisibilityReporter)) {
                        // Visibilities will be reported when the existing
                        // callback is executed.
                        return;
                    }
                    // Calculate when we're allowed to run the visibility
                    // reporter. Note that this timestamp might already have
                    // passed. That's OK, the callback will just be executed
                    // ASAP.
                    long nextReportUptimeMs =
                            mLastVisibilityReportUptimeMs + VISIBILITY_REPORT_MIN_DELAY_MS;
                    mHandler.postAtTime(mVisibilityReporter, nextReportUptimeMs);
                }
            };

    // Tracks notifications currently visible in mNotificationStackScroller and
    // emits visibility events via NoMan on changes.
    protected final Runnable mVisibilityReporter = new Runnable() {
        private final ArraySet<NotificationVisibility> mTmpNewlyVisibleNotifications =
                new ArraySet<>();
        private final ArraySet<NotificationVisibility> mTmpCurrentlyVisibleNotifications =
                new ArraySet<>();
        private final ArraySet<NotificationVisibility> mTmpNoLongerVisibleNotifications =
                new ArraySet<>();

        @Override
        public void run() {
            mLastVisibilityReportUptimeMs = SystemClock.uptimeMillis();

            // 1. Loop over mNotificationData entries:
            //   A. Keep list of visible notifications.
            //   B. Keep list of previously hidden, now visible notifications.
            // 2. Compute no-longer visible notifications by removing currently
            //    visible notifications from the set of previously visible
            //    notifications.
            // 3. Report newly visible and no-longer visible notifications.
            // 4. Keep currently visible notifications for next report.
            ArrayList<NotificationEntry> activeNotifications = mEntryManager
                    .getNotificationData().getActiveNotifications();
            int N = activeNotifications.size();
            for (int i = 0; i < N; i++) {
                NotificationEntry entry = activeNotifications.get(i);
                String key = entry.notification.getKey();
                boolean isVisible = mListContainer.isInVisibleLocation(entry);
                NotificationVisibility visObj = NotificationVisibility.obtain(key, i, N, isVisible,
                        getNotificationLocation(entry));
                boolean previouslyVisible = mCurrentlyVisibleNotifications.contains(visObj);
                if (isVisible) {
                    // Build new set of visible notifications.
                    mTmpCurrentlyVisibleNotifications.add(visObj);
                    if (!previouslyVisible) {
                        mTmpNewlyVisibleNotifications.add(visObj);
                    }
                } else {
                    // release object
                    visObj.recycle();
                }
            }
            mTmpNoLongerVisibleNotifications.addAll(mCurrentlyVisibleNotifications);
            mTmpNoLongerVisibleNotifications.removeAll(mTmpCurrentlyVisibleNotifications);

            logNotificationVisibilityChanges(
                    mTmpNewlyVisibleNotifications, mTmpNoLongerVisibleNotifications);

            recycleAllVisibilityObjects(mCurrentlyVisibleNotifications);
            mCurrentlyVisibleNotifications.addAll(mTmpCurrentlyVisibleNotifications);

            mExpansionStateLogger.onVisibilityChanged(
                    mTmpCurrentlyVisibleNotifications, mTmpCurrentlyVisibleNotifications);

            recycleAllVisibilityObjects(mTmpNoLongerVisibleNotifications);
            mTmpCurrentlyVisibleNotifications.clear();
            mTmpNewlyVisibleNotifications.clear();
            mTmpNoLongerVisibleNotifications.clear();
        }
    };

    /**
     * Returns the location of the notification referenced by the given {@link NotificationEntry}.
     */
    public static NotificationVisibility.NotificationLocation getNotificationLocation(
            NotificationEntry entry) {
        if (entry == null || entry.getRow() == null || entry.getRow().getViewState() == null) {
            return NotificationVisibility.NotificationLocation.LOCATION_UNKNOWN;
        }
        return convertNotificationLocation(entry.getRow().getViewState().location);
    }

    private static NotificationVisibility.NotificationLocation convertNotificationLocation(
            int location) {
        switch (location) {
            case ExpandableViewState.LOCATION_FIRST_HUN:
                return NotificationVisibility.NotificationLocation.LOCATION_FIRST_HEADS_UP;
            case ExpandableViewState.LOCATION_HIDDEN_TOP:
                return NotificationVisibility.NotificationLocation.LOCATION_HIDDEN_TOP;
            case ExpandableViewState.LOCATION_MAIN_AREA:
                return NotificationVisibility.NotificationLocation.LOCATION_MAIN_AREA;
            case ExpandableViewState.LOCATION_BOTTOM_STACK_PEEKING:
                return NotificationVisibility.NotificationLocation.LOCATION_BOTTOM_STACK_PEEKING;
            case ExpandableViewState.LOCATION_BOTTOM_STACK_HIDDEN:
                return NotificationVisibility.NotificationLocation.LOCATION_BOTTOM_STACK_HIDDEN;
            case ExpandableViewState.LOCATION_GONE:
                return NotificationVisibility.NotificationLocation.LOCATION_GONE;
            default:
                return NotificationVisibility.NotificationLocation.LOCATION_UNKNOWN;
        }
    }

    @Inject
    public NotificationLogger(NotificationListener notificationListener,
            UiOffloadThread uiOffloadThread,
            NotificationEntryManager entryManager,
            StatusBarStateController statusBarStateController,
            ExpansionStateLogger expansionStateLogger) {
        mNotificationListener = notificationListener;
        mUiOffloadThread = uiOffloadThread;
        mEntryManager = entryManager;
        mBarService = IStatusBarService.Stub.asInterface(
                ServiceManager.getService(Context.STATUS_BAR_SERVICE));
        mExpansionStateLogger = expansionStateLogger;
        // Not expected to be destroyed, don't need to unsubscribe
        statusBarStateController.addCallback(this);

        entryManager.addNotificationEntryListener(new NotificationEntryListener() {
            @Override
            public void onEntryRemoved(
                    NotificationEntry entry,
                    NotificationVisibility visibility,
                    boolean removedByUser) {
                if (removedByUser && visibility != null) {
                    logNotificationClear(entry.key, entry.notification, visibility);
                }
                mExpansionStateLogger.onEntryRemoved(entry.key);
            }

            @Override
            public void onEntryReinflated(NotificationEntry entry) {
                mExpansionStateLogger.onEntryReinflated(entry.key);
            }

            @Override
            public void onInflationError(
                    StatusBarNotification notification,
                    Exception exception) {
                logNotificationError(notification, exception);
            }
        });
    }

    public void setUpWithContainer(NotificationListContainer listContainer) {
        mListContainer = listContainer;
    }

    public void setHeadsUpManager(HeadsUpManager headsUpManager) {
        mHeadsUpManager = headsUpManager;
    }

    public void stopNotificationLogging() {
        // Report all notifications as invisible and turn down the
        // reporter.
        if (!mCurrentlyVisibleNotifications.isEmpty()) {
            logNotificationVisibilityChanges(
                    Collections.emptyList(), mCurrentlyVisibleNotifications);
            recycleAllVisibilityObjects(mCurrentlyVisibleNotifications);
        }
        mHandler.removeCallbacks(mVisibilityReporter);
        mListContainer.setChildLocationsChangedListener(null);
    }

    public void startNotificationLogging() {
        mListContainer.setChildLocationsChangedListener(mNotificationLocationsChangedListener);
        // Some transitions like mVisibleToUser=false -> mVisibleToUser=true don't
        // cause the scroller to emit child location events. Hence generate
        // one ourselves to guarantee that we're reporting visible
        // notifications.
        // (Note that in cases where the scroller does emit events, this
        // additional event doesn't break anything.)
        mNotificationLocationsChangedListener.onChildLocationsChanged();
    }

    private void setDozing(boolean dozing) {
        synchronized (mDozingLock) {
            mDozing = dozing;
        }
    }

    // TODO: This method has side effects, it is NOT just logging that a notification
    // was cleared, it also actually removes the notification
    private void logNotificationClear(String key, StatusBarNotification notification,
            NotificationVisibility nv) {
        final String pkg = notification.getPackageName();
        final String tag = notification.getTag();
        final int id = notification.getId();
        final int userId = notification.getUserId();
        try {
            int dismissalSurface = NotificationStats.DISMISSAL_SHADE;
            if (mHeadsUpManager.isAlerting(key)) {
                dismissalSurface = NotificationStats.DISMISSAL_PEEK;
            } else if (mListContainer.hasPulsingNotifications()) {
                dismissalSurface = NotificationStats.DISMISSAL_AOD;
            }
            int dismissalSentiment = NotificationStats.DISMISS_SENTIMENT_NEUTRAL;
            mBarService.onNotificationClear(pkg, tag, id, userId, notification.getKey(),
                    dismissalSurface,
                    dismissalSentiment, nv);
        } catch (RemoteException ex) {
            // system process is dead if we're here.
        }
    }

    private void logNotificationError(
            StatusBarNotification notification,
            Exception exception) {
        try {
            mBarService.onNotificationError(
                    notification.getPackageName(),
                    notification.getTag(),
                    notification.getId(),
                    notification.getUid(),
                    notification.getInitialPid(),
                    exception.getMessage(),
                    notification.getUserId());
        } catch (RemoteException ex) {
            // The end is nigh.
        }
    }

    private void logNotificationVisibilityChanges(
            Collection<NotificationVisibility> newlyVisible,
            Collection<NotificationVisibility> noLongerVisible) {
        if (newlyVisible.isEmpty() && noLongerVisible.isEmpty()) {
            return;
        }
        final NotificationVisibility[] newlyVisibleAr = cloneVisibilitiesAsArr(newlyVisible);
        final NotificationVisibility[] noLongerVisibleAr = cloneVisibilitiesAsArr(noLongerVisible);

        mUiOffloadThread.submit(() -> {
            try {
                mBarService.onNotificationVisibilityChanged(newlyVisibleAr, noLongerVisibleAr);
            } catch (RemoteException e) {
                // Ignore.
            }

            final int N = newlyVisibleAr.length;
            if (N > 0) {
                String[] newlyVisibleKeyAr = new String[N];
                for (int i = 0; i < N; i++) {
                    newlyVisibleKeyAr[i] = newlyVisibleAr[i].key;
                }

                synchronized (mDozingLock) {
                    // setNotificationsShown should only be called if we are confident that
                    // the user has seen the notification, aka not when ambient display is on
                    if (!mDozing) {
                        // TODO: Call NotificationEntryManager to do this, once it exists.
                        // TODO: Consider not catching all runtime exceptions here.
                        try {
                            mNotificationListener.setNotificationsShown(newlyVisibleKeyAr);
                        } catch (RuntimeException e) {
                            Log.d(TAG, "failed setNotificationsShown: ", e);
                        }
                    }
                }
            }
            recycleAllVisibilityObjects(newlyVisibleAr);
            recycleAllVisibilityObjects(noLongerVisibleAr);
        });
    }

    private void recycleAllVisibilityObjects(ArraySet<NotificationVisibility> array) {
        final int N = array.size();
        for (int i = 0 ; i < N; i++) {
            array.valueAt(i).recycle();
        }
        array.clear();
    }

    private void recycleAllVisibilityObjects(NotificationVisibility[] array) {
        final int N = array.length;
        for (int i = 0 ; i < N; i++) {
            if (array[i] != null) {
                array[i].recycle();
            }
        }
    }

    private static NotificationVisibility[] cloneVisibilitiesAsArr(
            Collection<NotificationVisibility> c) {
        final NotificationVisibility[] array = new NotificationVisibility[c.size()];
        int i = 0;
        for(NotificationVisibility nv: c) {
            if (nv != null) {
                array[i] = nv.clone();
            }
            i++;
        }
        return array;
    }

    @VisibleForTesting
    public Runnable getVisibilityReporter() {
        return mVisibilityReporter;
    }

    @Override
    public void onStateChanged(int newState) {
        // don't care about state change
    }

    @Override
    public void onDozingChanged(boolean isDozing) {
        setDozing(isDozing);
    }

    /**
     * Called when the notification is expanded / collapsed.
     */
    public void onExpansionChanged(String key, boolean isUserAction, boolean isExpanded) {
        NotificationVisibility.NotificationLocation location =
                getNotificationLocation(mEntryManager.getNotificationData().get(key));
        mExpansionStateLogger.onExpansionChanged(key, isUserAction, isExpanded, location);
    }

    /**
     * A listener that is notified when some child locations might have changed.
     */
    public interface OnChildLocationsChangedListener {
        void onChildLocationsChanged();
    }

    /**
     * Logs the expansion state change when the notification is visible.
     */
    public static class ExpansionStateLogger {
        /** Notification key -> state, should be accessed in UI offload thread only. */
        private final Map<String, State> mExpansionStates = new ArrayMap<>();

        /**
         * Notification key -> last logged expansion state, should be accessed in UI thread only.
         */
        private final Map<String, Boolean> mLoggedExpansionState = new ArrayMap<>();
        private final UiOffloadThread mUiOffloadThread;
        @VisibleForTesting
        IStatusBarService mBarService;

        @Inject
        public ExpansionStateLogger(UiOffloadThread uiOffloadThread) {
            mUiOffloadThread = uiOffloadThread;
            mBarService =
                    IStatusBarService.Stub.asInterface(
                            ServiceManager.getService(Context.STATUS_BAR_SERVICE));
        }

        @VisibleForTesting
        void onExpansionChanged(String key, boolean isUserAction, boolean isExpanded,
                NotificationVisibility.NotificationLocation location) {
            State state = getState(key);
            state.mIsUserAction = isUserAction;
            state.mIsExpanded = isExpanded;
            state.mLocation = location;
            maybeNotifyOnNotificationExpansionChanged(key, state);
        }

        @VisibleForTesting
        void onVisibilityChanged(
                Collection<NotificationVisibility> newlyVisible,
                Collection<NotificationVisibility> noLongerVisible) {
            final NotificationVisibility[] newlyVisibleAr =
                    cloneVisibilitiesAsArr(newlyVisible);
            final NotificationVisibility[] noLongerVisibleAr =
                    cloneVisibilitiesAsArr(noLongerVisible);

            for (NotificationVisibility nv : newlyVisibleAr) {
                State state = getState(nv.key);
                state.mIsVisible = true;
                state.mLocation = nv.location;
                maybeNotifyOnNotificationExpansionChanged(nv.key, state);
            }
            for (NotificationVisibility nv : noLongerVisibleAr) {
                State state = getState(nv.key);
                state.mIsVisible = false;
            }
        }

        @VisibleForTesting
        void onEntryRemoved(String key) {
            mExpansionStates.remove(key);
            mLoggedExpansionState.remove(key);
        }

        @VisibleForTesting
        void onEntryReinflated(String key) {
            // When the notification is updated, we should consider the notification as not
            // yet logged.
            mLoggedExpansionState.remove(key);
        }

        private State getState(String key) {
            State state = mExpansionStates.get(key);
            if (state == null) {
                state = new State();
                mExpansionStates.put(key, state);
            }
            return state;
        }

        private void maybeNotifyOnNotificationExpansionChanged(final String key, State state) {
            if (!state.isFullySet()) {
                return;
            }
            if (!state.mIsVisible) {
                return;
            }
            Boolean loggedExpansionState = mLoggedExpansionState.get(key);
            // Consider notification is initially collapsed, so only expanded is logged in the
            // first time.
            if (loggedExpansionState == null && !state.mIsExpanded) {
                return;
            }
            if (loggedExpansionState != null
                    && state.mIsExpanded == loggedExpansionState) {
                return;
            }
            mLoggedExpansionState.put(key, state.mIsExpanded);
            final State stateToBeLogged = new State(state);
            mUiOffloadThread.submit(() -> {
                try {
                    mBarService.onNotificationExpansionChanged(key, stateToBeLogged.mIsUserAction,
                            stateToBeLogged.mIsExpanded, stateToBeLogged.mLocation.ordinal());
                } catch (RemoteException e) {
                    Log.e(TAG, "Failed to call onNotificationExpansionChanged: ", e);
                }
            });
        }

        private static class State {
            @Nullable
            Boolean mIsUserAction;
            @Nullable
            Boolean mIsExpanded;
            @Nullable
            Boolean mIsVisible;
            @Nullable
            NotificationVisibility.NotificationLocation mLocation;

            private State() {}

            private State(State state) {
                this.mIsUserAction = state.mIsUserAction;
                this.mIsExpanded = state.mIsExpanded;
                this.mIsVisible = state.mIsVisible;
                this.mLocation = state.mLocation;
            }

            private boolean isFullySet() {
                return mIsUserAction != null && mIsExpanded != null && mIsVisible != null
                        && mLocation != null;
            }
        }
    }
}
