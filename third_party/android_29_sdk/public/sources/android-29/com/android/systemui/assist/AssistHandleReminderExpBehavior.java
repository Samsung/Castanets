/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.systemui.assist;

import android.app.ActivityManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ResolveInfo;
import android.os.Handler;
import android.os.SystemClock;
import android.provider.Settings;

import androidx.annotation.Nullable;

import com.android.internal.config.sysui.SystemUiDeviceConfigFlags;
import com.android.systemui.Dependency;
import com.android.systemui.assist.AssistHandleBehaviorController.BehaviorController;
import com.android.systemui.plugins.statusbar.StatusBarStateController;
import com.android.systemui.recents.OverviewProxyService;
import com.android.systemui.shared.system.ActivityManagerWrapper;
import com.android.systemui.shared.system.PackageManagerWrapper;
import com.android.systemui.shared.system.QuickStepContract;
import com.android.systemui.shared.system.TaskStackChangeListener;
import com.android.systemui.statusbar.StatusBarState;

import java.io.PrintWriter;
import java.time.LocalDate;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Assistant handle behavior that hides the handles when the phone is dozing or in immersive mode,
 * shows the handles when on lockscreen, and shows the handles temporarily when changing tasks or
 * entering overview.
 */
final class AssistHandleReminderExpBehavior implements BehaviorController {

    private static final String LEARNING_TIME_ELAPSED_KEY = "reminder_exp_learning_time_elapsed";
    private static final String LEARNING_EVENT_COUNT_KEY = "reminder_exp_learning_event_count";
    private static final String LEARNED_HINT_LAST_SHOWN_KEY =
            "reminder_exp_learned_hint_last_shown";
    private static final long DEFAULT_LEARNING_TIME_MS = TimeUnit.DAYS.toMillis(10);
    private static final int DEFAULT_LEARNING_COUNT = 10;
    private static final long DEFAULT_SHOW_AND_GO_DELAYED_SHORT_DELAY_MS = 150;
    private static final long DEFAULT_SHOW_AND_GO_DELAYED_LONG_DELAY_MS =
            TimeUnit.SECONDS.toMillis(1);
    private static final long DEFAULT_SHOW_AND_GO_DELAY_RESET_TIMEOUT_MS =
            TimeUnit.SECONDS.toMillis(3);
    private static final boolean DEFAULT_SUPPRESS_ON_LOCKSCREEN = false;
    private static final boolean DEFAULT_SUPPRESS_ON_LAUNCHER = false;
    private static final boolean DEFAULT_SUPPRESS_ON_APPS = true;
    private static final boolean DEFAULT_SHOW_WHEN_TAUGHT = false;

    private static final String[] DEFAULT_HOME_CHANGE_ACTIONS = new String[] {
            PackageManagerWrapper.ACTION_PREFERRED_ACTIVITY_CHANGED,
            Intent.ACTION_BOOT_COMPLETED,
            Intent.ACTION_PACKAGE_ADDED,
            Intent.ACTION_PACKAGE_CHANGED,
            Intent.ACTION_PACKAGE_REMOVED
    };

    private final StatusBarStateController.StateListener mStatusBarStateListener =
            new StatusBarStateController.StateListener() {
                @Override
                public void onStateChanged(int newState) {
                    handleStatusBarStateChanged(newState);
                }

                @Override
                public void onDozingChanged(boolean isDozing) {
                    handleDozingChanged(isDozing);
                }
            };
    private final TaskStackChangeListener mTaskStackChangeListener =
            new TaskStackChangeListener() {
                @Override
                public void onTaskMovedToFront(ActivityManager.RunningTaskInfo taskInfo) {
                    handleTaskStackTopChanged(taskInfo.taskId, taskInfo.topActivity);
                }

                @Override
                public void onTaskCreated(int taskId, ComponentName componentName) {
                    handleTaskStackTopChanged(taskId, componentName);
                }
            };
    private final OverviewProxyService.OverviewProxyListener mOverviewProxyListener =
            new OverviewProxyService.OverviewProxyListener() {
                @Override
                public void onOverviewShown(boolean fromHome) {
                    handleOverviewShown();
                }

                @Override
                public void onSystemUiStateChanged(int sysuiStateFlags) {
                    handleSystemUiStateChanged(sysuiStateFlags);
                }
            };
    private final BroadcastReceiver mDefaultHomeBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            mDefaultHome = getCurrentDefaultHome();
        }
    };
    private final IntentFilter mDefaultHomeIntentFilter;
    private final Runnable mResetConsecutiveTaskSwitches = this::resetConsecutiveTaskSwitches;

    private final Handler mHandler;
    private final PhenotypeHelper mPhenotypeHelper;
    private final StatusBarStateController mStatusBarStateController;
    private final ActivityManagerWrapper mActivityManagerWrapper;
    private final OverviewProxyService mOverviewProxyService;

    private boolean mOnLockscreen;
    private boolean mIsDozing;
    private int mRunningTaskId;
    private boolean mIsNavBarHidden;
    private boolean mIsLauncherShowing;
    private int mConsecutiveTaskSwitches;

    /** Whether user has learned the gesture. */
    private boolean mIsLearned;
    private long mLastLearningTimestamp;
    /** Uptime while in this behavior. */
    private long mLearningTimeElapsed;
    /** Number of successful Assistant invocations while in this behavior. */
    private int mLearningCount;
    private long mLearnedHintLastShownEpochDay;

    @Nullable private Context mContext;
    @Nullable private AssistHandleCallbacks mAssistHandleCallbacks;
    @Nullable private ComponentName mDefaultHome;

    AssistHandleReminderExpBehavior(Handler handler, PhenotypeHelper phenotypeHelper) {
        mHandler = handler;
        mPhenotypeHelper = phenotypeHelper;
        mStatusBarStateController = Dependency.get(StatusBarStateController.class);
        mActivityManagerWrapper = ActivityManagerWrapper.getInstance();
        mOverviewProxyService = Dependency.get(OverviewProxyService.class);
        mDefaultHomeIntentFilter = new IntentFilter();
        for (String action : DEFAULT_HOME_CHANGE_ACTIONS) {
            mDefaultHomeIntentFilter.addAction(action);
        }
    }

    @Override
    public void onModeActivated(Context context, AssistHandleCallbacks callbacks) {
        mContext = context;
        mAssistHandleCallbacks = callbacks;
        mConsecutiveTaskSwitches = 0;
        mDefaultHome = getCurrentDefaultHome();
        context.registerReceiver(mDefaultHomeBroadcastReceiver, mDefaultHomeIntentFilter);
        mOnLockscreen = onLockscreen(mStatusBarStateController.getState());
        mIsDozing = mStatusBarStateController.isDozing();
        mStatusBarStateController.addCallback(mStatusBarStateListener);
        ActivityManager.RunningTaskInfo runningTaskInfo = mActivityManagerWrapper.getRunningTask();
        mRunningTaskId = runningTaskInfo == null ? 0 : runningTaskInfo.taskId;
        mActivityManagerWrapper.registerTaskStackListener(mTaskStackChangeListener);
        mOverviewProxyService.addCallback(mOverviewProxyListener);

        mLearningTimeElapsed = Settings.Secure.getLong(
                context.getContentResolver(), LEARNING_TIME_ELAPSED_KEY, /* default = */ 0);
        mLearningCount = Settings.Secure.getInt(
                context.getContentResolver(), LEARNING_EVENT_COUNT_KEY, /* default = */ 0);
        mLearnedHintLastShownEpochDay = Settings.Secure.getLong(
                context.getContentResolver(), LEARNED_HINT_LAST_SHOWN_KEY, /* default = */ 0);
        mLastLearningTimestamp = SystemClock.uptimeMillis();

        callbackForCurrentState(/* justUnlocked = */ false);
    }

    @Override
    public void onModeDeactivated() {
        mAssistHandleCallbacks = null;
        if (mContext != null) {
            mContext.unregisterReceiver(mDefaultHomeBroadcastReceiver);
            Settings.Secure.putLong(mContext.getContentResolver(), LEARNING_TIME_ELAPSED_KEY, 0);
            Settings.Secure.putInt(mContext.getContentResolver(), LEARNING_EVENT_COUNT_KEY, 0);
            Settings.Secure.putLong(mContext.getContentResolver(), LEARNED_HINT_LAST_SHOWN_KEY, 0);
            mContext = null;
        }
        mStatusBarStateController.removeCallback(mStatusBarStateListener);
        mActivityManagerWrapper.unregisterTaskStackListener(mTaskStackChangeListener);
        mOverviewProxyService.removeCallback(mOverviewProxyListener);
    }

    @Override
    public void onAssistantGesturePerformed() {
        if (mContext == null) {
            return;
        }

        Settings.Secure.putLong(
                mContext.getContentResolver(), LEARNING_EVENT_COUNT_KEY, ++mLearningCount);
    }

    private static boolean isNavBarHidden(int sysuiStateFlags) {
        return (sysuiStateFlags & QuickStepContract.SYSUI_STATE_NAV_BAR_HIDDEN) != 0;
    }

    @Nullable
    private static ComponentName getCurrentDefaultHome() {
        List<ResolveInfo> homeActivities = new ArrayList<>();
        ComponentName defaultHome =
                PackageManagerWrapper.getInstance().getHomeActivities(homeActivities);
        if (defaultHome != null) {
            return defaultHome;
        }

        int topPriority = Integer.MIN_VALUE;
        ComponentName topComponent = null;
        for (ResolveInfo resolveInfo : homeActivities) {
            if (resolveInfo.priority > topPriority) {
                topComponent = resolveInfo.activityInfo.getComponentName();
                topPriority = resolveInfo.priority;
            } else if (resolveInfo.priority == topPriority) {
                topComponent = null;
            }
        }
        return topComponent;
    }

    private void handleStatusBarStateChanged(int newState) {
        boolean onLockscreen = onLockscreen(newState);
        if (mOnLockscreen == onLockscreen) {
            return;
        }

        resetConsecutiveTaskSwitches();
        mOnLockscreen = onLockscreen;
        callbackForCurrentState(!onLockscreen);
    }

    private void handleDozingChanged(boolean isDozing) {
        if (mIsDozing == isDozing) {
            return;
        }

        resetConsecutiveTaskSwitches();
        mIsDozing = isDozing;
        callbackForCurrentState(/* justUnlocked = */ false);
    }

    private void handleTaskStackTopChanged(int taskId, @Nullable ComponentName taskComponentName) {
        if (mRunningTaskId == taskId || taskComponentName == null) {
            return;
        }

        mRunningTaskId = taskId;
        mIsLauncherShowing = taskComponentName.equals(mDefaultHome);
        if (mIsLauncherShowing) {
            resetConsecutiveTaskSwitches();
        } else {
            rescheduleConsecutiveTaskSwitchesReset();
            mConsecutiveTaskSwitches++;
        }
        callbackForCurrentState(/* justUnlocked = */ false);
    }

    private void handleSystemUiStateChanged(int sysuiStateFlags) {
        boolean isNavBarHidden = isNavBarHidden(sysuiStateFlags);
        if (mIsNavBarHidden == isNavBarHidden) {
            return;
        }

        resetConsecutiveTaskSwitches();
        mIsNavBarHidden = isNavBarHidden;
        callbackForCurrentState(/* justUnlocked = */ false);
    }

    private void handleOverviewShown() {
        resetConsecutiveTaskSwitches();
        callbackForCurrentState(/* justUnlocked = */ false);
    }

    private boolean onLockscreen(int statusBarState) {
        return statusBarState == StatusBarState.KEYGUARD
                || statusBarState == StatusBarState.SHADE_LOCKED;
    }

    private void callbackForCurrentState(boolean justUnlocked) {
        updateLearningStatus();

        if (mIsLearned) {
            callbackForLearnedState(justUnlocked);
        } else {
            callbackForUnlearnedState();
        }
    }

    private void callbackForLearnedState(boolean justUnlocked) {
        if (mAssistHandleCallbacks == null) {
            return;
        }

        if (mIsDozing || mIsNavBarHidden || mOnLockscreen || !getShowWhenTaught()) {
            mAssistHandleCallbacks.hide();
        } else if (justUnlocked) {
            long currentEpochDay = LocalDate.now().toEpochDay();
            if (mLearnedHintLastShownEpochDay < currentEpochDay) {
                if (mContext != null) {
                    Settings.Secure.putLong(
                            mContext.getContentResolver(),
                            LEARNED_HINT_LAST_SHOWN_KEY,
                            currentEpochDay);
                }
                mLearnedHintLastShownEpochDay = currentEpochDay;
                mAssistHandleCallbacks.showAndGo();
            }
        }
    }

    private void callbackForUnlearnedState() {
        if (mAssistHandleCallbacks == null) {
            return;
        }

        if (mIsDozing || mIsNavBarHidden || isSuppressed()) {
            mAssistHandleCallbacks.hide();
        } else if (mOnLockscreen) {
            mAssistHandleCallbacks.showAndStay();
        } else if (mIsLauncherShowing) {
            mAssistHandleCallbacks.showAndGo();
        } else if (mConsecutiveTaskSwitches == 1) {
            mAssistHandleCallbacks.showAndGoDelayed(
                    getShowAndGoDelayedShortDelayMs(), /* hideIfShowing = */ false);
        } else {
            mAssistHandleCallbacks.showAndGoDelayed(
                    getShowAndGoDelayedLongDelayMs(), /* hideIfShowing = */ true);
        }
    }

    private boolean isSuppressed() {
        if (mOnLockscreen) {
            return getSuppressOnLockscreen();
        } else if (mIsLauncherShowing) {
            return getSuppressOnLauncher();
        } else {
            return getSuppressOnApps();
        }
    }

    private void updateLearningStatus() {
        if (mContext == null) {
            return;
        }

        long currentTimestamp = SystemClock.uptimeMillis();
        mLearningTimeElapsed += currentTimestamp - mLastLearningTimestamp;
        mLastLearningTimestamp = currentTimestamp;
        Settings.Secure.putLong(
                mContext.getContentResolver(), LEARNING_TIME_ELAPSED_KEY, mLearningTimeElapsed);

        mIsLearned =
                mLearningCount >= getLearningCount() || mLearningTimeElapsed >= getLearningTimeMs();
    }

    private void resetConsecutiveTaskSwitches() {
        mHandler.removeCallbacks(mResetConsecutiveTaskSwitches);
        mConsecutiveTaskSwitches = 0;
    }

    private void rescheduleConsecutiveTaskSwitchesReset() {
        mHandler.removeCallbacks(mResetConsecutiveTaskSwitches);
        mHandler.postDelayed(mResetConsecutiveTaskSwitches, getShowAndGoDelayResetTimeoutMs());
    }

    private long getLearningTimeMs() {
        return mPhenotypeHelper.getLong(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_LEARN_TIME_MS,
                DEFAULT_LEARNING_TIME_MS);
    }

    private int getLearningCount() {
        return mPhenotypeHelper.getInt(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_LEARN_COUNT,
                DEFAULT_LEARNING_COUNT);
    }

    private long getShowAndGoDelayedShortDelayMs() {
        return mPhenotypeHelper.getLong(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAYED_SHORT_DELAY_MS,
                DEFAULT_SHOW_AND_GO_DELAYED_SHORT_DELAY_MS);
    }

    private long getShowAndGoDelayedLongDelayMs() {
        return mPhenotypeHelper.getLong(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAYED_LONG_DELAY_MS,
                DEFAULT_SHOW_AND_GO_DELAYED_LONG_DELAY_MS);
    }

    private long getShowAndGoDelayResetTimeoutMs() {
        return mPhenotypeHelper.getLong(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAY_RESET_TIMEOUT_MS,
                DEFAULT_SHOW_AND_GO_DELAY_RESET_TIMEOUT_MS);
    }

    private boolean getSuppressOnLockscreen() {
        return mPhenotypeHelper.getBoolean(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_LOCKSCREEN,
                DEFAULT_SUPPRESS_ON_LOCKSCREEN);
    }

    private boolean getSuppressOnLauncher() {
        return mPhenotypeHelper.getBoolean(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_LAUNCHER,
                DEFAULT_SUPPRESS_ON_LAUNCHER);
    }

    private boolean getSuppressOnApps() {
        return mPhenotypeHelper.getBoolean(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_APPS,
                DEFAULT_SUPPRESS_ON_APPS);
    }

    private boolean getShowWhenTaught() {
        return mPhenotypeHelper.getBoolean(
                SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_WHEN_TAUGHT,
                DEFAULT_SHOW_WHEN_TAUGHT);
    }

    @Override
    public void dump(PrintWriter pw, String prefix) {
        pw.println(prefix + "Current AssistHandleReminderExpBehavior State:");
        pw.println(prefix + "   mOnLockscreen=" + mOnLockscreen);
        pw.println(prefix + "   mIsDozing=" + mIsDozing);
        pw.println(prefix + "   mRunningTaskId=" + mRunningTaskId);
        pw.println(prefix + "   mDefaultHome=" + mDefaultHome);
        pw.println(prefix + "   mIsNavBarHidden=" + mIsNavBarHidden);
        pw.println(prefix + "   mIsLauncherShowing=" + mIsLauncherShowing);
        pw.println(prefix + "   mConsecutiveTaskSwitches=" + mConsecutiveTaskSwitches);
        pw.println(prefix + "   mIsLearned=" + mIsLearned);
        pw.println(prefix + "   mLastLearningTimestamp=" + mLastLearningTimestamp);
        pw.println(prefix + "   mLearningTimeElapsed=" + mLearningTimeElapsed);
        pw.println(prefix + "   mLearningCount=" + mLearningCount);
        pw.println(prefix + "   mLearnedHintLastShownEpochDay=" + mLearnedHintLastShownEpochDay);
        pw.println(
                prefix + "   mAssistHandleCallbacks present: " + (mAssistHandleCallbacks != null));

        pw.println(prefix + "   Phenotype Flags:");
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_LEARN_TIME_MS
                + "="
                + getLearningTimeMs());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_LEARN_COUNT
                + "="
                + getLearningCount());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAYED_SHORT_DELAY_MS
                + "="
                + getShowAndGoDelayedShortDelayMs());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAYED_LONG_DELAY_MS
                + "="
                + getShowAndGoDelayedLongDelayMs());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_AND_GO_DELAY_RESET_TIMEOUT_MS
                + "="
                + getShowAndGoDelayResetTimeoutMs());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_LOCKSCREEN
                + "="
                + getSuppressOnLockscreen());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_LAUNCHER
                + "="
                + getSuppressOnLauncher());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SUPPRESS_ON_APPS
                + "="
                + getSuppressOnApps());
        pw.println(prefix + "      "
                + SystemUiDeviceConfigFlags.ASSIST_HANDLES_SHOW_WHEN_TAUGHT
                + "="
                + getShowWhenTaught());
    }
}
