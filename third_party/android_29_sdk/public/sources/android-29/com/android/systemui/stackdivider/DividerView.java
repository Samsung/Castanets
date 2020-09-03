/*
 * Copyright (C) 2015 The Android Open Source Project
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

package com.android.systemui.stackdivider;

import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
import static android.app.WindowConfiguration.WINDOWING_MODE_UNDEFINED;
import static android.view.PointerIcon.TYPE_HORIZONTAL_DOUBLE_ARROW;
import static android.view.PointerIcon.TYPE_VERTICAL_DOUBLE_ARROW;
import static android.view.WindowManager.DOCKED_LEFT;
import static android.view.WindowManager.DOCKED_RIGHT;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.annotation.Nullable;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;
import android.graphics.Region.Op;
import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.view.Choreographer;
import android.view.Display;
import android.view.DisplayInfo;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.VelocityTracker;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewConfiguration;
import android.view.ViewTreeObserver.InternalInsetsInfo;
import android.view.ViewTreeObserver.OnComputeInternalInsetsListener;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;
import android.view.animation.Interpolator;
import android.view.animation.PathInterpolator;
import android.widget.FrameLayout;

import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto.MetricsEvent;
import com.android.internal.policy.DividerSnapAlgorithm;
import com.android.internal.policy.DividerSnapAlgorithm.SnapTarget;
import com.android.internal.policy.DockedDividerUtils;
import com.android.internal.view.SurfaceFlingerVsyncChoreographer;
import com.android.systemui.Interpolators;
import com.android.systemui.R;
import com.android.systemui.shared.system.WindowManagerWrapper;
import com.android.systemui.statusbar.FlingAnimationUtils;

/**
 * Docked stack divider.
 */
public class DividerView extends FrameLayout implements OnTouchListener,
        OnComputeInternalInsetsListener {

    public interface DividerCallbacks {
        void onDraggingStart();
        void onDraggingEnd();
        void growRecents();
    }

    static final long TOUCH_ANIMATION_DURATION = 150;
    static final long TOUCH_RELEASE_ANIMATION_DURATION = 200;

    public static final int INVALID_RECENTS_GROW_TARGET = -1;

    private static final int LOG_VALUE_RESIZE_50_50 = 0;
    private static final int LOG_VALUE_RESIZE_DOCKED_SMALLER = 1;
    private static final int LOG_VALUE_RESIZE_DOCKED_LARGER = 2;

    private static final int LOG_VALUE_UNDOCK_MAX_DOCKED = 0;
    private static final int LOG_VALUE_UNDOCK_MAX_OTHER = 1;

    private static final int TASK_POSITION_SAME = Integer.MAX_VALUE;

    /**
     * How much the background gets scaled when we are in the minimized dock state.
     */
    private static final float MINIMIZE_DOCK_SCALE = 0f;
    private static final float ADJUSTED_FOR_IME_SCALE = 0.5f;

    private static final PathInterpolator SLOWDOWN_INTERPOLATOR =
            new PathInterpolator(0.5f, 1f, 0.5f, 1f);
    private static final PathInterpolator DIM_INTERPOLATOR =
            new PathInterpolator(.23f, .87f, .52f, -0.11f);
    private static final Interpolator IME_ADJUST_INTERPOLATOR =
            new PathInterpolator(0.2f, 0f, 0.1f, 1f);

    private static final int MSG_RESIZE_STACK = 0;

    private DividerHandleView mHandle;
    private View mBackground;
    private MinimizedDockShadow mMinimizedShadow;
    private int mStartX;
    private int mStartY;
    private int mStartPosition;
    private int mDockSide;
    private final int[] mTempInt2 = new int[2];
    private boolean mMoving;
    private int mTouchSlop;
    private boolean mBackgroundLifted;
    private boolean mIsInMinimizeInteraction;
    private SnapTarget mSnapTargetBeforeMinimized;

    private int mDividerInsets;
    private final Display mDefaultDisplay;
    private int mDisplayWidth;
    private int mDisplayHeight;
    private int mDisplayRotation;
    private int mDividerWindowWidth;
    private int mDividerSize;
    private int mTouchElevation;
    private int mLongPressEntraceAnimDuration;

    private final Rect mDockedRect = new Rect();
    private final Rect mDockedTaskRect = new Rect();
    private final Rect mOtherTaskRect = new Rect();
    private final Rect mOtherRect = new Rect();
    private final Rect mDockedInsetRect = new Rect();
    private final Rect mOtherInsetRect = new Rect();
    private final Rect mLastResizeRect = new Rect();
    private final Rect mTmpRect = new Rect();
    private final WindowManagerProxy mWindowManagerProxy = WindowManagerProxy.getInstance();
    private DividerWindowManager mWindowManager;
    private VelocityTracker mVelocityTracker;
    private FlingAnimationUtils mFlingAnimationUtils;
    private DividerSnapAlgorithm mSnapAlgorithm;
    private DividerSnapAlgorithm mMinimizedSnapAlgorithm;
    private DividerCallbacks mCallback;
    private final Rect mStableInsets = new Rect();

    private boolean mGrowRecents;
    private ValueAnimator mCurrentAnimator;
    private boolean mEntranceAnimationRunning;
    private boolean mExitAnimationRunning;
    private int mExitStartPosition;
    private boolean mDockedStackMinimized;
    private boolean mHomeStackResizable;
    private boolean mAdjustedForIme;
    private DividerState mState;
    private final SurfaceFlingerVsyncChoreographer mSfChoreographer;


    // The view is removed or in the process of been removed from the system.
    private boolean mRemoved;

    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch (msg.what) {
                case MSG_RESIZE_STACK:
                    resizeStack(msg.arg1, msg.arg2, (SnapTarget) msg.obj);
                    break;
                default:
                    super.handleMessage(msg);
            }
        }
    };

    private final AccessibilityDelegate mHandleDelegate = new AccessibilityDelegate() {
        @Override
        public void onInitializeAccessibilityNodeInfo(View host, AccessibilityNodeInfo info) {
            super.onInitializeAccessibilityNodeInfo(host, info);
            final DividerSnapAlgorithm snapAlgorithm = getSnapAlgorithm();
            if (isHorizontalDivision()) {
                info.addAction(new AccessibilityAction(R.id.action_move_tl_full,
                        mContext.getString(R.string.accessibility_action_divider_top_full)));
                if (snapAlgorithm.isFirstSplitTargetAvailable()) {
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_70,
                            mContext.getString(R.string.accessibility_action_divider_top_70)));
                }
                if (snapAlgorithm.showMiddleSplitTargetForAccessibility()) {
                    // Only show the middle target if there are more than 1 split target
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_50,
                        mContext.getString(R.string.accessibility_action_divider_top_50)));
                }
                if (snapAlgorithm.isLastSplitTargetAvailable()) {
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_30,
                            mContext.getString(R.string.accessibility_action_divider_top_30)));
                }
                info.addAction(new AccessibilityAction(R.id.action_move_rb_full,
                        mContext.getString(R.string.accessibility_action_divider_bottom_full)));
            } else {
                info.addAction(new AccessibilityAction(R.id.action_move_tl_full,
                        mContext.getString(R.string.accessibility_action_divider_left_full)));
                if (snapAlgorithm.isFirstSplitTargetAvailable()) {
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_70,
                            mContext.getString(R.string.accessibility_action_divider_left_70)));
                }
                if (snapAlgorithm.showMiddleSplitTargetForAccessibility()) {
                    // Only show the middle target if there are more than 1 split target
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_50,
                        mContext.getString(R.string.accessibility_action_divider_left_50)));
                }
                if (snapAlgorithm.isLastSplitTargetAvailable()) {
                    info.addAction(new AccessibilityAction(R.id.action_move_tl_30,
                            mContext.getString(R.string.accessibility_action_divider_left_30)));
                }
                info.addAction(new AccessibilityAction(R.id.action_move_rb_full,
                        mContext.getString(R.string.accessibility_action_divider_right_full)));
            }
        }

        @Override
        public boolean performAccessibilityAction(View host, int action, Bundle args) {
            int currentPosition = getCurrentPosition();
            SnapTarget nextTarget = null;
            if (action == R.id.action_move_tl_full) {
                nextTarget = mSnapAlgorithm.getDismissEndTarget();
            } else if (action == R.id.action_move_tl_70) {
                nextTarget = mSnapAlgorithm.getLastSplitTarget();
            } else if (action == R.id.action_move_tl_50) {
                nextTarget = mSnapAlgorithm.getMiddleTarget();
            } else if (action == R.id.action_move_tl_30) {
                nextTarget = mSnapAlgorithm.getFirstSplitTarget();
            } else if (action == R.id.action_move_rb_full) {
                nextTarget = mSnapAlgorithm.getDismissStartTarget();
            }
            if (nextTarget != null) {
                startDragging(true /* animate */, false /* touching */);
                stopDragging(currentPosition, nextTarget, 250, Interpolators.FAST_OUT_SLOW_IN);
                return true;
            }
            return super.performAccessibilityAction(host, action, args);
        }
    };

    private final Runnable mResetBackgroundRunnable = new Runnable() {
        @Override
        public void run() {
            resetBackground();
        }
    };

    public DividerView(Context context) {
        this(context, null);
    }

    public DividerView(Context context, @Nullable AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public DividerView(Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        this(context, attrs, defStyleAttr, 0);
    }

    public DividerView(Context context, @Nullable AttributeSet attrs, int defStyleAttr,
            int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        mSfChoreographer = new SurfaceFlingerVsyncChoreographer(mHandler, context.getDisplay(),
                Choreographer.getInstance());
        final DisplayManager displayManager =
                (DisplayManager) mContext.getSystemService(Context.DISPLAY_SERVICE);
        mDefaultDisplay = displayManager.getDisplay(Display.DEFAULT_DISPLAY);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mHandle = findViewById(R.id.docked_divider_handle);
        mBackground = findViewById(R.id.docked_divider_background);
        mMinimizedShadow = findViewById(R.id.minimized_dock_shadow);
        mHandle.setOnTouchListener(this);
        mDividerWindowWidth = getResources().getDimensionPixelSize(
                com.android.internal.R.dimen.docked_stack_divider_thickness);
        mDividerInsets = getResources().getDimensionPixelSize(
                com.android.internal.R.dimen.docked_stack_divider_insets);
        mDividerSize = mDividerWindowWidth - 2 * mDividerInsets;
        mTouchElevation = getResources().getDimensionPixelSize(
                R.dimen.docked_stack_divider_lift_elevation);
        mLongPressEntraceAnimDuration = getResources().getInteger(
                R.integer.long_press_dock_anim_duration);
        mGrowRecents = getResources().getBoolean(R.bool.recents_grow_in_multiwindow);
        mTouchSlop = ViewConfiguration.get(mContext).getScaledTouchSlop();
        mFlingAnimationUtils = new FlingAnimationUtils(getContext(), 0.3f);
        updateDisplayInfo();
        boolean landscape = getResources().getConfiguration().orientation
                == Configuration.ORIENTATION_LANDSCAPE;
        mHandle.setPointerIcon(PointerIcon.getSystemIcon(getContext(),
                landscape ? TYPE_HORIZONTAL_DOUBLE_ARROW : TYPE_VERTICAL_DOUBLE_ARROW));
        getViewTreeObserver().addOnComputeInternalInsetsListener(this);
        mHandle.setAccessibilityDelegate(mHandleDelegate);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        // Save the current target if not minimized once attached to window
        if (mHomeStackResizable && mDockSide != WindowManager.DOCKED_INVALID
                && !mIsInMinimizeInteraction) {
            saveSnapTargetBeforeMinimized(mSnapTargetBeforeMinimized);
        }
    }

    void onDividerRemoved() {
        mRemoved = true;
        mCallback = null;
        mHandler.removeMessages(MSG_RESIZE_STACK);
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        if (mStableInsets.left != insets.getStableInsetLeft()
                || mStableInsets.top != insets.getStableInsetTop()
                || mStableInsets.right != insets.getStableInsetRight()
                || mStableInsets.bottom != insets.getStableInsetBottom()) {
            mStableInsets.set(insets.getStableInsetLeft(), insets.getStableInsetTop(),
                    insets.getStableInsetRight(), insets.getStableInsetBottom());
            if (mSnapAlgorithm != null || mMinimizedSnapAlgorithm != null) {
                mSnapAlgorithm = null;
                mMinimizedSnapAlgorithm = null;
                initializeSnapAlgorithm();
            }
        }
        return super.onApplyWindowInsets(insets);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        int minimizeLeft = 0;
        int minimizeTop = 0;
        if (mDockSide == WindowManager.DOCKED_TOP) {
            minimizeTop = mBackground.getTop();
        } else if (mDockSide == WindowManager.DOCKED_LEFT) {
            minimizeLeft = mBackground.getLeft();
        } else if (mDockSide == WindowManager.DOCKED_RIGHT) {
            minimizeLeft = mBackground.getRight() - mMinimizedShadow.getWidth();
        }
        mMinimizedShadow.layout(minimizeLeft, minimizeTop,
                minimizeLeft + mMinimizedShadow.getMeasuredWidth(),
                minimizeTop + mMinimizedShadow.getMeasuredHeight());
        if (changed) {
            mWindowManagerProxy.setTouchRegion(new Rect(mHandle.getLeft(), mHandle.getTop(),
                    mHandle.getRight(), mHandle.getBottom()));
        }
    }

    public void injectDependencies(DividerWindowManager windowManager, DividerState dividerState,
            DividerCallbacks callback) {
        mWindowManager = windowManager;
        mState = dividerState;
        mCallback = callback;

        // Set the previous position ratio before minimized state after attaching this divider
        if (mStableInsets.isEmpty()) {
            WindowManagerWrapper.getInstance().getStableInsets(mStableInsets);
        }

        if (mState.mRatioPositionBeforeMinimized == 0) {
            // Set the middle target as the initial state
            mSnapTargetBeforeMinimized = mSnapAlgorithm.getMiddleTarget();
        } else {
            repositionSnapTargetBeforeMinimized();
        }
    }

    public WindowManagerProxy getWindowManagerProxy() {
        return mWindowManagerProxy;
    }

    public Rect getNonMinimizedSplitScreenSecondaryBounds() {
        calculateBoundsForPosition(mSnapTargetBeforeMinimized.position,
                DockedDividerUtils.invertDockSide(mDockSide), mOtherTaskRect);
        mOtherTaskRect.bottom -= mStableInsets.bottom;
        switch (mDockSide) {
            case WindowManager.DOCKED_LEFT:
                mOtherTaskRect.top += mStableInsets.top;
                mOtherTaskRect.right -= mStableInsets.right;
                break;
            case WindowManager.DOCKED_RIGHT:
                mOtherTaskRect.top += mStableInsets.top;
                mOtherTaskRect.left += mStableInsets.left;
                break;
        }
        return mOtherTaskRect;
    }

    public boolean startDragging(boolean animate, boolean touching) {
        cancelFlingAnimation();
        if (touching) {
            mHandle.setTouching(true, animate);
        }
        mDockSide = mWindowManagerProxy.getDockSide();

        // Update snap algorithm if rotation has occurred
        if (mDisplayRotation != mDefaultDisplay.getRotation()) {
            updateDisplayInfo();
        }
        initializeSnapAlgorithm();
        mWindowManagerProxy.setResizing(true);
        if (touching) {
            mWindowManager.setSlippery(false);
            liftBackground();
        }
        if (mCallback != null) {
            mCallback.onDraggingStart();
        }
        return mDockSide != WindowManager.DOCKED_INVALID;
    }

    public void stopDragging(int position, float velocity, boolean avoidDismissStart,
            boolean logMetrics) {
        mHandle.setTouching(false, true /* animate */);
        fling(position, velocity, avoidDismissStart, logMetrics);
        mWindowManager.setSlippery(true);
        releaseBackground();
    }

    public void stopDragging(int position, SnapTarget target, long duration,
            Interpolator interpolator) {
        stopDragging(position, target, duration, 0 /* startDelay*/, 0 /* endDelay */, interpolator);
    }

    public void stopDragging(int position, SnapTarget target, long duration,
            Interpolator interpolator, long endDelay) {
        stopDragging(position, target, duration, 0 /* startDelay*/, endDelay, interpolator);
    }

    public void stopDragging(int position, SnapTarget target, long duration, long startDelay,
            long endDelay, Interpolator interpolator) {
        mHandle.setTouching(false, true /* animate */);
        flingTo(position, target, duration, startDelay, endDelay, interpolator);
        mWindowManager.setSlippery(true);
        releaseBackground();
    }

    private void stopDragging() {
        mHandle.setTouching(false, true /* animate */);
        mWindowManager.setSlippery(true);
        releaseBackground();
    }

    private void updateDockSide() {
        mDockSide = mWindowManagerProxy.getDockSide();
        mMinimizedShadow.setDockSide(mDockSide);
    }

    private void initializeSnapAlgorithm() {
        if (mSnapAlgorithm == null) {
            mSnapAlgorithm = new DividerSnapAlgorithm(getContext().getResources(), mDisplayWidth,
                    mDisplayHeight, mDividerSize, isHorizontalDivision(), mStableInsets, mDockSide);
            if (mSnapTargetBeforeMinimized != null && mSnapTargetBeforeMinimized.isMiddleTarget) {
                mSnapTargetBeforeMinimized = mSnapAlgorithm.getMiddleTarget();
            }
        }
        if (mMinimizedSnapAlgorithm == null) {
            mMinimizedSnapAlgorithm = new DividerSnapAlgorithm(getContext().getResources(),
                    mDisplayWidth, mDisplayHeight, mDividerSize, isHorizontalDivision(),
                    mStableInsets, mDockSide, mDockedStackMinimized && mHomeStackResizable);
        }
    }

    public DividerSnapAlgorithm getSnapAlgorithm() {
        initializeSnapAlgorithm();
        return mDockedStackMinimized && mHomeStackResizable ? mMinimizedSnapAlgorithm :
                mSnapAlgorithm;
    }

    public int getCurrentPosition() {
        getLocationOnScreen(mTempInt2);
        if (isHorizontalDivision()) {
            return mTempInt2[1] + mDividerInsets;
        } else {
            return mTempInt2[0] + mDividerInsets;
        }
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        convertToScreenCoordinates(event);
        final int action = event.getAction() & MotionEvent.ACTION_MASK;
        switch (action) {
            case MotionEvent.ACTION_DOWN:
                mVelocityTracker = VelocityTracker.obtain();
                mVelocityTracker.addMovement(event);
                mStartX = (int) event.getX();
                mStartY = (int) event.getY();
                boolean result = startDragging(true /* animate */, true /* touching */);
                if (!result) {

                    // Weren't able to start dragging successfully, so cancel it again.
                    stopDragging();
                }
                mStartPosition = getCurrentPosition();
                mMoving = false;
                return result;
            case MotionEvent.ACTION_MOVE:
                mVelocityTracker.addMovement(event);
                int x = (int) event.getX();
                int y = (int) event.getY();
                boolean exceededTouchSlop =
                        isHorizontalDivision() && Math.abs(y - mStartY) > mTouchSlop
                                || (!isHorizontalDivision() && Math.abs(x - mStartX) > mTouchSlop);
                if (!mMoving && exceededTouchSlop) {
                    mStartX = x;
                    mStartY = y;
                    mMoving = true;
                }
                if (mMoving && mDockSide != WindowManager.DOCKED_INVALID) {
                    SnapTarget snapTarget = getSnapAlgorithm().calculateSnapTarget(
                            mStartPosition, 0 /* velocity */, false /* hardDismiss */);
                    resizeStackDelayed(calculatePosition(x, y), mStartPosition, snapTarget);
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mVelocityTracker.addMovement(event);

                x = (int) event.getRawX();
                y = (int) event.getRawY();

                mVelocityTracker.computeCurrentVelocity(1000);
                int position = calculatePosition(x, y);
                stopDragging(position, isHorizontalDivision() ? mVelocityTracker.getYVelocity()
                        : mVelocityTracker.getXVelocity(), false /* avoidDismissStart */,
                        true /* log */);
                mMoving = false;
                break;
        }
        return true;
    }

    private void logResizeEvent(SnapTarget snapTarget) {
        if (snapTarget == mSnapAlgorithm.getDismissStartTarget()) {
            MetricsLogger.action(
                    mContext, MetricsEvent.ACTION_WINDOW_UNDOCK_MAX, dockSideTopLeft(mDockSide)
                            ? LOG_VALUE_UNDOCK_MAX_OTHER
                            : LOG_VALUE_UNDOCK_MAX_DOCKED);
        } else if (snapTarget == mSnapAlgorithm.getDismissEndTarget()) {
            MetricsLogger.action(
                    mContext, MetricsEvent.ACTION_WINDOW_UNDOCK_MAX, dockSideBottomRight(mDockSide)
                            ? LOG_VALUE_UNDOCK_MAX_OTHER
                            : LOG_VALUE_UNDOCK_MAX_DOCKED);
        } else if (snapTarget == mSnapAlgorithm.getMiddleTarget()) {
            MetricsLogger.action(mContext, MetricsEvent.ACTION_WINDOW_DOCK_RESIZE,
                    LOG_VALUE_RESIZE_50_50);
        } else if (snapTarget == mSnapAlgorithm.getFirstSplitTarget()) {
            MetricsLogger.action(mContext, MetricsEvent.ACTION_WINDOW_DOCK_RESIZE,
                    dockSideTopLeft(mDockSide)
                            ? LOG_VALUE_RESIZE_DOCKED_SMALLER
                            : LOG_VALUE_RESIZE_DOCKED_LARGER);
        } else if (snapTarget == mSnapAlgorithm.getLastSplitTarget()) {
            MetricsLogger.action(mContext, MetricsEvent.ACTION_WINDOW_DOCK_RESIZE,
                    dockSideTopLeft(mDockSide)
                            ? LOG_VALUE_RESIZE_DOCKED_LARGER
                            : LOG_VALUE_RESIZE_DOCKED_SMALLER);
        }
    }

    private void convertToScreenCoordinates(MotionEvent event) {
        event.setLocation(event.getRawX(), event.getRawY());
    }

    private void fling(int position, float velocity, boolean avoidDismissStart,
            boolean logMetrics) {
        DividerSnapAlgorithm currentSnapAlgorithm = getSnapAlgorithm();
        SnapTarget snapTarget = currentSnapAlgorithm.calculateSnapTarget(position, velocity);
        if (avoidDismissStart && snapTarget == currentSnapAlgorithm.getDismissStartTarget()) {
            snapTarget = currentSnapAlgorithm.getFirstSplitTarget();
        }
        if (logMetrics) {
            logResizeEvent(snapTarget);
        }
        ValueAnimator anim = getFlingAnimator(position, snapTarget, 0 /* endDelay */);
        mFlingAnimationUtils.apply(anim, position, snapTarget.position, velocity);
        anim.start();
    }

    private void flingTo(int position, SnapTarget target, long duration, long startDelay,
            long endDelay, Interpolator interpolator) {
        ValueAnimator anim = getFlingAnimator(position, target, endDelay);
        anim.setDuration(duration);
        anim.setStartDelay(startDelay);
        anim.setInterpolator(interpolator);
        anim.start();
    }

    private ValueAnimator getFlingAnimator(int position, final SnapTarget snapTarget,
            final long endDelay) {
        if (mCurrentAnimator != null) {
            cancelFlingAnimation();
            updateDockSide();
        }
        final boolean taskPositionSameAtEnd = snapTarget.flag == SnapTarget.FLAG_NONE;
        ValueAnimator anim = ValueAnimator.ofInt(position, snapTarget.position);
        anim.addUpdateListener(animation -> resizeStackDelayed((int) animation.getAnimatedValue(),
                taskPositionSameAtEnd && animation.getAnimatedFraction() == 1f
                        ? TASK_POSITION_SAME
                        : snapTarget.taskPosition,
                snapTarget));
        Runnable endAction = () -> {
            commitSnapFlags(snapTarget);
            mWindowManagerProxy.setResizing(false);
            updateDockSide();
            mCurrentAnimator = null;
            mEntranceAnimationRunning = false;
            mExitAnimationRunning = false;
            if (mCallback != null) {
                mCallback.onDraggingEnd();
            }

            // Record last snap target the divider moved to
            if (mHomeStackResizable && !mIsInMinimizeInteraction) {
                // The last snapTarget position can be negative when the last divider position was
                // offscreen. In that case, save the middle (default) SnapTarget so calculating next
                // position isn't negative.
                final SnapTarget saveTarget;
                if (snapTarget.position < 0) {
                    saveTarget = mSnapAlgorithm.getMiddleTarget();
                } else {
                    saveTarget = snapTarget;
                }
                if (saveTarget.position != mSnapAlgorithm.getDismissEndTarget().position
                        && saveTarget.position != mSnapAlgorithm.getDismissStartTarget().position) {
                    saveSnapTargetBeforeMinimized(saveTarget);
                }
            }
        };
        Runnable notCancelledEndAction = () -> {
            // Reset minimized divider position after unminimized state animation finishes
            if (!mDockedStackMinimized && mIsInMinimizeInteraction) {
                mIsInMinimizeInteraction = false;
            }
        };
        anim.addListener(new AnimatorListenerAdapter() {

            private boolean mCancelled;

            @Override
            public void onAnimationCancel(Animator animation) {
                mHandler.removeMessages(MSG_RESIZE_STACK);
                mCancelled = true;
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                long delay = 0;
                if (endDelay != 0) {
                    delay = endDelay;
                } else if (mCancelled) {
                    delay = 0;
                } else if (mSfChoreographer.getSurfaceFlingerOffsetMs() > 0) {
                    delay = mSfChoreographer.getSurfaceFlingerOffsetMs();
                }
                if (delay == 0) {
                    if (!mCancelled) {
                        notCancelledEndAction.run();
                    }
                    endAction.run();
                } else {
                    if (!mCancelled) {
                        mHandler.postDelayed(notCancelledEndAction, delay);
                    }
                    mHandler.postDelayed(endAction, delay);
                }
            }
        });
        mCurrentAnimator = anim;
        return anim;
    }

    private void cancelFlingAnimation() {
        if (mCurrentAnimator != null) {
            mCurrentAnimator.cancel();
        }
    }

    private void commitSnapFlags(SnapTarget target) {
        if (target.flag == SnapTarget.FLAG_NONE) {
            return;
        }
        boolean dismissOrMaximize;
        if (target.flag == SnapTarget.FLAG_DISMISS_START) {
            dismissOrMaximize = mDockSide == WindowManager.DOCKED_LEFT
                    || mDockSide == WindowManager.DOCKED_TOP;
        } else {
            dismissOrMaximize = mDockSide == WindowManager.DOCKED_RIGHT
                    || mDockSide == WindowManager.DOCKED_BOTTOM;
        }
        if (dismissOrMaximize) {
            mWindowManagerProxy.dismissDockedStack();
        } else {
            mWindowManagerProxy.maximizeDockedStack();
        }
        mWindowManagerProxy.setResizeDimLayer(false, WINDOWING_MODE_UNDEFINED, 0f);
    }

    private void liftBackground() {
        if (mBackgroundLifted) {
            return;
        }
        if (isHorizontalDivision()) {
            mBackground.animate().scaleY(1.4f);
        } else {
            mBackground.animate().scaleX(1.4f);
        }
        mBackground.animate()
                .setInterpolator(Interpolators.TOUCH_RESPONSE)
                .setDuration(TOUCH_ANIMATION_DURATION)
                .translationZ(mTouchElevation)
                .start();

        // Lift handle as well so it doesn't get behind the background, even though it doesn't
        // cast shadow.
        mHandle.animate()
                .setInterpolator(Interpolators.TOUCH_RESPONSE)
                .setDuration(TOUCH_ANIMATION_DURATION)
                .translationZ(mTouchElevation)
                .start();
        mBackgroundLifted = true;
    }

    private void releaseBackground() {
        if (!mBackgroundLifted) {
            return;
        }
        mBackground.animate()
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN)
                .setDuration(TOUCH_RELEASE_ANIMATION_DURATION)
                .translationZ(0)
                .scaleX(1f)
                .scaleY(1f)
                .start();
        mHandle.animate()
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN)
                .setDuration(TOUCH_RELEASE_ANIMATION_DURATION)
                .translationZ(0)
                .start();
        mBackgroundLifted = false;
    }


    public void setMinimizedDockStack(boolean minimized, boolean isHomeStackResizable) {
        mHomeStackResizable = isHomeStackResizable;
        updateDockSide();
        if (!minimized) {
            resetBackground();
        } else if (!isHomeStackResizable) {
            if (mDockSide == WindowManager.DOCKED_TOP) {
                mBackground.setPivotY(0);
                mBackground.setScaleY(MINIMIZE_DOCK_SCALE);
            } else if (mDockSide == WindowManager.DOCKED_LEFT
                    || mDockSide == WindowManager.DOCKED_RIGHT) {
                mBackground.setPivotX(mDockSide == WindowManager.DOCKED_LEFT
                        ? 0
                        : mBackground.getWidth());
                mBackground.setScaleX(MINIMIZE_DOCK_SCALE);
            }
        }
        mMinimizedShadow.setAlpha(minimized ? 1f : 0f);
        if (!isHomeStackResizable) {
            mHandle.setAlpha(minimized ? 0f : 1f);
            mDockedStackMinimized = minimized;
        } else if (mDockedStackMinimized != minimized) {
            mDockedStackMinimized = minimized;
            if (mDisplayRotation != mDefaultDisplay.getRotation()) {
                // Splitscreen to minimize is about to starts after rotating landscape to seascape,
                // update insets, display info and snap algorithm targets
                WindowManagerWrapper.getInstance().getStableInsets(mStableInsets);
                repositionSnapTargetBeforeMinimized();
                updateDisplayInfo();
            } else {
                mMinimizedSnapAlgorithm = null;
                initializeSnapAlgorithm();
            }
            if (mIsInMinimizeInteraction != minimized || mCurrentAnimator != null) {
                cancelFlingAnimation();
                if (minimized) {
                    // Relayout to recalculate the divider shadow when minimizing
                    requestLayout();
                    mIsInMinimizeInteraction = true;
                    resizeStack(mMinimizedSnapAlgorithm.getMiddleTarget());
                } else {
                    resizeStack(mSnapTargetBeforeMinimized);
                    mIsInMinimizeInteraction = false;
                }
            }
        }
    }

    public void setMinimizedDockStack(boolean minimized, long animDuration,
            boolean isHomeStackResizable) {
        mHomeStackResizable = isHomeStackResizable;
        updateDockSide();
        if (!isHomeStackResizable) {
            mMinimizedShadow.animate()
                    .alpha(minimized ? 1f : 0f)
                    .setInterpolator(Interpolators.ALPHA_IN)
                    .setDuration(animDuration)
                    .start();
            mHandle.animate()
                    .setInterpolator(Interpolators.FAST_OUT_SLOW_IN)
                    .setDuration(animDuration)
                    .alpha(minimized ? 0f : 1f)
                    .start();
            if (mDockSide == WindowManager.DOCKED_TOP) {
                mBackground.setPivotY(0);
                mBackground.animate()
                        .scaleY(minimized ? MINIMIZE_DOCK_SCALE : 1f);
            } else if (mDockSide == WindowManager.DOCKED_LEFT
                    || mDockSide == WindowManager.DOCKED_RIGHT) {
                mBackground.setPivotX(mDockSide == WindowManager.DOCKED_LEFT
                        ? 0
                        : mBackground.getWidth());
                mBackground.animate()
                        .scaleX(minimized ? MINIMIZE_DOCK_SCALE : 1f);
            }
            mDockedStackMinimized = minimized;
        } else if (mDockedStackMinimized != minimized) {
            mIsInMinimizeInteraction = true;
            mMinimizedSnapAlgorithm = null;
            mDockedStackMinimized = minimized;
            initializeSnapAlgorithm();
            stopDragging(minimized
                            ? mSnapTargetBeforeMinimized.position
                            : getCurrentPosition(),
                    minimized
                            ? mMinimizedSnapAlgorithm.getMiddleTarget()
                            : mSnapTargetBeforeMinimized,
                    animDuration, Interpolators.FAST_OUT_SLOW_IN, 0);
            setAdjustedForIme(false, animDuration);
        }
        if (!minimized) {
            mBackground.animate().withEndAction(mResetBackgroundRunnable);
        }
        mBackground.animate()
                .setInterpolator(Interpolators.FAST_OUT_SLOW_IN)
                .setDuration(animDuration)
                .start();
    }

    public void setAdjustedForIme(boolean adjustedForIme) {
        updateDockSide();
        mHandle.setAlpha(adjustedForIme ? 0f : 1f);
        if (!adjustedForIme) {
            resetBackground();
        } else if (mDockSide == WindowManager.DOCKED_TOP) {
            mBackground.setPivotY(0);
            mBackground.setScaleY(ADJUSTED_FOR_IME_SCALE);
        }
        mAdjustedForIme = adjustedForIme;
    }

    public void setAdjustedForIme(boolean adjustedForIme, long animDuration) {
        updateDockSide();
        mHandle.animate()
                .setInterpolator(IME_ADJUST_INTERPOLATOR)
                .setDuration(animDuration)
                .alpha(adjustedForIme ? 0f : 1f)
                .start();
        if (mDockSide == WindowManager.DOCKED_TOP) {
            mBackground.setPivotY(0);
            mBackground.animate()
                    .scaleY(adjustedForIme ? ADJUSTED_FOR_IME_SCALE : 1f);
        }
        if (!adjustedForIme) {
            mBackground.animate().withEndAction(mResetBackgroundRunnable);
        }
        mBackground.animate()
                .setInterpolator(IME_ADJUST_INTERPOLATOR)
                .setDuration(animDuration)
                .start();
        mAdjustedForIme = adjustedForIme;
    }

    private void saveSnapTargetBeforeMinimized(SnapTarget target) {
        mSnapTargetBeforeMinimized = target;
        mState.mRatioPositionBeforeMinimized = (float) target.position /
                (isHorizontalDivision() ? mDisplayHeight : mDisplayWidth);
    }

    private void resetBackground() {
        mBackground.setPivotX(mBackground.getWidth() / 2);
        mBackground.setPivotY(mBackground.getHeight() / 2);
        mBackground.setScaleX(1f);
        mBackground.setScaleY(1f);
        mMinimizedShadow.setAlpha(0f);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateDisplayInfo();
    }

    public void notifyDockSideChanged(int newDockSide) {
        int oldDockSide = mDockSide;
        mDockSide = newDockSide;
        mMinimizedShadow.setDockSide(mDockSide);
        requestLayout();

        // Update the snap position to the new docked side with correct insets
        WindowManagerWrapper.getInstance().getStableInsets(mStableInsets);
        mMinimizedSnapAlgorithm = null;
        initializeSnapAlgorithm();

        if (oldDockSide == DOCKED_LEFT && mDockSide == DOCKED_RIGHT
                || oldDockSide == DOCKED_RIGHT && mDockSide == DOCKED_LEFT) {
            repositionSnapTargetBeforeMinimized();
        }

        // Landscape to seascape rotation requires minimized to resize docked app correctly
        if (mHomeStackResizable && mDockedStackMinimized) {
            resizeStack(mMinimizedSnapAlgorithm.getMiddleTarget());
        }
    }

    private void repositionSnapTargetBeforeMinimized() {
        int position = (int) (mState.mRatioPositionBeforeMinimized *
                (isHorizontalDivision() ? mDisplayHeight : mDisplayWidth));
        mSnapAlgorithm = null;
        initializeSnapAlgorithm();

        // Set the snap target before minimized but do not save until divider is attached and not
        // minimized because it does not know its minimized state yet.
        mSnapTargetBeforeMinimized = mSnapAlgorithm.calculateNonDismissingSnapTarget(position);
    }

    private void updateDisplayInfo() {
        mDisplayRotation = mDefaultDisplay.getRotation();
        final DisplayInfo info = new DisplayInfo();
        mDefaultDisplay.getDisplayInfo(info);
        mDisplayWidth = info.logicalWidth;
        mDisplayHeight = info.logicalHeight;
        mSnapAlgorithm = null;
        mMinimizedSnapAlgorithm = null;
        initializeSnapAlgorithm();
    }

    private int calculatePosition(int touchX, int touchY) {
        return isHorizontalDivision() ? calculateYPosition(touchY) : calculateXPosition(touchX);
    }

    public boolean isHorizontalDivision() {
        return getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT;
    }

    private int calculateXPosition(int touchX) {
        return mStartPosition + touchX - mStartX;
    }

    private int calculateYPosition(int touchY) {
        return mStartPosition + touchY - mStartY;
    }

    private void alignTopLeft(Rect containingRect, Rect rect) {
        int width = rect.width();
        int height = rect.height();
        rect.set(containingRect.left, containingRect.top,
                containingRect.left + width, containingRect.top + height);
    }

    private void alignBottomRight(Rect containingRect, Rect rect) {
        int width = rect.width();
        int height = rect.height();
        rect.set(containingRect.right - width, containingRect.bottom - height,
                containingRect.right, containingRect.bottom);
    }

    public void calculateBoundsForPosition(int position, int dockSide, Rect outRect) {
        DockedDividerUtils.calculateBoundsForPosition(position, dockSide, outRect, mDisplayWidth,
                mDisplayHeight, mDividerSize);
    }

    public void resizeStackDelayed(int position, int taskPosition, SnapTarget taskSnapTarget) {
        Message message = mHandler.obtainMessage(MSG_RESIZE_STACK, position, taskPosition,
                taskSnapTarget);
        message.setAsynchronous(true);
        mSfChoreographer.scheduleAtSfVsync(mHandler, message);
    }

    private void resizeStack(SnapTarget taskSnapTarget) {
        resizeStack(taskSnapTarget.position, taskSnapTarget.position, taskSnapTarget);
    }

    public void resizeStack(int position, int taskPosition, SnapTarget taskSnapTarget) {
        if (mRemoved) {
            // This divider view has been removed so shouldn't have any additional influence.
            return;
        }
        calculateBoundsForPosition(position, mDockSide, mDockedRect);

        if (mDockedRect.equals(mLastResizeRect) && !mEntranceAnimationRunning) {
            return;
        }

        // Make sure shadows are updated
        if (mBackground.getZ() > 0f) {
            mBackground.invalidate();
        }

        mLastResizeRect.set(mDockedRect);
        if (mHomeStackResizable && mIsInMinimizeInteraction) {
            calculateBoundsForPosition(mSnapTargetBeforeMinimized.position, mDockSide,
                    mDockedTaskRect);
            calculateBoundsForPosition(mSnapTargetBeforeMinimized.position,
                    DockedDividerUtils.invertDockSide(mDockSide), mOtherTaskRect);

            // Move a right-docked-app to line up with the divider while dragging it
            if (mDockSide == DOCKED_RIGHT) {
                mDockedTaskRect.offset(Math.max(position, mStableInsets.left - mDividerSize)
                        - mDockedTaskRect.left + mDividerSize, 0);
            }
            mWindowManagerProxy.resizeDockedStack(mDockedRect, mDockedTaskRect, mDockedTaskRect,
                    mOtherTaskRect, null);
            return;
        }

        if (mEntranceAnimationRunning && taskPosition != TASK_POSITION_SAME) {
            calculateBoundsForPosition(taskPosition, mDockSide, mDockedTaskRect);

            // Move a docked app if from the right in position with the divider up to insets
            if (mDockSide == DOCKED_RIGHT) {
                mDockedTaskRect.offset(Math.max(position, mStableInsets.left - mDividerSize)
                        - mDockedTaskRect.left + mDividerSize, 0);
            }
            calculateBoundsForPosition(taskPosition, DockedDividerUtils.invertDockSide(mDockSide),
                    mOtherTaskRect);
            mWindowManagerProxy.resizeDockedStack(mDockedRect, mDockedTaskRect, null,
                    mOtherTaskRect, null);
        } else if (mExitAnimationRunning && taskPosition != TASK_POSITION_SAME) {
            calculateBoundsForPosition(taskPosition, mDockSide, mDockedTaskRect);
            mDockedInsetRect.set(mDockedTaskRect);
            calculateBoundsForPosition(mExitStartPosition,
                    DockedDividerUtils.invertDockSide(mDockSide), mOtherTaskRect);
            mOtherInsetRect.set(mOtherTaskRect);
            applyExitAnimationParallax(mOtherTaskRect, position);

            // Move a right-docked-app to line up with the divider while dragging it
            if (mDockSide == DOCKED_RIGHT) {
                mDockedTaskRect.offset(position - mStableInsets.left + mDividerSize, 0);
            }
            mWindowManagerProxy.resizeDockedStack(mDockedRect, mDockedTaskRect, mDockedInsetRect,
                    mOtherTaskRect, mOtherInsetRect);
        } else if (taskPosition != TASK_POSITION_SAME) {
            calculateBoundsForPosition(position, DockedDividerUtils.invertDockSide(mDockSide),
                    mOtherRect);
            int dockSideInverted = DockedDividerUtils.invertDockSide(mDockSide);
            int taskPositionDocked =
                    restrictDismissingTaskPosition(taskPosition, mDockSide, taskSnapTarget);
            int taskPositionOther =
                    restrictDismissingTaskPosition(taskPosition, dockSideInverted, taskSnapTarget);
            calculateBoundsForPosition(taskPositionDocked, mDockSide, mDockedTaskRect);
            calculateBoundsForPosition(taskPositionOther, dockSideInverted, mOtherTaskRect);
            mTmpRect.set(0, 0, mDisplayWidth, mDisplayHeight);
            alignTopLeft(mDockedRect, mDockedTaskRect);
            alignTopLeft(mOtherRect, mOtherTaskRect);
            mDockedInsetRect.set(mDockedTaskRect);
            mOtherInsetRect.set(mOtherTaskRect);
            if (dockSideTopLeft(mDockSide)) {
                alignTopLeft(mTmpRect, mDockedInsetRect);
                alignBottomRight(mTmpRect, mOtherInsetRect);
            } else {
                alignBottomRight(mTmpRect, mDockedInsetRect);
                alignTopLeft(mTmpRect, mOtherInsetRect);
            }
            applyDismissingParallax(mDockedTaskRect, mDockSide, taskSnapTarget, position,
                    taskPositionDocked);
            applyDismissingParallax(mOtherTaskRect, dockSideInverted, taskSnapTarget, position,
                    taskPositionOther);
            mWindowManagerProxy.resizeDockedStack(mDockedRect, mDockedTaskRect, mDockedInsetRect,
                    mOtherTaskRect, mOtherInsetRect);
        } else {
            mWindowManagerProxy.resizeDockedStack(mDockedRect, null, null, null, null);
        }
        SnapTarget closestDismissTarget = getSnapAlgorithm().getClosestDismissTarget(position);
        float dimFraction = getDimFraction(position, closestDismissTarget);
        mWindowManagerProxy.setResizeDimLayer(dimFraction != 0f,
                getWindowingModeForDismissTarget(closestDismissTarget), dimFraction);
    }

    private void applyExitAnimationParallax(Rect taskRect, int position) {
        if (mDockSide == WindowManager.DOCKED_TOP) {
            taskRect.offset(0, (int) ((position - mExitStartPosition) * 0.25f));
        } else if (mDockSide == WindowManager.DOCKED_LEFT) {
            taskRect.offset((int) ((position - mExitStartPosition) * 0.25f), 0);
        } else if (mDockSide == WindowManager.DOCKED_RIGHT) {
            taskRect.offset((int) ((mExitStartPosition - position) * 0.25f), 0);
        }
    }

    private float getDimFraction(int position, SnapTarget dismissTarget) {
        if (mEntranceAnimationRunning) {
            return 0f;
        }
        float fraction = getSnapAlgorithm().calculateDismissingFraction(position);
        fraction = Math.max(0, Math.min(fraction, 1f));
        fraction = DIM_INTERPOLATOR.getInterpolation(fraction);
        if (hasInsetsAtDismissTarget(dismissTarget)) {

            // Less darkening with system insets.
            fraction *= 0.8f;
        }
        return fraction;
    }

    /**
     * @return true if and only if there are system insets at the location of the dismiss target
     */
    private boolean hasInsetsAtDismissTarget(SnapTarget dismissTarget) {
        if (isHorizontalDivision()) {
            if (dismissTarget == getSnapAlgorithm().getDismissStartTarget()) {
                return mStableInsets.top != 0;
            } else {
                return mStableInsets.bottom != 0;
            }
        } else {
            if (dismissTarget == getSnapAlgorithm().getDismissStartTarget()) {
                return mStableInsets.left != 0;
            } else {
                return mStableInsets.right != 0;
            }
        }
    }

    /**
     * When the snap target is dismissing one side, make sure that the dismissing side doesn't get
     * 0 size.
     */
    private int restrictDismissingTaskPosition(int taskPosition, int dockSide,
            SnapTarget snapTarget) {
        if (snapTarget.flag == SnapTarget.FLAG_DISMISS_START && dockSideTopLeft(dockSide)) {
            return Math.max(mSnapAlgorithm.getFirstSplitTarget().position, mStartPosition);
        } else if (snapTarget.flag == SnapTarget.FLAG_DISMISS_END
                && dockSideBottomRight(dockSide)) {
            return Math.min(mSnapAlgorithm.getLastSplitTarget().position, mStartPosition);
        } else {
            return taskPosition;
        }
    }

    /**
     * Applies a parallax to the task when dismissing.
     */
    private void applyDismissingParallax(Rect taskRect, int dockSide, SnapTarget snapTarget,
            int position, int taskPosition) {
        float fraction = Math.min(1, Math.max(0,
                mSnapAlgorithm.calculateDismissingFraction(position)));
        SnapTarget dismissTarget = null;
        SnapTarget splitTarget = null;
        int start = 0;
        if (position <= mSnapAlgorithm.getLastSplitTarget().position
                && dockSideTopLeft(dockSide)) {
            dismissTarget = mSnapAlgorithm.getDismissStartTarget();
            splitTarget = mSnapAlgorithm.getFirstSplitTarget();
            start = taskPosition;
        } else if (position >= mSnapAlgorithm.getLastSplitTarget().position
                && dockSideBottomRight(dockSide)) {
            dismissTarget = mSnapAlgorithm.getDismissEndTarget();
            splitTarget = mSnapAlgorithm.getLastSplitTarget();
            start = splitTarget.position;
        }
        if (dismissTarget != null && fraction > 0f
                && isDismissing(splitTarget, position, dockSide)) {
            fraction = calculateParallaxDismissingFraction(fraction, dockSide);
            int offsetPosition = (int) (start +
                    fraction * (dismissTarget.position - splitTarget.position));
            int width = taskRect.width();
            int height = taskRect.height();
            switch (dockSide) {
                case WindowManager.DOCKED_LEFT:
                    taskRect.left = offsetPosition - width;
                    taskRect.right = offsetPosition;
                    break;
                case WindowManager.DOCKED_RIGHT:
                    taskRect.left = offsetPosition + mDividerSize;
                    taskRect.right = offsetPosition + width + mDividerSize;
                    break;
                case WindowManager.DOCKED_TOP:
                    taskRect.top = offsetPosition - height;
                    taskRect.bottom = offsetPosition;
                    break;
                case WindowManager.DOCKED_BOTTOM:
                    taskRect.top = offsetPosition + mDividerSize;
                    taskRect.bottom = offsetPosition + height + mDividerSize;
                    break;
            }
        }
    }

    /**
     * @return for a specified {@code fraction}, this returns an adjusted value that simulates a
     *         slowing down parallax effect
     */
    private static float calculateParallaxDismissingFraction(float fraction, int dockSide) {
        float result = SLOWDOWN_INTERPOLATOR.getInterpolation(fraction) / 3.5f;

        // Less parallax at the top, just because.
        if (dockSide == WindowManager.DOCKED_TOP) {
            result /= 2f;
        }
        return result;
    }

    private static boolean isDismissing(SnapTarget snapTarget, int position, int dockSide) {
        if (dockSide == WindowManager.DOCKED_TOP || dockSide == WindowManager.DOCKED_LEFT) {
            return position < snapTarget.position;
        } else {
            return position > snapTarget.position;
        }
    }

    private int getWindowingModeForDismissTarget(SnapTarget dismissTarget) {
        if ((dismissTarget.flag == SnapTarget.FLAG_DISMISS_START && dockSideTopLeft(mDockSide))
                || (dismissTarget.flag == SnapTarget.FLAG_DISMISS_END
                        && dockSideBottomRight(mDockSide))) {
            return WINDOWING_MODE_SPLIT_SCREEN_PRIMARY;
        } else {
            return WINDOWING_MODE_SPLIT_SCREEN_SECONDARY;
        }
    }

    /**
     * @return true if and only if {@code dockSide} is top or left
     */
    private static boolean dockSideTopLeft(int dockSide) {
        return dockSide == WindowManager.DOCKED_TOP || dockSide == WindowManager.DOCKED_LEFT;
    }

    /**
     * @return true if and only if {@code dockSide} is bottom or right
     */
    private static boolean dockSideBottomRight(int dockSide) {
        return dockSide == WindowManager.DOCKED_BOTTOM || dockSide == WindowManager.DOCKED_RIGHT;
    }

    @Override
    public void onComputeInternalInsets(InternalInsetsInfo inoutInfo) {
        inoutInfo.setTouchableInsets(InternalInsetsInfo.TOUCHABLE_INSETS_REGION);
        inoutInfo.touchableRegion.set(mHandle.getLeft(), mHandle.getTop(), mHandle.getRight(),
                mHandle.getBottom());
        inoutInfo.touchableRegion.op(mBackground.getLeft(), mBackground.getTop(),
                mBackground.getRight(), mBackground.getBottom(), Op.UNION);
    }

    /**
     * Checks whether recents will grow when invoked. This happens in multi-window when recents is
     * very small. When invoking recents, we shrink the docked stack so recents has more space.
     *
     * @return the position of the divider when recents grows, or
     *         {@link #INVALID_RECENTS_GROW_TARGET} if recents won't grow
     */
    public int growsRecents() {
        boolean result = mGrowRecents
                && mDockSide == WindowManager.DOCKED_TOP
                && getCurrentPosition() == getSnapAlgorithm().getLastSplitTarget().position;
        if (result) {
            return getSnapAlgorithm().getMiddleTarget().position;
        } else {
            return INVALID_RECENTS_GROW_TARGET;
        }
    }

    void onRecentsActivityStarting() {
        if (mGrowRecents && mDockSide == WindowManager.DOCKED_TOP
                && getSnapAlgorithm().getMiddleTarget() != getSnapAlgorithm().getLastSplitTarget()
                && getCurrentPosition() == getSnapAlgorithm().getLastSplitTarget().position) {
            mState.growAfterRecentsDrawn = true;
            startDragging(false /* animate */, false /* touching */);
        }
    }

    void onDockedFirstAnimationFrame() {
        saveSnapTargetBeforeMinimized(mSnapAlgorithm.getMiddleTarget());
    }

    void onDockedTopTask() {
        mState.growAfterRecentsDrawn = false;
        mState.animateAfterRecentsDrawn = true;
        startDragging(false /* animate */, false /* touching */);
        updateDockSide();
        mEntranceAnimationRunning = true;

        resizeStack(calculatePositionForInsetBounds(), mSnapAlgorithm.getMiddleTarget().position,
                mSnapAlgorithm.getMiddleTarget());
    }

    void onRecentsDrawn() {
        updateDockSide();
        final int position = calculatePositionForInsetBounds();
        if (mState.animateAfterRecentsDrawn) {
            mState.animateAfterRecentsDrawn = false;

            mHandler.post(() -> {
                // Delay switching resizing mode because this might cause jank in recents animation
                // that's longer than this animation.
                stopDragging(position, getSnapAlgorithm().getMiddleTarget(),
                        mLongPressEntraceAnimDuration, Interpolators.FAST_OUT_SLOW_IN,
                        200 /* endDelay */);
            });
        }
        if (mState.growAfterRecentsDrawn) {
            mState.growAfterRecentsDrawn = false;
            updateDockSide();
            if (mCallback != null) {
                mCallback.growRecents();
            }
            stopDragging(position, getSnapAlgorithm().getMiddleTarget(), 336,
                    Interpolators.FAST_OUT_SLOW_IN);
        }
    }

    void onUndockingTask() {
        int dockSide = mWindowManagerProxy.getDockSide();
        if (dockSide != WindowManager.DOCKED_INVALID && (mHomeStackResizable
                || !mDockedStackMinimized)) {
            startDragging(false /* animate */, false /* touching */);
            SnapTarget target = dockSideTopLeft(dockSide)
                    ? mSnapAlgorithm.getDismissEndTarget()
                    : mSnapAlgorithm.getDismissStartTarget();

            // Don't start immediately - give a little bit time to settle the drag resize change.
            mExitAnimationRunning = true;
            mExitStartPosition = getCurrentPosition();
            stopDragging(mExitStartPosition, target, 336 /* duration */, 100 /* startDelay */,
                    0 /* endDelay */, Interpolators.FAST_OUT_SLOW_IN);
        }
    }

    private int calculatePositionForInsetBounds() {
        mTmpRect.set(0, 0, mDisplayWidth, mDisplayHeight);
        mTmpRect.inset(mStableInsets);
        return DockedDividerUtils.calculatePositionForBounds(mTmpRect, mDockSide, mDividerSize);
    }
}
