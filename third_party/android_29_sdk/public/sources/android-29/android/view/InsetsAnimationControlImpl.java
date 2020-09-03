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
 * limitations under the License
 */

package android.view;

import static android.view.InsetsState.INSET_SIDE_BOTTOM;
import static android.view.InsetsState.INSET_SIDE_LEFT;
import static android.view.InsetsState.INSET_SIDE_RIGHT;
import static android.view.InsetsState.INSET_SIDE_TOP;
import static android.view.InsetsState.toPublicType;

import android.annotation.Nullable;
import android.graphics.Insets;
import android.graphics.Matrix;
import android.graphics.Rect;
import android.os.UidProto.Sync;
import android.util.ArraySet;
import android.util.SparseArray;
import android.util.SparseIntArray;
import android.util.SparseSetArray;
import android.view.InsetsState.InsetSide;
import android.view.SyncRtSurfaceTransactionApplier.SurfaceParams;
import android.view.WindowInsets.Type.InsetType;
import android.view.WindowInsetsAnimationListener.InsetsAnimation;
import android.view.WindowManager.LayoutParams;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.function.Function;
import java.util.function.Supplier;

/**
 * Implements {@link WindowInsetsAnimationController}
 * @hide
 */
@VisibleForTesting
public class InsetsAnimationControlImpl implements WindowInsetsAnimationController  {

    private final Rect mTmpFrame = new Rect();

    private final WindowInsetsAnimationControlListener mListener;
    private final SparseArray<InsetsSourceConsumer> mConsumers;
    private final SparseIntArray mTypeSideMap = new SparseIntArray();
    private final SparseSetArray<InsetsSourceConsumer> mSideSourceMap = new SparseSetArray<>();

    /** @see WindowInsetsAnimationController#getHiddenStateInsets */
    private final Insets mHiddenInsets;

    /** @see WindowInsetsAnimationController#getShownStateInsets */
    private final Insets mShownInsets;
    private final Matrix mTmpMatrix = new Matrix();
    private final InsetsState mInitialInsetsState;
    private final @InsetType int mTypes;
    private final Supplier<SyncRtSurfaceTransactionApplier> mTransactionApplierSupplier;
    private final InsetsController mController;
    private final WindowInsetsAnimationListener.InsetsAnimation mAnimation;
    private final Rect mFrame;
    private Insets mCurrentInsets;
    private Insets mPendingInsets;
    private boolean mFinished;
    private boolean mCancelled;
    private int mFinishedShownTypes;

    @VisibleForTesting
    public InsetsAnimationControlImpl(SparseArray<InsetsSourceConsumer> consumers, Rect frame,
            InsetsState state, WindowInsetsAnimationControlListener listener,
            @InsetType int types,
            Supplier<SyncRtSurfaceTransactionApplier> transactionApplierSupplier,
            InsetsController controller) {
        mConsumers = consumers;
        mListener = listener;
        mTypes = types;
        mTransactionApplierSupplier = transactionApplierSupplier;
        mController = controller;
        mInitialInsetsState = new InsetsState(state, true /* copySources */);
        mCurrentInsets = getInsetsFromState(mInitialInsetsState, frame, null /* typeSideMap */);
        mHiddenInsets = calculateInsets(mInitialInsetsState, frame, consumers, false /* shown */,
                null /* typeSideMap */);
        mShownInsets = calculateInsets(mInitialInsetsState, frame, consumers, true /* shown */,
                mTypeSideMap);
        mFrame = new Rect(frame);
        buildTypeSourcesMap(mTypeSideMap, mSideSourceMap, mConsumers);

        // TODO: Check for controllability first and wait for IME if needed.
        listener.onReady(this, types);

        mAnimation = new WindowInsetsAnimationListener.InsetsAnimation(mTypes, mHiddenInsets,
                mShownInsets);
        mController.dispatchAnimationStarted(mAnimation);
    }

    @Override
    public Insets getHiddenStateInsets() {
        return mHiddenInsets;
    }

    @Override
    public Insets getShownStateInsets() {
        return mShownInsets;
    }

    @Override
    public Insets getCurrentInsets() {
        return mCurrentInsets;
    }

    @Override
    @InsetType
    public int getTypes() {
        return mTypes;
    }

    @Override
    public void changeInsets(Insets insets) {
        if (mFinished) {
            throw new IllegalStateException(
                    "Can't change insets on an animation that is finished.");
        }
        if (mCancelled) {
            throw new IllegalStateException(
                    "Can't change insets on an animation that is cancelled.");
        }
        mPendingInsets = sanitize(insets);
        mController.scheduleApplyChangeInsets();
    }

    @VisibleForTesting
    /**
     * @return Whether the finish callback of this animation should be invoked.
     */
    public boolean applyChangeInsets(InsetsState state) {
        if (mCancelled) {
            return false;
        }
        final Insets offset = Insets.subtract(mShownInsets, mPendingInsets);
        ArrayList<SurfaceParams> params = new ArrayList<>();
        if (offset.left != 0) {
            updateLeashesForSide(INSET_SIDE_LEFT, offset.left, mPendingInsets.left, params, state);
        }
        if (offset.top != 0) {
            updateLeashesForSide(INSET_SIDE_TOP, offset.top, mPendingInsets.top, params, state);
        }
        if (offset.right != 0) {
            updateLeashesForSide(INSET_SIDE_RIGHT, offset.right, mPendingInsets.right, params,
                    state);
        }
        if (offset.bottom != 0) {
            updateLeashesForSide(INSET_SIDE_BOTTOM, offset.bottom, mPendingInsets.bottom, params,
                    state);
        }
        SyncRtSurfaceTransactionApplier applier = mTransactionApplierSupplier.get();
        applier.scheduleApply(params.toArray(new SurfaceParams[params.size()]));
        mCurrentInsets = mPendingInsets;
        if (mFinished) {
            mController.notifyFinished(this, mFinishedShownTypes);
        }
        return mFinished;
    }

    @Override
    public void finish(int shownTypes) {
        if (mCancelled) {
            return;
        }
        InsetsState state = new InsetsState(mController.getState());
        for (int i = mConsumers.size() - 1; i >= 0; i--) {
            InsetsSourceConsumer consumer = mConsumers.valueAt(i);
            boolean visible = (shownTypes & toPublicType(consumer.getType())) != 0;
            state.getSource(consumer.getType()).setVisible(visible);
        }
        Insets insets = getInsetsFromState(state, mFrame, null /* typeSideMap */);
        changeInsets(insets);
        mFinished = true;
        mFinishedShownTypes = shownTypes;
    }

    @VisibleForTesting
    public void onCancelled() {
        if (mFinished) {
            return;
        }
        mCancelled = true;
        mListener.onCancelled();
    }

    InsetsAnimation getAnimation() {
        return mAnimation;
    }

    private Insets calculateInsets(InsetsState state, Rect frame,
            SparseArray<InsetsSourceConsumer> consumers, boolean shown,
            @Nullable @InsetSide SparseIntArray typeSideMap) {
        for (int i = consumers.size() - 1; i >= 0; i--) {
            state.getSource(consumers.valueAt(i).getType()).setVisible(shown);
        }
        return getInsetsFromState(state, frame, typeSideMap);
    }

    private Insets getInsetsFromState(InsetsState state, Rect frame,
            @Nullable @InsetSide SparseIntArray typeSideMap) {
        return state.calculateInsets(frame, false /* isScreenRound */,
                false /* alwaysConsumerNavBar */, null /* displayCutout */,
                null /* legacyContentInsets */, null /* legacyStableInsets */,
                LayoutParams.SOFT_INPUT_ADJUST_RESIZE /* legacySoftInputMode*/, typeSideMap)
               .getInsets(mTypes);
    }

    private Insets sanitize(Insets insets) {
        return Insets.max(Insets.min(insets, mShownInsets), mHiddenInsets);
    }

    private void updateLeashesForSide(@InsetSide int side, int offset, int inset,
            ArrayList<SurfaceParams> surfaceParams, InsetsState state) {
        ArraySet<InsetsSourceConsumer> items = mSideSourceMap.get(side);
        // TODO: Implement behavior when inset spans over multiple types
        for (int i = items.size() - 1; i >= 0; i--) {
            final InsetsSourceConsumer consumer = items.valueAt(i);
            final InsetsSource source = mInitialInsetsState.getSource(consumer.getType());
            final InsetsSourceControl control = consumer.getControl();
            final SurfaceControl leash = consumer.getControl().getLeash();

            mTmpMatrix.setTranslate(control.getSurfacePosition().x, control.getSurfacePosition().y);
            mTmpFrame.set(source.getFrame());
            addTranslationToMatrix(side, offset, mTmpMatrix, mTmpFrame);

            state.getSource(source.getType()).setFrame(mTmpFrame);
            surfaceParams.add(new SurfaceParams(leash, 1f, mTmpMatrix, null, 0, 0f, inset != 0));
        }
    }

    private void addTranslationToMatrix(@InsetSide int side, int inset, Matrix m, Rect frame) {
        switch (side) {
            case INSET_SIDE_LEFT:
                m.postTranslate(-inset, 0);
                frame.offset(-inset, 0);
                break;
            case INSET_SIDE_TOP:
                m.postTranslate(0, -inset);
                frame.offset(0, -inset);
                break;
            case INSET_SIDE_RIGHT:
                m.postTranslate(inset, 0);
                frame.offset(inset, 0);
                break;
            case INSET_SIDE_BOTTOM:
                m.postTranslate(0, inset);
                frame.offset(0, inset);
                break;
        }
    }

    private static void buildTypeSourcesMap(SparseIntArray typeSideMap,
            SparseSetArray<InsetsSourceConsumer> sideSourcesMap,
            SparseArray<InsetsSourceConsumer> consumers) {
        for (int i = typeSideMap.size() - 1; i >= 0; i--) {
            int type = typeSideMap.keyAt(i);
            int side = typeSideMap.valueAt(i);
            sideSourcesMap.add(side, consumers.get(type));
        }
    }
}
