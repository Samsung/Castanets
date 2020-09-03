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

package com.android.server.wm;

import static android.app.ActivityManager.START_TASK_TO_FRONT;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_HOME;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_RECENTS;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.Intent.FLAG_ACTIVITY_NO_ANIMATION;
import static android.os.Trace.TRACE_TAG_ACTIVITY_MANAGER;
import static android.view.WindowManager.TRANSIT_NONE;

import static com.android.server.wm.ActivityStackSupervisor.PRESERVE_WINDOWS;
import static com.android.server.wm.BoundsAnimationController.BOUNDS;
import static com.android.server.wm.BoundsAnimationController.FADE_IN;
import static com.android.server.wm.RecentsAnimationController.REORDER_KEEP_IN_PLACE;
import static com.android.server.wm.RecentsAnimationController.REORDER_MOVE_TO_ORIGINAL_POSITION;
import static com.android.server.wm.RecentsAnimationController.REORDER_MOVE_TO_TOP;
import static com.android.server.wm.WindowManagerDebugConfig.DEBUG_RECENTS_ANIMATIONS;

import android.app.ActivityOptions;
import android.app.IAssistDataReceiver;
import android.content.ComponentName;
import android.content.Intent;
import android.os.RemoteException;
import android.os.Trace;
import android.util.Slog;
import android.view.IRecentsAnimationRunner;

import com.android.server.wm.RecentsAnimationController.RecentsAnimationCallbacks;

/**
 * Manages the recents animation, including the reordering of the stacks for the transition and
 * cleanup. See {@link com.android.server.wm.RecentsAnimationController}.
 */
class RecentsAnimation implements RecentsAnimationCallbacks,
        ActivityDisplay.OnStackOrderChangedListener {
    private static final String TAG = RecentsAnimation.class.getSimpleName();
    private static final boolean DEBUG = DEBUG_RECENTS_ANIMATIONS;

    private final ActivityTaskManagerService mService;
    private final ActivityStackSupervisor mStackSupervisor;
    private final ActivityStartController mActivityStartController;
    private final WindowManagerService mWindowManager;
    private final ActivityDisplay mDefaultDisplay;
    private final int mCallingPid;

    /**
     * The activity which has been launched behind. We need to remember the activity because the
     * target stack may have other activities, then we are able to restore the launch-behind state
     * for the exact activity.
     */
    private ActivityRecord mLaunchedTargetActivity;
    private int mTargetActivityType;

    // The stack to restore the target stack behind when the animation is finished
    private ActivityStack mRestoreTargetBehindStack;

    RecentsAnimation(ActivityTaskManagerService atm, ActivityStackSupervisor stackSupervisor,
            ActivityStartController activityStartController, WindowManagerService wm,
            int callingPid) {
        mService = atm;
        mStackSupervisor = stackSupervisor;
        mDefaultDisplay = mService.mRootActivityContainer.getDefaultDisplay();
        mActivityStartController = activityStartController;
        mWindowManager = wm;
        mCallingPid = callingPid;
    }

    void startRecentsActivity(Intent intent, IRecentsAnimationRunner recentsAnimationRunner,
            ComponentName recentsComponent, int recentsUid,
            @Deprecated IAssistDataReceiver assistDataReceiver) {
        if (DEBUG) Slog.d(TAG, "startRecentsActivity(): intent=" + intent
                + " assistDataReceiver=" + assistDataReceiver);
        Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "RecentsAnimation#startRecentsActivity");

        // TODO(multi-display) currently only support recents animation in default display.
        final DisplayContent dc =
                mService.mRootActivityContainer.getDefaultDisplay().mDisplayContent;
        if (!mWindowManager.canStartRecentsAnimation()) {
            notifyAnimationCancelBeforeStart(recentsAnimationRunner);
            if (DEBUG) Slog.d(TAG, "Can't start recents animation, nextAppTransition="
                        + dc.mAppTransition.getAppTransition());
            return;
        }

        // If the activity is associated with the recents stack, then try and get that first
        final int userId = mService.getCurrentUserId();
        mTargetActivityType = intent.getComponent() != null
                && recentsComponent.equals(intent.getComponent())
                        ? ACTIVITY_TYPE_RECENTS
                        : ACTIVITY_TYPE_HOME;
        ActivityStack targetStack = mDefaultDisplay.getStack(WINDOWING_MODE_UNDEFINED,
                mTargetActivityType);
        ActivityRecord targetActivity = getTargetActivity(targetStack, intent.getComponent(),
                userId);
        final boolean hasExistingActivity = targetActivity != null;
        if (hasExistingActivity) {
            final ActivityDisplay display = targetActivity.getDisplay();
            mRestoreTargetBehindStack = display.getStackAbove(targetStack);
            if (mRestoreTargetBehindStack == null) {
                notifyAnimationCancelBeforeStart(recentsAnimationRunner);
                if (DEBUG) Slog.d(TAG, "No stack above target stack=" + targetStack);
                return;
            }
        }

        // Send launch hint if we are actually launching the target. If it's already visible
        // (shouldn't happen in general) we don't need to send it.
        if (targetActivity == null || !targetActivity.visible) {
            mService.mRootActivityContainer.sendPowerHintForLaunchStartIfNeeded(
                    true /* forceSend */, targetActivity);
        }

        mStackSupervisor.getActivityMetricsLogger().notifyActivityLaunching(intent);

        mService.mH.post(() -> mService.mAmInternal.setRunningRemoteAnimation(mCallingPid, true));

        mWindowManager.deferSurfaceLayout();
        try {
            if (hasExistingActivity) {
                // Move the recents activity into place for the animation if it is not top most
                mDefaultDisplay.moveStackBehindBottomMostVisibleStack(targetStack);
                if (DEBUG) Slog.d(TAG, "Moved stack=" + targetStack + " behind stack="
                            + mDefaultDisplay.getStackAbove(targetStack));

                // If there are multiple tasks in the target stack (ie. the home stack, with 3p
                // and default launchers coexisting), then move the task to the top as a part of
                // moving the stack to the front
                if (targetStack.topTask() != targetActivity.getTaskRecord()) {
                    targetStack.addTask(targetActivity.getTaskRecord(), true /* toTop */,
                            "startRecentsActivity");
                }
            } else {
                // No recents activity, create the new recents activity bottom most
                ActivityOptions options = ActivityOptions.makeBasic();
                options.setLaunchActivityType(mTargetActivityType);
                options.setAvoidMoveToFront();
                intent.addFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_NO_ANIMATION);

                mActivityStartController
                        .obtainStarter(intent, "startRecentsActivity_noTargetActivity")
                        .setCallingUid(recentsUid)
                        .setCallingPackage(recentsComponent.getPackageName())
                        .setActivityOptions(SafeActivityOptions.fromBundle(options.toBundle()))
                        .setMayWait(userId)
                        .execute();

                // Move the recents activity into place for the animation
                targetStack = mDefaultDisplay.getStack(WINDOWING_MODE_UNDEFINED,
                        mTargetActivityType);
                targetActivity = getTargetActivity(targetStack, intent.getComponent(), userId);
                mDefaultDisplay.moveStackBehindBottomMostVisibleStack(targetStack);
                if (DEBUG) {
                    Slog.d(TAG, "Moved stack=" + targetStack + " behind stack="
                            + mDefaultDisplay.getStackAbove(targetStack));
                }

                mWindowManager.prepareAppTransition(TRANSIT_NONE, false);
                mWindowManager.executeAppTransition();

                // TODO: Maybe wait for app to draw in this particular case?

                if (DEBUG) Slog.d(TAG, "Started intent=" + intent);
            }

            // Mark the target activity as launch-behind to bump its visibility for the
            // duration of the gesture that is driven by the recents component
            targetActivity.mLaunchTaskBehind = true;
            mLaunchedTargetActivity = targetActivity;

            // Fetch all the surface controls and pass them to the client to get the animation
            // started. Cancel any existing recents animation running synchronously (do not hold the
            // WM lock)
            mWindowManager.cancelRecentsAnimationSynchronously(REORDER_MOVE_TO_ORIGINAL_POSITION,
                    "startRecentsActivity");
            mWindowManager.initializeRecentsAnimation(mTargetActivityType, recentsAnimationRunner,
                    this, mDefaultDisplay.mDisplayId,
                    mStackSupervisor.mRecentTasks.getRecentTaskIds());

            // If we updated the launch-behind state, update the visibility of the activities after
            // we fetch the visible tasks to be controlled by the animation
            mService.mRootActivityContainer.ensureActivitiesVisible(null, 0, PRESERVE_WINDOWS);

            mStackSupervisor.getActivityMetricsLogger().notifyActivityLaunched(START_TASK_TO_FRONT,
                    targetActivity);

            // Register for stack order changes
            mDefaultDisplay.registerStackOrderChangedListener(this);
        } catch (Exception e) {
            Slog.e(TAG, "Failed to start recents activity", e);
            throw e;
        } finally {
            mWindowManager.continueSurfaceLayout();
            Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
        }
    }

    private void finishAnimation(@RecentsAnimationController.ReorderMode int reorderMode,
            boolean sendUserLeaveHint) {
        synchronized (mService.mGlobalLock) {
            if (DEBUG) Slog.d(TAG, "onAnimationFinished(): controller="
                    + mWindowManager.getRecentsAnimationController()
                    + " reorderMode=" + reorderMode);

            // Unregister for stack order changes
            mDefaultDisplay.unregisterStackOrderChangedListener(this);

            final RecentsAnimationController controller =
                    mWindowManager.getRecentsAnimationController();
            if (controller == null) return;

            // Just to be sure end the launch hint in case the target activity was never launched.
            // However, if we're keeping the activity and making it visible, we can leave it on.
            if (reorderMode != REORDER_KEEP_IN_PLACE) {
                mService.mRootActivityContainer.sendPowerHintForLaunchEndIfNeeded();
            }

            // Once the target is shown, prevent spurious background app switches
            if (reorderMode == REORDER_MOVE_TO_TOP) {
                mService.stopAppSwitches();
            }

            mService.mH.post(
                    () -> mService.mAmInternal.setRunningRemoteAnimation(mCallingPid, false));

            mWindowManager.inSurfaceTransaction(() -> {
                Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER,
                        "RecentsAnimation#onAnimationFinished_inSurfaceTransaction");
                mWindowManager.deferSurfaceLayout();
                try {
                    mWindowManager.cleanupRecentsAnimation(reorderMode);

                    final ActivityStack targetStack = mDefaultDisplay.getStack(
                            WINDOWING_MODE_UNDEFINED, mTargetActivityType);
                    // Prefer to use the original target activity instead of top activity because
                    // we may have moved another task to top (starting 3p launcher).
                    final ActivityRecord targetActivity = targetStack != null
                            ? targetStack.isInStackLocked(mLaunchedTargetActivity)
                            : null;
                    if (DEBUG) Slog.d(TAG, "onAnimationFinished(): targetStack=" + targetStack
                            + " targetActivity=" + targetActivity
                            + " mRestoreTargetBehindStack=" + mRestoreTargetBehindStack);
                    if (targetActivity == null) {
                        return;
                    }

                    // Restore the launched-behind state
                    targetActivity.mLaunchTaskBehind = false;

                    if (reorderMode == REORDER_MOVE_TO_TOP) {
                        // Bring the target stack to the front
                        mStackSupervisor.mNoAnimActivities.add(targetActivity);

                        if (sendUserLeaveHint) {
                            // Setting this allows the previous app to PiP.
                            mStackSupervisor.mUserLeaving = true;
                            targetStack.moveTaskToFrontLocked(targetActivity.getTaskRecord(),
                                    true /* noAnimation */, null /* activityOptions */,
                                    targetActivity.appTimeTracker,
                                    "RecentsAnimation.onAnimationFinished()");
                        } else {
                            targetStack.moveToFront("RecentsAnimation.onAnimationFinished()");
                        }

                        if (DEBUG) {
                            final ActivityStack topStack = getTopNonAlwaysOnTopStack();
                            if (topStack != targetStack) {
                                Slog.w(TAG, "Expected target stack=" + targetStack
                                        + " to be top most but found stack=" + topStack);
                            }
                        }
                    } else if (reorderMode == REORDER_MOVE_TO_ORIGINAL_POSITION){
                        // Restore the target stack to its previous position
                        final ActivityDisplay display = targetActivity.getDisplay();
                        display.moveStackBehindStack(targetStack, mRestoreTargetBehindStack);
                        if (DEBUG) {
                            final ActivityStack aboveTargetStack =
                                    mDefaultDisplay.getStackAbove(targetStack);
                            if (mRestoreTargetBehindStack != null
                                    && aboveTargetStack != mRestoreTargetBehindStack) {
                                Slog.w(TAG, "Expected target stack=" + targetStack
                                        + " to restored behind stack=" + mRestoreTargetBehindStack
                                        + " but it is behind stack=" + aboveTargetStack);
                            }
                        }
                    } else {
                        // If there is no recents screenshot animation, we can update the visibility
                        // of target stack immediately because it is visually invisible and the
                        // launch-behind state is restored. That also prevents the next transition
                        // type being disturbed if the visibility is updated after setting the next
                        // transition (the target activity will be one of closing apps).
                        if (!controller.shouldCancelWithDeferredScreenshot()
                                && !targetStack.isFocusedStackOnDisplay()) {
                            targetStack.ensureActivitiesVisibleLocked(null /* starting */,
                                    0 /* starting */, false /* preserveWindows */);
                        }
                        // Keep target stack in place, nothing changes, so ignore the transition
                        // logic below
                        return;
                    }

                    mWindowManager.prepareAppTransition(TRANSIT_NONE, false);
                    mService.mRootActivityContainer.ensureActivitiesVisible(null, 0, false);
                    mService.mRootActivityContainer.resumeFocusedStacksTopActivities();

                    // No reason to wait for the pausing activity in this case, as the hiding of
                    // surfaces needs to be done immediately.
                    mWindowManager.executeAppTransition();

                    // After reordering the stacks, reset the minimized state. At this point, either
                    // the target activity is now top-most and we will stay minimized (if in
                    // split-screen), or we will have returned to the app, and the minimized state
                    // should be reset
                    mWindowManager.checkSplitScreenMinimizedChanged(true /* animate */);
                } catch (Exception e) {
                    Slog.e(TAG, "Failed to clean up recents activity", e);
                    throw e;
                } finally {
                    mWindowManager.continueSurfaceLayout();
                    Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
                }
            });
        }
    }

    @Override
    public void onAnimationFinished(@RecentsAnimationController.ReorderMode int reorderMode,
            boolean runSychronously, boolean sendUserLeaveHint) {
        if (runSychronously) {
            finishAnimation(reorderMode, sendUserLeaveHint);
        } else {
            mService.mH.post(() -> finishAnimation(reorderMode, sendUserLeaveHint));
        }
    }

    @Override
    public void onStackOrderChanged(ActivityStack stack) {
        if (DEBUG) Slog.d(TAG, "onStackOrderChanged(): stack=" + stack);
        if (mDefaultDisplay.getIndexOf(stack) == -1 || !stack.shouldBeVisible(null)) {
            // The stack is not visible, so ignore this change
            return;
        }
        final RecentsAnimationController controller =
                mWindowManager.getRecentsAnimationController();
        if (controller == null) {
            return;
        }

        final DisplayContent dc =
                mService.mRootActivityContainer.getDefaultDisplay().mDisplayContent;
        dc.mBoundsAnimationController.setAnimationType(
                controller.shouldCancelWithDeferredScreenshot() ? FADE_IN : BOUNDS);

        // Cancel running recents animation and screenshot previous task when the next
        // transition starts in below cases:
        // 1) The next launching task is not in recents animation task.
        // 2) The next task is home activity. (i.e. pressing home key to back home in recents).
        if ((!controller.isAnimatingTask(stack.getTaskStack().getTopChild())
                || controller.isTargetApp(stack.getTopActivity().mAppWindowToken))
                && controller.shouldCancelWithDeferredScreenshot()) {
            controller.cancelOnNextTransitionStart();
        } else {
            // Just cancel directly to unleash from launcher when the next launching task is the
            // current top task.
            mWindowManager.cancelRecentsAnimationSynchronously(REORDER_KEEP_IN_PLACE,
                    "stackOrderChanged");
        }
    }

    /**
     * Called only when the animation should be canceled prior to starting.
     */
    private void notifyAnimationCancelBeforeStart(IRecentsAnimationRunner recentsAnimationRunner) {
        try {
            recentsAnimationRunner.onAnimationCanceled(false /* deferredWithScreenshot */);
        } catch (RemoteException e) {
            Slog.e(TAG, "Failed to cancel recents animation before start", e);
        }
    }

    /**
     * @return The top stack that is not always-on-top.
     */
    private ActivityStack getTopNonAlwaysOnTopStack() {
        for (int i = mDefaultDisplay.getChildCount() - 1; i >= 0; i--) {
            final ActivityStack s = mDefaultDisplay.getChildAt(i);
            if (s.getWindowConfiguration().isAlwaysOnTop()) {
                continue;
            }
            return s;
        }
        return null;
    }

    /**
     * @return the top activity in the {@param targetStack} matching the {@param component}, or just
     * the top activity of the top task if no task matches the component.
     */
    private ActivityRecord getTargetActivity(ActivityStack targetStack, ComponentName component,
            int userId) {
        if (targetStack == null) {
            return null;
        }

        for (int i = targetStack.getChildCount() - 1; i >= 0; i--) {
            final TaskRecord task = targetStack.getChildAt(i);
            if (task.userId == userId && task.getBaseIntent().getComponent().equals(component)) {
                return task.getTopActivity();
            }
        }
        return null;
    }
}
