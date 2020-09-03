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
package com.android.systemui.recents;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Rect;

import com.android.systemui.SysUiServiceProvider;

import java.io.PrintWriter;

interface RecentsImplementation {
    default void onStart(Context context, SysUiServiceProvider sysUiServiceProvider) {}
    default void onBootCompleted() {}
    default void onAppTransitionFinished() {}
    default void onConfigurationChanged(Configuration newConfig) {}

    default void preloadRecentApps() {}
    default void cancelPreloadRecentApps() {}
    default void showRecentApps(boolean triggeredFromAltTab) {}
    default void hideRecentApps(boolean triggeredFromAltTab, boolean triggeredFromHomeKey) {}
    default void toggleRecentApps() {}
    default void growRecents() {}
    default boolean splitPrimaryTask(int stackCreateMode, Rect initialBounds,
            int metricsDockAction) {
        return false;
    }

    default void dump(PrintWriter pw) {}
}
