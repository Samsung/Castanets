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

package com.android.internal.inputmethod;

import android.annotation.AnyThread;
import android.annotation.DrawableRes;
import android.annotation.Nullable;
import android.net.Uri;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodSubtype;

import com.android.internal.annotations.GuardedBy;

/**
 * A utility class to take care of boilerplate code around IPCs.
 */
public final class InputMethodPrivilegedOperations {
    private static final String TAG = "InputMethodPrivilegedOperations";

    private static final class OpsHolder {
        @Nullable
        @GuardedBy("this")
        private IInputMethodPrivilegedOperations mPrivOps;

        /**
         * Sets {@link IInputMethodPrivilegedOperations}.
         *
         * <p>This method can be called only once.</p>
         *
         * @param privOps Binder interface to be set
         */
        @AnyThread
        public synchronized void set(IInputMethodPrivilegedOperations privOps) {
            if (mPrivOps != null) {
                throw new IllegalStateException(
                        "IInputMethodPrivilegedOperations must be set at most once."
                                + " privOps=" + privOps);
            }
            mPrivOps = privOps;
        }

        /**
         * A simplified version of {@link android.os.Debug#getCaller()}.
         *
         * @return method name of the caller.
         */
        @AnyThread
        private static String getCallerMethodName() {
            final StackTraceElement[] callStack = Thread.currentThread().getStackTrace();
            if (callStack.length <= 4) {
                return "<bottom of call stack>";
            }
            return callStack[4].getMethodName();
        }

        @AnyThread
        @Nullable
        public synchronized IInputMethodPrivilegedOperations getAndWarnIfNull() {
            if (mPrivOps == null) {
                Log.e(TAG, getCallerMethodName() + " is ignored."
                        + " Call it within attachToken() and InputMethodService.onDestroy()");
            }
            return mPrivOps;
        }
    }
    private final OpsHolder mOps = new OpsHolder();

    /**
     * Sets {@link IInputMethodPrivilegedOperations}.
     *
     * <p>This method can be called only once.</p>
     *
     * @param privOps Binder interface to be set
     */
    @AnyThread
    public void set(IInputMethodPrivilegedOperations privOps) {
        mOps.set(privOps);
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#setImeWindowStatus(int, int)}.
     *
     * @param vis visibility flags
     * @param backDisposition disposition flags
     * @see android.inputmethodservice.InputMethodService#IME_ACTIVE
     * @see android.inputmethodservice.InputMethodService#IME_VISIBLE
     * @see android.inputmethodservice.InputMethodService#IME_INVISIBLE
     * @see android.inputmethodservice.InputMethodService#BACK_DISPOSITION_DEFAULT
     * @see android.inputmethodservice.InputMethodService#BACK_DISPOSITION_ADJUST_NOTHING
     */
    @AnyThread
    public void setImeWindowStatus(int vis, int backDisposition) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.setImeWindowStatus(vis, backDisposition);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#reportStartInput(IBinder)}.
     *
     * @param startInputToken {@link IBinder} token to distinguish startInput session
     */
    @AnyThread
    public void reportStartInput(IBinder startInputToken) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.reportStartInput(startInputToken);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#createInputContentUriToken(Uri, String)}.
     *
     * @param contentUri Content URI to which a temporary read permission should be granted
     * @param packageName Indicates what package needs to have a temporary read permission
     * @return special Binder token that should be set to
     *         {@link android.view.inputmethod.InputContentInfo#setUriToken(IInputContentUriToken)}
     */
    @AnyThread
    public IInputContentUriToken createInputContentUriToken(Uri contentUri, String packageName) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return null;
        }
        try {
            return ops.createInputContentUriToken(contentUri, packageName);
        } catch (RemoteException e) {
            // For historical reasons, this error was silently ignored.
            // Note that the caller already logs error so we do not need additional Log.e() here.
            // TODO(team): Check if it is safe to rethrow error here.
            return null;
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#reportFullscreenMode(boolean)}.
     *
     * @param fullscreen {@code true} if the IME enters full screen mode
     */
    @AnyThread
    public void reportFullscreenMode(boolean fullscreen) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.reportFullscreenMode(fullscreen);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#updateStatusIcon(String, int)}.
     *
     * @param packageName package name from which the status icon should be loaded
     * @param iconResId resource ID of the icon to be loaded
     */
    @AnyThread
    public void updateStatusIcon(String packageName, @DrawableRes int iconResId) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.updateStatusIcon(packageName, iconResId);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#setInputMethod(String)}.
     *
     * @param id IME ID of the IME to switch to
     * @see android.view.inputmethod.InputMethodInfo#getId()
     */
    @AnyThread
    public void setInputMethod(String id) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.setInputMethod(id);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#setInputMethodAndSubtype(String,
     * InputMethodSubtype)}
     *
     * @param id IME ID of the IME to switch to
     * @param subtype {@link InputMethodSubtype} to switch to
     * @see android.view.inputmethod.InputMethodInfo#getId()
     */
    @AnyThread
    public void setInputMethodAndSubtype(String id, InputMethodSubtype subtype) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.setInputMethodAndSubtype(id, subtype);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#hideMySoftInput(int)}
     *
     * @param flags additional operating flags
     * @see android.view.inputmethod.InputMethodManager#HIDE_IMPLICIT_ONLY
     * @see android.view.inputmethod.InputMethodManager#HIDE_NOT_ALWAYS
     */
    @AnyThread
    public void hideMySoftInput(int flags) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.hideMySoftInput(flags);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#showMySoftInput(int)}
     *
     * @param flags additional operating flags
     * @see android.view.inputmethod.InputMethodManager#SHOW_IMPLICIT
     * @see android.view.inputmethod.InputMethodManager#SHOW_FORCED
     */
    @AnyThread
    public void showMySoftInput(int flags) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.showMySoftInput(flags);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#switchToPreviousInputMethod()}
     *
     * @return {@code true} if handled
     */
    @AnyThread
    public boolean switchToPreviousInputMethod() {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return false;
        }
        try {
            return ops.switchToPreviousInputMethod();
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#switchToNextInputMethod(boolean)}
     *
     * @param onlyCurrentIme {@code true} to switch to a {@link InputMethodSubtype} within the same
     *                       IME
     * @return {@code true} if handled
     */
    @AnyThread
    public boolean switchToNextInputMethod(boolean onlyCurrentIme) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return false;
        }
        try {
            return ops.switchToNextInputMethod(onlyCurrentIme);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#shouldOfferSwitchingToNextInputMethod()}
     *
     * @return {@code true} if the IEM should offer a way to globally switch IME
     */
    @AnyThread
    public boolean shouldOfferSwitchingToNextInputMethod() {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return false;
        }
        try {
            return ops.shouldOfferSwitchingToNextInputMethod();
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#notifyUserAction()}
     */
    @AnyThread
    public void notifyUserAction() {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.notifyUserAction();
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#reportPreRendered(info)}.
     *
     * @param info {@link EditorInfo} of the currently rendered {@link TextView}.
     */
    @AnyThread
    public void reportPreRendered(EditorInfo info) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.reportPreRendered(info);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }

    /**
     * Calls {@link IInputMethodPrivilegedOperations#applyImeVisibility(boolean)}.
     *
     * @param setVisible {@code true} to set IME visible, else hidden.
     */
    @AnyThread
    public void applyImeVisibility(boolean setVisible) {
        final IInputMethodPrivilegedOperations ops = mOps.getAndWarnIfNull();
        if (ops == null) {
            return;
        }
        try {
            ops.applyImeVisibility(setVisible);
        } catch (RemoteException e) {
            throw e.rethrowFromSystemServer();
        }
    }
}
