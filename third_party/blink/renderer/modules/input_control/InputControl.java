// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.TextView;

import com.samsung.android.knox.EnterpriseDeviceManager;
import com.samsung.android.knox.license.KnoxEnterpriseLicenseManager;
import com.samsung.android.knox.remotecontrol.RemoteInjection;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

@JNINamespace("base::android")
class InputControl {
    private DevicePolicyManager mDPM;
    private ComponentName mDeviceAdmin;
    private static final int DEVICE_ADMIN_ADD_RESULT_ENABLE = 1;

    InputControl() {};

    @CalledByNative
    private static InputControl CreateInputControl() {
        return new InputControl();
    }

    @CalledByNative
    public void SendInput(String type, int x, int y, int code) {
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        RemoteInjection remoteInjection = edm.getRemoteInjection();

        if (type.equals("keydown")) {
            InjectKeyEvent(remoteInjection, code, true);
            return;
        } else if (type.equals("keyup")) {
            InjectKeyEvent(remoteInjection, code, false);
            return;
        }

        int action = -1;
        if (type.equals("mousedown"))
            action = MotionEvent.ACTION_DOWN;
        else if (type.equals("mousemove"))
            action = MotionEvent.ACTION_MOVE;
        else if (type.equals("mouseup"))
            action = MotionEvent.ACTION_UP;

        InjectMouseEvent(remoteInjection, type, action, x, y);
    }

    private void InjectMouseEvent(RemoteInjection remoteInjection, String type,
                                  int action, int x, int y) {
        try {
            boolean result = remoteInjection.injectPointerEvent(
                    MotionEvent.obtain(SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        action, x, y, 0),
                    false);

            Log.i("InputCTRL", "Inject Pointer Event (%s: %d x %d) - %s",
                type, x, y, (result ? "true" : "false"));
        } catch (SecurityException se) {
            Log.i("InputCTRL", "Exception: " + se);
        }
    }

    private void InjectKeyEvent(RemoteInjection remoteInjection, int code,
                                boolean bDown) {
        int action = bDown ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP;
        try {
            boolean result = remoteInjection.injectKeyEvent(
              new KeyEvent(action, code), true);

            Log.i("InputCTRL", "Inject Key Event (code[%d] %s) - %s", code,
                (bDown ? "down" : "up"), (result ? "true" : "false"));
        } catch (SecurityException se) {
            Log.w("InputCTRL", "Exception: " + se);
        }
    }
};
