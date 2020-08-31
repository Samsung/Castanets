package org.chromium.media;

import android.annotation.SuppressLint;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.projection.MediaProjection;
import android.os.Build;
import android.util.Log;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Arrays;

public class MediaAudioEncoder {
    public interface AudioEncoderListener {
        public void onFrameReady(int bytesRead);
    }

    private static final String TAG = "[WebRTCGameStreaming] MediaAudioEncoder";

    private static final String MIME_TYPE = "audio/mp4a-latm";
    private static final int SAMPLE_RATE = 48000;
    private static final int BIT_RATE = 64000;
    public static final int SAMPLES_PER_FRAME = 1920;
    public static final int FRAMES_PER_BUFFER = 25;

    private AudioThread mAudioThread;

    public MediaProjection mMediaProjection;

    public ScreenCaptureService mService;

    private final AudioEncoderListener mListener;
    private boolean mIsCapturing;

    public MediaAudioEncoder(final ScreenCaptureService service, MediaProjection projection, final AudioEncoderListener listener) {
        if (listener == null) throw new NullPointerException("AudioEncoderListener is null");
        mListener = listener;

        mService = service;
        mMediaProjection = projection;
        mAudioThread = null;
        ScreenCaptureService.sAudioBuffer = ByteBuffer.allocateDirect(SAMPLES_PER_FRAME);
    }

    void prepare() {
        Log.d(TAG, "prepare");
    }

    public void startRecording() {
        mIsCapturing = true;

        Log.d(TAG, "startRecording");
        // create and execute audio capturing thread using internal mic
        if (mAudioThread == null) {
            mAudioThread = new AudioThread();
            mAudioThread.start();
        }
    }

    public void stopRecording() {
        Log.v(TAG, "stopRecording");
        synchronized (ScreenCaptureService.sAudioSync) {
            mIsCapturing = false;
            ScreenCaptureService.sAudioSync.notifyAll();
        }
    }

    public void pauseRecording() {
        Log.v(TAG, "pauseRecording");
    }

    public void resumeRecording() {
        Log.v(TAG, "resumeRecording");
    }

    public void release() {
        Log.d(TAG, "release:");
        mAudioThread = null;
        mIsCapturing = false;
    }

    /**
     * Thread to capture audio data from internal mic as uncompressed 16bit PCM data
     * and write them to the MediaCodec encoder
     */
    private class AudioThread extends Thread {
        @Override
        @SuppressLint("NewApi")
        public void run() {
            Log.d(TAG, "AudioThread run");
            android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO);
            try {
                AudioRecord audioRecord = null;
                AudioPlaybackCaptureConfiguration config = new AudioPlaybackCaptureConfiguration.Builder(mMediaProjection).
                                                               addMatchingUsage(AudioAttributes.USAGE_MEDIA).build();
                audioRecord = new AudioRecord.Builder()
                        .setAudioPlaybackCaptureConfig(config)
                        .setAudioFormat(new AudioFormat.Builder()
                                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                                .setSampleRate(SAMPLE_RATE)
                                .setChannelMask(AudioFormat.CHANNEL_IN_MONO)
                                .build())
                        .build();

                if (audioRecord != null) {
                    try {
                        if (mIsCapturing) {
                            Log.d(TAG, "AudioThread:start audio recording");
                            int readBytes;
                            audioRecord.startRecording();
                            try {
                                while(mIsCapturing) {
                                    synchronized (ScreenCaptureService.sAudioSync) {
                                        // read audio data from internal mic
                                        ScreenCaptureService.sAudioBuffer.clear();
                                        readBytes = audioRecord.read(ScreenCaptureService.sAudioBuffer, SAMPLES_PER_FRAME);
                                        if (readBytes > 0) {
                                            // set audio data to encoder
                                            ScreenCaptureService.sAudioBuffer.position(readBytes);
                                            ScreenCaptureService.sAudioBuffer.flip();
                                            frameReady(readBytes);
                                        }
                                        ScreenCaptureService.sAudioSync.wait();
                                    }
                                }
                            } finally {
                                audioRecord.stop();
                            }
                        }
                    } finally {
                        audioRecord.release();
                    }
                } else {
                    Log.e(TAG, "failed to initialize AudioRecord");
                }
            } catch (final Exception e) {
                Log.e(TAG, "AudioThread#run", e);
            }
            Log.v(TAG, "AudioThread:finished");
        }
    }

    public void frameReady(int bytesRead) {
       if (mListener != null)
           mListener.onFrameReady(bytesRead);
    }
}

