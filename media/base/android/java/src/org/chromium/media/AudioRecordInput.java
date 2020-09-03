// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.SuppressLint;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder.AudioSource;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.AudioEffect;
import android.media.audiofx.AudioEffect.Descriptor;
import android.os.Process;

import org.chromium.base.AudioRecordBridge;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.Log;

import java.nio.ByteBuffer;

// Owned by its native counterpart declared in audio_record_input.h. Refer to
// that class for general comments.
@JNINamespace("media")
public class AudioRecordInput extends AudioRecordBridge {
    private static final String TAG = "[Service_Offloading] AudioRecordInput";
    // We are unable to obtain a precise measurement of the hardware delay on
    // Android. This is a conservative lower-bound based on measurments. It
    // could surely be tightened with further testing.
    // TODO(dalecurtis): This should use AudioRecord.getTimestamp() in API 24+.
    private static final int HARDWARE_DELAY_MS = 100;

    public static AudioRecordInput sAudioRecordInput;

    private final long mNativeAudioRecordInputStream;
    private final int mSampleRate;
    private final int mChannels;
    private final int mBitsPerSample;
    private final boolean mUsePlatformAEC;
    private boolean mIsStopped;

    @CalledByNative
    private static AudioRecordInput createAudioRecordInput(long nativeAudioRecordInputStream,
            int sampleRate, int channels, int bitsPerSample, int bytesPerBuffer,
            boolean usePlatformAEC) {
        return new AudioRecordInput(nativeAudioRecordInputStream, sampleRate, channels,
                                    bitsPerSample, bytesPerBuffer, usePlatformAEC);
    }

    private AudioRecordInput(long nativeAudioRecordInputStream, int sampleRate, int channels,
                             int bitsPerSample, int bytesPerBuffer, boolean usePlatformAEC) {
        mNativeAudioRecordInputStream = nativeAudioRecordInputStream;
        mSampleRate = sampleRate;
        mChannels = channels;
        mBitsPerSample = bitsPerSample;
        mUsePlatformAEC = usePlatformAEC;

        // We use a direct buffer so that the native class can have access to
        // the underlying memory address. This avoids the need to copy from a
        // jbyteArray to native memory. More discussion of this here:
        // http://developer.android.com/training/articles/perf-jni.html
        sAudioBuffer = ByteBuffer.allocateDirect(bytesPerBuffer);
        // Rather than passing the ByteBuffer with every OnData call (requiring
        // the potentially expensive GetDirectBufferAddress) we simply have the
        // the native class cache the address to the memory once.
        //
        // Unfortunately, profiling with traceview was unable to either confirm
        // or deny the advantage of this approach, as the values for
        // nativeOnData() were not stable across runs.
        nativeCacheDirectBufferAddress(mNativeAudioRecordInputStream, sAudioBuffer);

        sAudioRecordInput = this;
    }

    @SuppressLint("NewApi")
    @CalledByNative
    private boolean open() {
        Log.d(TAG, "open");
        return true;
    }

    @CalledByNative
    private void start() {
        Log.d(TAG, "start");
        mIsStopped = false;
    }

    @CalledByNative
    private void stop() {
        Log.d(TAG, "stop");
        mIsStopped = true;
    }

    @SuppressLint("NewApi")
    @CalledByNative
    private void close() {
        Log.d(TAG, "close");
    }

    private native void nativeCacheDirectBufferAddress(long nativeAudioRecordInputStream,
                                                       ByteBuffer buffer);
    private native void nativeOnData(
            long nativeAudioRecordInputStream, int size, int hardwareDelayMs);

    @Override
    public void OnData(int bytesRead) {
        if (!mIsStopped)
            nativeOnData(mNativeAudioRecordInputStream, bytesRead, HARDWARE_DELAY_MS);
    }
}
