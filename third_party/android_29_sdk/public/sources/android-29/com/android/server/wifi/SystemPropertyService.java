/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.wifi;

/** PropertyService implementation which uses the real Android property store */
class SystemPropertyService implements PropertyService {
    @Override
    public String get(String key, String defaultValue) {
        return android.os.SystemProperties.get(key, defaultValue);
    }

    @Override
    public void set(String key, String val) {
        android.os.SystemProperties.set(key, val);
    }

    @Override
    public boolean getBoolean(String key, boolean defaultValue) {
        return android.os.SystemProperties.getBoolean(key, defaultValue);
    }

    @Override
    public String getString(String key, String defaultValue) {
        return android.os.SystemProperties.get(key, defaultValue);
    }
}
