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

package com.android.systemui.statusbar;

import static java.lang.annotation.RetentionPolicy.SOURCE;

import android.annotation.IntDef;

import com.android.systemui.plugins.statusbar.StatusBarStateController;
import com.android.systemui.statusbar.phone.StatusBar;

import java.lang.annotation.Retention;

/**
 * Sends updates to {@link StateListener}s about changes to the status bar state and dozing state
 */
public interface SysuiStatusBarStateController extends StatusBarStateController {

    // TODO: b/115739177 (remove this explicit ordering if we can)
    @Retention(SOURCE)
    @IntDef({RANK_STATUS_BAR, RANK_STATUS_BAR_WINDOW_CONTROLLER, RANK_STACK_SCROLLER, RANK_SHELF})
    @interface SbStateListenerRank {}
    // This is the set of known dependencies when updating StatusBarState
    int RANK_STATUS_BAR = 0;
    int RANK_STATUS_BAR_WINDOW_CONTROLLER = 1;
    int RANK_STACK_SCROLLER = 2;
    int RANK_SHELF = 3;

    /**
     * Add a listener and a rank based on the priority of this message
     * @param listener the listener
     * @param rank the order in which you'd like to be called. Ranked listeners will be
     * notified before unranked, and we will sort ranked listeners from low to high
     *
     * @deprecated This method exists only to solve latent inter-dependencies from refactoring
     * StatusBarState out of StatusBar.java. Any new listeners should be built not to need ranking
     * (i.e., they are non-dependent on the order of operations of StatusBarState listeners).
     */
    @Deprecated
    void addCallback(StateListener listener, int rank);

    /**
     * Update the status bar state
     * @param state see {@link StatusBarState} for valid options
     * @return {@code true} if the state changed, else {@code false}
     */
    boolean setState(int state);

    /**
     * Update the dozing state from {@link StatusBar}'s perspective
     * @param isDozing well, are we dozing?
     * @return {@code true} if the state changed, else {@code false}
     */
    boolean setIsDozing(boolean isDozing);

    /**
     * Changes the current doze amount.
     *
     * @param dozeAmount New doze/dark amount.
     * @param animated If change should be animated or not. This will cancel current animations.
     */
    void setDozeAmount(float dozeAmount, boolean animated);

    /**
     * Sets whether to leave status bar open when hiding keyguard
     */
    void setLeaveOpenOnKeyguardHide(boolean leaveOpen);

    /**
     * Whether to leave status bar open when hiding keyguard
     */
    boolean leaveOpenOnKeyguardHide();

    /**
     * Interpolated doze amount
     */
    float getInterpolatedDozeAmount();

    /**
     * Whether status bar is going to full shade
     */
    boolean goingToFullShade();

    /**
     * Whether the previous state of the status bar was the shade locked
     */
    boolean fromShadeLocked();

    /**
     * Set keyguard requested
     */
    void setKeyguardRequested(boolean keyguardRequested);

    /**
     * Is keyguard requested
     */
    boolean isKeyguardRequested();

    /**
     * Listener with rankings SbStateListenerRank that have dependencies so must be updated
     * in a certain order
     */
    class RankedListener {
        final StateListener mListener;
        final int mRank;

        RankedListener(StateListener l, int r) {
            mListener = l;
            mRank = r;
        }
    }
}
