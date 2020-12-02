// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.MotionEvent.PointerProperties;
import android.widget.TextView;

import com.samsung.android.knox.EnterpriseDeviceManager;
import com.samsung.android.knox.license.KnoxEnterpriseLicenseManager;
import com.samsung.android.knox.remotecontrol.RemoteInjection;
import com.samsung.android.knox.application.ApplicationPolicy;

import java.math.BigDecimal;
import java.util.Iterator;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.chrome.browser.MotionEventBuilder;

import org.json.JSONObject;

@JNINamespace("base::android")
class InputControl {
    private static final String TAG = "InputCTRL";
    private static final int DEVICE_ADMIN_ADD_RESULT_ENABLE = 1;

    private DevicePolicyManager mDPM;
    private ComponentName mDeviceAdmin;
    private boolean mIsShiftLeftOn;
    private boolean mIsShiftRightOn;
    private boolean mIsAltLeftOn;
    private boolean mIsAltRightOn;
    private boolean mIsCtrlLeftOn;
    private boolean mIsCtrlRightOn;

    private int mNumTouch;
    private MotionEventBuilder mEventBuilder;

    InputControl() {
        mIsShiftLeftOn = false;
        mIsShiftRightOn = false;
        mIsAltLeftOn = false;
        mIsAltRightOn = false;
        mIsCtrlLeftOn = false;
        mIsCtrlRightOn = false;
        mNumTouch = 0;
        mEventBuilder = MotionEventBuilder.newBuilder();
    };

    @CalledByNative
    private static InputControl CreateInputControl() {
        return new InputControl();
    }

    @CalledByNative
    public void SendMouseInput(String type, int x, int y) {
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        RemoteInjection remoteInjection = edm.getRemoteInjection();
        int action = -1;

        if(type.equals("mousedown"))
            action = MotionEvent.ACTION_DOWN;
        else if(type.equals("mousemove"))
            action = MotionEvent.ACTION_MOVE;
        else if(type.equals("mouseup"))
            action = MotionEvent.ACTION_UP;

        InjectMouseEvent(remoteInjection, type, action, x, y);
    }

    @CalledByNative
    public void SendTouchInput(String type, String json) {
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        RemoteInjection remoteInjection = edm.getRemoteInjection();
        int action = -1;

        if (type.equals("touchstart")) {
            if(mNumTouch == 0 /* First Touch */ ) {
                action = MotionEvent.ACTION_DOWN;
            } else {
                action = MotionEvent.ACTION_POINTER_DOWN;
            }
            mNumTouch++;
        } else if (type.equals("touchend")) {
            if (mNumTouch == 1 /* Last Touch */){
                action = MotionEvent.ACTION_UP;
            } else {
                action = MotionEvent.ACTION_POINTER_UP;
            }
            mNumTouch--;
        } else if (type.equals("touchmove")) {
            action = MotionEvent.ACTION_MOVE;
        }

        InjectTouchEvent(remoteInjection, json, action);
    }

    @CalledByNative
    public void SendKeyboardInput(String type, int code) {
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        RemoteInjection remoteInjection = edm.getRemoteInjection();

        if (type.equals("keydown")) {
            switch(code){
                case 57: //AltLeft
                    mIsAltLeftOn = true;
                break;
                case 58: //AltRight
                    mIsAltRightOn = true;
                break;
                case 59: // ShiftLeft
                    mIsShiftLeftOn = true;
                break;
                case 60: // ShiftRight
                    mIsShiftRightOn = true;
                break;
                case 113: //CtrlLeft
                    mIsCtrlLeftOn = true;
                break;
                case 114:
                    mIsCtrlRightOn = true;
                break;
            }
            InjectKeyEvent(remoteInjection, code, true);
            return;
        } else if (type.equals("keyup")) {
            switch(code){
                case 57: //AltLeft
                    mIsAltLeftOn = false;
                break;
                case 58: //AltRight
                    mIsAltRightOn = false;
                break;
                case 59: // ShiftLeft
                    mIsShiftLeftOn = false;
                break;
                case 60: // ShiftRight
                    mIsShiftRightOn = false;
                break;
                case 113: //CtrlLeft
                    mIsCtrlLeftOn = false;
                break;
                case 114:
                    mIsCtrlRightOn = false;
                break;
            }

            InjectKeyEvent(remoteInjection, code, false);
            return;
        }
    }

    private void InjectTouchEvent(RemoteInjection remoteInjection, String data, int action){
       try{
            long eventTime = SystemClock.uptimeMillis();
            JSONObject jsonObj = new JSONObject(data);
            Iterator<String> itrTouchInfo = jsonObj.keys();
            int idx = 0;

            while (itrTouchInfo.hasNext()) {
                Float x = 0.0f;
                Float y = 0.0f;
                int motionAction = 0;
                String strFingerIdx = itrTouchInfo.next();
                String strFingerCoord = jsonObj.getString(strFingerIdx);
                JSONObject jsonFingerCoord = new JSONObject(strFingerCoord);

                x = BigDecimal.valueOf(jsonFingerCoord.getDouble("x")).floatValue();
                y = BigDecimal.valueOf(jsonFingerCoord.getDouble("y")).floatValue();
                idx = Integer.parseInt(strFingerIdx);
                mEventBuilder.setAction(action)
                            .setActionIndex(idx)
                            .setDownTime(eventTime)
                            .setEventTime(eventTime)
                            .setPointer(x, y, idx);
            }
            boolean result = false;
            result = remoteInjection.injectPointerEvent(mEventBuilder.build(), false);
            if(action == MotionEvent.ACTION_POINTER_UP || action == MotionEvent.ACTION_UP){
                mEventBuilder.removePointer(idx);
            }
        } catch(Exception e){
            Log.w(TAG, "Error: " + e);
        }
    }

    private void InjectMouseEvent(RemoteInjection remoteInjection, String type,
                                  int action, int x, int y) {
        try {
            boolean result = remoteInjection.injectPointerEvent(
                    MotionEvent.obtain(SystemClock.uptimeMillis(),
                        SystemClock.uptimeMillis(),
                        action, x, y, 0),
                    false);

            Log.i(TAG, "Inject Pointer Event (%s: %d x %d) - %s",
                type, x, y, (result ? "true" : "false"));
        } catch (SecurityException se) {
            Log.e(TAG, "Exception: " + se);
        }
    }

    private void InjectKeyEvent(RemoteInjection remoteInjection, int code,
                                boolean bDown) {
        int action = bDown ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP;
        long eventTime = SystemClock.uptimeMillis();
        boolean result;
        try {
            if(mIsShiftLeftOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_SHIFT_LEFT_ON)), true);
            }else if(mIsShiftRightOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_SHIFT_RIGHT_ON)), true);
            }else if(mIsAltLeftOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_ALT_LEFT_ON)), true);
            }else if(mIsAltRightOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_ALT_RIGHT_ON)), true);
            }else if(mIsCtrlLeftOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_CTRL_LEFT_ON)), true);
            }else if(mIsCtrlRightOn){
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(eventTime, eventTime, action, code, 0, KeyEvent.normalizeMetaState(KeyEvent.META_CTRL_RIGHT_ON)), true);
            }else {
                result = remoteInjection.injectKeyEvent(
                    new KeyEvent(action, code), true);
            }

            Log.i(TAG, "Inject Key Event (code[%d] %s) - %s", code,
                (bDown ? "down" : "up"), (result ? "true" : "false"));
        } catch (SecurityException se) {
            Log.e(TAG, "Exception: " + se);
        }
    }

    @CalledByNative
    public String GetIPAddr() {
        Context ctx = ContextUtils.getApplicationContext();
        WifiManager wm = (WifiManager) ctx.getSystemService(Context.WIFI_SERVICE);
        WifiInfo wifiInfo = wm.getConnectionInfo();
        int ip = wifiInfo.getIpAddress();
        if(ip == 0){
            ip = 0x0100007f;
        }
        return String.format("%d.%d.%d.%d", (ip & 0xff), (ip >> 8 & 0xff), (ip >> 16 & 0xff), (ip >> 24 & 0xff));
    }

    @CalledByNative
    public void StopApplication(String pkgName) {
        Log.i(TAG, "StopApplication package: " + pkgName);
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        ApplicationPolicy appPolicy = edm.getApplicationPolicy();
        try {
            boolean result = appPolicy.stopApp(pkgName);
        } catch (SecurityException se) {
            Log.e(TAG, "SecurityException: " + se);
        }
    }

    @CalledByNative
    public void StartApplication(String pkgName) {
        Log.i(TAG, "StartApplication package: " + pkgName);
        EnterpriseDeviceManager edm = EnterpriseDeviceManager.getInstance(
          ContextUtils.getApplicationContext());
        ApplicationPolicy appPolicy = edm.getApplicationPolicy();
        try {
            boolean result = appPolicy.startApp(pkgName, null);
        } catch (SecurityException se) {
            Log.e(TAG, "SecurityException: " + se);
        }
    }

};
