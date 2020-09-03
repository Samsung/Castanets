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
 * limitations under the License
 */

package com.android.systemui.statusbar.policy;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.annotation.Nullable;
import android.app.ActivityManager;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.RemoteInput;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutManager;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.UserHandle;
import android.text.Editable;
import android.text.SpannedString;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewAnimationUtils;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.view.inputmethod.CompletionInfo;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import com.android.internal.logging.MetricsLogger;
import com.android.internal.logging.nano.MetricsProto;
import com.android.systemui.Dependency;
import com.android.systemui.Interpolators;
import com.android.systemui.R;
import com.android.systemui.statusbar.RemoteInputController;
import com.android.systemui.statusbar.notification.collection.NotificationEntry;
import com.android.systemui.statusbar.notification.collection.NotificationEntry.EditedSuggestionInfo;
import com.android.systemui.statusbar.notification.row.wrapper.NotificationViewWrapper;
import com.android.systemui.statusbar.notification.stack.StackStateAnimator;
import com.android.systemui.statusbar.phone.LightBarController;

import java.util.function.Consumer;

/**
 * Host for the remote input.
 */
public class RemoteInputView extends LinearLayout implements View.OnClickListener, TextWatcher {

    private static final String TAG = "RemoteInput";

    // A marker object that let's us easily find views of this class.
    public static final Object VIEW_TAG = new Object();

    public final Object mToken = new Object();

    private RemoteEditText mEditText;
    private ImageButton mSendButton;
    private ProgressBar mProgressBar;
    private PendingIntent mPendingIntent;
    private RemoteInput[] mRemoteInputs;
    private RemoteInput mRemoteInput;
    private RemoteInputController mController;
    private RemoteInputQuickSettingsDisabler mRemoteInputQuickSettingsDisabler;

    private NotificationEntry mEntry;

    private boolean mRemoved;

    private int mRevealCx;
    private int mRevealCy;
    private int mRevealR;

    private boolean mResetting;
    private NotificationViewWrapper mWrapper;
    private Consumer<Boolean> mOnVisibilityChangedListener;

    public RemoteInputView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mRemoteInputQuickSettingsDisabler = Dependency.get(RemoteInputQuickSettingsDisabler.class);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mProgressBar = findViewById(R.id.remote_input_progress);

        mSendButton = findViewById(R.id.remote_input_send);
        mSendButton.setOnClickListener(this);

        mEditText = (RemoteEditText) getChildAt(0);
        mEditText.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                final boolean isSoftImeEvent = event == null
                        && (actionId == EditorInfo.IME_ACTION_DONE
                        || actionId == EditorInfo.IME_ACTION_NEXT
                        || actionId == EditorInfo.IME_ACTION_SEND);
                final boolean isKeyboardEnterKey = event != null
                        && KeyEvent.isConfirmKey(event.getKeyCode())
                        && event.getAction() == KeyEvent.ACTION_DOWN;

                if (isSoftImeEvent || isKeyboardEnterKey) {
                    if (mEditText.length() > 0) {
                        sendRemoteInput();
                    }
                    // Consume action to prevent IME from closing.
                    return true;
                }
                return false;
            }
        });
        mEditText.addTextChangedListener(this);
        mEditText.setInnerFocusable(false);
        mEditText.mRemoteInputView = this;
    }

    private void sendRemoteInput() {
        Bundle results = new Bundle();
        results.putString(mRemoteInput.getResultKey(), mEditText.getText().toString());
        Intent fillInIntent = new Intent().addFlags(Intent.FLAG_RECEIVER_FOREGROUND);
        RemoteInput.addResultsToIntent(mRemoteInputs, fillInIntent,
                results);
        if (mEntry.editedSuggestionInfo == null) {
            RemoteInput.setResultsSource(fillInIntent, RemoteInput.SOURCE_FREE_FORM_INPUT);
        } else {
            RemoteInput.setResultsSource(fillInIntent, RemoteInput.SOURCE_CHOICE);
        }

        mEditText.setEnabled(false);
        mSendButton.setVisibility(INVISIBLE);
        mProgressBar.setVisibility(VISIBLE);
        mEntry.remoteInputText = mEditText.getText();
        mEntry.lastRemoteInputSent = SystemClock.elapsedRealtime();
        mController.addSpinning(mEntry.key, mToken);
        mController.removeRemoteInput(mEntry, mToken);
        mEditText.mShowImeOnInputConnection = false;
        mController.remoteInputSent(mEntry);
        mEntry.setHasSentReply();

        // Tell ShortcutManager that this package has been "activated".  ShortcutManager
        // will reset the throttling for this package.
        // Strictly speaking, the intent receiver may be different from the notification publisher,
        // but that's an edge case, and also because we can't always know which package will receive
        // an intent, so we just reset for the publisher.
        getContext().getSystemService(ShortcutManager.class).onApplicationActive(
                mEntry.notification.getPackageName(),
                mEntry.notification.getUser().getIdentifier());

        MetricsLogger.action(mContext, MetricsProto.MetricsEvent.ACTION_REMOTE_INPUT_SEND,
                mEntry.notification.getPackageName());
        try {
            mPendingIntent.send(mContext, 0, fillInIntent);
        } catch (PendingIntent.CanceledException e) {
            Log.i(TAG, "Unable to send remote input result", e);
            MetricsLogger.action(mContext, MetricsProto.MetricsEvent.ACTION_REMOTE_INPUT_FAIL,
                    mEntry.notification.getPackageName());
        }
    }

    public CharSequence getText() {
        return mEditText.getText();
    }

    public static RemoteInputView inflate(Context context, ViewGroup root,
            NotificationEntry entry,
            RemoteInputController controller) {
        RemoteInputView v = (RemoteInputView)
                LayoutInflater.from(context).inflate(R.layout.remote_input, root, false);
        v.mController = controller;
        v.mEntry = entry;
        v.mEditText.setTextOperationUser(computeTextOperationUser(entry.notification.getUser()));
        v.setTag(VIEW_TAG);

        return v;
    }

    @Override
    public void onClick(View v) {
        if (v == mSendButton) {
            sendRemoteInput();
        }
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        super.onTouchEvent(event);

        // We never want for a touch to escape to an outer view or one we covered.
        return true;
    }

    private void onDefocus(boolean animate) {
        mController.removeRemoteInput(mEntry, mToken);
        mEntry.remoteInputText = mEditText.getText();

        // During removal, we get reattached and lose focus. Not hiding in that
        // case to prevent flicker.
        if (!mRemoved) {
            if (animate && mRevealR > 0) {
                Animator reveal = ViewAnimationUtils.createCircularReveal(
                        this, mRevealCx, mRevealCy, mRevealR, 0);
                reveal.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN);
                reveal.setDuration(StackStateAnimator.ANIMATION_DURATION_CLOSE_REMOTE_INPUT);
                reveal.addListener(new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        setVisibility(INVISIBLE);
                        if (mWrapper != null) {
                            mWrapper.setRemoteInputVisible(false);
                        }
                    }
                });
                reveal.start();
            } else {
                setVisibility(INVISIBLE);
                if (mWrapper != null) {
                    mWrapper.setRemoteInputVisible(false);
                }
            }
        }

        mRemoteInputQuickSettingsDisabler.setRemoteInputActive(false);

        MetricsLogger.action(mContext, MetricsProto.MetricsEvent.ACTION_REMOTE_INPUT_CLOSE,
                mEntry.notification.getPackageName());
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mEntry.getRow().isChangingPosition()) {
            if (getVisibility() == VISIBLE && mEditText.isFocusable()) {
                mEditText.requestFocus();
            }
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mEntry.getRow().isChangingPosition() || isTemporarilyDetached()) {
            return;
        }
        mController.removeRemoteInput(mEntry, mToken);
        mController.removeSpinning(mEntry.key, mToken);
    }

    public void setPendingIntent(PendingIntent pendingIntent) {
        mPendingIntent = pendingIntent;
    }

    /**
     * Sets the remote input for this view.
     *
     * @param remoteInputs The remote inputs that need to be sent to the app.
     * @param remoteInput The remote input that needs to be activated.
     * @param editedSuggestionInfo The smart reply that should be inserted in the remote input, or
     *         {@code null} if the user is not editing a smart reply.
     */
    public void setRemoteInput(RemoteInput[] remoteInputs, RemoteInput remoteInput,
            @Nullable EditedSuggestionInfo editedSuggestionInfo) {
        mRemoteInputs = remoteInputs;
        mRemoteInput = remoteInput;
        mEditText.setHint(mRemoteInput.getLabel());

        mEntry.editedSuggestionInfo = editedSuggestionInfo;
        if (editedSuggestionInfo != null) {
            mEntry.remoteInputText = editedSuggestionInfo.originalText;
        }
    }

    public void focusAnimated() {
        if (getVisibility() != VISIBLE) {
            Animator animator = ViewAnimationUtils.createCircularReveal(
                    this, mRevealCx, mRevealCy, 0, mRevealR);
            animator.setDuration(StackStateAnimator.ANIMATION_DURATION_STANDARD);
            animator.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN);
            animator.start();
        }
        focus();
    }

    private static UserHandle computeTextOperationUser(UserHandle notificationUser) {
        return UserHandle.ALL.equals(notificationUser)
                ? UserHandle.of(ActivityManager.getCurrentUser()) : notificationUser;
    }

    public void focus() {
        MetricsLogger.action(mContext, MetricsProto.MetricsEvent.ACTION_REMOTE_INPUT_OPEN,
                mEntry.notification.getPackageName());

        setVisibility(VISIBLE);
        if (mWrapper != null) {
            mWrapper.setRemoteInputVisible(true);
        }
        mEditText.setInnerFocusable(true);
        mEditText.mShowImeOnInputConnection = true;
        mEditText.setText(mEntry.remoteInputText);
        mEditText.setSelection(mEditText.getText().length());
        mEditText.requestFocus();
        mController.addRemoteInput(mEntry, mToken);

        mRemoteInputQuickSettingsDisabler.setRemoteInputActive(true);

        updateSendButton();
    }

    public void onNotificationUpdateOrReset() {
        boolean sending = mProgressBar.getVisibility() == VISIBLE;

        if (sending) {
            // Update came in after we sent the reply, time to reset.
            reset();
        }

        if (isActive() && mWrapper != null) {
            mWrapper.setRemoteInputVisible(true);
        }
    }

    private void reset() {
        mResetting = true;
        mEntry.remoteInputTextWhenReset = SpannedString.valueOf(mEditText.getText());

        mEditText.getText().clear();
        mEditText.setEnabled(true);
        mSendButton.setVisibility(VISIBLE);
        mProgressBar.setVisibility(INVISIBLE);
        mController.removeSpinning(mEntry.key, mToken);
        updateSendButton();
        onDefocus(false /* animate */);

        mResetting = false;
    }

    @Override
    public boolean onRequestSendAccessibilityEvent(View child, AccessibilityEvent event) {
        if (mResetting && child == mEditText) {
            // Suppress text events if it happens during resetting. Ideally this would be
            // suppressed by the text view not being shown, but that doesn't work here because it
            // needs to stay visible for the animation.
            return false;
        }
        return super.onRequestSendAccessibilityEvent(child, event);
    }

    private void updateSendButton() {
        mSendButton.setEnabled(mEditText.getText().length() != 0);
    }

    @Override
    public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

    @Override
    public void onTextChanged(CharSequence s, int start, int before, int count) {}

    @Override
    public void afterTextChanged(Editable s) {
        updateSendButton();
    }

    public void close() {
        mEditText.defocusIfNeeded(false /* animated */);
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        if (ev.getAction() == MotionEvent.ACTION_DOWN) {
            mController.requestDisallowLongPressAndDismiss();
        }
        return super.onInterceptTouchEvent(ev);
    }

    public boolean requestScrollTo() {
        mController.lockScrollTo(mEntry);
        return true;
    }

    public boolean isActive() {
        return mEditText.isFocused() && mEditText.isEnabled();
    }

    public void stealFocusFrom(RemoteInputView other) {
        other.close();
        setPendingIntent(other.mPendingIntent);
        setRemoteInput(other.mRemoteInputs, other.mRemoteInput, mEntry.editedSuggestionInfo);
        setRevealParameters(other.mRevealCx, other.mRevealCy, other.mRevealR);
        focus();
    }

    /**
     * Tries to find an action in {@param actions} that matches the current pending intent
     * of this view and updates its state to that of the found action
     *
     * @return true if a matching action was found, false otherwise
     */
    public boolean updatePendingIntentFromActions(Notification.Action[] actions) {
        if (mPendingIntent == null || actions == null) {
            return false;
        }
        Intent current = mPendingIntent.getIntent();
        if (current == null) {
            return false;
        }

        for (Notification.Action a : actions) {
            RemoteInput[] inputs = a.getRemoteInputs();
            if (a.actionIntent == null || inputs == null) {
                continue;
            }
            Intent candidate = a.actionIntent.getIntent();
            if (!current.filterEquals(candidate)) {
                continue;
            }

            RemoteInput input = null;
            for (RemoteInput i : inputs) {
                if (i.getAllowFreeFormInput()) {
                    input = i;
                }
            }
            if (input == null) {
                continue;
            }
            setPendingIntent(a.actionIntent);
            setRemoteInput(inputs, input, null /* editedSuggestionInfo*/);
            return true;
        }
        return false;
    }

    public PendingIntent getPendingIntent() {
        return mPendingIntent;
    }

    public void setRemoved() {
        mRemoved = true;
    }

    public void setRevealParameters(int cx, int cy, int r) {
        mRevealCx = cx;
        mRevealCy = cy;
        mRevealR = r;
    }

    @Override
    public void dispatchStartTemporaryDetach() {
        super.dispatchStartTemporaryDetach();
        // Detach the EditText temporarily such that it doesn't get onDetachedFromWindow and
        // won't lose IME focus.
        detachViewFromParent(mEditText);
    }

    @Override
    public void dispatchFinishTemporaryDetach() {
        if (isAttachedToWindow()) {
            attachViewToParent(mEditText, 0, mEditText.getLayoutParams());
        } else {
            removeDetachedView(mEditText, false /* animate */);
        }
        super.dispatchFinishTemporaryDetach();
    }

    public void setWrapper(NotificationViewWrapper wrapper) {
        mWrapper = wrapper;
    }

    public void setOnVisibilityChangedListener(Consumer<Boolean> visibilityChangedListener) {
        mOnVisibilityChangedListener = visibilityChangedListener;
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (changedView == this && mOnVisibilityChangedListener != null) {
            mOnVisibilityChangedListener.accept(visibility == VISIBLE);
        }
    }

    public boolean isSending() {
        return getVisibility() == VISIBLE && mController.isSpinning(mEntry.key, mToken);
    }

    /**
     * An EditText that changes appearance based on whether it's focusable and becomes
     * un-focusable whenever the user navigates away from it or it becomes invisible.
     */
    public static class RemoteEditText extends EditText {

        private final Drawable mBackground;
        private RemoteInputView mRemoteInputView;
        boolean mShowImeOnInputConnection;
        private LightBarController mLightBarController;

        public RemoteEditText(Context context, AttributeSet attrs) {
            super(context, attrs);
            mBackground = getBackground();
            mLightBarController = Dependency.get(LightBarController.class);
        }

        private void defocusIfNeeded(boolean animate) {
            if (mRemoteInputView != null && mRemoteInputView.mEntry.getRow().isChangingPosition()
                    || isTemporarilyDetached()) {
                if (isTemporarilyDetached()) {
                    // We might get reattached but then the other one of HUN / expanded might steal
                    // our focus, so we'll need to save our text here.
                    if (mRemoteInputView != null) {
                        mRemoteInputView.mEntry.remoteInputText = getText();
                    }
                }
                return;
            }
            if (isFocusable() && isEnabled()) {
                setInnerFocusable(false);
                if (mRemoteInputView != null) {
                    mRemoteInputView.onDefocus(animate);
                }
                mShowImeOnInputConnection = false;
            }
        }

        @Override
        protected void onVisibilityChanged(View changedView, int visibility) {
            super.onVisibilityChanged(changedView, visibility);

            if (!isShown()) {
                defocusIfNeeded(false /* animate */);
            }
        }

        @Override
        protected void onFocusChanged(boolean focused, int direction, Rect previouslyFocusedRect) {
            super.onFocusChanged(focused, direction, previouslyFocusedRect);
            if (!focused) {
                defocusIfNeeded(true /* animate */);
            }
            if (!mRemoteInputView.mRemoved) {
                mLightBarController.setDirectReplying(focused);
            }
        }

        @Override
        public void getFocusedRect(Rect r) {
            super.getFocusedRect(r);
            r.top = mScrollY;
            r.bottom = mScrollY + (mBottom - mTop);
        }

        @Override
        public boolean requestRectangleOnScreen(Rect rectangle) {
            return mRemoteInputView.requestScrollTo();
        }

        @Override
        public boolean onKeyDown(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                // Eat the DOWN event here to prevent any default behavior.
                return true;
            }
            return super.onKeyDown(keyCode, event);
        }

        @Override
        public boolean onKeyUp(int keyCode, KeyEvent event) {
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                defocusIfNeeded(true /* animate */);
                return true;
            }
            return super.onKeyUp(keyCode, event);
        }

        @Override
        public boolean onKeyPreIme(int keyCode, KeyEvent event) {
            // When BACK key is pressed, this method would be invoked twice.
            if (event.getKeyCode() == KeyEvent.KEYCODE_BACK &&
                    event.getAction() == KeyEvent.ACTION_UP) {
                defocusIfNeeded(true /* animate */);
            }
            return super.onKeyPreIme(keyCode, event);
        }

        @Override
        public boolean onCheckIsTextEditor() {
            // Stop being editable while we're being removed. During removal, we get reattached,
            // and editable views get their spellchecking state re-evaluated which is too costly
            // during the removal animation.
            boolean flyingOut = mRemoteInputView != null && mRemoteInputView.mRemoved;
            return !flyingOut && super.onCheckIsTextEditor();
        }

        @Override
        public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
            final InputConnection inputConnection = super.onCreateInputConnection(outAttrs);

            if (mShowImeOnInputConnection && inputConnection != null) {
                final InputMethodManager imm =
                        getContext().getSystemService(InputMethodManager.class);
                if (imm != null) {
                    // onCreateInputConnection is called by InputMethodManager in the middle of
                    // setting up the connection to the IME; wait with requesting the IME until that
                    // work has completed.
                    post(new Runnable() {
                        @Override
                        public void run() {
                            imm.viewClicked(RemoteEditText.this);
                            imm.showSoftInput(RemoteEditText.this, 0);
                        }
                    });
                }
            }

            return inputConnection;
        }

        @Override
        public void onCommitCompletion(CompletionInfo text) {
            clearComposingText();
            setText(text.getText());
            setSelection(getText().length());
        }

        void setInnerFocusable(boolean focusable) {
            setFocusableInTouchMode(focusable);
            setFocusable(focusable);
            setCursorVisible(focusable);

            if (focusable) {
                requestFocus();
                setBackground(mBackground);
            } else {
                setBackground(null);
            }

        }
    }
}
