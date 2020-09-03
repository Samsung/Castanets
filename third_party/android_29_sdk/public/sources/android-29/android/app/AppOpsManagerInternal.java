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

package android.app;

import android.annotation.NonNull;
import android.util.SparseIntArray;

import com.android.internal.util.function.QuadFunction;
import com.android.internal.util.function.TriFunction;

/**
 * App ops service local interface.
 *
 * @hide Only for use within the system server.
 */
public abstract class AppOpsManagerInternal {
    /** Interface to override app ops checks via composition */
    public interface CheckOpsDelegate {
        /**
         * Allows overriding check operation behavior.
         *
         * @param code The op code to check.
         * @param uid The UID for which to check.
         * @param packageName The package for which to check.
         * @param superImpl The super implementation.
         * @param raw Whether to check the raw op i.e. not interpret the mode based on UID state.
         * @return The app op check result.
         */
        int checkOperation(int code, int uid, String packageName, boolean raw,
                QuadFunction<Integer, Integer, String, Boolean, Integer> superImpl);

        /**
         * Allows overriding check audio operation behavior.
         *
         * @param code The op code to check.
         * @param usage The audio op usage.
         * @param uid The UID for which to check.
         * @param packageName The package for which to check.
         * @param superImpl The super implementation.
         * @return The app op check result.
         */
        int checkAudioOperation(int code, int usage, int uid, String packageName,
                QuadFunction<Integer, Integer, Integer, String, Integer> superImpl);

        /**
         * Allows overriding note operation behavior.
         *
         * @param code The op code to note.
         * @param uid The UID for which to note.
         * @param packageName The package for which to note.
         * @param superImpl The super implementation.
         * @return The app op note result.
         */
        int noteOperation(int code, int uid, String packageName,
                TriFunction<Integer, Integer, String, Integer> superImpl);
    }

    /**
     * Set the currently configured device and profile owners.  Specifies the package uid (value)
     * that has been configured for each user (key) that has one.  These will be allowed privileged
     * access to app ops for their user.
     */
    public abstract void setDeviceAndProfileOwners(SparseIntArray owners);

    /**
     * Sets the app-ops mode for a certain app-op and uid.
     *
     * <p>Similar as {@link AppOpsManager#setUidMode} but does not require the package manager to be
     * working. Hence this can be used very early during boot.
     *
     * <p>Only for internal callers. Does <u>not</u> verify that package name belongs to uid.
     *
     * @param code The op code to set.
     * @param uid The UID for which to set.
     * @param mode The new mode to set.
     */
    public abstract void setUidMode(int code, int uid, int mode);

    /**
     * Set all {@link #setMode (package) modes} for this uid to the default value.
     *
     * @param code The app-op
     * @param uid The uid
     */
    public abstract void setAllPkgModesToDefault(int code, int uid);

    /**
     * Get the (raw) mode of an app-op.
     *
     * <p>Does <u>not</u> verify that package belongs to uid. The caller needs to do that.
     *
     * @param code The code of the op
     * @param uid The uid of the package the op belongs to
     * @param packageName The package the op belongs to
     *
     * @return The mode of the op
     */
    public abstract @AppOpsManager.Mode int checkOperationUnchecked(int code, int uid,
            @NonNull String packageName);
}
