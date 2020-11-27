package org.chromium.media;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Context;
import android.content.Intent;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.Image.Plane;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.IBinder;
import android.support.annotation.IntDef;
import android.text.TextUtils;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;
import android.widget.TextView;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.io.IOException;
import java.io.Serializable;
import java.nio.ByteBuffer;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.capture.R;
import org.chromium.media.ImageWrapper;

public class ScreenCaptureService extends Service {
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

    private final Object mCaptureStateLock = new Object();
    private @CaptureState int mCaptureState = CaptureState.STOPPED;

    private static final String BASE = "WebRTCGamingService.ScreenCaptureService.";
    public static final String ACTION_START = BASE + "ACTION_START";
    public static final String ACTION_STOP = BASE + "ACTION_STOP";
    public static final String ACTION_PAUSE = BASE + "ACTION_PAUSE";
    public static final String ACTION_RESUME = BASE + "ACTION_RESUME";
    public static final String ACTION_QUERY_STATUS = BASE + "ACTION_QUERY_STATUS";
    public static final String ACTION_QUERY_STATUS_RESULT = BASE + "ACTION_QUERY_STATUS_RESULT";
    public static final String ACTION_AUDIO_RESULT = BASE + "ACTION_AUDIO_RESULT";
    public static final String ACTION_IMAGE_RESULT = BASE + "ACTION_IMAGE_RESULT";
    public static final String ACTION_ROTATE_RESULT = BASE + "ACTION_ROTATE_RESULT";
    public static final String EXTRA_QUERY_RESULT_RECORDING = BASE + "EXTRA_QUERY_RESULT_RECORDING";
    public static final String EXTRA_QUERY_RESULT_PAUSING = BASE + "EXTRA_QUERY_RESULT_PAUSING";
    public static final String EXTRA_RESULT_CODE = BASE + "EXTRA_RESULT_CODE";
    public static final String EXTRA_RESULT_AUDIO_BYTES = BASE + "EXTRA_RESULT_AUDIO_BYTES";
    public static final String EXTRA_RESULT_IMAGE = BASE + "EXTRA_RESULT_IMAGE";
    public static final String EXTRA_RESULT_BUFFER = BASE + "EXTRA_RESULT_BUFFER";
    public static final String EXTRA_RESULT_ROTATION = BASE + "EXTRA_RESULT_ROTATION";
    private static final int NOTIFICATION = R.string.app_name;
    private static final String APP_NAME = "Service Offloading";

    public static final Object sAudioSync = new Object();

    public static final String TAG = "[Service_Offloading] ScreenCaptureService";
    private MediaProjectionManager mMediaProjectionManager;
    private MediaProjection mMediaProjection;
    private NotificationManager mNotificationManager;

    private int mScreenDensity;
    private int mWidth;
    private int mHeight;
    private int mFormat;
    private int mResultCode;
    private Intent mResultData;
    private VirtualDisplay mVirtualDisplay;
    private Surface mSurface;
    private ImageReader mImageReader;
    private HandlerThread mThread;
    private Handler mBackgroundHandler;
    private Display mDisplay;
    private @DeviceOrientation int mCurrentOrientation;

    public static ByteBuffer[] sVideoBuffer;
    public MediaProjectionCallback mCallback;
    public AudioEncoderListener mAudioEncoderListener;

    private MediaAudioEncoder mAudioEncoder;
    public static ByteBuffer sAudioBuffer;

    // To make a transparent view to keep screen on.
    private TextView mTextView;

    public ScreenCaptureService() {
        Log.d(TAG, "Service");
        WindowManager windowManager =
                (WindowManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.WINDOW_SERVICE);
        mDisplay = windowManager.getDefaultDisplay();
        DisplayMetrics metrics = new DisplayMetrics();
        mDisplay.getMetrics(metrics);
        mScreenDensity = metrics.densityDpi;
        mCurrentOrientation = -1;
    }

    private class CrImageReaderListener implements ImageReader.OnImageAvailableListener {
        @Override
        public void onImageAvailable(ImageReader reader) {
            synchronized (mCaptureStateLock) {
                if (mCaptureState != CaptureState.STARTED) {
                    Log.e(TAG, "Get captured frame in unexpected state.");
                    return;
                }
            }

            // If device is rotated, inform native, then re-create ImageReader and VirtualDisplay
            // with the new orientation, and drop the current frame.
            if (maybeDoRotation()) {
                createImageReaderWithFormat();
                createVirtualDisplay();
                return;
            }

            try (Image image = reader.acquireLatestImage()) {
                if (image == null) return;
                if (reader.getWidth() != image.getWidth()
                        || reader.getHeight() != image.getHeight()) {
                    Log.e(TAG, "ImageReader size (" + reader.getWidth() + "x" + reader.getHeight()
                                    + ") did not match Image size (" + image.getWidth() + "x"
                                    + image.getHeight() + ")");
                    throw new IllegalStateException();
                }

                final Intent result = new Intent();
                result.setAction(ACTION_IMAGE_RESULT);
                result.putExtra(EXTRA_RESULT_IMAGE, new ImageWrapper(image));
                sendBroadcast(result);
            } catch (IllegalStateException ex) {
                Log.e(TAG, "acquireLatestImage():" + ex);
            } catch (UnsupportedOperationException ex) {
                Log.e(TAG, "acquireLatestImage():" + ex);
                if (mFormat == ImageFormat.YUV_420_888) {
                    // YUV_420_888 is the preference, but not all devices support it,
                    // fall-back to RGBA_8888 then.
                    mFormat = PixelFormat.RGBA_8888;
                    createImageReaderWithFormat();
                    createVirtualDisplay();
                }
            }
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        // TODO: Return the communication channel to the service.
        throw new UnsupportedOperationException("Not yet implemented");
    }

    @Override
    public void onCreate() {
        Log.d(TAG, "Service OnCreate");
        super.onCreate();
        mMediaProjectionManager = (MediaProjectionManager)getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        WindowManager windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        mTextView = new TextView(this);

        WindowManager.LayoutParams myParam = new WindowManager.LayoutParams(0, 0, // Hide the view
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE |
                WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON, PixelFormat.TRANSLUCENT);
        windowManager.addView(mTextView, myParam);

        mNotificationManager = (NotificationManager)getSystemService(NOTIFICATION_SERVICE);
        showNotification(TAG);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "Service onDestroy");

        if (mTextView != null) {
            ((WindowManager) getSystemService(WINDOW_SERVICE)).removeView(mTextView);
            mTextView = null;
        }
    }


    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(TAG, "onStartCommand:intent=" + intent);

        int result = START_STICKY;
        final String action = intent != null ? intent.getAction() : null;
        if (ACTION_START.equals(action)) {
            startScreenRecord(intent);
        } else if (ACTION_STOP.equals(action) || TextUtils.isEmpty(action)) {
            stopScreenRecord();
            result = START_NOT_STICKY;
        } else if (ACTION_QUERY_STATUS.equals(action)) {
            stopSelf();
            result = START_NOT_STICKY;
        } else if (ACTION_PAUSE.equals(action)) {
            pauseScreenRecord();
        } else if (ACTION_RESUME.equals(action)) {
            resumeScreenRecord();
        }
        return result;
    }

    private boolean startScreenRecord(final Intent intent) {
        Log.d(TAG, "startScreenRecord");

        // For video capture
        mResultCode = intent.getExtras().getInt(EXTRA_RESULT_CODE);
        mResultData = intent;
        mWidth = intent.getExtras().getInt("width");
        mHeight = intent.getExtras().getInt("height");
        // Fix the resolution to FHD.
        if (mWidth > mHeight) {
            mWidth = 1920;
            mHeight = 1088;
        } else {
            mWidth = 1088;
            mHeight = 1920;
        }

        synchronized (mCaptureStateLock) {
            if (mCaptureState != CaptureState.ALLOWED) {
                Log.e(TAG, "startCapture() invoked without user permission.");
                // return false;
            }
        }
        if (mMediaProjectionManager == null) {
            Log.e(TAG, "mMediaProjectionManager is null");
            return false;
        }
        try {
            mMediaProjection = mMediaProjectionManager.getMediaProjection(mResultCode, mResultData);
        } catch (IllegalStateException e) {
            Log.e(TAG, "getMediaProjection " + e);
            return false;
        }
        if (mMediaProjection == null) {
            Log.e(TAG, "mMediaProjection is null");
            return false;
        }
        mCallback = new MediaProjectionCallback();
        mMediaProjection.registerCallback(mCallback, null);

        mThread = new HandlerThread("ScreenCapture");
        mThread.start();
        mBackgroundHandler = new Handler(mThread.getLooper());

        // YUV420 is preferred. But not all devices supports it and it even will
        // crash some devices. See https://crbug.com/674989 . A feature request
        // was already filed to support YUV420 in VirturalDisplay. Before YUV420
        // is available, stay with RGBA_8888 at present.
        mFormat = PixelFormat.RGBA_8888;

        if (mFormat == PixelFormat.RGBA_8888) {
            int bufferSize = mWidth * mHeight * 4;
            sVideoBuffer = new ByteBuffer[1];
            sVideoBuffer[0] = ByteBuffer.allocateDirect(bufferSize);
        } else if (mFormat == ImageFormat.YUV_420_888) {
            int bufferSize = mWidth * mHeight * 3;
            sVideoBuffer = new ByteBuffer[3];
            for (int i = 0; i < 3; i++) {
                sVideoBuffer[i] = ByteBuffer.allocateDirect(bufferSize);
            }
        }

        maybeDoRotation();
        createImageReaderWithFormat();
        createVirtualDisplay();

        changeCaptureStateAndNotify(CaptureState.STARTED);

        // For audio capture
        if (mAudioEncoder != null)
            return false;

        mAudioEncoderListener = new AudioEncoderListener();
        mAudioEncoder = new MediaAudioEncoder(this, mMediaProjection, mAudioEncoderListener);
        mAudioEncoder.prepare();
        mAudioEncoder.startRecording();

        return true;
    }

    @SuppressLint("NewApi")
    private class MediaProjectionCallback extends MediaProjection.Callback {
        @Override
        public void onStop() {
            super.onStop();
            changeCaptureStateAndNotify(CaptureState.STOPPED);
            mMediaProjection = null;
            if (mVirtualDisplay == null) return;
            mVirtualDisplay.release();
            mVirtualDisplay = null;
        }
    }

    private void createImageReaderWithFormat() {
        Log.d(TAG, "createImageReaderWithFormat");
        if (mImageReader != null) {
            mImageReader.close();
        }

        final int maxImages = 2;
        mImageReader = ImageReader.newInstance(mWidth, mHeight, mFormat, maxImages);
        mSurface = mImageReader.getSurface();
        final CrImageReaderListener imageReaderListener = new CrImageReaderListener();
        mImageReader.setOnImageAvailableListener(imageReaderListener, mBackgroundHandler);
    }

    @SuppressLint("NewApi")
    private void createVirtualDisplay() {
        Log.d(TAG, "createVirtualDisplay");
        if (mVirtualDisplay != null) {
            mVirtualDisplay.release();
        }

        mVirtualDisplay = mMediaProjection.createVirtualDisplay("ScreenCapture", mWidth, mHeight,
                mScreenDensity, DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR, mSurface, null,
                null);
    }

    private void changeCaptureStateAndNotify(@CaptureState int state) {
        Log.d(TAG, "changeCaptureStateAndNotify state:" + state);
        synchronized (mCaptureStateLock) {
            mCaptureState = state;
            mCaptureStateLock.notifyAll();
       }
    }

    /**
     * stop screen recording
     */
    @SuppressLint("NewApi")
    private void stopScreenRecord() {
        Log.d(TAG, "stopScreenRecord");
        synchronized (mCaptureStateLock) {
            if (mMediaProjection != null && mCaptureState == CaptureState.STARTED) {
                mMediaProjection.stop();
                changeCaptureStateAndNotify(CaptureState.STOPPING);
            } else {
                changeCaptureStateAndNotify(CaptureState.STOPPED);
            }

            if (mAudioEncoder != null) {
                mAudioEncoder.stopRecording();
            }
        }

        mAudioEncoder = null;

        stopForeground(true/*removeNotification*/);
        if (mNotificationManager != null) {
            mNotificationManager.cancel(NOTIFICATION);
            mNotificationManager = null;
        }
        stopSelf();
    }

    private void pauseScreenRecord() {
    }

    private void resumeScreenRecord() {
    }

    private final class AudioEncoderListener implements MediaAudioEncoder.AudioEncoderListener {
        @Override
        public void onFrameReady(int bytesRead) {
            final Intent result_audio = new Intent();
            result_audio.putExtra(EXTRA_RESULT_AUDIO_BYTES, bytesRead);
            result_audio.setAction(ACTION_AUDIO_RESULT);
            sendBroadcast(result_audio);
        }
    };

    @SuppressLint("NewApi")
    private void showNotification(final CharSequence text) {
        Log.d(TAG, "showNotification:" + text);
        NotificationChannel channel = new NotificationChannel(APP_NAME, APP_NAME, NotificationManager.IMPORTANCE_DEFAULT);

        ((NotificationManager) getSystemService(Context.NOTIFICATION_SERVICE)).createNotificationChannel(channel);

        // Set the info for the views that show in the notification panel.
        final Notification notification = new Notification.Builder(this, APP_NAME)
                .setSmallIcon(R.drawable.ic_launcher_foreground)
                .setTicker(text)  // the status text
                .setWhen(System.currentTimeMillis())  // the time stamp
                .setContentTitle(getText(R.string.app_name))  // the label of the entry
                .setContentText(text)  // the contents of the entry
                .setContentIntent(createPendingIntent())  // The intent to send when the entry is clicked
                .build();

        startForeground(NOTIFICATION, notification);
        // Send the notification.
        mNotificationManager.notify(NOTIFICATION, notification);

    }

    protected PendingIntent createPendingIntent() {
        return PendingIntent.getActivity(this, 0, new Intent(this, ApplicationStatus.getLastTrackedFocusedActivity().getClass()), 0);
    }

    private int getDeviceRotation() {
        switch (mDisplay.getRotation()) {
            case Surface.ROTATION_0:
                return 0;
            case Surface.ROTATION_90:
                return 90;
            case Surface.ROTATION_180:
                return 180;
            case Surface.ROTATION_270:
                return 270;
            default:
                // This should not happen.
                assert false;
                return 0;
        }
    }

    private @DeviceOrientation int getDeviceOrientation(int rotation) {
        switch (rotation) {
            case 0:
            case 180:
                return DeviceOrientation.PORTRAIT;
            case 90:
            case 270:
                return DeviceOrientation.LANDSCAPE;
            default:
                // This should not happen;
                assert false;
                return DeviceOrientation.LANDSCAPE;
        }
    }

    private boolean maybeDoRotation() {
        final int rotation = getDeviceRotation();
        final @DeviceOrientation int orientation = getDeviceOrientation(rotation);
        if (orientation == mCurrentOrientation) {
            return false;
        }

        Log.i(TAG, "maybeDoRotation");
        mCurrentOrientation = orientation;
        rotateCaptureOrientation(orientation);

        final Intent result = new Intent();
        result.setAction(ACTION_ROTATE_RESULT);
        result.putExtra(EXTRA_RESULT_ROTATION, rotation);
        sendBroadcast(result);
        return true;
    }

    private void rotateCaptureOrientation(@DeviceOrientation int orientation) {
        if ((orientation == DeviceOrientation.LANDSCAPE && mWidth < mHeight)
                || (orientation == DeviceOrientation.PORTRAIT && mHeight < mWidth)) {
            mWidth += mHeight - (mHeight = mWidth);
        }
    }
}
