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

package com.android.systemui.car;

import android.content.Context;

import com.android.systemui.statusbar.notification.NotificationInterruptionStateProvider;
import com.android.systemui.statusbar.notification.collection.NotificationEntry;

/** Auto-specific implementation of {@link NotificationInterruptionStateProvider}. */
public class CarNotificationInterruptionStateProvider extends
        NotificationInterruptionStateProvider {
    public CarNotificationInterruptionStateProvider(Context context) {
        super(context);
    }

    @Override
    public boolean shouldHeadsUp(NotificationEntry entry) {
        // Because space is usually constrained in the auto use-case, there should not be a
        // pinned notification when the shade has been expanded. Ensure this by not pinning any
        // notification if the shade is already opened.
        if (!getPresenter().isPresenterFullyCollapsed()) {
            return false;
        }

        return super.shouldHeadsUp(entry);
    }
}
