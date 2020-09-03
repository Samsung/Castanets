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
package android.telephony.ims;

import android.annotation.SystemService;
import android.content.Context;

/**
 * The manager class for RCS related utilities.
 *
 * @hide
 */
@SystemService(Context.TELEPHONY_RCS_SERVICE)
public class RcsManager {
    private final RcsMessageStore mRcsMessageStore;

    /**
     * @hide
     */
    public RcsManager(Context context) {
        mRcsMessageStore = new RcsMessageStore(context);
    }

    /**
     * Returns an instance of {@link RcsMessageStore}
     */
    public RcsMessageStore getRcsMessageStore() {
        return mRcsMessageStore;
    }
}
