/*
 * Copyright (C) 2006 The Android Open Source Project
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

import static android.app.ActivityTaskManager.INVALID_STACK_ID;
import static android.app.ActivityTaskManager.INVALID_TASK_ID;
import static android.app.ActivityTaskManager.RESIZE_MODE_FORCED;
import static android.app.ActivityTaskManager.RESIZE_MODE_SYSTEM;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_STANDARD;
import static android.app.WindowConfiguration.ACTIVITY_TYPE_UNDEFINED;
import static android.app.WindowConfiguration.ROTATION_UNDEFINED;
import static android.app.WindowConfiguration.WINDOWING_MODE_FREEFORM;
import static android.app.WindowConfiguration.WINDOWING_MODE_FULLSCREEN;
import static android.app.WindowConfiguration.WINDOWING_MODE_PINNED;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.content.Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
import static android.content.Intent.FLAG_ACTIVITY_NEW_TASK;
import static android.content.Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS;
import static android.content.Intent.FLAG_ACTIVITY_TASK_ON_HOME;
import static android.content.pm.ActivityInfo.FLAG_RELINQUISH_TASK_IDENTITY;
import static android.content.pm.ActivityInfo.LOCK_TASK_LAUNCH_MODE_ALWAYS;
import static android.content.pm.ActivityInfo.LOCK_TASK_LAUNCH_MODE_DEFAULT;
import static android.content.pm.ActivityInfo.LOCK_TASK_LAUNCH_MODE_IF_WHITELISTED;
import static android.content.pm.ActivityInfo.LOCK_TASK_LAUNCH_MODE_NEVER;
import static android.content.pm.ActivityInfo.RESIZE_MODE_FORCE_RESIZABLE_LANDSCAPE_ONLY;
import static android.content.pm.ActivityInfo.RESIZE_MODE_FORCE_RESIZABLE_PORTRAIT_ONLY;
import static android.content.pm.ActivityInfo.RESIZE_MODE_FORCE_RESIZABLE_PRESERVE_ORIENTATION;
import static android.content.pm.ActivityInfo.RESIZE_MODE_FORCE_RESIZEABLE;
import static android.content.pm.ActivityInfo.RESIZE_MODE_RESIZEABLE;
import static android.content.pm.ActivityInfo.RESIZE_MODE_RESIZEABLE_AND_PIPABLE_DEPRECATED;
import static android.content.pm.ActivityInfo.RESIZE_MODE_RESIZEABLE_VIA_SDK_VERSION;
import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;
import static android.content.res.Configuration.ORIENTATION_PORTRAIT;
import static android.content.res.Configuration.ORIENTATION_UNDEFINED;
import static android.os.Trace.TRACE_TAG_ACTIVITY_MANAGER;
import static android.provider.Settings.Secure.USER_SETUP_COMPLETE;
import static android.view.Display.DEFAULT_DISPLAY;

import static com.android.server.EventLogTags.WM_TASK_CREATED;
import static com.android.server.am.TaskRecordProto.ACTIVITIES;
import static com.android.server.am.TaskRecordProto.ACTIVITY_TYPE;
import static com.android.server.am.TaskRecordProto.BOUNDS;
import static com.android.server.am.TaskRecordProto.CONFIGURATION_CONTAINER;
import static com.android.server.am.TaskRecordProto.FULLSCREEN;
import static com.android.server.am.TaskRecordProto.ID;
import static com.android.server.am.TaskRecordProto.LAST_NON_FULLSCREEN_BOUNDS;
import static com.android.server.am.TaskRecordProto.MIN_HEIGHT;
import static com.android.server.am.TaskRecordProto.MIN_WIDTH;
import static com.android.server.am.TaskRecordProto.ORIG_ACTIVITY;
import static com.android.server.am.TaskRecordProto.REAL_ACTIVITY;
import static com.android.server.am.TaskRecordProto.RESIZE_MODE;
import static com.android.server.am.TaskRecordProto.STACK_ID;
import static com.android.server.wm.ActivityRecord.STARTING_WINDOW_SHOWN;
import static com.android.server.wm.ActivityStack.REMOVE_TASK_MODE_MOVING;
import static com.android.server.wm.ActivityStack.REMOVE_TASK_MODE_MOVING_TO_TOP;
import static com.android.server.wm.ActivityStackSupervisor.ON_TOP;
import static com.android.server.wm.ActivityStackSupervisor.PAUSE_IMMEDIATELY;
import static com.android.server.wm.ActivityStackSupervisor.PRESERVE_WINDOWS;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.DEBUG_ADD_REMOVE;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.DEBUG_LOCKTASK;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.DEBUG_RECENTS;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.DEBUG_TASKS;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.POSTFIX_ADD_REMOVE;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.POSTFIX_LOCKTASK;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.POSTFIX_RECENTS;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.POSTFIX_TASKS;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.TAG_ATM;
import static com.android.server.wm.ActivityTaskManagerDebugConfig.TAG_WITH_CLASS_NAME;
import static com.android.server.wm.WindowContainer.POSITION_BOTTOM;
import static com.android.server.wm.WindowContainer.POSITION_TOP;
import static com.android.server.wm.WindowManagerDebugConfig.DEBUG_STACK;
import static com.android.server.wm.WindowManagerDebugConfig.TAG_WM;

import static java.lang.Integer.MAX_VALUE;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.Nullable;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.TaskDescription;
import android.app.ActivityManager.TaskSnapshot;
import android.app.ActivityOptions;
import android.app.ActivityTaskManager;
import android.app.AppGlobals;
import android.app.TaskInfo;
import android.app.WindowConfiguration;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.IPackageManager;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.os.Debug;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.Trace;
import android.os.UserHandle;
import android.provider.Settings;
import android.service.voice.IVoiceInteractionSession;
import android.util.DisplayMetrics;
import android.util.EventLog;
import android.util.Slog;
import android.util.proto.ProtoOutputStream;
import android.view.Display;
import android.view.DisplayInfo;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.app.IVoiceInteractor;
import com.android.internal.util.XmlUtils;
import com.android.server.wm.ActivityStack.ActivityState;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;
import org.xmlpull.v1.XmlSerializer;

import java.io.IOException;
import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Objects;

class TaskRecord extends ConfigurationContainer {
    private static final String TAG = TAG_WITH_CLASS_NAME ? "TaskRecord" : TAG_ATM;
    private static final String TAG_ADD_REMOVE = TAG + POSTFIX_ADD_REMOVE;
    private static final String TAG_RECENTS = TAG + POSTFIX_RECENTS;
    private static final String TAG_LOCKTASK = TAG + POSTFIX_LOCKTASK;
    private static final String TAG_TASKS = TAG + POSTFIX_TASKS;

    private static final String ATTR_TASKID = "task_id";
    private static final String TAG_INTENT = "intent";
    private static final String TAG_AFFINITYINTENT = "affinity_intent";
    private static final String ATTR_REALACTIVITY = "real_activity";
    private static final String ATTR_REALACTIVITY_SUSPENDED = "real_activity_suspended";
    private static final String ATTR_ORIGACTIVITY = "orig_activity";
    private static final String TAG_ACTIVITY = "activity";
    private static final String ATTR_AFFINITY = "affinity";
    private static final String ATTR_ROOT_AFFINITY = "root_affinity";
    private static final String ATTR_ROOTHASRESET = "root_has_reset";
    private static final String ATTR_AUTOREMOVERECENTS = "auto_remove_recents";
    private static final String ATTR_ASKEDCOMPATMODE = "asked_compat_mode";
    private static final String ATTR_USERID = "user_id";
    private static final String ATTR_USER_SETUP_COMPLETE = "user_setup_complete";
    private static final String ATTR_EFFECTIVE_UID = "effective_uid";
    @Deprecated
    private static final String ATTR_TASKTYPE = "task_type";
    private static final String ATTR_LASTDESCRIPTION = "last_description";
    private static final String ATTR_LASTTIMEMOVED = "last_time_moved";
    private static final String ATTR_NEVERRELINQUISH = "never_relinquish_identity";
    private static final String ATTR_TASK_AFFILIATION = "task_affiliation";
    private static final String ATTR_PREV_AFFILIATION = "prev_affiliation";
    private static final String ATTR_NEXT_AFFILIATION = "next_affiliation";
    private static final String ATTR_TASK_AFFILIATION_COLOR = "task_affiliation_color";
    private static final String ATTR_CALLING_UID = "calling_uid";
    private static final String ATTR_CALLING_PACKAGE = "calling_package";
    private static final String ATTR_SUPPORTS_PICTURE_IN_PICTURE = "supports_picture_in_picture";
    private static final String ATTR_RESIZE_MODE = "resize_mode";
    private static final String ATTR_NON_FULLSCREEN_BOUNDS = "non_fullscreen_bounds";
    private static final String ATTR_MIN_WIDTH = "min_width";
    private static final String ATTR_MIN_HEIGHT = "min_height";
    private static final String ATTR_PERSIST_TASK_VERSION = "persist_task_version";

    // Current version of the task record we persist. Used to check if we need to run any upgrade
    // code.
    private static final int PERSIST_TASK_VERSION = 1;

    private static final int INVALID_MIN_SIZE = -1;

    /**
     * The modes to control how the stack is moved to the front when calling
     * {@link TaskRecord#reparent}.
     */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            REPARENT_MOVE_STACK_TO_FRONT,
            REPARENT_KEEP_STACK_AT_FRONT,
            REPARENT_LEAVE_STACK_IN_PLACE
    })
    @interface ReparentMoveStackMode {}
    // Moves the stack to the front if it was not at the front
    static final int REPARENT_MOVE_STACK_TO_FRONT = 0;
    // Only moves the stack to the front if it was focused or front most already
    static final int REPARENT_KEEP_STACK_AT_FRONT = 1;
    // Do not move the stack as a part of reparenting
    static final int REPARENT_LEAVE_STACK_IN_PLACE = 2;

    /**
     * The factory used to create {@link TaskRecord}. This allows OEM subclass {@link TaskRecord}.
     */
    private static TaskRecordFactory sTaskRecordFactory;

    final int taskId;       // Unique identifier for this task.
    String affinity;        // The affinity name for this task, or null; may change identity.
    String rootAffinity;    // Initial base affinity, or null; does not change from initial root.
    final IVoiceInteractionSession voiceSession;    // Voice interaction session driving task
    final IVoiceInteractor voiceInteractor;         // Associated interactor to provide to app
    Intent intent;          // The original intent that started the task. Note that this value can
                            // be null.
    Intent affinityIntent;  // Intent of affinity-moved activity that started this task.
    int effectiveUid;       // The current effective uid of the identity of this task.
    ComponentName origActivity; // The non-alias activity component of the intent.
    ComponentName realActivity; // The actual activity component that started the task.
    boolean realActivitySuspended; // True if the actual activity component that started the
                                   // task is suspended.
    boolean inRecents;      // Actually in the recents list?
    long lastActiveTime;    // Last time this task was active in the current device session,
                            // including sleep. This time is initialized to the elapsed time when
                            // restored from disk.
    boolean isAvailable;    // Is the activity available to be launched?
    boolean rootWasReset;   // True if the intent at the root of the task had
                            // the FLAG_ACTIVITY_RESET_TASK_IF_NEEDED flag.
    boolean autoRemoveRecents;  // If true, we should automatically remove the task from
                                // recents when activity finishes
    boolean askedCompatMode;// Have asked the user about compat mode for this task.
    boolean hasBeenVisible; // Set if any activities in the task have been visible to the user.

    String stringName;      // caching of toString() result.
    int userId;             // user for which this task was created
    boolean mUserSetupComplete; // The user set-up is complete as of the last time the task activity
                                // was changed.

    int numFullscreen;      // Number of fullscreen activities.

    int mResizeMode;        // The resize mode of this task and its activities.
                            // Based on the {@link ActivityInfo#resizeMode} of the root activity.
    private boolean mSupportsPictureInPicture;  // Whether or not this task and its activities
            // support PiP. Based on the {@link ActivityInfo#FLAG_SUPPORTS_PICTURE_IN_PICTURE} flag
            // of the root activity.
    /** Can't be put in lockTask mode. */
    final static int LOCK_TASK_AUTH_DONT_LOCK = 0;
    /** Can enter app pinning with user approval. Can never start over existing lockTask task. */
    final static int LOCK_TASK_AUTH_PINNABLE = 1;
    /** Starts in LOCK_TASK_MODE_LOCKED automatically. Can start over existing lockTask task. */
    final static int LOCK_TASK_AUTH_LAUNCHABLE = 2;
    /** Can enter lockTask without user approval. Can start over existing lockTask task. */
    final static int LOCK_TASK_AUTH_WHITELISTED = 3;
    /** Priv-app that starts in LOCK_TASK_MODE_LOCKED automatically. Can start over existing
     * lockTask task. */
    final static int LOCK_TASK_AUTH_LAUNCHABLE_PRIV = 4;
    int mLockTaskAuth = LOCK_TASK_AUTH_PINNABLE;

    int mLockTaskUid = -1;  // The uid of the application that called startLockTask().

    // This represents the last resolved activity values for this task
    // NOTE: This value needs to be persisted with each task
    TaskDescription lastTaskDescription = new TaskDescription();

    /** List of all activities in the task arranged in history order */
    final ArrayList<ActivityRecord> mActivities;

    /** Current stack. Setter must always be used to update the value. */
    private ActivityStack mStack;

    /** The process that had previously hosted the root activity of this task.
     * Used to know that we should try harder to keep this process around, in case the
     * user wants to return to it. */
    private WindowProcessController mRootProcess;

    /** Takes on same value as first root activity */
    boolean isPersistable = false;
    int maxRecents;

    /** Only used for persistable tasks, otherwise 0. The last time this task was moved. Used for
     * determining the order when restoring. Sign indicates whether last task movement was to front
     * (positive) or back (negative). Absolute value indicates time. */
    long mLastTimeMoved = System.currentTimeMillis();

    /** If original intent did not allow relinquishing task identity, save that information */
    private boolean mNeverRelinquishIdentity = true;

    // Used in the unique case where we are clearing the task in order to reuse it. In that case we
    // do not want to delete the stack when the task goes empty.
    private boolean mReuseTask = false;

    CharSequence lastDescription; // Last description captured for this item.

    int mAffiliatedTaskId; // taskId of parent affiliation or self if no parent.
    int mAffiliatedTaskColor; // color of the parent task affiliation.
    TaskRecord mPrevAffiliate; // previous task in affiliated chain.
    int mPrevAffiliateTaskId = INVALID_TASK_ID; // previous id for persistence.
    TaskRecord mNextAffiliate; // next task in affiliated chain.
    int mNextAffiliateTaskId = INVALID_TASK_ID; // next id for persistence.

    // For relaunching the task from recents as though it was launched by the original launcher.
    int mCallingUid;
    String mCallingPackage;

    final ActivityTaskManagerService mService;

    private final Rect mTmpStableBounds = new Rect();
    private final Rect mTmpNonDecorBounds = new Rect();
    private final Rect mTmpBounds = new Rect();
    private final Rect mTmpInsets = new Rect();

    // Last non-fullscreen bounds the task was launched in or resized to.
    // The information is persisted and used to determine the appropriate stack to launch the
    // task into on restore.
    Rect mLastNonFullscreenBounds = null;
    // Minimal width and height of this task when it's resizeable. -1 means it should use the
    // default minimal width/height.
    int mMinWidth;
    int mMinHeight;

    // Ranking (from top) of this task among all visible tasks. (-1 means it's not visible)
    // This number will be assigned when we evaluate OOM scores for all visible tasks.
    int mLayerRank = -1;

    // When non-empty, this represents the bounds this task will be drawn at. This gets set during
    // transient operations such as split-divider dragging and animations.
    // TODO(b/119687367): This member is temporary.
    final Rect mDisplayedBounds = new Rect();

    /** Helper object used for updating override configuration. */
    private Configuration mTmpConfig = new Configuration();

    // TODO: remove after unification
    Task mTask;

    /** Used by fillTaskInfo */
    final TaskActivitiesReport mReuseActivitiesReport = new TaskActivitiesReport();

    /**
     * Don't use constructor directly. Use {@link #create(ActivityTaskManagerService, int,
     * ActivityInfo, Intent, TaskDescription)} instead.
     */
    TaskRecord(ActivityTaskManagerService service, int _taskId, ActivityInfo info, Intent _intent,
            IVoiceInteractionSession _voiceSession, IVoiceInteractor _voiceInteractor) {
        mService = service;
        userId = UserHandle.getUserId(info.applicationInfo.uid);
        taskId = _taskId;
        lastActiveTime = SystemClock.elapsedRealtime();
        mAffiliatedTaskId = _taskId;
        voiceSession = _voiceSession;
        voiceInteractor = _voiceInteractor;
        isAvailable = true;
        mActivities = new ArrayList<>();
        mCallingUid = info.applicationInfo.uid;
        mCallingPackage = info.packageName;
        setIntent(_intent, info);
        setMinDimensions(info);
        touchActiveTime();
        mService.getTaskChangeNotificationController().notifyTaskCreated(_taskId, realActivity);
    }

    /**
     * Don't use constructor directly.
     * Use {@link #create(ActivityTaskManagerService, int, ActivityInfo,
     * Intent, IVoiceInteractionSession, IVoiceInteractor)} instead.
     */
    TaskRecord(ActivityTaskManagerService service, int _taskId, ActivityInfo info, Intent _intent,
            TaskDescription _taskDescription) {
        mService = service;
        userId = UserHandle.getUserId(info.applicationInfo.uid);
        taskId = _taskId;
        lastActiveTime = SystemClock.elapsedRealtime();
        mAffiliatedTaskId = _taskId;
        voiceSession = null;
        voiceInteractor = null;
        isAvailable = true;
        mActivities = new ArrayList<>();
        mCallingUid = info.applicationInfo.uid;
        mCallingPackage = info.packageName;
        setIntent(_intent, info);
        setMinDimensions(info);

        isPersistable = true;
        // Clamp to [1, max].
        maxRecents = Math.min(Math.max(info.maxRecents, 1),
                ActivityTaskManager.getMaxAppRecentsLimitStatic());

        lastTaskDescription = _taskDescription;
        touchActiveTime();
        mService.getTaskChangeNotificationController().notifyTaskCreated(_taskId, realActivity);
    }

    /**
     * Don't use constructor directly. This is only used by XML parser.
     */
    TaskRecord(ActivityTaskManagerService service, int _taskId, Intent _intent,
            Intent _affinityIntent, String _affinity, String _rootAffinity,
            ComponentName _realActivity, ComponentName _origActivity, boolean _rootWasReset,
            boolean _autoRemoveRecents, boolean _askedCompatMode, int _userId,
            int _effectiveUid, String _lastDescription, ArrayList<ActivityRecord> activities,
            long lastTimeMoved, boolean neverRelinquishIdentity,
            TaskDescription _lastTaskDescription, int taskAffiliation, int prevTaskId,
            int nextTaskId, int taskAffiliationColor, int callingUid, String callingPackage,
            int resizeMode, boolean supportsPictureInPicture, boolean _realActivitySuspended,
            boolean userSetupComplete, int minWidth, int minHeight) {
        mService = service;
        taskId = _taskId;
        intent = _intent;
        affinityIntent = _affinityIntent;
        affinity = _affinity;
        rootAffinity = _rootAffinity;
        voiceSession = null;
        voiceInteractor = null;
        realActivity = _realActivity;
        realActivitySuspended = _realActivitySuspended;
        origActivity = _origActivity;
        rootWasReset = _rootWasReset;
        isAvailable = true;
        autoRemoveRecents = _autoRemoveRecents;
        askedCompatMode = _askedCompatMode;
        userId = _userId;
        mUserSetupComplete = userSetupComplete;
        effectiveUid = _effectiveUid;
        lastActiveTime = SystemClock.elapsedRealtime();
        lastDescription = _lastDescription;
        mActivities = activities;
        mLastTimeMoved = lastTimeMoved;
        mNeverRelinquishIdentity = neverRelinquishIdentity;
        lastTaskDescription = _lastTaskDescription;
        mAffiliatedTaskId = taskAffiliation;
        mAffiliatedTaskColor = taskAffiliationColor;
        mPrevAffiliateTaskId = prevTaskId;
        mNextAffiliateTaskId = nextTaskId;
        mCallingUid = callingUid;
        mCallingPackage = callingPackage;
        mResizeMode = resizeMode;
        mSupportsPictureInPicture = supportsPictureInPicture;
        mMinWidth = minWidth;
        mMinHeight = minHeight;
        mService.getTaskChangeNotificationController().notifyTaskCreated(_taskId, realActivity);
    }

    Task getTask() {
        return mTask;
    }

    void createTask(boolean onTop, boolean showForAllUsers) {
        if (mTask != null) {
            throw new IllegalArgumentException("mTask=" + mTask
                    + " already created for task=" + this);
        }

        final Rect bounds = updateOverrideConfigurationFromLaunchBounds();
        final TaskStack stack = getStack().getTaskStack();

        if (stack == null) {
            throw new IllegalArgumentException("TaskRecord: invalid stack=" + mStack);
        }
        EventLog.writeEvent(WM_TASK_CREATED, taskId, stack.mStackId);
        mTask = new Task(taskId, stack, userId, mService.mWindowManager, mResizeMode,
                mSupportsPictureInPicture, lastTaskDescription, this);
        final int position = onTop ? POSITION_TOP : POSITION_BOTTOM;

        if (!mDisplayedBounds.isEmpty()) {
            mTask.setOverrideDisplayedBounds(mDisplayedBounds);
        }
        // We only want to move the parents to the parents if we are creating this task at the
        // top of its stack.
        stack.addTask(mTask, position, showForAllUsers, onTop /* moveParents */);
    }

    void setTask(Task task) {
        mTask = task;
    }

    void cleanUpResourcesForDestroy() {
        if (!mActivities.isEmpty()) {
            return;
        }

        // This task is going away, so save the last state if necessary.
        saveLaunchingStateIfNeeded();

        // TODO: VI what about activity?
        final boolean isVoiceSession = voiceSession != null;
        if (isVoiceSession) {
            try {
                voiceSession.taskFinished(intent, taskId);
            } catch (RemoteException e) {
            }
        }
        if (autoRemoveFromRecents() || isVoiceSession) {
            // Task creator asked to remove this when done, or this task was a voice
            // interaction, so it should not remain on the recent tasks list.
            mService.mStackSupervisor.mRecentTasks.remove(this);
        }

        removeWindowContainer();
    }

    @VisibleForTesting
    void removeWindowContainer() {
        mService.getLockTaskController().clearLockedTask(this);
        if (mTask == null) {
            if (DEBUG_STACK) Slog.i(TAG_WM, "removeTask: could not find taskId=" + taskId);
            return;
        }
        mTask.removeIfPossible();
        mTask = null;
        if (!getWindowConfiguration().persistTaskBounds()) {
            // Reset current bounds for task whose bounds shouldn't be persisted so it uses
            // default configuration the next time it launches.
            updateOverrideConfiguration(null);
        }
        mService.getTaskChangeNotificationController().notifyTaskRemoved(taskId);
    }

    public void onSnapshotChanged(TaskSnapshot snapshot) {
        mService.getTaskChangeNotificationController().notifyTaskSnapshotChanged(taskId, snapshot);
    }

    void setResizeMode(int resizeMode) {
        if (mResizeMode == resizeMode) {
            return;
        }
        mResizeMode = resizeMode;
        mTask.setResizeable(resizeMode);
        mService.mRootActivityContainer.ensureActivitiesVisible(null, 0, !PRESERVE_WINDOWS);
        mService.mRootActivityContainer.resumeFocusedStacksTopActivities();
    }

    void setTaskDockedResizing(boolean resizing) {
        if (mTask == null) {
            Slog.w(TAG_WM, "setTaskDockedResizing: taskId " + taskId + " not found.");
            return;
        }
        mTask.setTaskDockedResizing(resizing);
    }

    // TODO: Consolidate this with the resize() method below.
    public void requestResize(Rect bounds, int resizeMode) {
        mService.resizeTask(taskId, bounds, resizeMode);
    }

    boolean resize(Rect bounds, int resizeMode, boolean preserveWindow, boolean deferResume) {
        mService.mWindowManager.deferSurfaceLayout();

        try {
            if (!isResizeable()) {
                Slog.w(TAG, "resizeTask: task " + this + " not resizeable.");
                return true;
            }

            // If this is a forced resize, let it go through even if the bounds is not changing,
            // as we might need a relayout due to surface size change (to/from fullscreen).
            final boolean forced = (resizeMode & RESIZE_MODE_FORCED) != 0;
            if (equivalentRequestedOverrideBounds(bounds) && !forced) {
                // Nothing to do here...
                return true;
            }

            if (mTask == null) {
                // Task doesn't exist in window manager yet (e.g. was restored from recents).
                // All we can do for now is update the bounds so it can be used when the task is
                // added to window manager.
                updateOverrideConfiguration(bounds);
                if (!inFreeformWindowingMode()) {
                    // re-restore the task so it can have the proper stack association.
                    mService.mStackSupervisor.restoreRecentTaskLocked(this, null, !ON_TOP);
                }
                return true;
            }

            if (!canResizeToBounds(bounds)) {
                throw new IllegalArgumentException("resizeTask: Can not resize task=" + this
                        + " to bounds=" + bounds + " resizeMode=" + mResizeMode);
            }

            // Do not move the task to another stack here.
            // This method assumes that the task is already placed in the right stack.
            // we do not mess with that decision and we only do the resize!

            Trace.traceBegin(TRACE_TAG_ACTIVITY_MANAGER, "am.resizeTask_" + taskId);

            final boolean updatedConfig = updateOverrideConfiguration(bounds);
            // This variable holds information whether the configuration didn't change in a significant

            // way and the activity was kept the way it was. If it's false, it means the activity
            // had
            // to be relaunched due to configuration change.
            boolean kept = true;
            if (updatedConfig) {
                final ActivityRecord r = topRunningActivityLocked();
                if (r != null && !deferResume) {
                    kept = r.ensureActivityConfiguration(0 /* globalChanges */,
                            preserveWindow);
                    // Preserve other windows for resizing because if resizing happens when there
                    // is a dialog activity in the front, the activity that still shows some
                    // content to the user will become black and cause flickers. Note in most cases
                    // this won't cause tons of irrelevant windows being preserved because only
                    // activities in this task may experience a bounds change. Configs for other
                    // activities stay the same.
                    mService.mRootActivityContainer.ensureActivitiesVisible(r, 0, preserveWindow);
                    if (!kept) {
                        mService.mRootActivityContainer.resumeFocusedStacksTopActivities();
                    }
                }
            }
            mTask.resize(kept, forced);

            saveLaunchingStateIfNeeded();

            Trace.traceEnd(TRACE_TAG_ACTIVITY_MANAGER);
            return kept;
        } finally {
            mService.mWindowManager.continueSurfaceLayout();
        }
    }

    // TODO: Investigate combining with the resize() method above.
    void resizeWindowContainer() {
        mTask.resize(false /* relayout */, false /* forced */);
    }

    void getWindowContainerBounds(Rect bounds) {
        if (mTask != null) {
            mTask.getBounds(bounds);
        } else {
            bounds.setEmpty();
        }
    }

    /**
     * Convenience method to reparent a task to the top or bottom position of the stack.
     */
    boolean reparent(ActivityStack preferredStack, boolean toTop,
            @ReparentMoveStackMode int moveStackMode, boolean animate, boolean deferResume,
            String reason) {
        return reparent(preferredStack, toTop ? MAX_VALUE : 0, moveStackMode, animate, deferResume,
                true /* schedulePictureInPictureModeChange */, reason);
    }

    /**
     * Convenience method to reparent a task to the top or bottom position of the stack, with
     * an option to skip scheduling the picture-in-picture mode change.
     */
    boolean reparent(ActivityStack preferredStack, boolean toTop,
            @ReparentMoveStackMode int moveStackMode, boolean animate, boolean deferResume,
            boolean schedulePictureInPictureModeChange, String reason) {
        return reparent(preferredStack, toTop ? MAX_VALUE : 0, moveStackMode, animate,
                deferResume, schedulePictureInPictureModeChange, reason);
    }

    /** Convenience method to reparent a task to a specific position of the stack. */
    boolean reparent(ActivityStack preferredStack, int position,
            @ReparentMoveStackMode int moveStackMode, boolean animate, boolean deferResume,
            String reason) {
        return reparent(preferredStack, position, moveStackMode, animate, deferResume,
                true /* schedulePictureInPictureModeChange */, reason);
    }

    /**
     * Reparents the task into a preferred stack, creating it if necessary.
     *
     * @param preferredStack the target stack to move this task
     * @param position the position to place this task in the new stack
     * @param animate whether or not we should wait for the new window created as a part of the
     *            reparenting to be drawn and animated in
     * @param moveStackMode whether or not to move the stack to the front always, only if it was
     *            previously focused & in front, or never
     * @param deferResume whether or not to update the visibility of other tasks and stacks that may
     *            have changed as a result of this reparenting
     * @param schedulePictureInPictureModeChange specifies whether or not to schedule the PiP mode
     *            change. Callers may set this to false if they are explicitly scheduling PiP mode
     *            changes themselves, like during the PiP animation
     * @param reason the caller of this reparenting
     * @return whether the task was reparented
     */
    // TODO: Inspect all call sites and change to just changing windowing mode of the stack vs.
    // re-parenting the task. Can only be done when we are no longer using static stack Ids.
    boolean reparent(ActivityStack preferredStack, int position,
            @ReparentMoveStackMode int moveStackMode, boolean animate, boolean deferResume,
            boolean schedulePictureInPictureModeChange, String reason) {
        final ActivityStackSupervisor supervisor = mService.mStackSupervisor;
        final RootActivityContainer root = mService.mRootActivityContainer;
        final WindowManagerService windowManager = mService.mWindowManager;
        final ActivityStack sourceStack = getStack();
        final ActivityStack toStack = supervisor.getReparentTargetStack(this, preferredStack,
                position == MAX_VALUE);
        if (toStack == sourceStack) {
            return false;
        }
        if (!canBeLaunchedOnDisplay(toStack.mDisplayId)) {
            return false;
        }

        final boolean toTopOfStack = position == MAX_VALUE;
        if (toTopOfStack && toStack.getResumedActivity() != null
                && toStack.topRunningActivityLocked() != null) {
            // Pause the resumed activity on the target stack while re-parenting task on top of it.
            toStack.startPausingLocked(false /* userLeaving */, false /* uiSleeping */,
                    null /* resuming */, false /* pauseImmediately */);
        }

        final int toStackWindowingMode = toStack.getWindowingMode();
        final ActivityRecord topActivity = getTopActivity();

        final boolean mightReplaceWindow = topActivity != null
                && replaceWindowsOnTaskMove(getWindowingMode(), toStackWindowingMode);
        if (mightReplaceWindow) {
            // We are about to relaunch the activity because its configuration changed due to
            // being maximized, i.e. size change. The activity will first remove the old window
            // and then add a new one. This call will tell window manager about this, so it can
            // preserve the old window until the new one is drawn. This prevents having a gap
            // between the removal and addition, in which no window is visible. We also want the
            // entrance of the new window to be properly animated.
            // Note here we always set the replacing window first, as the flags might be needed
            // during the relaunch. If we end up not doing any relaunch, we clear the flags later.
            windowManager.setWillReplaceWindow(topActivity.appToken, animate);
        }

        windowManager.deferSurfaceLayout();
        boolean kept = true;
        try {
            final ActivityRecord r = topRunningActivityLocked();
            final boolean wasFocused = r != null && root.isTopDisplayFocusedStack(sourceStack)
                    && (topRunningActivityLocked() == r);
            final boolean wasResumed = r != null && sourceStack.getResumedActivity() == r;
            final boolean wasPaused = r != null && sourceStack.mPausingActivity == r;

            // In some cases the focused stack isn't the front stack. E.g. pinned stack.
            // Whenever we are moving the top activity from the front stack we want to make sure to
            // move the stack to the front.
            final boolean wasFront = r != null && sourceStack.isTopStackOnDisplay()
                    && (sourceStack.topRunningActivityLocked() == r);

            // Adjust the position for the new parent stack as needed.
            position = toStack.getAdjustedPositionForTask(this, position, null /* starting */);

            // Must reparent first in window manager to avoid a situation where AM can delete the
            // we are coming from in WM before we reparent because it became empty.
            mTask.reparent(toStack.getTaskStack(), position,
                    moveStackMode == REPARENT_MOVE_STACK_TO_FRONT);

            final boolean moveStackToFront = moveStackMode == REPARENT_MOVE_STACK_TO_FRONT
                    || (moveStackMode == REPARENT_KEEP_STACK_AT_FRONT && (wasFocused || wasFront));
            // Move the task
            sourceStack.removeTask(this, reason, moveStackToFront
                    ? REMOVE_TASK_MODE_MOVING_TO_TOP : REMOVE_TASK_MODE_MOVING);
            toStack.addTask(this, position, false /* schedulePictureInPictureModeChange */, reason);

            if (schedulePictureInPictureModeChange) {
                // Notify of picture-in-picture mode changes
                supervisor.scheduleUpdatePictureInPictureModeIfNeeded(this, sourceStack);
            }

            // TODO: Ensure that this is actually necessary here
            // Notify the voice session if required
            if (voiceSession != null) {
                try {
                    voiceSession.taskStarted(intent, taskId);
                } catch (RemoteException e) {
                }
            }

            // If the task had focus before (or we're requested to move focus), move focus to the
            // new stack by moving the stack to the front.
            if (r != null) {
                toStack.moveToFrontAndResumeStateIfNeeded(r, moveStackToFront, wasResumed,
                        wasPaused, reason);
            }
            if (!animate) {
                mService.mStackSupervisor.mNoAnimActivities.add(topActivity);
            }

            // We might trigger a configuration change. Save the current task bounds for freezing.
            // TODO: Should this call be moved inside the resize method in WM?
            toStack.prepareFreezingTaskBounds();

            // Make sure the task has the appropriate bounds/size for the stack it is in.
            final boolean toStackSplitScreenPrimary =
                    toStackWindowingMode == WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
            final Rect configBounds = getRequestedOverrideBounds();
            if ((toStackWindowingMode == WINDOWING_MODE_FULLSCREEN
                    || toStackWindowingMode == WINDOWING_MODE_SPLIT_SCREEN_SECONDARY)
                    && !Objects.equals(configBounds, toStack.getRequestedOverrideBounds())) {
                kept = resize(toStack.getRequestedOverrideBounds(), RESIZE_MODE_SYSTEM,
                        !mightReplaceWindow, deferResume);
            } else if (toStackWindowingMode == WINDOWING_MODE_FREEFORM) {
                Rect bounds = getLaunchBounds();
                if (bounds == null) {
                    mService.mStackSupervisor.getLaunchParamsController().layoutTask(this, null);
                    bounds = configBounds;
                }
                kept = resize(bounds, RESIZE_MODE_FORCED, !mightReplaceWindow, deferResume);
            } else if (toStackSplitScreenPrimary || toStackWindowingMode == WINDOWING_MODE_PINNED) {
                if (toStackSplitScreenPrimary && moveStackMode == REPARENT_KEEP_STACK_AT_FRONT) {
                    // Move recents to front so it is not behind home stack when going into docked
                    // mode
                    mService.mStackSupervisor.moveRecentsStackToFront(reason);
                }
                kept = resize(toStack.getRequestedOverrideBounds(), RESIZE_MODE_SYSTEM,
                        !mightReplaceWindow, deferResume);
            }
        } finally {
            windowManager.continueSurfaceLayout();
        }

        if (mightReplaceWindow) {
            // If we didn't actual do a relaunch (indicated by kept==true meaning we kept the old
            // window), we need to clear the replace window settings. Otherwise, we schedule a
            // timeout to remove the old window if the replacing window is not coming in time.
            windowManager.scheduleClearWillReplaceWindows(topActivity.appToken, !kept);
        }

        if (!deferResume) {
            // The task might have already been running and its visibility needs to be synchronized
            // with the visibility of the stack / windows.
            root.ensureActivitiesVisible(null, 0, !mightReplaceWindow);
            root.resumeFocusedStacksTopActivities();
        }

        // TODO: Handle incorrect request to move before the actual move, not after.
        supervisor.handleNonResizableTaskIfNeeded(this, preferredStack.getWindowingMode(),
                DEFAULT_DISPLAY, toStack);

        return (preferredStack == toStack);
    }

    /**
     * @return True if the windows of tasks being moved to the target stack from the source stack
     * should be replaced, meaning that window manager will keep the old window around until the new
     * is ready.
     */
    private static boolean replaceWindowsOnTaskMove(
            int sourceWindowingMode, int targetWindowingMode) {
        return sourceWindowingMode == WINDOWING_MODE_FREEFORM
                || targetWindowingMode == WINDOWING_MODE_FREEFORM;
    }

    void cancelWindowTransition() {
        if (mTask == null) {
            Slog.w(TAG_WM, "cancelWindowTransition: taskId " + taskId + " not found.");
            return;
        }
        mTask.cancelTaskWindowTransition();
    }

    /**
     * DO NOT HOLD THE ACTIVITY MANAGER LOCK WHEN CALLING THIS METHOD!
     */
    TaskSnapshot getSnapshot(boolean reducedResolution, boolean restoreFromDisk) {

        // TODO: Move this to {@link TaskWindowContainerController} once recent tasks are more
        // synchronized between AM and WM.
        return mService.mWindowManager.getTaskSnapshot(taskId, userId, reducedResolution,
                restoreFromDisk);
    }

    void touchActiveTime() {
        lastActiveTime = SystemClock.elapsedRealtime();
    }

    long getInactiveDuration() {
        return SystemClock.elapsedRealtime() - lastActiveTime;
    }

    /** Sets the original intent, and the calling uid and package. */
    void setIntent(ActivityRecord r) {
        mCallingUid = r.launchedFromUid;
        mCallingPackage = r.launchedFromPackage;
        setIntent(r.intent, r.info);
        setLockTaskAuth(r);
    }

    /** Sets the original intent, _without_ updating the calling uid or package. */
    private void setIntent(Intent _intent, ActivityInfo info) {
        if (intent == null) {
            mNeverRelinquishIdentity =
                    (info.flags & FLAG_RELINQUISH_TASK_IDENTITY) == 0;
        } else if (mNeverRelinquishIdentity) {
            return;
        }

        affinity = info.taskAffinity;
        if (intent == null) {
            // If this task already has an intent associated with it, don't set the root
            // affinity -- we don't want it changing after initially set, but the initially
            // set value may be null.
            rootAffinity = affinity;
        }
        effectiveUid = info.applicationInfo.uid;
        stringName = null;

        if (info.targetActivity == null) {
            if (_intent != null) {
                // If this Intent has a selector, we want to clear it for the
                // recent task since it is not relevant if the user later wants
                // to re-launch the app.
                if (_intent.getSelector() != null || _intent.getSourceBounds() != null) {
                    _intent = new Intent(_intent);
                    _intent.setSelector(null);
                    _intent.setSourceBounds(null);
                }
            }
            if (DEBUG_TASKS) Slog.v(TAG_TASKS, "Setting Intent of " + this + " to " + _intent);
            intent = _intent;
            realActivity = _intent != null ? _intent.getComponent() : null;
            origActivity = null;
        } else {
            ComponentName targetComponent = new ComponentName(
                    info.packageName, info.targetActivity);
            if (_intent != null) {
                Intent targetIntent = new Intent(_intent);
                targetIntent.setSelector(null);
                targetIntent.setSourceBounds(null);
                if (DEBUG_TASKS) Slog.v(TAG_TASKS,
                        "Setting Intent of " + this + " to target " + targetIntent);
                intent = targetIntent;
                realActivity = targetComponent;
                origActivity = _intent.getComponent();
            } else {
                intent = null;
                realActivity = targetComponent;
                origActivity = new ComponentName(info.packageName, info.name);
            }
        }

        final int intentFlags = intent == null ? 0 : intent.getFlags();
        if ((intentFlags & Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED) != 0) {
            // Once we are set to an Intent with this flag, we count this
            // task as having a true root activity.
            rootWasReset = true;
        }
        userId = UserHandle.getUserId(info.applicationInfo.uid);
        mUserSetupComplete = Settings.Secure.getIntForUser(mService.mContext.getContentResolver(),
                USER_SETUP_COMPLETE, 0, userId) != 0;
        if ((info.flags & ActivityInfo.FLAG_AUTO_REMOVE_FROM_RECENTS) != 0) {
            // If the activity itself has requested auto-remove, then just always do it.
            autoRemoveRecents = true;
        } else if ((intentFlags & (FLAG_ACTIVITY_NEW_DOCUMENT | FLAG_ACTIVITY_RETAIN_IN_RECENTS))
                == FLAG_ACTIVITY_NEW_DOCUMENT) {
            // If the caller has not asked for the document to be retained, then we may
            // want to turn on auto-remove, depending on whether the target has set its
            // own document launch mode.
            if (info.documentLaunchMode != ActivityInfo.DOCUMENT_LAUNCH_NONE) {
                autoRemoveRecents = false;
            } else {
                autoRemoveRecents = true;
            }
        } else {
            autoRemoveRecents = false;
        }
        mResizeMode = info.resizeMode;
        mSupportsPictureInPicture = info.supportsPictureInPicture();
    }

    /** Sets the original minimal width and height. */
    private void setMinDimensions(ActivityInfo info) {
        if (info != null && info.windowLayout != null) {
            mMinWidth = info.windowLayout.minWidth;
            mMinHeight = info.windowLayout.minHeight;
        } else {
            mMinWidth = INVALID_MIN_SIZE;
            mMinHeight = INVALID_MIN_SIZE;
        }
    }

    /**
     * Return true if the input activity has the same intent filter as the intent this task
     * record is based on (normally the root activity intent).
     */
    boolean isSameIntentFilter(ActivityRecord r) {
        final Intent intent = new Intent(r.intent);
        // Make sure the component are the same if the input activity has the same real activity
        // as the one in the task because either one of them could be the alias activity.
        if (Objects.equals(realActivity, r.mActivityComponent) && this.intent != null) {
            intent.setComponent(this.intent.getComponent());
        }
        return intent.filterEquals(this.intent);
    }

    boolean returnsToHomeStack() {
        final int returnHomeFlags = FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_TASK_ON_HOME;
        return intent != null && (intent.getFlags() & returnHomeFlags) == returnHomeFlags;
    }

    void setPrevAffiliate(TaskRecord prevAffiliate) {
        mPrevAffiliate = prevAffiliate;
        mPrevAffiliateTaskId = prevAffiliate == null ? INVALID_TASK_ID : prevAffiliate.taskId;
    }

    void setNextAffiliate(TaskRecord nextAffiliate) {
        mNextAffiliate = nextAffiliate;
        mNextAffiliateTaskId = nextAffiliate == null ? INVALID_TASK_ID : nextAffiliate.taskId;
    }

    <T extends ActivityStack> T getStack() {
        return (T) mStack;
    }

    /**
     * Must be used for setting parent stack because it performs configuration updates.
     * Must be called after adding task as a child to the stack.
     */
    void setStack(ActivityStack stack) {
        if (stack != null && !stack.isInStackLocked(this)) {
            throw new IllegalStateException("Task must be added as a Stack child first.");
        }
        final ActivityStack oldStack = mStack;
        mStack = stack;

        // If the new {@link TaskRecord} is from a different {@link ActivityStack}, remove this
        // {@link ActivityRecord} from its current {@link ActivityStack}.

        if (oldStack != mStack) {
            for (int i = getChildCount() - 1; i >= 0; --i) {
                final ActivityRecord activity = getChildAt(i);

                if (oldStack != null) {
                    oldStack.onActivityRemovedFromStack(activity);
                }

                if (mStack != null) {
                    stack.onActivityAddedToStack(activity);
                }
            }
        }

        onParentChanged();
    }

    /**
     * @return Id of current stack, {@link INVALID_STACK_ID} if no stack is set.
     */
    int getStackId() {
        return mStack != null ? mStack.mStackId : INVALID_STACK_ID;
    }

    @Override
    protected int getChildCount() {
        return mActivities.size();
    }

    @Override
    protected ActivityRecord getChildAt(int index) {
        return mActivities.get(index);
    }

    @Override
    protected ConfigurationContainer getParent() {
        return mStack;
    }

    @Override
    protected void onParentChanged() {
        super.onParentChanged();
        mService.mRootActivityContainer.updateUIDsPresentOnDisplay();
    }

    // Close up recents linked list.
    private void closeRecentsChain() {
        if (mPrevAffiliate != null) {
            mPrevAffiliate.setNextAffiliate(mNextAffiliate);
        }
        if (mNextAffiliate != null) {
            mNextAffiliate.setPrevAffiliate(mPrevAffiliate);
        }
        setPrevAffiliate(null);
        setNextAffiliate(null);
    }

    void removedFromRecents() {
        closeRecentsChain();
        if (inRecents) {
            inRecents = false;
            mService.notifyTaskPersisterLocked(this, false);
        }

        clearRootProcess();

        // TODO: Use window container controller once tasks are better synced between AM and WM
        mService.mWindowManager.notifyTaskRemovedFromRecents(taskId, userId);
    }

    void setTaskToAffiliateWith(TaskRecord taskToAffiliateWith) {
        closeRecentsChain();
        mAffiliatedTaskId = taskToAffiliateWith.mAffiliatedTaskId;
        mAffiliatedTaskColor = taskToAffiliateWith.mAffiliatedTaskColor;
        // Find the end
        while (taskToAffiliateWith.mNextAffiliate != null) {
            final TaskRecord nextRecents = taskToAffiliateWith.mNextAffiliate;
            if (nextRecents.mAffiliatedTaskId != mAffiliatedTaskId) {
                Slog.e(TAG, "setTaskToAffiliateWith: nextRecents=" + nextRecents + " affilTaskId="
                        + nextRecents.mAffiliatedTaskId + " should be " + mAffiliatedTaskId);
                if (nextRecents.mPrevAffiliate == taskToAffiliateWith) {
                    nextRecents.setPrevAffiliate(null);
                }
                taskToAffiliateWith.setNextAffiliate(null);
                break;
            }
            taskToAffiliateWith = nextRecents;
        }
        taskToAffiliateWith.setNextAffiliate(this);
        setPrevAffiliate(taskToAffiliateWith);
        setNextAffiliate(null);
    }

    /** Returns the intent for the root activity for this task */
    Intent getBaseIntent() {
        return intent != null ? intent : affinityIntent;
    }

    /** Returns the first non-finishing activity from the root. */
    ActivityRecord getRootActivity() {
        for (int i = 0; i < mActivities.size(); i++) {
            final ActivityRecord r = mActivities.get(i);
            if (r.finishing) {
                continue;
            }
            return r;
        }
        return null;
    }

    ActivityRecord getTopActivity() {
        return getTopActivity(true /* includeOverlays */);
    }

    ActivityRecord getTopActivity(boolean includeOverlays) {
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            final ActivityRecord r = mActivities.get(i);
            if (r.finishing || (!includeOverlays && r.mTaskOverlay)) {
                continue;
            }
            return r;
        }
        return null;
    }

    ActivityRecord topRunningActivityLocked() {
        if (mStack != null) {
            for (int activityNdx = mActivities.size() - 1; activityNdx >= 0; --activityNdx) {
                ActivityRecord r = mActivities.get(activityNdx);
                if (!r.finishing && r.okToShowLocked()) {
                    return r;
                }
            }
        }
        return null;
    }

    boolean isVisible() {
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            final ActivityRecord r = mActivities.get(i);
            if (r.visible) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return true if any activities in this task belongs to input uid.
     */
    boolean containsAppUid(int uid) {
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            final ActivityRecord r = mActivities.get(i);
            if (r.getUid() == uid) {
                return true;
            }
        }
        return false;
    }

    void getAllRunningVisibleActivitiesLocked(ArrayList<ActivityRecord> outActivities) {
        if (mStack != null) {
            for (int activityNdx = mActivities.size() - 1; activityNdx >= 0; --activityNdx) {
                ActivityRecord r = mActivities.get(activityNdx);
                if (!r.finishing && r.okToShowLocked() && r.visibleIgnoringKeyguard) {
                    outActivities.add(r);
                }
            }
        }
    }

    ActivityRecord topRunningActivityWithStartingWindowLocked() {
        if (mStack != null) {
            for (int activityNdx = mActivities.size() - 1; activityNdx >= 0; --activityNdx) {
                ActivityRecord r = mActivities.get(activityNdx);
                if (r.mStartingWindowState != STARTING_WINDOW_SHOWN
                        || r.finishing || !r.okToShowLocked()) {
                    continue;
                }
                return r;
            }
        }
        return null;
    }

    /**
     * Return the number of running activities, and the number of non-finishing/initializing
     * activities in the provided {@param reportOut} respectively.
     */
    void getNumRunningActivities(TaskActivitiesReport reportOut) {
        reportOut.reset();
        for (int i = mActivities.size() - 1; i >= 0; --i) {
            final ActivityRecord r = mActivities.get(i);
            if (r.finishing) {
                continue;
            }

            reportOut.base = r;

            // Increment the total number of non-finishing activities
            reportOut.numActivities++;

            if (reportOut.top == null || (reportOut.top.isState(ActivityState.INITIALIZING))) {
                reportOut.top = r;
                // Reset the number of running activities until we hit the first non-initializing
                // activity
                reportOut.numRunning = 0;
            }
            if (r.attachedToProcess()) {
                // Increment the number of actually running activities
                reportOut.numRunning++;
            }
        }
    }

    boolean okToShowLocked() {
        // NOTE: If {@link TaskRecord#topRunningActivity} return is not null then it is
        // okay to show the activity when locked.
        return mService.mStackSupervisor.isCurrentProfileLocked(userId)
                || topRunningActivityLocked() != null;
    }

    /** Call after activity movement or finish to make sure that frontOfTask is set correctly */
    final void setFrontOfTask() {
        boolean foundFront = false;
        final int numActivities = mActivities.size();
        for (int activityNdx = 0; activityNdx < numActivities; ++activityNdx) {
            final ActivityRecord r = mActivities.get(activityNdx);
            if (foundFront || r.finishing) {
                r.frontOfTask = false;
            } else {
                r.frontOfTask = true;
                // Set frontOfTask false for every following activity.
                foundFront = true;
            }
        }
        if (!foundFront && numActivities > 0) {
            // All activities of this task are finishing. As we ought to have a frontOfTask
            // activity, make the bottom activity front.
            mActivities.get(0).frontOfTask = true;
        }
    }

    /**
     * Reorder the history stack so that the passed activity is brought to the front.
     */
    final void moveActivityToFrontLocked(ActivityRecord newTop) {
        if (DEBUG_ADD_REMOVE) Slog.i(TAG_ADD_REMOVE,
                "Removing and adding activity " + newTop
                + " to stack at top callers=" + Debug.getCallers(4));

        mActivities.remove(newTop);
        mActivities.add(newTop);

        // Make sure window manager is aware of the position change.
        mTask.positionChildAtTop(newTop.mAppWindowToken);
        updateEffectiveIntent();

        setFrontOfTask();
    }

    void addActivityToTop(ActivityRecord r) {
        addActivityAtIndex(mActivities.size(), r);
    }

    @Override
    /*@WindowConfiguration.ActivityType*/
    public int getActivityType() {
        final int applicationType = super.getActivityType();
        if (applicationType != ACTIVITY_TYPE_UNDEFINED || mActivities.isEmpty()) {
            return applicationType;
        }
        return mActivities.get(0).getActivityType();
    }

    /**
     * Adds an activity {@param r} at the given {@param index}. The activity {@param r} must either
     * be in the current task or unparented to any task.
     */
    void addActivityAtIndex(int index, ActivityRecord r) {
        TaskRecord task = r.getTaskRecord();
        if (task != null && task != this) {
            throw new IllegalArgumentException("Can not add r=" + " to task=" + this
                    + " current parent=" + task);
        }

        r.setTask(this);

        // Remove r first, and if it wasn't already in the list and it's fullscreen, count it.
        if (!mActivities.remove(r) && r.fullscreen) {
            // Was not previously in list.
            numFullscreen++;
        }
        // Only set this based on the first activity
        if (mActivities.isEmpty()) {
            if (r.getActivityType() == ACTIVITY_TYPE_UNDEFINED) {
                // Normally non-standard activity type for the activity record will be set when the
                // object is created, however we delay setting the standard application type until
                // this point so that the task can set the type for additional activities added in
                // the else condition below.
                r.setActivityType(ACTIVITY_TYPE_STANDARD);
            }
            setActivityType(r.getActivityType());
            isPersistable = r.isPersistable();
            mCallingUid = r.launchedFromUid;
            mCallingPackage = r.launchedFromPackage;
            // Clamp to [1, max].
            maxRecents = Math.min(Math.max(r.info.maxRecents, 1),
                    ActivityTaskManager.getMaxAppRecentsLimitStatic());
        } else {
            // Otherwise make all added activities match this one.
            r.setActivityType(getActivityType());
        }

        final int size = mActivities.size();

        if (index == size && size > 0) {
            final ActivityRecord top = mActivities.get(size - 1);
            if (top.mTaskOverlay) {
                // Place below the task overlay activity since the overlay activity should always
                // be on top.
                index--;
            }
        }

        index = Math.min(size, index);
        mActivities.add(index, r);

        updateEffectiveIntent();
        if (r.isPersistable()) {
            mService.notifyTaskPersisterLocked(this, false);
        }

        if (r.mAppWindowToken != null) {
            // Only attempt to move in WM if the child has a controller. It is possible we haven't
            // created controller for the activity we are starting yet.
            mTask.positionChildAt(r.mAppWindowToken, index);
        }

        // Make sure the list of display UID whitelists is updated
        // now that this record is in a new task.
        mService.mRootActivityContainer.updateUIDsPresentOnDisplay();
    }

    /**
     * Removes the specified activity from this task.
     * @param r The {@link ActivityRecord} to remove.
     * @return true if this was the last activity in the task.
     */
    boolean removeActivity(ActivityRecord r) {
        return removeActivity(r, false /* reparenting */);
    }

    boolean removeActivity(ActivityRecord r, boolean reparenting) {
        if (r.getTaskRecord() != this) {
            throw new IllegalArgumentException(
                    "Activity=" + r + " does not belong to task=" + this);
        }

        r.setTask(null /* task */, reparenting /* reparenting */);

        if (mActivities.remove(r) && r.fullscreen) {
            // Was previously in list.
            numFullscreen--;
        }
        if (r.isPersistable()) {
            mService.notifyTaskPersisterLocked(this, false);
        }

        if (inPinnedWindowingMode()) {
            // We normally notify listeners of task stack changes on pause, however pinned stack
            // activities are normally in the paused state so no notification will be sent there
            // before the activity is removed. We send it here so instead.
            mService.getTaskChangeNotificationController().notifyTaskStackChanged();
        }

        if (mActivities.isEmpty()) {
            return !mReuseTask;
        }
        updateEffectiveIntent();
        return false;
    }

    /**
     * @return whether or not there are ONLY task overlay activities in the stack.
     *         If {@param excludeFinishing} is set, then ignore finishing activities in the check.
     *         If there are no task overlay activities, this call returns false.
     */
    boolean onlyHasTaskOverlayActivities(boolean excludeFinishing) {
        int count = 0;
        for (int i = mActivities.size() - 1; i >= 0; i--) {
            final ActivityRecord r = mActivities.get(i);
            if (excludeFinishing && r.finishing) {
                continue;
            }
            if (!r.mTaskOverlay) {
                return false;
            }
            count++;
        }
        return count > 0;
    }

    boolean autoRemoveFromRecents() {
        // We will automatically remove the task either if it has explicitly asked for
        // this, or it is empty and has never contained an activity that got shown to
        // the user.
        return autoRemoveRecents || (mActivities.isEmpty() && !hasBeenVisible);
    }

    /**
     * Completely remove all activities associated with an existing
     * task starting at a specified index.
     */
    final void performClearTaskAtIndexLocked(int activityNdx, boolean pauseImmediately,
            String reason) {
        int numActivities = mActivities.size();
        for ( ; activityNdx < numActivities; ++activityNdx) {
            final ActivityRecord r = mActivities.get(activityNdx);
            if (r.finishing) {
                continue;
            }
            if (mStack == null) {
                // Task was restored from persistent storage.
                r.takeFromHistory();
                mActivities.remove(activityNdx);
                --activityNdx;
                --numActivities;
            } else if (mStack.finishActivityLocked(r, Activity.RESULT_CANCELED, null,
                    reason, false, pauseImmediately)) {
                --activityNdx;
                --numActivities;
            }
        }
    }

    /**
     * Completely remove all activities associated with an existing task.
     */
    void performClearTaskLocked() {
        mReuseTask = true;
        performClearTaskAtIndexLocked(0, !PAUSE_IMMEDIATELY, "clear-task-all");
        mReuseTask = false;
    }

    ActivityRecord performClearTaskForReuseLocked(ActivityRecord newR, int launchFlags) {
        mReuseTask = true;
        final ActivityRecord result = performClearTaskLocked(newR, launchFlags);
        mReuseTask = false;
        return result;
    }

    /**
     * Perform clear operation as requested by
     * {@link Intent#FLAG_ACTIVITY_CLEAR_TOP}: search from the top of the
     * stack to the given task, then look for
     * an instance of that activity in the stack and, if found, finish all
     * activities on top of it and return the instance.
     *
     * @param newR Description of the new activity being started.
     * @return Returns the old activity that should be continued to be used,
     * or null if none was found.
     */
    final ActivityRecord performClearTaskLocked(ActivityRecord newR, int launchFlags) {
        int numActivities = mActivities.size();
        for (int activityNdx = numActivities - 1; activityNdx >= 0; --activityNdx) {
            ActivityRecord r = mActivities.get(activityNdx);
            if (r.finishing) {
                continue;
            }
            if (r.mActivityComponent.equals(newR.mActivityComponent)) {
                // Here it is!  Now finish everything in front...
                final ActivityRecord ret = r;

                for (++activityNdx; activityNdx < numActivities; ++activityNdx) {
                    r = mActivities.get(activityNdx);
                    if (r.finishing) {
                        continue;
                    }
                    ActivityOptions opts = r.takeOptionsLocked(false /* fromClient */);
                    if (opts != null) {
                        ret.updateOptionsLocked(opts);
                    }
                    if (mStack != null && mStack.finishActivityLocked(
                            r, Activity.RESULT_CANCELED, null, "clear-task-stack", false)) {
                        --activityNdx;
                        --numActivities;
                    }
                }

                // Finally, if this is a normal launch mode (that is, not
                // expecting onNewIntent()), then we will finish the current
                // instance of the activity so a new fresh one can be started.
                if (ret.launchMode == ActivityInfo.LAUNCH_MULTIPLE
                        && (launchFlags & Intent.FLAG_ACTIVITY_SINGLE_TOP) == 0
                        && !ActivityStarter.isDocumentLaunchesIntoExisting(launchFlags)) {
                    if (!ret.finishing) {
                        if (mStack != null) {
                            mStack.finishActivityLocked(
                                    ret, Activity.RESULT_CANCELED, null, "clear-task-top", false);
                        }
                        return null;
                    }
                }

                return ret;
            }
        }

        return null;
    }

    void removeTaskActivitiesLocked(boolean pauseImmediately, String reason) {
        // Just remove the entire task.
        performClearTaskAtIndexLocked(0, pauseImmediately, reason);
    }

    String lockTaskAuthToString() {
        switch (mLockTaskAuth) {
            case LOCK_TASK_AUTH_DONT_LOCK: return "LOCK_TASK_AUTH_DONT_LOCK";
            case LOCK_TASK_AUTH_PINNABLE: return "LOCK_TASK_AUTH_PINNABLE";
            case LOCK_TASK_AUTH_LAUNCHABLE: return "LOCK_TASK_AUTH_LAUNCHABLE";
            case LOCK_TASK_AUTH_WHITELISTED: return "LOCK_TASK_AUTH_WHITELISTED";
            case LOCK_TASK_AUTH_LAUNCHABLE_PRIV: return "LOCK_TASK_AUTH_LAUNCHABLE_PRIV";
            default: return "unknown=" + mLockTaskAuth;
        }
    }

    void setLockTaskAuth() {
        setLockTaskAuth(getRootActivity());
    }

    private void setLockTaskAuth(@Nullable ActivityRecord r) {
        if (r == null) {
            mLockTaskAuth = LOCK_TASK_AUTH_PINNABLE;
            return;
        }

        final String pkg = (realActivity != null) ? realActivity.getPackageName() : null;
        final LockTaskController lockTaskController = mService.getLockTaskController();
        switch (r.lockTaskLaunchMode) {
            case LOCK_TASK_LAUNCH_MODE_DEFAULT:
                mLockTaskAuth = lockTaskController.isPackageWhitelisted(userId, pkg)
                        ? LOCK_TASK_AUTH_WHITELISTED : LOCK_TASK_AUTH_PINNABLE;
                break;

            case LOCK_TASK_LAUNCH_MODE_NEVER:
                mLockTaskAuth = LOCK_TASK_AUTH_DONT_LOCK;
                break;

            case LOCK_TASK_LAUNCH_MODE_ALWAYS:
                mLockTaskAuth = LOCK_TASK_AUTH_LAUNCHABLE_PRIV;
                break;

            case LOCK_TASK_LAUNCH_MODE_IF_WHITELISTED:
                mLockTaskAuth = lockTaskController.isPackageWhitelisted(userId, pkg)
                        ? LOCK_TASK_AUTH_LAUNCHABLE : LOCK_TASK_AUTH_PINNABLE;
                break;
        }
        if (DEBUG_LOCKTASK) Slog.d(TAG_LOCKTASK, "setLockTaskAuth: task=" + this +
                " mLockTaskAuth=" + lockTaskAuthToString());
    }

    private boolean isResizeable(boolean checkSupportsPip) {
        return (mService.mForceResizableActivities || ActivityInfo.isResizeableMode(mResizeMode)
                || (checkSupportsPip && mSupportsPictureInPicture));
    }

    boolean isResizeable() {
        return isResizeable(true /* checkSupportsPip */);
    }

    @Override
    public boolean supportsSplitScreenWindowingMode() {
        // A task can not be docked even if it is considered resizeable because it only supports
        // picture-in-picture mode but has a non-resizeable resizeMode
        return super.supportsSplitScreenWindowingMode()
                && mService.mSupportsSplitScreenMultiWindow
                && (mService.mForceResizableActivities
                        || (isResizeable(false /* checkSupportsPip */)
                                && !ActivityInfo.isPreserveOrientationMode(mResizeMode)));
    }

    /**
     * Check whether this task can be launched on the specified display.
     *
     * @param displayId Target display id.
     * @return {@code true} if either it is the default display or this activity can be put on a
     *         secondary display.
     */
    boolean canBeLaunchedOnDisplay(int displayId) {
        return mService.mStackSupervisor.canPlaceEntityOnDisplay(displayId,
                -1 /* don't check PID */, -1 /* don't check UID */, null /* activityInfo */);
    }

    /**
     * Check that a given bounds matches the application requested orientation.
     *
     * @param bounds The bounds to be tested.
     * @return True if the requested bounds are okay for a resizing request.
     */
    private boolean canResizeToBounds(Rect bounds) {
        if (bounds == null || !inFreeformWindowingMode()) {
            // Note: If not on the freeform workspace, we ignore the bounds.
            return true;
        }
        final boolean landscape = bounds.width() > bounds.height();
        final Rect configBounds = getRequestedOverrideBounds();
        if (mResizeMode == RESIZE_MODE_FORCE_RESIZABLE_PRESERVE_ORIENTATION) {
            return configBounds.isEmpty()
                    || landscape == (configBounds.width() > configBounds.height());
        }
        return (mResizeMode != RESIZE_MODE_FORCE_RESIZABLE_PORTRAIT_ONLY || !landscape)
                && (mResizeMode != RESIZE_MODE_FORCE_RESIZABLE_LANDSCAPE_ONLY || landscape);
    }

    /**
     * @return {@code true} if the task is being cleared for the purposes of being reused.
     */
    boolean isClearingToReuseTask() {
        return mReuseTask;
    }

    /**
     * Find the activity in the history stack within the given task.  Returns
     * the index within the history at which it's found, or < 0 if not found.
     */
    final ActivityRecord findActivityInHistoryLocked(ActivityRecord r) {
        final ComponentName realActivity = r.mActivityComponent;
        for (int activityNdx = mActivities.size() - 1; activityNdx >= 0; --activityNdx) {
            ActivityRecord candidate = mActivities.get(activityNdx);
            if (candidate.finishing) {
                continue;
            }
            if (candidate.mActivityComponent.equals(realActivity)) {
                return candidate;
            }
        }
        return null;
    }

    /** Updates the last task description values. */
    void updateTaskDescription() {
        // Traverse upwards looking for any break between main task activities and
        // utility activities.
        int activityNdx;
        final int numActivities = mActivities.size();
        final boolean relinquish = numActivities != 0 &&
                (mActivities.get(0).info.flags & FLAG_RELINQUISH_TASK_IDENTITY) != 0;
        for (activityNdx = Math.min(numActivities, 1); activityNdx < numActivities;
                ++activityNdx) {
            final ActivityRecord r = mActivities.get(activityNdx);
            if (relinquish && (r.info.flags & FLAG_RELINQUISH_TASK_IDENTITY) == 0) {
                // This will be the top activity for determining taskDescription. Pre-inc to
                // overcome initial decrement below.
                ++activityNdx;
                break;
            }
            if (r.intent != null &&
                    (r.intent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET) != 0) {
                break;
            }
        }
        if (activityNdx > 0) {
            // Traverse downwards starting below break looking for set label, icon.
            // Note that if there are activities in the task but none of them set the
            // recent activity values, then we do not fall back to the last set
            // values in the TaskRecord.
            String label = null;
            String iconFilename = null;
            int iconResource = -1;
            int colorPrimary = 0;
            int colorBackground = 0;
            int statusBarColor = 0;
            int navigationBarColor = 0;
            boolean statusBarContrastWhenTransparent = false;
            boolean navigationBarContrastWhenTransparent = false;
            boolean topActivity = true;
            for (--activityNdx; activityNdx >= 0; --activityNdx) {
                final ActivityRecord r = mActivities.get(activityNdx);
                if (r.mTaskOverlay) {
                    continue;
                }
                if (r.taskDescription != null) {
                    if (label == null) {
                        label = r.taskDescription.getLabel();
                    }
                    if (iconResource == -1) {
                        iconResource = r.taskDescription.getIconResource();
                    }
                    if (iconFilename == null) {
                        iconFilename = r.taskDescription.getIconFilename();
                    }
                    if (colorPrimary == 0) {
                        colorPrimary = r.taskDescription.getPrimaryColor();
                    }
                    if (topActivity) {
                        colorBackground = r.taskDescription.getBackgroundColor();
                        statusBarColor = r.taskDescription.getStatusBarColor();
                        navigationBarColor = r.taskDescription.getNavigationBarColor();
                        statusBarContrastWhenTransparent =
                                r.taskDescription.getEnsureStatusBarContrastWhenTransparent();
                        navigationBarContrastWhenTransparent =
                                r.taskDescription.getEnsureNavigationBarContrastWhenTransparent();
                    }
                }
                topActivity = false;
            }
            lastTaskDescription = new TaskDescription(label, null, iconResource, iconFilename,
                    colorPrimary, colorBackground, statusBarColor, navigationBarColor,
                    statusBarContrastWhenTransparent, navigationBarContrastWhenTransparent);
            if (mTask != null) {
                mTask.setTaskDescription(lastTaskDescription);
            }
            // Update the task affiliation color if we are the parent of the group
            if (taskId == mAffiliatedTaskId) {
                mAffiliatedTaskColor = lastTaskDescription.getPrimaryColor();
            }
        }
    }

    int findEffectiveRootIndex() {
        int effectiveNdx = 0;
        final int topActivityNdx = mActivities.size() - 1;
        for (int activityNdx = 0; activityNdx <= topActivityNdx; ++activityNdx) {
            final ActivityRecord r = mActivities.get(activityNdx);
            if (r.finishing) {
                continue;
            }
            effectiveNdx = activityNdx;
            if ((r.info.flags & FLAG_RELINQUISH_TASK_IDENTITY) == 0) {
                break;
            }
        }
        return effectiveNdx;
    }

    void updateEffectiveIntent() {
        final int effectiveRootIndex = findEffectiveRootIndex();
        final ActivityRecord r = mActivities.get(effectiveRootIndex);
        setIntent(r);

        // Update the task description when the activities change
        updateTaskDescription();
    }

    void adjustForMinimalTaskDimensions(Rect bounds, Rect previousBounds) {
        if (bounds == null) {
            return;
        }
        int minWidth = mMinWidth;
        int minHeight = mMinHeight;
        // If the task has no requested minimal size, we'd like to enforce a minimal size
        // so that the user can not render the task too small to manipulate. We don't need
        // to do this for the pinned stack as the bounds are controlled by the system.
        if (!inPinnedWindowingMode() && mStack != null) {
            final int defaultMinSizeDp =
                    mService.mRootActivityContainer.mDefaultMinSizeOfResizeableTaskDp;
            final ActivityDisplay display =
                    mService.mRootActivityContainer.getActivityDisplay(mStack.mDisplayId);
            final float density =
                    (float) display.getConfiguration().densityDpi / DisplayMetrics.DENSITY_DEFAULT;
            final int defaultMinSize = (int) (defaultMinSizeDp * density);

            if (minWidth == INVALID_MIN_SIZE) {
                minWidth = defaultMinSize;
            }
            if (minHeight == INVALID_MIN_SIZE) {
                minHeight = defaultMinSize;
            }
        }
        final boolean adjustWidth = minWidth > bounds.width();
        final boolean adjustHeight = minHeight > bounds.height();
        if (!(adjustWidth || adjustHeight)) {
            return;
        }

        if (adjustWidth) {
            if (!previousBounds.isEmpty() && bounds.right == previousBounds.right) {
                bounds.left = bounds.right - minWidth;
            } else {
                // Either left bounds match, or neither match, or the previous bounds were
                // fullscreen and we default to keeping left.
                bounds.right = bounds.left + minWidth;
            }
        }
        if (adjustHeight) {
            if (!previousBounds.isEmpty() && bounds.bottom == previousBounds.bottom) {
                bounds.top = bounds.bottom - minHeight;
            } else {
                // Either top bounds match, or neither match, or the previous bounds were
                // fullscreen and we default to keeping top.
                bounds.bottom = bounds.top + minHeight;
            }
        }
    }

    /**
     * Update task's override configuration based on the bounds.
     * @param bounds The bounds of the task.
     * @return True if the override configuration was updated.
     */
    boolean updateOverrideConfiguration(Rect bounds) {
        return updateOverrideConfiguration(bounds, null /* insetBounds */);
    }

    void setLastNonFullscreenBounds(Rect bounds) {
        if (mLastNonFullscreenBounds == null) {
            mLastNonFullscreenBounds = new Rect(bounds);
        } else {
            mLastNonFullscreenBounds.set(bounds);
        }
    }

    /**
     * Update task's override configuration based on the bounds.
     * @param bounds The bounds of the task.
     * @param insetBounds The bounds used to calculate the system insets, which is used here to
     *                    subtract the navigation bar/status bar size from the screen size reported
     *                    to the application. See {@link IActivityTaskManager#resizeDockedStack}.
     * @return True if the override configuration was updated.
     */
    boolean updateOverrideConfiguration(Rect bounds, @Nullable Rect insetBounds) {
        final boolean hasSetDisplayedBounds = (insetBounds != null && !insetBounds.isEmpty());
        if (hasSetDisplayedBounds) {
            setDisplayedBounds(bounds);
        } else {
            setDisplayedBounds(null);
        }
        // "steady" bounds do not include any temporary offsets from animation or interaction.
        Rect steadyBounds = hasSetDisplayedBounds ? insetBounds : bounds;
        if (equivalentRequestedOverrideBounds(steadyBounds)) {
            return false;
        }

        mTmpConfig.setTo(getResolvedOverrideConfiguration());
        setBounds(steadyBounds);
        return !mTmpConfig.equals(getResolvedOverrideConfiguration());
    }

    /**
     * This should be called when an child activity changes state. This should only
     * be called from
     * {@link ActivityRecord#setState(ActivityState, String)} .
     * @param record The {@link ActivityRecord} whose state has changed.
     * @param state The new state.
     * @param reason The reason for the change.
     */
    void onActivityStateChanged(ActivityRecord record, ActivityState state, String reason) {
        final ActivityStack parent = getStack();

        if (parent != null) {
            parent.onActivityStateChanged(record, state, reason);
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newParentConfig) {
        // Check if the new configuration supports persistent bounds (eg. is Freeform) and if so
        // restore the last recorded non-fullscreen bounds.
        final boolean prevPersistTaskBounds = getWindowConfiguration().persistTaskBounds();
        final boolean nextPersistTaskBounds =
                getRequestedOverrideConfiguration().windowConfiguration.persistTaskBounds()
                || newParentConfig.windowConfiguration.persistTaskBounds();
        if (!prevPersistTaskBounds && nextPersistTaskBounds
                && mLastNonFullscreenBounds != null && !mLastNonFullscreenBounds.isEmpty()) {
            // Bypass onRequestedOverrideConfigurationChanged here to avoid infinite loop.
            getRequestedOverrideConfiguration().windowConfiguration
                    .setBounds(mLastNonFullscreenBounds);
        }

        final boolean wasInMultiWindowMode = inMultiWindowMode();
        super.onConfigurationChanged(newParentConfig);
        if (wasInMultiWindowMode != inMultiWindowMode()) {
            mService.mStackSupervisor.scheduleUpdateMultiWindowMode(this);
        }

        // If the configuration supports persistent bounds (eg. Freeform), keep track of the
        // current (non-fullscreen) bounds for persistence.
        if (getWindowConfiguration().persistTaskBounds()) {
            final Rect currentBounds = getRequestedOverrideBounds();
            if (!currentBounds.isEmpty()) {
                setLastNonFullscreenBounds(currentBounds);
            }
        }
        // TODO: Should also take care of Pip mode changes here.

        saveLaunchingStateIfNeeded();
    }

    /**
     * Saves launching state if necessary so that we can launch the activity to its latest state.
     * It only saves state if this task has been shown to user and it's in fullscreen or freeform
     * mode.
     */
    void saveLaunchingStateIfNeeded() {
        if (!hasBeenVisible) {
            // Not ever visible to user.
            return;
        }

        final int windowingMode = getWindowingMode();
        if (windowingMode != WindowConfiguration.WINDOWING_MODE_FULLSCREEN
                && windowingMode != WindowConfiguration.WINDOWING_MODE_FREEFORM) {
            return;
        }

        // Saves the new state so that we can launch the activity at the same location.
        mService.mStackSupervisor.mLaunchParamsPersister.saveTask(this);
    }

    /**
     * Adjust bounds to stay within stack bounds.
     *
     * Since bounds might be outside of stack bounds, this method tries to move the bounds in a way
     * that keep them unchanged, but be contained within the stack bounds.
     *
     * @param bounds Bounds to be adjusted.
     * @param stackBounds Bounds within which the other bounds should remain.
     * @param overlapPxX The amount of px required to be visible in the X dimension.
     * @param overlapPxY The amount of px required to be visible in the Y dimension.
     */
    private static void fitWithinBounds(Rect bounds, Rect stackBounds, int overlapPxX,
            int overlapPxY) {
        if (stackBounds == null || stackBounds.isEmpty() || stackBounds.contains(bounds)) {
            return;
        }

        // For each side of the parent (eg. left), check if the opposing side of the window (eg.
        // right) is at least overlap pixels away. If less, offset the window by that difference.
        int horizontalDiff = 0;
        // If window is smaller than overlap, use it's smallest dimension instead
        int overlapLR = Math.min(overlapPxX, bounds.width());
        if (bounds.right < (stackBounds.left + overlapLR)) {
            horizontalDiff = overlapLR - (bounds.right - stackBounds.left);
        } else if (bounds.left > (stackBounds.right - overlapLR)) {
            horizontalDiff = -(overlapLR - (stackBounds.right - bounds.left));
        }
        int verticalDiff = 0;
        int overlapTB = Math.min(overlapPxY, bounds.width());
        if (bounds.bottom < (stackBounds.top + overlapTB)) {
            verticalDiff = overlapTB - (bounds.bottom - stackBounds.top);
        } else if (bounds.top > (stackBounds.bottom - overlapTB)) {
            verticalDiff = -(overlapTB - (stackBounds.bottom - bounds.top));
        }
        bounds.offset(horizontalDiff, verticalDiff);
    }

    /**
     * Displayed bounds are used to set where the task is drawn at any given time. This is
     * separate from its actual bounds so that the app doesn't see any meaningful configuration
     * changes during transitionary periods.
     */
    void setDisplayedBounds(Rect bounds) {
        if (bounds == null) {
            mDisplayedBounds.setEmpty();
        } else {
            mDisplayedBounds.set(bounds);
        }
        if (mTask != null) {
            mTask.setOverrideDisplayedBounds(
                    mDisplayedBounds.isEmpty() ? null : mDisplayedBounds);
        }
    }

    /**
     * Gets the current overridden displayed bounds. These will be empty if the task is not
     * currently overriding where it is displayed.
     */
    Rect getDisplayedBounds() {
        return mDisplayedBounds;
    }

    /**
     * @return {@code true} if this has overridden displayed bounds.
     */
    boolean hasDisplayedBounds() {
        return !mDisplayedBounds.isEmpty();
    }

    /**
     * Intersects inOutBounds with intersectBounds-intersectInsets. If inOutBounds is larger than
     * intersectBounds on a side, then the respective side will not be intersected.
     *
     * The assumption is that if inOutBounds is initially larger than intersectBounds, then the
     * inset on that side is no-longer applicable. This scenario happens when a task's minimal
     * bounds are larger than the provided parent/display bounds.
     *
     * @param inOutBounds the bounds to intersect.
     * @param intersectBounds the bounds to intersect with.
     * @param intersectInsets insets to apply to intersectBounds before intersecting.
     */
    static void intersectWithInsetsIfFits(
            Rect inOutBounds, Rect intersectBounds, Rect intersectInsets) {
        if (inOutBounds.right <= intersectBounds.right) {
            inOutBounds.right =
                    Math.min(intersectBounds.right - intersectInsets.right, inOutBounds.right);
        }
        if (inOutBounds.bottom <= intersectBounds.bottom) {
            inOutBounds.bottom =
                    Math.min(intersectBounds.bottom - intersectInsets.bottom, inOutBounds.bottom);
        }
        if (inOutBounds.left >= intersectBounds.left) {
            inOutBounds.left =
                    Math.max(intersectBounds.left + intersectInsets.left, inOutBounds.left);
        }
        if (inOutBounds.top >= intersectBounds.top) {
            inOutBounds.top =
                    Math.max(intersectBounds.top + intersectInsets.top, inOutBounds.top);
        }
    }

    /**
     * Gets bounds with non-decor and stable insets applied respectively.
     *
     * If bounds overhangs the display, those edges will not get insets. See
     * {@link #intersectWithInsetsIfFits}
     *
     * @param outNonDecorBounds where to place bounds with non-decor insets applied.
     * @param outStableBounds where to place bounds with stable insets applied.
     * @param bounds the bounds to inset.
     */
    private void calculateInsetFrames(Rect outNonDecorBounds, Rect outStableBounds, Rect bounds,
            DisplayInfo displayInfo) {
        outNonDecorBounds.set(bounds);
        outStableBounds.set(bounds);
        if (getStack() == null || getStack().getDisplay() == null) {
            return;
        }
        DisplayPolicy policy = getStack().getDisplay().mDisplayContent.getDisplayPolicy();
        if (policy == null) {
            return;
        }
        mTmpBounds.set(0, 0, displayInfo.logicalWidth, displayInfo.logicalHeight);

        policy.getNonDecorInsetsLw(displayInfo.rotation, displayInfo.logicalWidth,
                displayInfo.logicalHeight, displayInfo.displayCutout, mTmpInsets);
        intersectWithInsetsIfFits(outNonDecorBounds, mTmpBounds, mTmpInsets);

        policy.convertNonDecorInsetsToStableInsets(mTmpInsets, displayInfo.rotation);
        intersectWithInsetsIfFits(outStableBounds, mTmpBounds, mTmpInsets);
    }

    /**
     * Asks docked-divider controller for the smallestwidthdp given bounds.
     * @param bounds bounds to calculate smallestwidthdp for.
     */
    private int getSmallestScreenWidthDpForDockedBounds(Rect bounds) {
        DisplayContent dc = mStack.getDisplay().mDisplayContent;
        if (dc != null) {
            return dc.getDockedDividerController().getSmallestWidthDpForBounds(bounds);
        }
        return Configuration.SMALLEST_SCREEN_WIDTH_DP_UNDEFINED;
    }

    void computeConfigResourceOverrides(@NonNull Configuration inOutConfig,
            @NonNull Configuration parentConfig) {
        computeConfigResourceOverrides(inOutConfig, parentConfig, null /* compatInsets */);
    }

    /**
     * Calculates configuration values used by the client to get resources. This should be run
     * using app-facing bounds (bounds unmodified by animations or transient interactions).
     *
     * This assumes bounds are non-empty/null. For the null-bounds case, the caller is likely
     * configuring an "inherit-bounds" window which means that all configuration settings would
     * just be inherited from the parent configuration.
     **/
    void computeConfigResourceOverrides(@NonNull Configuration inOutConfig,
            @NonNull Configuration parentConfig,
            @Nullable ActivityRecord.CompatDisplayInsets compatInsets) {
        int windowingMode = inOutConfig.windowConfiguration.getWindowingMode();
        if (windowingMode == WINDOWING_MODE_UNDEFINED) {
            windowingMode = parentConfig.windowConfiguration.getWindowingMode();
        }

        float density = inOutConfig.densityDpi;
        if (density == Configuration.DENSITY_DPI_UNDEFINED) {
            density = parentConfig.densityDpi;
        }
        density *= DisplayMetrics.DENSITY_DEFAULT_SCALE;

        final Rect bounds = inOutConfig.windowConfiguration.getBounds();
        Rect outAppBounds = inOutConfig.windowConfiguration.getAppBounds();
        if (outAppBounds == null || outAppBounds.isEmpty()) {
            inOutConfig.windowConfiguration.setAppBounds(bounds);
            outAppBounds = inOutConfig.windowConfiguration.getAppBounds();
        }
        // Non-null compatibility insets means the activity prefers to keep its original size, so
        // the out bounds doesn't need to be restricted by the parent.
        final boolean insideParentBounds = compatInsets == null;
        if (insideParentBounds && windowingMode != WINDOWING_MODE_FREEFORM) {
            final Rect parentAppBounds = parentConfig.windowConfiguration.getAppBounds();
            if (parentAppBounds != null && !parentAppBounds.isEmpty()) {
                outAppBounds.intersect(parentAppBounds);
            }
        }

        if (inOutConfig.screenWidthDp == Configuration.SCREEN_WIDTH_DP_UNDEFINED
                || inOutConfig.screenHeightDp == Configuration.SCREEN_HEIGHT_DP_UNDEFINED) {
            if (insideParentBounds && mStack != null) {
                final DisplayInfo di = new DisplayInfo();
                mStack.getDisplay().mDisplay.getDisplayInfo(di);

                // For calculating screenWidthDp, screenWidthDp, we use the stable inset screen
                // area, i.e. the screen area without the system bars.
                // The non decor inset are areas that could never be removed in Honeycomb. See
                // {@link WindowManagerPolicy#getNonDecorInsetsLw}.
                calculateInsetFrames(mTmpNonDecorBounds, mTmpStableBounds, bounds, di);
            } else {
                // Apply the given non-decor and stable insets to calculate the corresponding bounds
                // for screen size of configuration.
                final int rotation = parentConfig.windowConfiguration.getRotation();
                if (rotation != ROTATION_UNDEFINED && compatInsets != null) {
                    mTmpNonDecorBounds.set(bounds);
                    mTmpStableBounds.set(bounds);
                    compatInsets.getDisplayBoundsByRotation(mTmpBounds, rotation);
                    intersectWithInsetsIfFits(mTmpNonDecorBounds, mTmpBounds,
                            compatInsets.mNonDecorInsets[rotation]);
                    intersectWithInsetsIfFits(mTmpStableBounds, mTmpBounds,
                            compatInsets.mStableInsets[rotation]);
                    outAppBounds.set(mTmpNonDecorBounds);
                } else {
                    // Set to app bounds because it excludes decor insets.
                    mTmpNonDecorBounds.set(outAppBounds);
                    mTmpStableBounds.set(outAppBounds);
                }
            }

            if (inOutConfig.screenWidthDp == Configuration.SCREEN_WIDTH_DP_UNDEFINED) {
                final int overrideScreenWidthDp = (int) (mTmpStableBounds.width() / density);
                inOutConfig.screenWidthDp = insideParentBounds
                        ? Math.min(overrideScreenWidthDp, parentConfig.screenWidthDp)
                        : overrideScreenWidthDp;
            }
            if (inOutConfig.screenHeightDp == Configuration.SCREEN_HEIGHT_DP_UNDEFINED) {
                final int overrideScreenHeightDp = (int) (mTmpStableBounds.height() / density);
                inOutConfig.screenHeightDp = insideParentBounds
                        ? Math.min(overrideScreenHeightDp, parentConfig.screenHeightDp)
                        : overrideScreenHeightDp;
            }

            if (inOutConfig.smallestScreenWidthDp
                    == Configuration.SMALLEST_SCREEN_WIDTH_DP_UNDEFINED) {
                if (WindowConfiguration.isFloating(windowingMode)) {
                    // For floating tasks, calculate the smallest width from the bounds of the task
                    inOutConfig.smallestScreenWidthDp = (int) (
                            Math.min(bounds.width(), bounds.height()) / density);
                } else if (WindowConfiguration.isSplitScreenWindowingMode(windowingMode)) {
                    // Iterating across all screen orientations, and return the minimum of the task
                    // width taking into account that the bounds might change because the snap
                    // algorithm snaps to a different value
                    inOutConfig.smallestScreenWidthDp =
                            getSmallestScreenWidthDpForDockedBounds(bounds);
                }
                // otherwise, it will just inherit
            }
        }

        if (inOutConfig.orientation == ORIENTATION_UNDEFINED) {
            inOutConfig.orientation = (inOutConfig.screenWidthDp <= inOutConfig.screenHeightDp)
                    ? ORIENTATION_PORTRAIT : ORIENTATION_LANDSCAPE;
        }
        if (inOutConfig.screenLayout == Configuration.SCREENLAYOUT_UNDEFINED) {
            // For calculating screen layout, we need to use the non-decor inset screen area for the
            // calculation for compatibility reasons, i.e. screen area without system bars that
            // could never go away in Honeycomb.
            final int compatScreenWidthDp = (int) (mTmpNonDecorBounds.width() / density);
            final int compatScreenHeightDp = (int) (mTmpNonDecorBounds.height() / density);
            // We're only overriding LONG, SIZE and COMPAT parts of screenLayout, so we start
            // override calculation with partial default.
            // Reducing the screen layout starting from its parent config.
            final int sl = parentConfig.screenLayout
                    & (Configuration.SCREENLAYOUT_LONG_MASK | Configuration.SCREENLAYOUT_SIZE_MASK);
            final int longSize = Math.max(compatScreenHeightDp, compatScreenWidthDp);
            final int shortSize = Math.min(compatScreenHeightDp, compatScreenWidthDp);
            inOutConfig.screenLayout = Configuration.reduceScreenLayout(sl, longSize, shortSize);
        }
    }

    @Override
    void resolveOverrideConfiguration(Configuration newParentConfig) {
        mTmpBounds.set(getResolvedOverrideConfiguration().windowConfiguration.getBounds());
        super.resolveOverrideConfiguration(newParentConfig);
        int windowingMode =
                getRequestedOverrideConfiguration().windowConfiguration.getWindowingMode();
        if (windowingMode == WINDOWING_MODE_UNDEFINED) {
            windowingMode = newParentConfig.windowConfiguration.getWindowingMode();
        }
        Rect outOverrideBounds =
                getResolvedOverrideConfiguration().windowConfiguration.getBounds();

        if (windowingMode == WINDOWING_MODE_FULLSCREEN) {
            computeFullscreenBounds(outOverrideBounds, null /* refActivity */,
                    newParentConfig.windowConfiguration.getBounds(),
                    newParentConfig.orientation);
        }

        if (outOverrideBounds.isEmpty()) {
            // If the task fills the parent, just inherit all the other configs from parent.
            return;
        }

        adjustForMinimalTaskDimensions(outOverrideBounds, mTmpBounds);
        if (windowingMode == WINDOWING_MODE_FREEFORM) {
            // by policy, make sure the window remains within parent somewhere
            final float density =
                    ((float) newParentConfig.densityDpi) / DisplayMetrics.DENSITY_DEFAULT;
            final Rect parentBounds =
                    new Rect(newParentConfig.windowConfiguration.getBounds());
            final ActivityDisplay display = mStack.getDisplay();
            if (display != null && display.mDisplayContent != null) {
                // If a freeform window moves below system bar, there is no way to move it again
                // by touch. Because its caption is covered by system bar. So we exclude them
                // from stack bounds. and then caption will be shown inside stable area.
                final Rect stableBounds = new Rect();
                display.mDisplayContent.getStableRect(stableBounds);
                parentBounds.intersect(stableBounds);
            }

            fitWithinBounds(outOverrideBounds, parentBounds,
                    (int) (density * WindowState.MINIMUM_VISIBLE_WIDTH_IN_DP),
                    (int) (density * WindowState.MINIMUM_VISIBLE_HEIGHT_IN_DP));

            // Prevent to overlap caption with stable insets.
            final int offsetTop = parentBounds.top - outOverrideBounds.top;
            if (offsetTop > 0) {
                outOverrideBounds.offset(0, offsetTop);
            }
        }
        computeConfigResourceOverrides(getResolvedOverrideConfiguration(), newParentConfig);
    }

    /** @see WindowContainer#handlesOrientationChangeFromDescendant */
    boolean handlesOrientationChangeFromDescendant() {
        return mTask != null && mTask.getParent() != null
                && mTask.getParent().handlesOrientationChangeFromDescendant();
    }

    /**
     * Compute bounds (letterbox or pillarbox) for {@link #WINDOWING_MODE_FULLSCREEN} when the
     * parent doesn't handle the orientation change and the requested orientation is different from
     * the parent.
     */
    void computeFullscreenBounds(@NonNull Rect outBounds, @Nullable ActivityRecord refActivity,
            @NonNull Rect parentBounds, int parentOrientation) {
        // In FULLSCREEN mode, always start with empty bounds to indicate "fill parent".
        outBounds.setEmpty();
        if (handlesOrientationChangeFromDescendant()) {
            return;
        }
        if (refActivity == null) {
            // Use the top activity as the reference of orientation. Don't include overlays because
            // it is usually not the actual content or just temporarily shown.
            // E.g. ForcedResizableInfoActivity.
            refActivity = getTopActivity(false /* includeOverlays */);
        }

        // If the task or the reference activity requires a different orientation (either by
        // override or activityInfo), make it fit the available bounds by scaling down its bounds.
        final int overrideOrientation = getRequestedOverrideConfiguration().orientation;
        final int forcedOrientation =
                (overrideOrientation != ORIENTATION_UNDEFINED || refActivity == null)
                        ? overrideOrientation : refActivity.getRequestedConfigurationOrientation();
        if (forcedOrientation == ORIENTATION_UNDEFINED || forcedOrientation == parentOrientation) {
            return;
        }

        final int parentWidth = parentBounds.width();
        final int parentHeight = parentBounds.height();
        final float aspect = ((float) parentHeight) / parentWidth;
        if (forcedOrientation == ORIENTATION_LANDSCAPE) {
            final int height = (int) (parentWidth / aspect);
            final int top = parentBounds.centerY() - height / 2;
            outBounds.set(parentBounds.left, top, parentBounds.right, top + height);
        } else {
            final int width = (int) (parentHeight * aspect);
            final int left = parentBounds.centerX() - width / 2;
            outBounds.set(left, parentBounds.top, left + width, parentBounds.bottom);
        }
    }

    Rect updateOverrideConfigurationFromLaunchBounds() {
        final Rect bounds = getLaunchBounds();
        updateOverrideConfiguration(bounds);
        if (bounds != null && !bounds.isEmpty()) {
            // TODO: Review if we actually want to do this - we are setting the launch bounds
            // directly here.
            bounds.set(getRequestedOverrideBounds());
        }
        return bounds;
    }

    /** Updates the task's bounds and override configuration to match what is expected for the
     * input stack. */
    void updateOverrideConfigurationForStack(ActivityStack inStack) {
        if (mStack != null && mStack == inStack) {
            return;
        }

        if (inStack.inFreeformWindowingMode()) {
            if (!isResizeable()) {
                throw new IllegalArgumentException("Can not position non-resizeable task="
                        + this + " in stack=" + inStack);
            }
            if (!matchParentBounds()) {
                return;
            }
            if (mLastNonFullscreenBounds != null) {
                updateOverrideConfiguration(mLastNonFullscreenBounds);
            } else {
                mService.mStackSupervisor.getLaunchParamsController().layoutTask(this, null);
            }
        } else {
            updateOverrideConfiguration(inStack.getRequestedOverrideBounds());
        }
    }

    /** Returns the bounds that should be used to launch this task. */
    Rect getLaunchBounds() {
        if (mStack == null) {
            return null;
        }

        final int windowingMode = getWindowingMode();
        if (!isActivityTypeStandardOrUndefined()
                || windowingMode == WINDOWING_MODE_FULLSCREEN
                || (windowingMode == WINDOWING_MODE_SPLIT_SCREEN_PRIMARY && !isResizeable())) {
            return isResizeable() ? mStack.getRequestedOverrideBounds() : null;
        } else if (!getWindowConfiguration().persistTaskBounds()) {
            return mStack.getRequestedOverrideBounds();
        }
        return mLastNonFullscreenBounds;
    }

    void addStartingWindowsForVisibleActivities(boolean taskSwitch) {
        for (int activityNdx = mActivities.size() - 1; activityNdx >= 0; --activityNdx) {
            final ActivityRecord r = mActivities.get(activityNdx);
            if (r.visible) {
                r.showStartingWindow(null /* prev */, false /* newTask */, taskSwitch);
            }
        }
    }

    void setRootProcess(WindowProcessController proc) {
        clearRootProcess();
        if (intent != null &&
                (intent.getFlags() & Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS) == 0) {
            mRootProcess = proc;
            mRootProcess.addRecentTask(this);
        }
    }

    void clearRootProcess() {
        if (mRootProcess != null) {
            mRootProcess.removeRecentTask(this);
            mRootProcess = null;
        }
    }

    void clearAllPendingOptions() {
        for (int i = getChildCount() - 1; i >= 0; i--) {
            getChildAt(i).clearOptionsLocked(false /* withAbort */);
        }
    }

    /**
     * Fills in a {@link TaskInfo} with information from this task.
     * @param info the {@link TaskInfo} to fill in
     */
    void fillTaskInfo(TaskInfo info) {
        getNumRunningActivities(mReuseActivitiesReport);
        info.userId = userId;
        info.stackId = getStackId();
        info.taskId = taskId;
        info.displayId = mStack == null ? Display.INVALID_DISPLAY : mStack.mDisplayId;
        info.isRunning = getTopActivity() != null;
        info.baseIntent = new Intent(getBaseIntent());
        info.baseActivity = mReuseActivitiesReport.base != null
                ? mReuseActivitiesReport.base.intent.getComponent()
                : null;
        info.topActivity = mReuseActivitiesReport.top != null
                ? mReuseActivitiesReport.top.mActivityComponent
                : null;
        info.origActivity = origActivity;
        info.realActivity = realActivity;
        info.numActivities = mReuseActivitiesReport.numActivities;
        info.lastActiveTime = lastActiveTime;
        info.taskDescription = new ActivityManager.TaskDescription(lastTaskDescription);
        info.supportsSplitScreenMultiWindow = supportsSplitScreenWindowingMode();
        info.resizeMode = mResizeMode;
        info.configuration.setTo(getConfiguration());
    }

    /**
     * Returns a  {@link TaskInfo} with information from this task.
     */
    ActivityManager.RunningTaskInfo getTaskInfo() {
        ActivityManager.RunningTaskInfo info = new ActivityManager.RunningTaskInfo();
        fillTaskInfo(info);
        return info;
    }

    void dump(PrintWriter pw, String prefix) {
        pw.print(prefix); pw.print("userId="); pw.print(userId);
                pw.print(" effectiveUid="); UserHandle.formatUid(pw, effectiveUid);
                pw.print(" mCallingUid="); UserHandle.formatUid(pw, mCallingUid);
                pw.print(" mUserSetupComplete="); pw.print(mUserSetupComplete);
                pw.print(" mCallingPackage="); pw.println(mCallingPackage);
        if (affinity != null || rootAffinity != null) {
            pw.print(prefix); pw.print("affinity="); pw.print(affinity);
            if (affinity == null || !affinity.equals(rootAffinity)) {
                pw.print(" root="); pw.println(rootAffinity);
            } else {
                pw.println();
            }
        }
        if (voiceSession != null || voiceInteractor != null) {
            pw.print(prefix); pw.print("VOICE: session=0x");
            pw.print(Integer.toHexString(System.identityHashCode(voiceSession)));
            pw.print(" interactor=0x");
            pw.println(Integer.toHexString(System.identityHashCode(voiceInteractor)));
        }
        if (intent != null) {
            StringBuilder sb = new StringBuilder(128);
            sb.append(prefix); sb.append("intent={");
            intent.toShortString(sb, false, true, false, true);
            sb.append('}');
            pw.println(sb.toString());
        }
        if (affinityIntent != null) {
            StringBuilder sb = new StringBuilder(128);
            sb.append(prefix); sb.append("affinityIntent={");
            affinityIntent.toShortString(sb, false, true, false, true);
            sb.append('}');
            pw.println(sb.toString());
        }
        if (origActivity != null) {
            pw.print(prefix); pw.print("origActivity=");
            pw.println(origActivity.flattenToShortString());
        }
        if (realActivity != null) {
            pw.print(prefix); pw.print("mActivityComponent=");
            pw.println(realActivity.flattenToShortString());
        }
        if (autoRemoveRecents || isPersistable || !isActivityTypeStandard() || numFullscreen != 0) {
            pw.print(prefix); pw.print("autoRemoveRecents="); pw.print(autoRemoveRecents);
                    pw.print(" isPersistable="); pw.print(isPersistable);
                    pw.print(" numFullscreen="); pw.print(numFullscreen);
                    pw.print(" activityType="); pw.println(getActivityType());
        }
        if (rootWasReset || mNeverRelinquishIdentity || mReuseTask
                || mLockTaskAuth != LOCK_TASK_AUTH_PINNABLE) {
            pw.print(prefix); pw.print("rootWasReset="); pw.print(rootWasReset);
                    pw.print(" mNeverRelinquishIdentity="); pw.print(mNeverRelinquishIdentity);
                    pw.print(" mReuseTask="); pw.print(mReuseTask);
                    pw.print(" mLockTaskAuth="); pw.println(lockTaskAuthToString());
        }
        if (mAffiliatedTaskId != taskId || mPrevAffiliateTaskId != INVALID_TASK_ID
                || mPrevAffiliate != null || mNextAffiliateTaskId != INVALID_TASK_ID
                || mNextAffiliate != null) {
            pw.print(prefix); pw.print("affiliation="); pw.print(mAffiliatedTaskId);
                    pw.print(" prevAffiliation="); pw.print(mPrevAffiliateTaskId);
                    pw.print(" (");
                    if (mPrevAffiliate == null) {
                        pw.print("null");
                    } else {
                        pw.print(Integer.toHexString(System.identityHashCode(mPrevAffiliate)));
                    }
                    pw.print(") nextAffiliation="); pw.print(mNextAffiliateTaskId);
                    pw.print(" (");
                    if (mNextAffiliate == null) {
                        pw.print("null");
                    } else {
                        pw.print(Integer.toHexString(System.identityHashCode(mNextAffiliate)));
                    }
                    pw.println(")");
        }
        pw.print(prefix); pw.print("Activities="); pw.println(mActivities);
        if (!askedCompatMode || !inRecents || !isAvailable) {
            pw.print(prefix); pw.print("askedCompatMode="); pw.print(askedCompatMode);
                    pw.print(" inRecents="); pw.print(inRecents);
                    pw.print(" isAvailable="); pw.println(isAvailable);
        }
        if (lastDescription != null) {
            pw.print(prefix); pw.print("lastDescription="); pw.println(lastDescription);
        }
        if (mRootProcess != null) {
            pw.print(prefix); pw.print("mRootProcess="); pw.println(mRootProcess);
        }
        pw.print(prefix); pw.print("stackId="); pw.println(getStackId());
        pw.print(prefix + "hasBeenVisible=" + hasBeenVisible);
                pw.print(" mResizeMode=" + ActivityInfo.resizeModeToString(mResizeMode));
                pw.print(" mSupportsPictureInPicture=" + mSupportsPictureInPicture);
                pw.print(" isResizeable=" + isResizeable());
                pw.print(" lastActiveTime=" + lastActiveTime);
                pw.println(" (inactive for " + (getInactiveDuration() / 1000) + "s)");
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder(128);
        if (stringName != null) {
            sb.append(stringName);
            sb.append(" U=");
            sb.append(userId);
            sb.append(" StackId=");
            sb.append(getStackId());
            sb.append(" sz=");
            sb.append(mActivities.size());
            sb.append('}');
            return sb.toString();
        }
        sb.append("TaskRecord{");
        sb.append(Integer.toHexString(System.identityHashCode(this)));
        sb.append(" #");
        sb.append(taskId);
        if (affinity != null) {
            sb.append(" A=");
            sb.append(affinity);
        } else if (intent != null) {
            sb.append(" I=");
            sb.append(intent.getComponent().flattenToShortString());
        } else if (affinityIntent != null && affinityIntent.getComponent() != null) {
            sb.append(" aI=");
            sb.append(affinityIntent.getComponent().flattenToShortString());
        } else {
            sb.append(" ??");
        }
        stringName = sb.toString();
        return toString();
    }

    public void writeToProto(ProtoOutputStream proto, long fieldId,
            @WindowTraceLogLevel int logLevel) {
        if (logLevel == WindowTraceLogLevel.CRITICAL && !isVisible()) {
            return;
        }

        final long token = proto.start(fieldId);
        super.writeToProto(proto, CONFIGURATION_CONTAINER, logLevel);
        proto.write(ID, taskId);
        for (int i = mActivities.size() - 1; i >= 0; i--) {
            ActivityRecord activity = mActivities.get(i);
            activity.writeToProto(proto, ACTIVITIES);
        }
        proto.write(STACK_ID, mStack.mStackId);
        if (mLastNonFullscreenBounds != null) {
            mLastNonFullscreenBounds.writeToProto(proto, LAST_NON_FULLSCREEN_BOUNDS);
        }
        if (realActivity != null) {
            proto.write(REAL_ACTIVITY, realActivity.flattenToShortString());
        }
        if (origActivity != null) {
            proto.write(ORIG_ACTIVITY, origActivity.flattenToShortString());
        }
        proto.write(ACTIVITY_TYPE, getActivityType());
        proto.write(RESIZE_MODE, mResizeMode);
        // TODO: Remove, no longer needed with windowingMode.
        proto.write(FULLSCREEN, matchParentBounds());

        if (!matchParentBounds()) {
            final Rect bounds = getRequestedOverrideBounds();
            bounds.writeToProto(proto, BOUNDS);
        }
        proto.write(MIN_WIDTH, mMinWidth);
        proto.write(MIN_HEIGHT, mMinHeight);
        proto.end(token);
    }

    /**
     * See {@link #getNumRunningActivities(TaskActivitiesReport)}.
     */
    static class TaskActivitiesReport {
        int numRunning;
        int numActivities;
        ActivityRecord top;
        ActivityRecord base;

        void reset() {
            numRunning = numActivities = 0;
            top = base = null;
        }
    }

    /**
     * Saves this {@link TaskRecord} to XML using given serializer.
     */
    void saveToXml(XmlSerializer out) throws IOException, XmlPullParserException {
        if (DEBUG_RECENTS) Slog.i(TAG_RECENTS, "Saving task=" + this);

        out.attribute(null, ATTR_TASKID, String.valueOf(taskId));
        if (realActivity != null) {
            out.attribute(null, ATTR_REALACTIVITY, realActivity.flattenToShortString());
        }
        out.attribute(null, ATTR_REALACTIVITY_SUSPENDED, String.valueOf(realActivitySuspended));
        if (origActivity != null) {
            out.attribute(null, ATTR_ORIGACTIVITY, origActivity.flattenToShortString());
        }
        // Write affinity, and root affinity if it is different from affinity.
        // We use the special string "@" for a null root affinity, so we can identify
        // later whether we were given a root affinity or should just make it the
        // same as the affinity.
        if (affinity != null) {
            out.attribute(null, ATTR_AFFINITY, affinity);
            if (!affinity.equals(rootAffinity)) {
                out.attribute(null, ATTR_ROOT_AFFINITY, rootAffinity != null ? rootAffinity : "@");
            }
        } else if (rootAffinity != null) {
            out.attribute(null, ATTR_ROOT_AFFINITY, rootAffinity != null ? rootAffinity : "@");
        }
        out.attribute(null, ATTR_ROOTHASRESET, String.valueOf(rootWasReset));
        out.attribute(null, ATTR_AUTOREMOVERECENTS, String.valueOf(autoRemoveRecents));
        out.attribute(null, ATTR_ASKEDCOMPATMODE, String.valueOf(askedCompatMode));
        out.attribute(null, ATTR_USERID, String.valueOf(userId));
        out.attribute(null, ATTR_USER_SETUP_COMPLETE, String.valueOf(mUserSetupComplete));
        out.attribute(null, ATTR_EFFECTIVE_UID, String.valueOf(effectiveUid));
        out.attribute(null, ATTR_LASTTIMEMOVED, String.valueOf(mLastTimeMoved));
        out.attribute(null, ATTR_NEVERRELINQUISH, String.valueOf(mNeverRelinquishIdentity));
        if (lastDescription != null) {
            out.attribute(null, ATTR_LASTDESCRIPTION, lastDescription.toString());
        }
        if (lastTaskDescription != null) {
            lastTaskDescription.saveToXml(out);
        }
        out.attribute(null, ATTR_TASK_AFFILIATION_COLOR, String.valueOf(mAffiliatedTaskColor));
        out.attribute(null, ATTR_TASK_AFFILIATION, String.valueOf(mAffiliatedTaskId));
        out.attribute(null, ATTR_PREV_AFFILIATION, String.valueOf(mPrevAffiliateTaskId));
        out.attribute(null, ATTR_NEXT_AFFILIATION, String.valueOf(mNextAffiliateTaskId));
        out.attribute(null, ATTR_CALLING_UID, String.valueOf(mCallingUid));
        out.attribute(null, ATTR_CALLING_PACKAGE, mCallingPackage == null ? "" : mCallingPackage);
        out.attribute(null, ATTR_RESIZE_MODE, String.valueOf(mResizeMode));
        out.attribute(null, ATTR_SUPPORTS_PICTURE_IN_PICTURE,
                String.valueOf(mSupportsPictureInPicture));
        if (mLastNonFullscreenBounds != null) {
            out.attribute(
                    null, ATTR_NON_FULLSCREEN_BOUNDS, mLastNonFullscreenBounds.flattenToString());
        }
        out.attribute(null, ATTR_MIN_WIDTH, String.valueOf(mMinWidth));
        out.attribute(null, ATTR_MIN_HEIGHT, String.valueOf(mMinHeight));
        out.attribute(null, ATTR_PERSIST_TASK_VERSION, String.valueOf(PERSIST_TASK_VERSION));

        if (affinityIntent != null) {
            out.startTag(null, TAG_AFFINITYINTENT);
            affinityIntent.saveToXml(out);
            out.endTag(null, TAG_AFFINITYINTENT);
        }

        if (intent != null) {
            out.startTag(null, TAG_INTENT);
            intent.saveToXml(out);
            out.endTag(null, TAG_INTENT);
        }

        final ArrayList<ActivityRecord> activities = mActivities;
        final int numActivities = activities.size();
        for (int activityNdx = 0; activityNdx < numActivities; ++activityNdx) {
            final ActivityRecord r = activities.get(activityNdx);
            if (r.info.persistableMode == ActivityInfo.PERSIST_ROOT_ONLY || !r.isPersistable() ||
                    ((r.intent.getFlags() & FLAG_ACTIVITY_NEW_DOCUMENT
                            | FLAG_ACTIVITY_RETAIN_IN_RECENTS) == FLAG_ACTIVITY_NEW_DOCUMENT) &&
                            activityNdx > 0) {
                // Stop at first non-persistable or first break in task (CLEAR_WHEN_TASK_RESET).
                break;
            }
            out.startTag(null, TAG_ACTIVITY);
            r.saveToXml(out);
            out.endTag(null, TAG_ACTIVITY);
        }
    }

    @VisibleForTesting
    static TaskRecordFactory getTaskRecordFactory() {
        if (sTaskRecordFactory == null) {
            setTaskRecordFactory(new TaskRecordFactory());
        }
        return sTaskRecordFactory;
    }

    static void setTaskRecordFactory(TaskRecordFactory factory) {
        sTaskRecordFactory = factory;
    }

    static TaskRecord create(ActivityTaskManagerService service, int taskId, ActivityInfo info,
            Intent intent, IVoiceInteractionSession voiceSession,
            IVoiceInteractor voiceInteractor) {
        return getTaskRecordFactory().create(
                service, taskId, info, intent, voiceSession, voiceInteractor);
    }

    static TaskRecord create(ActivityTaskManagerService service, int taskId, ActivityInfo info,
            Intent intent, TaskDescription taskDescription) {
        return getTaskRecordFactory().create(service, taskId, info, intent, taskDescription);
    }

    static TaskRecord restoreFromXml(XmlPullParser in, ActivityStackSupervisor stackSupervisor)
            throws IOException, XmlPullParserException {
        return getTaskRecordFactory().restoreFromXml(in, stackSupervisor);
    }

    /**
     * A factory class used to create {@link TaskRecord} or its subclass if any. This can be
     * specified when system boots by setting it with
     * {@link #setTaskRecordFactory(TaskRecordFactory)}.
     */
    static class TaskRecordFactory {

        TaskRecord create(ActivityTaskManagerService service, int taskId, ActivityInfo info,
                Intent intent, IVoiceInteractionSession voiceSession,
                IVoiceInteractor voiceInteractor) {
            return new TaskRecord(
                    service, taskId, info, intent, voiceSession, voiceInteractor);
        }

        TaskRecord create(ActivityTaskManagerService service, int taskId, ActivityInfo info,
                Intent intent, TaskDescription taskDescription) {
            return new TaskRecord(service, taskId, info, intent, taskDescription);
        }

        /**
         * Should only be used when we're restoring {@link TaskRecord} from storage.
         */
        TaskRecord create(ActivityTaskManagerService service, int taskId, Intent intent,
                Intent affinityIntent, String affinity, String rootAffinity,
                ComponentName realActivity, ComponentName origActivity, boolean rootWasReset,
                boolean autoRemoveRecents, boolean askedCompatMode, int userId,
                int effectiveUid, String lastDescription, ArrayList<ActivityRecord> activities,
                long lastTimeMoved, boolean neverRelinquishIdentity,
                TaskDescription lastTaskDescription, int taskAffiliation, int prevTaskId,
                int nextTaskId, int taskAffiliationColor, int callingUid, String callingPackage,
                int resizeMode, boolean supportsPictureInPicture, boolean realActivitySuspended,
                boolean userSetupComplete, int minWidth, int minHeight) {
            return new TaskRecord(service, taskId, intent, affinityIntent, affinity,
                    rootAffinity, realActivity, origActivity, rootWasReset, autoRemoveRecents,
                    askedCompatMode, userId, effectiveUid, lastDescription, activities,
                    lastTimeMoved, neverRelinquishIdentity, lastTaskDescription, taskAffiliation,
                    prevTaskId, nextTaskId, taskAffiliationColor, callingUid, callingPackage,
                    resizeMode, supportsPictureInPicture, realActivitySuspended, userSetupComplete,
                    minWidth, minHeight);
        }

        TaskRecord restoreFromXml(XmlPullParser in, ActivityStackSupervisor stackSupervisor)
                throws IOException, XmlPullParserException {
            Intent intent = null;
            Intent affinityIntent = null;
            ArrayList<ActivityRecord> activities = new ArrayList<>();
            ComponentName realActivity = null;
            boolean realActivitySuspended = false;
            ComponentName origActivity = null;
            String affinity = null;
            String rootAffinity = null;
            boolean hasRootAffinity = false;
            boolean rootHasReset = false;
            boolean autoRemoveRecents = false;
            boolean askedCompatMode = false;
            int taskType = 0;
            int userId = 0;
            boolean userSetupComplete = true;
            int effectiveUid = -1;
            String lastDescription = null;
            long lastTimeOnTop = 0;
            boolean neverRelinquishIdentity = true;
            int taskId = INVALID_TASK_ID;
            final int outerDepth = in.getDepth();
            TaskDescription taskDescription = new TaskDescription();
            int taskAffiliation = INVALID_TASK_ID;
            int taskAffiliationColor = 0;
            int prevTaskId = INVALID_TASK_ID;
            int nextTaskId = INVALID_TASK_ID;
            int callingUid = -1;
            String callingPackage = "";
            int resizeMode = RESIZE_MODE_FORCE_RESIZEABLE;
            boolean supportsPictureInPicture = false;
            Rect lastNonFullscreenBounds = null;
            int minWidth = INVALID_MIN_SIZE;
            int minHeight = INVALID_MIN_SIZE;
            int persistTaskVersion = 0;

            for (int attrNdx = in.getAttributeCount() - 1; attrNdx >= 0; --attrNdx) {
                final String attrName = in.getAttributeName(attrNdx);
                final String attrValue = in.getAttributeValue(attrNdx);
                if (TaskPersister.DEBUG) Slog.d(TaskPersister.TAG, "TaskRecord: attribute name=" +
                        attrName + " value=" + attrValue);
                switch (attrName) {
                    case ATTR_TASKID:
                        if (taskId == INVALID_TASK_ID) taskId = Integer.parseInt(attrValue);
                        break;
                    case ATTR_REALACTIVITY:
                        realActivity = ComponentName.unflattenFromString(attrValue);
                        break;
                    case ATTR_REALACTIVITY_SUSPENDED:
                        realActivitySuspended = Boolean.valueOf(attrValue);
                        break;
                    case ATTR_ORIGACTIVITY:
                        origActivity = ComponentName.unflattenFromString(attrValue);
                        break;
                    case ATTR_AFFINITY:
                        affinity = attrValue;
                        break;
                    case ATTR_ROOT_AFFINITY:
                        rootAffinity = attrValue;
                        hasRootAffinity = true;
                        break;
                    case ATTR_ROOTHASRESET:
                        rootHasReset = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_AUTOREMOVERECENTS:
                        autoRemoveRecents = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_ASKEDCOMPATMODE:
                        askedCompatMode = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_USERID:
                        userId = Integer.parseInt(attrValue);
                        break;
                    case ATTR_USER_SETUP_COMPLETE:
                        userSetupComplete = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_EFFECTIVE_UID:
                        effectiveUid = Integer.parseInt(attrValue);
                        break;
                    case ATTR_TASKTYPE:
                        taskType = Integer.parseInt(attrValue);
                        break;
                    case ATTR_LASTDESCRIPTION:
                        lastDescription = attrValue;
                        break;
                    case ATTR_LASTTIMEMOVED:
                        lastTimeOnTop = Long.parseLong(attrValue);
                        break;
                    case ATTR_NEVERRELINQUISH:
                        neverRelinquishIdentity = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_TASK_AFFILIATION:
                        taskAffiliation = Integer.parseInt(attrValue);
                        break;
                    case ATTR_PREV_AFFILIATION:
                        prevTaskId = Integer.parseInt(attrValue);
                        break;
                    case ATTR_NEXT_AFFILIATION:
                        nextTaskId = Integer.parseInt(attrValue);
                        break;
                    case ATTR_TASK_AFFILIATION_COLOR:
                        taskAffiliationColor = Integer.parseInt(attrValue);
                        break;
                    case ATTR_CALLING_UID:
                        callingUid = Integer.parseInt(attrValue);
                        break;
                    case ATTR_CALLING_PACKAGE:
                        callingPackage = attrValue;
                        break;
                    case ATTR_RESIZE_MODE:
                        resizeMode = Integer.parseInt(attrValue);
                        break;
                    case ATTR_SUPPORTS_PICTURE_IN_PICTURE:
                        supportsPictureInPicture = Boolean.parseBoolean(attrValue);
                        break;
                    case ATTR_NON_FULLSCREEN_BOUNDS:
                        lastNonFullscreenBounds = Rect.unflattenFromString(attrValue);
                        break;
                    case ATTR_MIN_WIDTH:
                        minWidth = Integer.parseInt(attrValue);
                        break;
                    case ATTR_MIN_HEIGHT:
                        minHeight = Integer.parseInt(attrValue);
                        break;
                    case ATTR_PERSIST_TASK_VERSION:
                        persistTaskVersion = Integer.parseInt(attrValue);
                        break;
                    default:
                        if (attrName.startsWith(TaskDescription.ATTR_TASKDESCRIPTION_PREFIX)) {
                            taskDescription.restoreFromXml(attrName, attrValue);
                        } else {
                            Slog.w(TAG, "TaskRecord: Unknown attribute=" + attrName);
                        }
                }
            }

            int event;
            while (((event = in.next()) != XmlPullParser.END_DOCUMENT) &&
                    (event != XmlPullParser.END_TAG || in.getDepth() >= outerDepth)) {
                if (event == XmlPullParser.START_TAG) {
                    final String name = in.getName();
                    if (TaskPersister.DEBUG) Slog.d(TaskPersister.TAG,
                            "TaskRecord: START_TAG name=" + name);
                    if (TAG_AFFINITYINTENT.equals(name)) {
                        affinityIntent = Intent.restoreFromXml(in);
                    } else if (TAG_INTENT.equals(name)) {
                        intent = Intent.restoreFromXml(in);
                    } else if (TAG_ACTIVITY.equals(name)) {
                        ActivityRecord activity =
                                ActivityRecord.restoreFromXml(in, stackSupervisor);
                        if (TaskPersister.DEBUG) Slog.d(TaskPersister.TAG, "TaskRecord: activity=" +
                                activity);
                        if (activity != null) {
                            activities.add(activity);
                        }
                    } else {
                        handleUnknownTag(name, in);
                    }
                }
            }
            if (!hasRootAffinity) {
                rootAffinity = affinity;
            } else if ("@".equals(rootAffinity)) {
                rootAffinity = null;
            }
            if (effectiveUid <= 0) {
                Intent checkIntent = intent != null ? intent : affinityIntent;
                effectiveUid = 0;
                if (checkIntent != null) {
                    IPackageManager pm = AppGlobals.getPackageManager();
                    try {
                        ApplicationInfo ai = pm.getApplicationInfo(
                                checkIntent.getComponent().getPackageName(),
                                PackageManager.MATCH_UNINSTALLED_PACKAGES
                                        | PackageManager.MATCH_DISABLED_COMPONENTS, userId);
                        if (ai != null) {
                            effectiveUid = ai.uid;
                        }
                    } catch (RemoteException e) {
                    }
                }
                Slog.w(TAG, "Updating task #" + taskId + " for " + checkIntent
                        + ": effectiveUid=" + effectiveUid);
            }

            if (persistTaskVersion < 1) {
                // We need to convert the resize mode of home activities saved before version one if
                // they are marked as RESIZE_MODE_RESIZEABLE to
                // RESIZE_MODE_RESIZEABLE_VIA_SDK_VERSION since we didn't have that differentiation
                // before version 1 and the system didn't resize home activities before then.
                if (taskType == 1 /* old home type */ && resizeMode == RESIZE_MODE_RESIZEABLE) {
                    resizeMode = RESIZE_MODE_RESIZEABLE_VIA_SDK_VERSION;
                }
            } else {
                // This activity has previously marked itself explicitly as both resizeable and
                // supporting picture-in-picture.  Since there is no longer a requirement for
                // picture-in-picture activities to be resizeable, we can mark this simply as
                // resizeable and supporting picture-in-picture separately.
                if (resizeMode == RESIZE_MODE_RESIZEABLE_AND_PIPABLE_DEPRECATED) {
                    resizeMode = RESIZE_MODE_RESIZEABLE;
                    supportsPictureInPicture = true;
                }
            }

            final TaskRecord task = create(stackSupervisor.mService,
                    taskId, intent, affinityIntent,
                    affinity, rootAffinity, realActivity, origActivity, rootHasReset,
                    autoRemoveRecents, askedCompatMode, userId, effectiveUid, lastDescription,
                    activities, lastTimeOnTop, neverRelinquishIdentity, taskDescription,
                    taskAffiliation, prevTaskId, nextTaskId, taskAffiliationColor, callingUid,
                    callingPackage, resizeMode, supportsPictureInPicture, realActivitySuspended,
                    userSetupComplete, minWidth, minHeight);
            task.mLastNonFullscreenBounds = lastNonFullscreenBounds;
            task.setBounds(lastNonFullscreenBounds);

            for (int activityNdx = activities.size() - 1; activityNdx >=0; --activityNdx) {
                activities.get(activityNdx).setTask(task);
            }

            if (DEBUG_RECENTS) Slog.d(TAG_RECENTS, "Restored task=" + task);
            return task;
        }

        void handleUnknownTag(String name, XmlPullParser in)
                throws IOException, XmlPullParserException {
            Slog.e(TAG, "restoreTask: Unexpected name=" + name);
            XmlUtils.skipCurrentTag(in);
        }
    }
}
