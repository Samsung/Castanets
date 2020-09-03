/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.systemui.plugins;

import android.annotation.Nullable;
import android.app.PendingIntent;
import android.content.Intent;
import android.view.View;

import com.android.systemui.plugins.annotations.ProvidesInterface;

/**
 * An interface to start activities. This is used as a callback from the views to
 * {@link PhoneStatusBar} to allow custom handling for starting the activity, i.e. dismissing the
 * Keyguard.
 */
@ProvidesInterface(version = ActivityStarter.VERSION)
public interface ActivityStarter {
    int VERSION = 2;

    void startPendingIntentDismissingKeyguard(PendingIntent intent);

    /**
     * Similar to {@link #startPendingIntentDismissingKeyguard(PendingIntent)}, but allows
     * you to specify the callback that is executed on the UI thread after the intent is sent.
     */
    void startPendingIntentDismissingKeyguard(PendingIntent intent,
            Runnable intentSentUiThreadCallback);

    /**
     * Similar to {@link #startPendingIntentDismissingKeyguard(PendingIntent, Runnable)}, but also
     * specifies an associated view that should be used for the activity launch animation.
     */
    void startPendingIntentDismissingKeyguard(PendingIntent intent,
            Runnable intentSentUiThreadCallback, View associatedView);

    /**
     * The intent flag can be specified in startActivity().
     */
    void startActivity(Intent intent, boolean onlyProvisioned, boolean dismissShade, int flags);
    void startActivity(Intent intent, boolean dismissShade);
    void startActivity(Intent intent, boolean onlyProvisioned, boolean dismissShade);
    void startActivity(Intent intent, boolean dismissShade, Callback callback);
    void postStartActivityDismissingKeyguard(Intent intent, int delay);
    void postStartActivityDismissingKeyguard(PendingIntent intent);
    void postQSRunnableDismissingKeyguard(Runnable runnable);

    void dismissKeyguardThenExecute(OnDismissAction action, @Nullable Runnable cancel,
            boolean afterKeyguardGone);

    interface Callback {
        void onActivityStarted(int resultCode);
    }

    interface OnDismissAction {
        /**
         * @return {@code true} if the dismiss should be deferred
         */
        boolean onDismiss();
    }
}
