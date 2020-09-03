/*
 * Copyright (C) 2014 The Android Open Source Project
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

package android.preference;

import com.android.layoutlib.bridge.android.BridgeContext;
import com.android.layoutlib.bridge.android.BridgeXmlBlockParser;

import android.content.Context;
import android.util.AttributeSet;
import android.view.InflateException;

public class BridgePreferenceInflater extends PreferenceInflater {

    public BridgePreferenceInflater(Context context, PreferenceManager preferenceManager) {
        super(context, preferenceManager);
    }

    @Override
    public Preference createItem(String name, String prefix, AttributeSet attrs)
            throws ClassNotFoundException {
        Object viewKey = null;
        BridgeContext bc = null;

        Context context = getContext();
        if (context instanceof BridgeContext) {
            bc = (BridgeContext) context;
        }

        if (attrs instanceof BridgeXmlBlockParser) {
            viewKey = ((BridgeXmlBlockParser) attrs).getViewCookie();
        }

        Preference preference = null;
        try {
            preference = super.createItem(name, prefix, attrs);
        } catch (ClassNotFoundException | InflateException exception) {
            // name is probably not a valid preference type
            if (("android.support.v7.preference".equals(prefix) ||
                    "androidx.preference".equals(prefix)) &&
                    "SwitchPreferenceCompat".equals(name)) {
                preference = super.createItem("SwitchPreference", prefix, attrs);
            }
        }

        if (viewKey != null && bc != null) {
            bc.addCookie(preference, viewKey);
        }
        return preference;
    }
}
