/*
 * Copyright (C) 2007 The Android Open Source Project
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

package org.apache.harmony.dalvik;

import dalvik.annotation.optimization.CriticalNative;
import dalvik.annotation.optimization.FastNative;

/**
 * Methods used to test calling into native code. The methods in this
 * class are all effectively no-ops and may be used to test the mechanisms
 * and performance of calling native methods.
 * @hide
 */
@libcore.api.CorePlatformApi
public final class NativeTestTarget {
    @libcore.api.CorePlatformApi
    public NativeTestTarget() {
    }

    /**
     * This is used to benchmark dalvik's inline natives.
     */
    @libcore.api.CorePlatformApi
    public static void emptyInlineMethod() {
    }

    /**
     * This is used to benchmark dalvik's inline natives.
     */
    @libcore.api.CorePlatformApi
    public static native void emptyInternalStaticMethod();

    // Synchronized methods. Test normal JNI only.
    @libcore.api.CorePlatformApi
    public static native synchronized void emptyJniStaticSynchronizedMethod0();
    @libcore.api.CorePlatformApi
    public native synchronized void emptyJniSynchronizedMethod0();

    // Static methods without object parameters. Test all optimization combinations.

    // Normal native.
    @libcore.api.CorePlatformApi
    public static native void emptyJniStaticMethod0();
    // Normal native.
    @libcore.api.CorePlatformApi
    public static native void emptyJniStaticMethod6(int a, int b, int c, int d, int e, int f);

    @libcore.api.CorePlatformApi
    @FastNative
    public static native void emptyJniStaticMethod0_Fast();
    @libcore.api.CorePlatformApi
    @FastNative
    public static native void emptyJniStaticMethod6_Fast(int a, int b, int c, int d, int e, int f);

    @libcore.api.CorePlatformApi
    @CriticalNative
    public static native void emptyJniStaticMethod0_Critical();
    @libcore.api.CorePlatformApi
    @CriticalNative
    public static native void emptyJniStaticMethod6_Critical(int a, int b, int c, int d, int e, int f);
    // Instance methods or methods with object parameters. Test {Normal, @FastNative} combinations.

    // Normal native.
    @libcore.api.CorePlatformApi
    public native void emptyJniMethod0();
    // Normal native.
    @libcore.api.CorePlatformApi
    public native void emptyJniMethod6(int a, int b, int c, int d, int e, int f);

    /**
     * This is an empty native static method with six args, hooked up
     * using JNI. These have more complex args to show the cost of
     * parsing the signature. All six values should be null
     * references.
     */
    // Normal native.
    @libcore.api.CorePlatformApi
    public static native void emptyJniStaticMethod6L(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);

    // Normal native.
    @libcore.api.CorePlatformApi
    public native void emptyJniMethod6L(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);

    @libcore.api.CorePlatformApi
    @FastNative
    public native void emptyJniMethod0_Fast();
    @libcore.api.CorePlatformApi
    @FastNative
    public native void emptyJniMethod6_Fast(int a, int b, int c, int d, int e, int f);

    /**
     * This is an empty native static method with six args, hooked up
     * using JNI. These have more complex args to show the cost of
     * parsing the signature. All six values should be null
     * references.
     */
    @libcore.api.CorePlatformApi
    @FastNative
    public static native void emptyJniStaticMethod6L_Fast(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);

    @libcore.api.CorePlatformApi
    @FastNative
    public native void emptyJniMethod6L_Fast(String a, String[] b,
        int[][] c, Object d, Object[] e, Object[][][][] f);
}
