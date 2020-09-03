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

package com.android.systemui;

import android.app.PendingIntent;
import android.content.Intent;
import android.view.View;

import com.android.systemui.plugins.ActivityStarter;

import javax.inject.Inject;
import javax.inject.Singleton;

/**
 * Single common instance of ActivityStarter that can be gotten and referenced from anywhere, but
 * delegates to an actual implementation such as StatusBar, assuming it exists.
 */
@Singleton
public class ActivityStarterDelegate implements ActivityStarter {

    private ActivityStarter mActualStarter;

    @Inject
    public ActivityStarterDelegate() {
    }

    @Override
    public void startPendingIntentDismissingKeyguard(PendingIntent intent) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startPendingIntentDismissingKeyguard(intent);
    }

    @Override
    public void startPendingIntentDismissingKeyguard(PendingIntent intent,
            Runnable intentSentCallback) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startPendingIntentDismissingKeyguard(intent, intentSentCallback);
    }

    @Override
    public void startPendingIntentDismissingKeyguard(PendingIntent intent,
            Runnable intentSentCallback, View associatedView) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startPendingIntentDismissingKeyguard(intent, intentSentCallback,
                associatedView);
    }

    @Override
    public void startActivity(Intent intent, boolean onlyProvisioned, boolean dismissShade,
            int flags) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startActivity(intent, onlyProvisioned, dismissShade, flags);
    }

    @Override
    public void startActivity(Intent intent, boolean dismissShade) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startActivity(intent, dismissShade);
    }

    @Override
    public void startActivity(Intent intent, boolean onlyProvisioned, boolean dismissShade) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startActivity(intent, onlyProvisioned, dismissShade);
    }

    @Override
    public void startActivity(Intent intent, boolean dismissShade, Callback callback) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.startActivity(intent, dismissShade, callback);
    }

    @Override
    public void postStartActivityDismissingKeyguard(Intent intent, int delay) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.postStartActivityDismissingKeyguard(intent, delay);
    }

    @Override
    public void postStartActivityDismissingKeyguard(PendingIntent intent) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.postStartActivityDismissingKeyguard(intent);
    }

    @Override
    public void postQSRunnableDismissingKeyguard(Runnable runnable) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.postQSRunnableDismissingKeyguard(runnable);
    }

    @Override
    public void dismissKeyguardThenExecute(OnDismissAction action, Runnable cancel,
            boolean afterKeyguardGone) {
        if (mActualStarter == null) {
            return;
        }
        mActualStarter.dismissKeyguardThenExecute(action, cancel, afterKeyguardGone);
    }

    public void setActivityStarterImpl(ActivityStarter starter) {
        mActualStarter = starter;
    }
}
