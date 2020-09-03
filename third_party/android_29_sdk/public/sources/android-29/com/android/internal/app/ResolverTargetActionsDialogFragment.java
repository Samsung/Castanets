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


package com.android.internal.app;

import android.app.AlertDialog.Builder;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;

import com.android.internal.R;

/**
 * Shows a dialog with actions to take on a chooser target
 */
public class ResolverTargetActionsDialogFragment extends DialogFragment
        implements DialogInterface.OnClickListener {
    private static final String NAME_KEY = "componentName";
    private static final String TITLE_KEY = "title";

    // Sync with R.array.resolver_target_actions_* resources
    private static final int APP_INFO_INDEX = 0;

    public ResolverTargetActionsDialogFragment() {
    }

    public ResolverTargetActionsDialogFragment(CharSequence title, ComponentName name) {
        Bundle args = new Bundle();
        args.putCharSequence(TITLE_KEY, title);
        args.putParcelable(NAME_KEY, name);
        setArguments(args);
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        final Bundle args = getArguments();
        return new Builder(getContext())
                .setCancelable(true)
                .setItems(R.array.resolver_target_actions, this)
                .setTitle(args.getCharSequence(TITLE_KEY))
                .create();
    }

    @Override
    public void onClick(DialogInterface dialog, int which) {
        final Bundle args = getArguments();
        ComponentName name = args.getParcelable(NAME_KEY);
        switch (which) {
            case APP_INFO_INDEX:
                Intent in = new Intent().setAction(Settings.ACTION_APPLICATION_DETAILS_SETTINGS)
                        .setData(Uri.fromParts("package", name.getPackageName(), null))
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
                startActivity(in);
                break;
        }
        dismiss();
    }
}
