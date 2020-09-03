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

package com.android.systemui.pip.phone;

import static com.android.systemui.pip.phone.PipMenuActivityController.MENU_STATE_CLOSE;
import static com.android.systemui.pip.phone.PipMenuActivityController.MENU_STATE_FULL;
import static com.android.systemui.pip.phone.PipMenuActivityController.MENU_STATE_NONE;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.animation.ValueAnimator.AnimatorUpdateListener;
import android.app.IActivityManager;
import android.app.IActivityTaskManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.PointF;
import android.graphics.Rect;
import android.os.Handler;
import android.os.RemoteException;
import android.util.Log;
import android.util.Size;
import android.view.IPinnedStackController;
import android.view.InputEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.accessibility.AccessibilityEvent;
import android.view.accessibility.AccessibilityManager;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityWindowInfo;

import com.android.internal.os.logging.MetricsLoggerWrapper;
import com.android.internal.policy.PipSnapAlgorithm;
import com.android.systemui.R;
import com.android.systemui.shared.system.InputConsumerController;
import com.android.systemui.statusbar.FlingAnimationUtils;

import java.io.PrintWriter;

/**
 * Manages all the touch handling for PIP on the Phone, including moving, dismissing and expanding
 * the PIP.
 */
public class PipTouchHandler {
    private static final String TAG = "PipTouchHandler";

    // Allow the PIP to be dragged to the edge of the screen to be minimized.
    private static final boolean ENABLE_MINIMIZE = false;
    // Allow the PIP to be flung from anywhere on the screen to the bottom to be dismissed.
    private static final boolean ENABLE_FLING_DISMISS = false;

    private static final int SHOW_DISMISS_AFFORDANCE_DELAY = 225;
    private static final int BOTTOM_OFFSET_BUFFER_DP = 1;

    // Allow dragging the PIP to a location to close it
    private final boolean mEnableDimissDragToEdge;
    private final Context mContext;
    private final IActivityManager mActivityManager;
    private final IActivityTaskManager mActivityTaskManager;
    private final ViewConfiguration mViewConfig;
    private final PipMenuListener mMenuListener = new PipMenuListener();
    private IPinnedStackController mPinnedStackController;

    private final PipMenuActivityController mMenuController;
    private final PipDismissViewController mDismissViewController;
    private final PipSnapAlgorithm mSnapAlgorithm;
    private final AccessibilityManager mAccessibilityManager;
    private boolean mShowPipMenuOnAnimationEnd = false;

    // The current movement bounds
    private Rect mMovementBounds = new Rect();

    // The reference inset bounds, used to determine the dismiss fraction
    private Rect mInsetBounds = new Rect();
    // The reference bounds used to calculate the normal/expanded target bounds
    private Rect mNormalBounds = new Rect();
    private Rect mNormalMovementBounds = new Rect();
    private Rect mExpandedBounds = new Rect();
    private Rect mExpandedMovementBounds = new Rect();
    private int mExpandedShortestEdgeSize;

    // Used to workaround an issue where the WM rotation happens before we are notified, allowing
    // us to send stale bounds
    private int mDeferResizeToNormalBoundsUntilRotation = -1;
    private int mDisplayRotation;

    private Handler mHandler = new Handler();
    private Runnable mShowDismissAffordance = new Runnable() {
        @Override
        public void run() {
            if (mEnableDimissDragToEdge) {
                mDismissViewController.showDismissTarget();
            }
        }
    };
    private ValueAnimator.AnimatorUpdateListener mUpdateScrimListener =
            new AnimatorUpdateListener() {
                @Override
                public void onAnimationUpdate(ValueAnimator animation) {
                    updateDismissFraction();
                }
            };

    // Behaviour states
    private int mMenuState = MENU_STATE_NONE;
    private boolean mIsMinimized;
    private boolean mIsImeShowing;
    private int mImeHeight;
    private int mImeOffset;
    private boolean mIsShelfShowing;
    private int mShelfHeight;
    private int mMovementBoundsExtraOffsets;
    private float mSavedSnapFraction = -1f;
    private boolean mSendingHoverAccessibilityEvents;
    private boolean mMovementWithinMinimize;
    private boolean mMovementWithinDismiss;

    // Touch state
    private final PipTouchState mTouchState;
    private final FlingAnimationUtils mFlingAnimationUtils;
    private final PipTouchGesture[] mGestures;
    private final PipMotionHelper mMotionHelper;

    // Temp vars
    private final Rect mTmpBounds = new Rect();

    /**
     * A listener for the PIP menu activity.
     */
    private class PipMenuListener implements PipMenuActivityController.Listener {
        @Override
        public void onPipMenuStateChanged(int menuState, boolean resize) {
            setMenuState(menuState, resize);
        }

        @Override
        public void onPipExpand() {
            if (!mIsMinimized) {
                mMotionHelper.expandPip();
            }
        }

        @Override
        public void onPipMinimize() {
            setMinimizedStateInternal(true);
            mMotionHelper.animateToClosestMinimizedState(mMovementBounds, null /* updateListener */);
        }

        @Override
        public void onPipDismiss() {
            MetricsLoggerWrapper.logPictureInPictureDismissByTap(mContext,
                    PipUtils.getTopPinnedActivity(mContext, mActivityManager));
            mMotionHelper.dismissPip();
        }

        @Override
        public void onPipShowMenu() {
            mMenuController.showMenu(MENU_STATE_FULL, mMotionHelper.getBounds(),
                    mMovementBounds, true /* allowMenuTimeout */, willResizeMenu());
        }
    }

    public PipTouchHandler(Context context, IActivityManager activityManager,
            IActivityTaskManager activityTaskManager, PipMenuActivityController menuController,
            InputConsumerController inputConsumerController) {

        // Initialize the Pip input consumer
        mContext = context;
        mActivityManager = activityManager;
        mActivityTaskManager = activityTaskManager;
        mAccessibilityManager = context.getSystemService(AccessibilityManager.class);
        mViewConfig = ViewConfiguration.get(context);
        mMenuController = menuController;
        mMenuController.addListener(mMenuListener);
        mDismissViewController = new PipDismissViewController(context);
        mSnapAlgorithm = new PipSnapAlgorithm(mContext);
        mFlingAnimationUtils = new FlingAnimationUtils(context, 2.5f);
        mGestures = new PipTouchGesture[] {
                mDefaultMovementGesture
        };
        mMotionHelper = new PipMotionHelper(mContext, mActivityManager, mActivityTaskManager,
                mMenuController, mSnapAlgorithm, mFlingAnimationUtils);
        mTouchState = new PipTouchState(mViewConfig, mHandler,
                () -> mMenuController.showMenu(MENU_STATE_FULL, mMotionHelper.getBounds(),
                        mMovementBounds, true /* allowMenuTimeout */, willResizeMenu()));

        Resources res = context.getResources();
        mExpandedShortestEdgeSize = res.getDimensionPixelSize(
                R.dimen.pip_expanded_shortest_edge_size);
        mImeOffset = res.getDimensionPixelSize(R.dimen.pip_ime_offset);

        mEnableDimissDragToEdge = res.getBoolean(R.bool.config_pipEnableDismissDragToEdge);

        // Register the listener for input consumer touch events
        inputConsumerController.setInputListener(this::handleTouchEvent);
        inputConsumerController.setRegistrationListener(this::onRegistrationChanged);
        onRegistrationChanged(inputConsumerController.isRegistered());
    }

    public void setTouchEnabled(boolean enabled) {
        mTouchState.setAllowTouches(enabled);
    }

    public void showPictureInPictureMenu() {
        // Only show the menu if the user isn't currently interacting with the PiP
        if (!mTouchState.isUserInteracting()) {
            mMenuController.showMenu(MENU_STATE_FULL, mMotionHelper.getBounds(),
                    mMovementBounds, false /* allowMenuTimeout */, willResizeMenu());
        }
    }

    public void onActivityPinned() {
        cleanUp();
        mShowPipMenuOnAnimationEnd = true;
    }

    public void onActivityUnpinned(ComponentName topPipActivity) {
        if (topPipActivity == null) {
            // Clean up state after the last PiP activity is removed
            cleanUp();
        }
    }

    public void onPinnedStackAnimationEnded() {
        // Always synchronize the motion helper bounds once PiP animations finish
        mMotionHelper.synchronizePinnedStackBounds();

        if (mShowPipMenuOnAnimationEnd) {
            mMenuController.showMenu(MENU_STATE_CLOSE, mMotionHelper.getBounds(),
                    mMovementBounds, true /* allowMenuTimeout */, false /* willResizeMenu */);
            mShowPipMenuOnAnimationEnd = false;
        }
    }

    public void onConfigurationChanged() {
        mMotionHelper.onConfigurationChanged();
        mMotionHelper.synchronizePinnedStackBounds();
    }

    public void onImeVisibilityChanged(boolean imeVisible, int imeHeight) {
        mIsImeShowing = imeVisible;
        mImeHeight = imeHeight;
    }

    public void onShelfVisibilityChanged(boolean shelfVisible, int shelfHeight) {
        mIsShelfShowing = shelfVisible;
        mShelfHeight = shelfHeight;
    }

    public void onMovementBoundsChanged(Rect insetBounds, Rect normalBounds, Rect curBounds,
            boolean fromImeAdjustment, boolean fromShelfAdjustment, int displayRotation) {
        final int bottomOffset = mIsImeShowing ? mImeHeight : 0;

        // Re-calculate the expanded bounds
        mNormalBounds = normalBounds;
        Rect normalMovementBounds = new Rect();
        mSnapAlgorithm.getMovementBounds(mNormalBounds, insetBounds, normalMovementBounds,
                bottomOffset);

        // Calculate the expanded size
        float aspectRatio = (float) normalBounds.width() / normalBounds.height();
        Point displaySize = new Point();
        mContext.getDisplay().getRealSize(displaySize);
        Size expandedSize = mSnapAlgorithm.getSizeForAspectRatio(aspectRatio,
                mExpandedShortestEdgeSize, displaySize.x, displaySize.y);
        mExpandedBounds.set(0, 0, expandedSize.getWidth(), expandedSize.getHeight());
        Rect expandedMovementBounds = new Rect();
        mSnapAlgorithm.getMovementBounds(mExpandedBounds, insetBounds, expandedMovementBounds,
                bottomOffset);

        // The extra offset does not really affect the movement bounds, but are applied based on the
        // current state (ime showing, or shelf offset) when we need to actually shift
        int extraOffset = Math.max(
                mIsImeShowing ? mImeOffset : 0,
                !mIsImeShowing && mIsShelfShowing ? mShelfHeight : 0);

        // If this is from an IME or shelf adjustment, then we should move the PiP so that it is not
        // occluded by the IME or shelf.
        if (fromImeAdjustment || fromShelfAdjustment) {
            if (mTouchState.isUserInteracting()) {
                // Defer the update of the current movement bounds until after the user finishes
                // touching the screen
            } else {
                final float offsetBufferPx = BOTTOM_OFFSET_BUFFER_DP
                        * mContext.getResources().getDisplayMetrics().density;
                final Rect toMovementBounds = mMenuState == MENU_STATE_FULL
                        ? new Rect(expandedMovementBounds)
                        : new Rect(normalMovementBounds);
                final int prevBottom = mMovementBounds.bottom - mMovementBoundsExtraOffsets;
                final int toBottom = toMovementBounds.bottom < toMovementBounds.top
                        ? toMovementBounds.bottom
                        : toMovementBounds.bottom - extraOffset;
                if ((Math.min(prevBottom, toBottom) - offsetBufferPx) <= curBounds.top
                        && curBounds.top <= (Math.max(prevBottom, toBottom) + offsetBufferPx)) {
                    mMotionHelper.animateToOffset(curBounds, toBottom - curBounds.top);
                }
            }
        }

        // Update the movement bounds after doing the calculations based on the old movement bounds
        // above
        mNormalMovementBounds = normalMovementBounds;
        mExpandedMovementBounds = expandedMovementBounds;
        mDisplayRotation = displayRotation;
        mInsetBounds.set(insetBounds);
        updateMovementBounds(mMenuState);
        mMovementBoundsExtraOffsets = extraOffset;

        // If we have a deferred resize, apply it now
        if (mDeferResizeToNormalBoundsUntilRotation == displayRotation) {
            mMotionHelper.animateToUnexpandedState(normalBounds, mSavedSnapFraction,
                    mNormalMovementBounds, mMovementBounds, mIsMinimized,
                    true /* immediate */);
            mSavedSnapFraction = -1f;
            mDeferResizeToNormalBoundsUntilRotation = -1;
        }
    }

    private void onRegistrationChanged(boolean isRegistered) {
        mAccessibilityManager.setPictureInPictureActionReplacingConnection(isRegistered
                ? new PipAccessibilityInteractionConnection(mMotionHelper,
                        this::onAccessibilityShowMenu, mHandler) : null);

        if (!isRegistered && mTouchState.isUserInteracting()) {
            // If the input consumer is unregistered while the user is interacting, then we may not
            // get the final TOUCH_UP event, so clean up the dismiss target as well
            cleanUpDismissTarget();
        }
    }

    private void onAccessibilityShowMenu() {
        mMenuController.showMenu(MENU_STATE_FULL, mMotionHelper.getBounds(),
                mMovementBounds, true /* allowMenuTimeout */, willResizeMenu());
    }

    private boolean handleTouchEvent(InputEvent inputEvent) {
        // Skip any non motion events
        if (!(inputEvent instanceof MotionEvent)) {
            return true;
        }
        // Skip touch handling until we are bound to the controller
        if (mPinnedStackController == null) {
            return true;
        }
        MotionEvent ev = (MotionEvent) inputEvent;

        // Update the touch state
        mTouchState.onTouchEvent(ev);

        switch (ev.getAction()) {
            case MotionEvent.ACTION_DOWN: {
                mMotionHelper.synchronizePinnedStackBounds();

                for (PipTouchGesture gesture : mGestures) {
                    gesture.onDown(mTouchState);
                }
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (PipTouchGesture gesture : mGestures) {
                    if (gesture.onMove(mTouchState)) {
                        break;
                    }
                }
                break;
            }
            case MotionEvent.ACTION_UP: {
                // Update the movement bounds again if the state has changed since the user started
                // dragging (ie. when the IME shows)
                updateMovementBounds(mMenuState);

                for (PipTouchGesture gesture : mGestures) {
                    if (gesture.onUp(mTouchState)) {
                        break;
                    }
                }

                // Fall through to clean up
            }
            case MotionEvent.ACTION_CANCEL: {
                mTouchState.reset();
                break;
            }
            case MotionEvent.ACTION_HOVER_ENTER:
            case MotionEvent.ACTION_HOVER_MOVE: {
                if (mAccessibilityManager.isEnabled() && !mSendingHoverAccessibilityEvents) {
                    AccessibilityEvent event = AccessibilityEvent.obtain(
                            AccessibilityEvent.TYPE_VIEW_HOVER_ENTER);
                    event.setImportantForAccessibility(true);
                    event.setSourceNodeId(AccessibilityNodeInfo.ROOT_NODE_ID);
                    event.setWindowId(
                            AccessibilityWindowInfo.PICTURE_IN_PICTURE_ACTION_REPLACER_WINDOW_ID);
                    mAccessibilityManager.sendAccessibilityEvent(event);
                    mSendingHoverAccessibilityEvents = true;
                }
                break;
            }
            case MotionEvent.ACTION_HOVER_EXIT: {
                if (mAccessibilityManager.isEnabled() && mSendingHoverAccessibilityEvents) {
                    AccessibilityEvent event = AccessibilityEvent.obtain(
                            AccessibilityEvent.TYPE_VIEW_HOVER_EXIT);
                    event.setImportantForAccessibility(true);
                    event.setSourceNodeId(AccessibilityNodeInfo.ROOT_NODE_ID);
                    event.setWindowId(
                            AccessibilityWindowInfo.PICTURE_IN_PICTURE_ACTION_REPLACER_WINDOW_ID);
                    mAccessibilityManager.sendAccessibilityEvent(event);
                    mSendingHoverAccessibilityEvents = false;
                }
                break;
            }
        }
        return mMenuState == MENU_STATE_NONE;
    }

    /**
     * Updates the appearance of the menu and scrim on top of the PiP while dismissing.
     */
    private void updateDismissFraction() {
        // Skip updating the dismiss fraction when the IME is showing. This is to work around an
        // issue where starting the menu activity for the dismiss overlay will steal the window
        // focus, which closes the IME.
        if (mMenuController != null && !mIsImeShowing) {
            Rect bounds = mMotionHelper.getBounds();
            final float target = mInsetBounds.bottom;
            float fraction = 0f;
            if (bounds.bottom > target) {
                final float distance = bounds.bottom - target;
                fraction = Math.min(distance / bounds.height(), 1f);
            }
            if (Float.compare(fraction, 0f) != 0 || mMenuController.isMenuActivityVisible()) {
                // Update if the fraction > 0, or if fraction == 0 and the menu was already visible
                mMenuController.setDismissFraction(fraction);
            }
        }
    }

    /**
     * Sets the controller to update the system of changes from user interaction.
     */
    void setPinnedStackController(IPinnedStackController controller) {
        mPinnedStackController = controller;
    }

    /**
     * Sets the minimized state.
     */
    private void setMinimizedStateInternal(boolean isMinimized) {
        if (!ENABLE_MINIMIZE) {
            return;
        }
        setMinimizedState(isMinimized, false /* fromController */);
    }

    /**
     * Sets the minimized state.
     */
    void setMinimizedState(boolean isMinimized, boolean fromController) {
        if (!ENABLE_MINIMIZE) {
            return;
        }
        if (mIsMinimized != isMinimized) {
            MetricsLoggerWrapper.logPictureInPictureMinimize(mContext,
                    isMinimized, PipUtils.getTopPinnedActivity(mContext, mActivityManager));
        }
        mIsMinimized = isMinimized;
        mSnapAlgorithm.setMinimized(isMinimized);

        if (fromController) {
            if (isMinimized) {
                // Move the PiP to the new bounds immediately if minimized
                mMotionHelper.movePip(mMotionHelper.getClosestMinimizedBounds(mNormalBounds,
                        mMovementBounds));
            }
        } else if (mPinnedStackController != null) {
            try {
                mPinnedStackController.setIsMinimized(isMinimized);
            } catch (RemoteException e) {
                Log.e(TAG, "Could not set minimized state", e);
            }
        }
    }

    /**
     * Sets the menu visibility.
     */
    private void setMenuState(int menuState, boolean resize) {
        if (menuState == MENU_STATE_FULL && mMenuState != MENU_STATE_FULL) {
            // Save the current snap fraction and if we do not drag or move the PiP, then
            // we store back to this snap fraction.  Otherwise, we'll reset the snap
            // fraction and snap to the closest edge
            Rect expandedBounds = new Rect(mExpandedBounds);
            if (resize) {
                mSavedSnapFraction = mMotionHelper.animateToExpandedState(expandedBounds,
                        mMovementBounds, mExpandedMovementBounds);
            }
        } else if (menuState == MENU_STATE_NONE && mMenuState == MENU_STATE_FULL) {
            // Try and restore the PiP to the closest edge, using the saved snap fraction
            // if possible
            if (resize) {
                if (mDeferResizeToNormalBoundsUntilRotation == -1) {
                    // This is a very special case: when the menu is expanded and visible,
                    // navigating to another activity can trigger auto-enter PiP, and if the
                    // revealed activity has a forced rotation set, then the controller will get
                    // updated with the new rotation of the display. However, at the same time,
                    // SystemUI will try to hide the menu by creating an animation to the normal
                    // bounds which are now stale.  In such a case we defer the animation to the
                    // normal bounds until after the next onMovementBoundsChanged() call to get the
                    // bounds in the new orientation
                    try {
                        int displayRotation = mPinnedStackController.getDisplayRotation();
                        if (mDisplayRotation != displayRotation) {
                            mDeferResizeToNormalBoundsUntilRotation = displayRotation;
                        }
                    } catch (RemoteException e) {
                        Log.e(TAG, "Could not get display rotation from controller");
                    }
                }

                if (mDeferResizeToNormalBoundsUntilRotation == -1) {
                    Rect normalBounds = new Rect(mNormalBounds);
                    mMotionHelper.animateToUnexpandedState(normalBounds, mSavedSnapFraction,
                            mNormalMovementBounds, mMovementBounds, mIsMinimized,
                            false /* immediate */);
                    mSavedSnapFraction = -1f;
                }
            } else {
                // If resizing is not allowed, then the PiP should be frozen until the transition
                // ends as well
                setTouchEnabled(false);
                mSavedSnapFraction = -1f;
            }
        }
        mMenuState = menuState;
        updateMovementBounds(menuState);
        if (menuState != MENU_STATE_CLOSE) {
            MetricsLoggerWrapper.logPictureInPictureMenuVisible(mContext, menuState == MENU_STATE_FULL);
        }
    }

    /**
     * @return the motion helper.
     */
    public PipMotionHelper getMotionHelper() {
        return mMotionHelper;
    }

    /**
     * Gesture controlling normal movement of the PIP.
     */
    private PipTouchGesture mDefaultMovementGesture = new PipTouchGesture() {
        // Whether the PiP was on the left side of the screen at the start of the gesture
        private boolean mStartedOnLeft;
        private final Point mStartPosition = new Point();
        private final PointF mDelta = new PointF();

        @Override
        public void onDown(PipTouchState touchState) {
            if (!touchState.isUserInteracting()) {
                return;
            }

            Rect bounds = mMotionHelper.getBounds();
            mDelta.set(0f, 0f);
            mStartPosition.set(bounds.left, bounds.top);
            mStartedOnLeft = bounds.left < mMovementBounds.centerX();
            mMovementWithinMinimize = true;
            mMovementWithinDismiss = touchState.getDownTouchPosition().y >= mMovementBounds.bottom;

            // If the menu is still visible, and we aren't minimized, then just poke the menu
            // so that it will timeout after the user stops touching it
            if (mMenuState != MENU_STATE_NONE && !mIsMinimized) {
                mMenuController.pokeMenu();
            }

            if (mEnableDimissDragToEdge) {
                mDismissViewController.createDismissTarget();
                mHandler.postDelayed(mShowDismissAffordance, SHOW_DISMISS_AFFORDANCE_DELAY);
            }
        }

        @Override
        boolean onMove(PipTouchState touchState) {
            if (!touchState.isUserInteracting()) {
                return false;
            }

            if (touchState.startedDragging()) {
                mSavedSnapFraction = -1f;

                if (mEnableDimissDragToEdge) {
                    mHandler.removeCallbacks(mShowDismissAffordance);
                    mDismissViewController.showDismissTarget();
                }
            }

            if (touchState.isDragging()) {
                // Move the pinned stack freely
                final PointF lastDelta = touchState.getLastTouchDelta();
                float lastX = mStartPosition.x + mDelta.x;
                float lastY = mStartPosition.y + mDelta.y;
                float left = lastX + lastDelta.x;
                float top = lastY + lastDelta.y;
                if (!touchState.allowDraggingOffscreen() || !ENABLE_MINIMIZE) {
                    left = Math.max(mMovementBounds.left, Math.min(mMovementBounds.right, left));
                }
                if (mEnableDimissDragToEdge) {
                    // Allow pip to move past bottom bounds
                    top = Math.max(mMovementBounds.top, top);
                } else {
                    top = Math.max(mMovementBounds.top, Math.min(mMovementBounds.bottom, top));
                }

                // Add to the cumulative delta after bounding the position
                mDelta.x += left - lastX;
                mDelta.y += top - lastY;

                mTmpBounds.set(mMotionHelper.getBounds());
                mTmpBounds.offsetTo((int) left, (int) top);
                mMotionHelper.movePip(mTmpBounds);

                if (mEnableDimissDragToEdge) {
                    updateDismissFraction();
                }

                final PointF curPos = touchState.getLastTouchPosition();
                if (mMovementWithinMinimize) {
                    // Track if movement remains near starting edge to identify swipes to minimize
                    mMovementWithinMinimize = mStartedOnLeft
                            ? curPos.x <= mMovementBounds.left + mTmpBounds.width()
                            : curPos.x >= mMovementBounds.right;
                }
                if (mMovementWithinDismiss) {
                    // Track if movement remains near the bottom edge to identify swipe to dismiss
                    mMovementWithinDismiss = curPos.y >= mMovementBounds.bottom;
                }
                return true;
            }
            return false;
        }

        @Override
        public boolean onUp(PipTouchState touchState) {
            if (mEnableDimissDragToEdge) {
                // Clean up the dismiss target regardless of the touch state in case the touch
                // enabled state changes while the user is interacting
                cleanUpDismissTarget();
            }

            if (!touchState.isUserInteracting()) {
                return false;
            }

            final PointF vel = touchState.getVelocity();
            final boolean isHorizontal = Math.abs(vel.x) > Math.abs(vel.y);
            final float velocity = PointF.length(vel.x, vel.y);
            final boolean isFling = velocity > mFlingAnimationUtils.getMinVelocityPxPerSecond();
            final boolean isUpWithinDimiss = ENABLE_FLING_DISMISS
                    && touchState.getLastTouchPosition().y >= mMovementBounds.bottom
                    && mMotionHelper.isGestureToDismissArea(mMotionHelper.getBounds(), vel.x,
                            vel.y, isFling);
            final boolean isFlingToBot = isFling && vel.y > 0 && !isHorizontal
                    && (mMovementWithinDismiss || isUpWithinDimiss);
            if (mEnableDimissDragToEdge) {
                // Check if the user dragged or flung the PiP offscreen to dismiss it
                if (mMotionHelper.shouldDismissPip() || isFlingToBot) {
                    MetricsLoggerWrapper.logPictureInPictureDismissByDrag(mContext,
                            PipUtils.getTopPinnedActivity(mContext, mActivityManager));
                    mMotionHelper.animateDismiss(mMotionHelper.getBounds(), vel.x,
                        vel.y, mUpdateScrimListener);
                    return true;
                }
            }

            if (touchState.isDragging()) {
                final boolean isFlingToEdge = isFling && isHorizontal && mMovementWithinMinimize
                        && (mStartedOnLeft ? vel.x < 0 : vel.x > 0);
                if (ENABLE_MINIMIZE &&
                        !mIsMinimized && (mMotionHelper.shouldMinimizePip() || isFlingToEdge)) {
                    // Pip should be minimized
                    setMinimizedStateInternal(true);
                    if (mMenuState == MENU_STATE_FULL) {
                        // If the user dragged the expanded PiP to the edge, then hiding the menu
                        // will trigger the PiP to be scaled back to the normal size with the
                        // minimize offset adjusted
                        mMenuController.hideMenu();
                    } else {
                        mMotionHelper.animateToClosestMinimizedState(mMovementBounds,
                                mUpdateScrimListener);
                    }
                    return true;
                }
                if (mIsMinimized) {
                    // If we're dragging and it wasn't a minimize gesture then we shouldn't be
                    // minimized.
                    setMinimizedStateInternal(false);
                }

                AnimatorListenerAdapter postAnimationCallback = null;
                if (mMenuState != MENU_STATE_NONE) {
                    // If the menu is still visible, and we aren't minimized, then just poke the
                    // menu so that it will timeout after the user stops touching it
                    mMenuController.showMenu(mMenuState, mMotionHelper.getBounds(),
                            mMovementBounds, true /* allowMenuTimeout */, willResizeMenu());
                } else {
                    // If the menu is not visible, then we can still be showing the activity for the
                    // dismiss overlay, so just finish it after the animation completes
                    postAnimationCallback = new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            mMenuController.hideMenu();
                        }
                    };
                }

                if (isFling) {
                    mMotionHelper.flingToSnapTarget(velocity, vel.x, vel.y, mMovementBounds,
                            mUpdateScrimListener, postAnimationCallback,
                            mStartPosition);
                } else {
                    mMotionHelper.animateToClosestSnapTarget(mMovementBounds, mUpdateScrimListener,
                            postAnimationCallback);
                }
            } else if (mIsMinimized) {
                // This was a tap, so no longer minimized
                mMotionHelper.animateToClosestSnapTarget(mMovementBounds, null /* updateListener */,
                        null /* animatorListener */);
                setMinimizedStateInternal(false);
            } else if (mMenuState != MENU_STATE_FULL) {
                if (mTouchState.isDoubleTap()) {
                    // Expand to fullscreen if this is a double tap
                    mMotionHelper.expandPip();
                } else if (!mTouchState.isWaitingForDoubleTap()) {
                    // User has stalled long enough for this not to be a drag or a double tap, just
                    // expand the menu
                    mMenuController.showMenu(MENU_STATE_FULL, mMotionHelper.getBounds(),
                            mMovementBounds, true /* allowMenuTimeout */, willResizeMenu());
                } else {
                    // Next touch event _may_ be the second tap for the double-tap, schedule a
                    // fallback runnable to trigger the menu if no touch event occurs before the
                    // next tap
                    mTouchState.scheduleDoubleTapTimeoutCallback();
                }
            } else {
                mMenuController.hideMenu();
                mMotionHelper.expandPip();
            }
            return true;
        }
    };

    /**
     * Updates the current movement bounds based on whether the menu is currently visible.
     */
    private void updateMovementBounds(int menuState) {
        boolean isMenuExpanded = menuState == MENU_STATE_FULL;
        mMovementBounds = isMenuExpanded
                ? mExpandedMovementBounds
                : mNormalMovementBounds;
        try {
            if (mPinnedStackController != null) {
                mPinnedStackController.setMinEdgeSize(
                        isMenuExpanded ? mExpandedShortestEdgeSize : 0);
            }
        } catch (RemoteException e) {
            Log.e(TAG, "Could not set minimized state", e);
        }
    }

    /**
     * Removes the dismiss target and cancels any pending callbacks to show it.
     */
    private void cleanUpDismissTarget() {
        mHandler.removeCallbacks(mShowDismissAffordance);
        mDismissViewController.destroyDismissTarget();
    }

    /**
     * Resets some states related to the touch handling.
     */
    private void cleanUp() {
        if (mIsMinimized) {
            setMinimizedStateInternal(false);
        }
        cleanUpDismissTarget();
    }

    /**
     * @return whether the menu will resize as a part of showing the full menu.
     */
    private boolean willResizeMenu() {
        return mExpandedBounds.width() != mNormalBounds.width() ||
                mExpandedBounds.height() != mNormalBounds.height();
    }

    public void dump(PrintWriter pw, String prefix) {
        final String innerPrefix = prefix + "  ";
        pw.println(prefix + TAG);
        pw.println(innerPrefix + "mMovementBounds=" + mMovementBounds);
        pw.println(innerPrefix + "mNormalBounds=" + mNormalBounds);
        pw.println(innerPrefix + "mNormalMovementBounds=" + mNormalMovementBounds);
        pw.println(innerPrefix + "mExpandedBounds=" + mExpandedBounds);
        pw.println(innerPrefix + "mExpandedMovementBounds=" + mExpandedMovementBounds);
        pw.println(innerPrefix + "mMenuState=" + mMenuState);
        pw.println(innerPrefix + "mIsMinimized=" + mIsMinimized);
        pw.println(innerPrefix + "mIsImeShowing=" + mIsImeShowing);
        pw.println(innerPrefix + "mImeHeight=" + mImeHeight);
        pw.println(innerPrefix + "mIsShelfShowing=" + mIsShelfShowing);
        pw.println(innerPrefix + "mShelfHeight=" + mShelfHeight);
        pw.println(innerPrefix + "mSavedSnapFraction=" + mSavedSnapFraction);
        pw.println(innerPrefix + "mEnableDragToEdgeDismiss=" + mEnableDimissDragToEdge);
        pw.println(innerPrefix + "mEnableMinimize=" + ENABLE_MINIMIZE);
        mSnapAlgorithm.dump(pw, innerPrefix);
        mTouchState.dump(pw, innerPrefix);
        mMotionHelper.dump(pw, innerPrefix);
    }

}
