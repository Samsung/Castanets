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

package com.android.server.biometrics;

import android.content.Context;
import android.os.Build;
import android.provider.Settings;

public class Utils {
    public static boolean isDebugEnabled(Context context, int targetUserId) {
        if (!(Build.IS_ENG || Build.IS_USERDEBUG)) {
            return false;
        }

        if (Settings.Secure.getIntForUser(context.getContentResolver(),
                Settings.Secure.BIOMETRIC_DEBUG_ENABLED, 0,
                targetUserId) == 0) {
            return false;
        }
        return true;
    }
}
