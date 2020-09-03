// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentManager;
import android.app.FragmentTransaction;
import android.app.Service;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.annotation.IntDef;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.AudioRecordBridge;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.media.ImageWrapper;
import org.chromium.media.ScreenCaptureService;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.Arrays;

/**
 * This class implements Screen Capture using projection API, introduced in Android
 * API 21 (L Release). Capture takes place in the current Looper, while pixel
 * download takes place in another thread used by ImageReader.
 **/
@JNINamespace("media")
@TargetApi(Build.VERSION_CODES.LOLLIPOP)
public class ScreenCapture extends Fragment {
    private static final String TAG = "[Service_Offloading] ScreenCapture";

    private static final int REQUEST_MEDIA_PROJECTION = 1;

    @IntDef({CaptureState.ATTACHED, CaptureState.ALLOWED, CaptureState.STARTED,
            CaptureState.STOPPING, CaptureState.STOPPED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface CaptureState {
        int ATTACHED = 0;
        int ALLOWED = 1;
        int STARTED = 2;
        int STOPPING = 3;
        int STOPPED = 4;
    }

    @IntDef({DeviceOrientation.PORTRAIT, DeviceOrientation.LANDSCAPE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DeviceOrientation {
        int PORTRAIT = 0;
        int LANDSCAPE = 1;
    }

    // Native callback context variable.
    private final long mNativeScreenCaptureMachineAndroid;

    private final Object mCaptureStateLock = new Object();
    private @CaptureState int mCaptureState = CaptureState.STOPPED;

    private MediaProjection mMediaProjection;
    private MediaProjectionManager mMediaProjectionManager;
    private Intent mResultData;

    private int mScreenDensity;
    private int mWidth;
    private int mHeight;
    private int mResultCode;
    private boolean mIsStopped;

    private MyBroadcastReceiver mReceiver;

    ScreenCapture(long nativeScreenCaptureMachineAndroid) {
        mNativeScreenCaptureMachineAndroid = nativeScreenCaptureMachineAndroid;
        if (mReceiver == null) {
            mReceiver = new MyBroadcastReceiver(this);
        }
        final IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(ScreenCaptureService.ACTION_QUERY_STATUS_RESULT);
        intentFilter.addAction(ScreenCaptureService.ACTION_IMAGE_RESULT);
        intentFilter.addAction(ScreenCaptureService.ACTION_AUDIO_RESULT);
        ApplicationStatus.getLastTrackedFocusedActivity().registerReceiver(mReceiver, intentFilter);
    }

    // Factory method.
    @CalledByNative
    static ScreenCapture createScreenCaptureMachine(long nativeScreenCaptureMachineAndroid) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            return new ScreenCapture(nativeScreenCaptureMachineAndroid);
        }
        return null;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    // This method was deprecated in API level 23 by onAttach(Context).
    // TODO(braveyao): remove this method after the minSdkVersion of chrome is 23,
    // https://crbug.com/614172.
    @SuppressWarnings("deprecation")
    @Override
    public void onAttach(Activity activity) {
        super.onAttach(activity);
        Log.d(TAG, "onAttach");
        changeCaptureStateAndNotify(CaptureState.ATTACHED);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        Log.d(TAG, "onDetach");
        stopCapture();
    }

    @CalledByNative
    public boolean allocate(int width, int height) {
        Log.d(TAG, "allocate width: " + width + " height: " + height);

        mMediaProjectionManager =
                (MediaProjectionManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.MEDIA_PROJECTION_SERVICE);
        if (mMediaProjectionManager == null) {
            Log.e(TAG, "mMediaProjectionManager is null");
            return false;
        }
        return true;
    }

    @CalledByNative
    public boolean startPrompt() {
        Log.d(TAG, "startPrompt");
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity == null) {
            Log.e(TAG, "activity is null");
            return false;
        }
        FragmentManager fragmentManager = activity.getFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        fragmentTransaction.add(this, "screencapture");
        try {
            fragmentTransaction.commit();
        } catch (RuntimeException e) {
            Log.e(TAG, "ScreenCaptureExcaption " + e);
            return false;
        }

        synchronized (mCaptureStateLock) {
            while (mCaptureState != CaptureState.ATTACHED) {
                try {
                    mCaptureStateLock.wait();
                } catch (InterruptedException ex) {
                    Log.e(TAG, "ScreenCaptureException: " + ex);
                }
            }
        }

        try {
            startActivityForResult(
                    mMediaProjectionManager.createScreenCaptureIntent(), REQUEST_MEDIA_PROJECTION);
        } catch (android.content.ActivityNotFoundException e) {
            Log.e(TAG, "ScreenCaptureException " + e);
            return false;
        }
        return true;
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "onActivityResult requestCode:" + requestCode + " resultCode:" + resultCode);
        if (requestCode != REQUEST_MEDIA_PROJECTION) return;

        if (resultCode == Activity.RESULT_OK) {
			Log.d(TAG, "onActivityResult result is OK");
            mResultCode = resultCode;
            mResultData = data;
            changeCaptureStateAndNotify(CaptureState.ALLOWED);
        }

        nativeOnActivityResult(
                mNativeScreenCaptureMachineAndroid, resultCode == Activity.RESULT_OK);

    }

    private void startScreenRecorder(final int resultCode, final Intent data) {
        Log.d(TAG, "startScreenRecorder");
        mIsStopped = false;
        final Intent intent = new Intent(ApplicationStatus.getLastTrackedFocusedActivity(), ScreenCaptureService.class);
        intent.setAction(ScreenCaptureService.ACTION_START);
        intent.putExtra(ScreenCaptureService.EXTRA_RESULT_CODE, resultCode);
        intent.putExtras(data);
        ApplicationStatus.getLastTrackedFocusedActivity().startService(intent);
    }

    private void stopScreenRecorder() {
        Log.d(TAG, "stopScreenRecorder");
        mIsStopped = true;
        final Intent intent = new Intent(ApplicationStatus.getLastTrackedFocusedActivity(), ScreenCaptureService.class);
        intent.setAction(ScreenCaptureService.ACTION_STOP);
        ApplicationStatus.getLastTrackedFocusedActivity().startService(intent);
    }

    @CalledByNative
    public boolean startCapture() {
        Log.d(TAG, "startCapture");
        startScreenRecorder(mResultCode, mResultData);
        return true;
    }

    @CalledByNative
    public void stopCapture() {
        Log.d(TAG, "stopCapture");
        stopScreenRecorder();
    }

    private void changeCaptureStateAndNotify(@CaptureState int state) {
        Log.d(TAG, "changeCaptureStateAndNotify");
        synchronized (mCaptureStateLock) {
            mCaptureState = state;
            mCaptureStateLock.notifyAll();
        }
    }

    // Method for ScreenCapture implementations to call back native code.
    private native void nativeOnRGBAFrameAvailable(long nativeScreenCaptureMachineAndroid,
            ByteBuffer buf, int rowStride, int left, int top, int width, int height,
            long timestamp);

    private native void nativeOnI420FrameAvailable(long nativeScreenCaptureMachineAndroid,
            ByteBuffer yBuffer, int yStride, ByteBuffer uBuffer, ByteBuffer vBuffer,
            int uvRowStride, int uvPixelStride, int left, int top, int width, int height,
            long timestamp);

    // Method for ScreenCapture implementations to notify activity result.
    private native void nativeOnActivityResult(
            long nativeScreenCaptureMachineAndroid, boolean result);

    // Method for ScreenCapture implementations to notify orientation change.
    private native void nativeOnOrientationChange(
            long nativeScreenCaptureMachineAndroid, int rotation);

    private void handleImage(ImageWrapper imageWrapper) {
        if (mIsStopped == true)
            return;

        switch (imageWrapper.format) {
            case PixelFormat.RGBA_8888:
                nativeOnRGBAFrameAvailable(mNativeScreenCaptureMachineAndroid,
                        ScreenCaptureService.sVideoBuffer[0],
                        imageWrapper.rowStride[0], imageWrapper.rectLeft,
                        imageWrapper.rectTop, imageWrapper.rectWidth,
                        imageWrapper.rectHeight, imageWrapper.timestamp);
                break;
            case ImageFormat.YUV_420_888:
                // The pixel stride of Y plane is always 1. The U/V planes are guaranteed
                // to have the same row stride and pixel stride.
                nativeOnI420FrameAvailable(mNativeScreenCaptureMachineAndroid,
                        ScreenCaptureService.sVideoBuffer[0],
                        imageWrapper.rowStride[0],
                        ScreenCaptureService.sVideoBuffer[1],
                        ScreenCaptureService.sVideoBuffer[2],
                        imageWrapper.rowStride[1],
                        imageWrapper.pixelStride,
                        imageWrapper.rectLeft,
                        imageWrapper.rectTop, imageWrapper.rectWidth,
                        imageWrapper.rectHeight, imageWrapper.timestamp);
                break;
            default:
                Log.e(TAG, "Unexpected image format: " + imageWrapper.format);
                throw new IllegalStateException();
        }
    }

    private void handleAudio(int bytesRead) {
        if (mIsStopped == true)
            return;
        if (AudioRecordBridge.getAudioRecord() != null) {

            byte[] temp = new byte[ScreenCaptureService.sAudioBuffer.remaining()];
            synchronized (ScreenCaptureService.sAudioSync) {
                ScreenCaptureService.sAudioBuffer.position(0);
                ScreenCaptureService.sAudioBuffer.get(temp);

                ScreenCaptureService.sAudioSync.notifyAll();
            }

            AudioRecordBridge.getAudioRecord().sAudioBuffer.clear();
            AudioRecordBridge.getAudioRecord().sAudioBuffer.put(temp);
            AudioRecordBridge.getAudioRecord().OnData(bytesRead);
        }

    }

    private static final class MyBroadcastReceiver extends BroadcastReceiver {
        String TAG = "[NSW] MyBroadcastReceiver";
        private final WeakReference<ScreenCapture> mWeakParent;
        public MyBroadcastReceiver(final ScreenCapture parent) {
            mWeakParent = new WeakReference<ScreenCapture>(parent);
        }

        @Override
        public void onReceive(final Context context, final Intent intent) {
            final String action = intent.getAction();
            if (ScreenCaptureService.ACTION_QUERY_STATUS_RESULT.equals(action)) {
            } else if (ScreenCaptureService.ACTION_IMAGE_RESULT.equals(action)) {
                final ScreenCapture parent = mWeakParent.get();
                ImageWrapper imageWrapper = (ImageWrapper) intent.getSerializableExtra(ScreenCaptureService.EXTRA_RESULT_IMAGE);
                parent.handleImage(imageWrapper);
            } else if (ScreenCaptureService.ACTION_AUDIO_RESULT.equals(action)) {
                final ScreenCapture parent = mWeakParent.get();
                int bytesRead = intent.getIntExtra("BytesRead", 0);
                parent.handleAudio(bytesRead);
            }
        }
    }
}
