/*
 * Copyright (C) 2009 The Android Open Source Project
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

package android.service.wallpaper;

import android.annotation.Nullable;
import android.annotation.SdkConstant;
import android.annotation.SdkConstant.SdkConstantType;
import android.annotation.SystemApi;
import android.annotation.UnsupportedAppUsage;
import android.app.Service;
import android.app.WallpaperColors;
import android.app.WallpaperInfo;
import android.app.WallpaperManager;
import android.content.Context;
import android.content.Intent;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.hardware.display.DisplayManager;
import android.hardware.display.DisplayManager.DisplayListener;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.os.SystemClock;
import android.util.Log;
import android.util.MergedConfiguration;
import android.view.Display;
import android.view.DisplayCutout;
import android.view.DisplayInfo;
import android.view.Gravity;
import android.view.IWindowSession;
import android.view.InputChannel;
import android.view.InputDevice;
import android.view.InputEvent;
import android.view.InputEventReceiver;
import android.view.InsetsState;
import android.view.MotionEvent;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowInsets;
import android.view.WindowManager;
import android.view.WindowManagerGlobal;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.os.HandlerCaller;
import com.android.internal.view.BaseIWindow;
import com.android.internal.view.BaseSurfaceHolder;

import java.io.FileDescriptor;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Supplier;

/**
 * A wallpaper service is responsible for showing a live wallpaper behind
 * applications that would like to sit on top of it.  This service object
 * itself does very little -- its only purpose is to generate instances of
 * {@link Engine} as needed.  Implementing a wallpaper thus
 * involves subclassing from this, subclassing an Engine implementation,
 * and implementing {@link #onCreateEngine()} to return a new instance of
 * your engine.
 */
public abstract class WallpaperService extends Service {
    /**
     * The {@link Intent} that must be declared as handled by the service.
     * To be supported, the service must also require the
     * {@link android.Manifest.permission#BIND_WALLPAPER} permission so
     * that other applications can not abuse it.
     */
    @SdkConstant(SdkConstantType.SERVICE_ACTION)
    public static final String SERVICE_INTERFACE =
            "android.service.wallpaper.WallpaperService";

    /**
     * Name under which a WallpaperService component publishes information
     * about itself.  This meta-data must reference an XML resource containing
     * a <code>&lt;{@link android.R.styleable#Wallpaper wallpaper}&gt;</code>
     * tag.
     */
    public static final String SERVICE_META_DATA = "android.service.wallpaper";

    static final String TAG = "WallpaperService";
    static final boolean DEBUG = false;
    
    private static final int DO_ATTACH = 10;
    private static final int DO_DETACH = 20;
    private static final int DO_SET_DESIRED_SIZE = 30;
    private static final int DO_SET_DISPLAY_PADDING = 40;
    private static final int DO_IN_AMBIENT_MODE = 50;

    private static final int MSG_UPDATE_SURFACE = 10000;
    private static final int MSG_VISIBILITY_CHANGED = 10010;
    private static final int MSG_WALLPAPER_OFFSETS = 10020;
    private static final int MSG_WALLPAPER_COMMAND = 10025;
    @UnsupportedAppUsage
    private static final int MSG_WINDOW_RESIZED = 10030;
    private static final int MSG_WINDOW_MOVED = 10035;
    private static final int MSG_TOUCH_EVENT = 10040;
    private static final int MSG_REQUEST_WALLPAPER_COLORS = 10050;

    private static final int NOTIFY_COLORS_RATE_LIMIT_MS = 1000;

    private final ArrayList<Engine> mActiveEngines
            = new ArrayList<Engine>();
    
    static final class WallpaperCommand {
        String action;
        int x;
        int y;
        int z;
        Bundle extras;
        boolean sync;
    }

    /**
     * The actual implementation of a wallpaper.  A wallpaper service may
     * have multiple instances running (for example as a real wallpaper
     * and as a preview), each of which is represented by its own Engine
     * instance.  You must implement {@link WallpaperService#onCreateEngine()}
     * to return your concrete Engine implementation.
     */
    public class Engine {
        IWallpaperEngineWrapper mIWallpaperEngine;
        
        // Copies from mIWallpaperEngine.
        HandlerCaller mCaller;
        IWallpaperConnection mConnection;
        IBinder mWindowToken;

        boolean mInitializing = true;
        boolean mVisible;
        boolean mReportedVisible;
        boolean mDestroyed;
        
        // Current window state.
        boolean mCreated;
        boolean mSurfaceCreated;
        boolean mIsCreating;
        boolean mDrawingAllowed;
        boolean mOffsetsChanged;
        boolean mFixedSizeAllowed;
        int mWidth;
        int mHeight;
        int mFormat;
        int mType;
        int mCurWidth;
        int mCurHeight;
        int mWindowFlags = WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE;
        int mWindowPrivateFlags =
                WindowManager.LayoutParams.PRIVATE_FLAG_WANTS_OFFSET_NOTIFICATIONS;
        int mCurWindowFlags = mWindowFlags;
        int mCurWindowPrivateFlags = mWindowPrivateFlags;
        final Rect mVisibleInsets = new Rect();
        final Rect mWinFrame = new Rect();
        final Rect mOverscanInsets = new Rect();
        final Rect mContentInsets = new Rect();
        final Rect mStableInsets = new Rect();
        final Rect mOutsets = new Rect();
        final Rect mDispatchedOverscanInsets = new Rect();
        final Rect mDispatchedContentInsets = new Rect();
        final Rect mDispatchedStableInsets = new Rect();
        final Rect mDispatchedOutsets = new Rect();
        final Rect mFinalSystemInsets = new Rect();
        final Rect mFinalStableInsets = new Rect();
        final Rect mBackdropFrame = new Rect();
        final DisplayCutout.ParcelableWrapper mDisplayCutout =
                new DisplayCutout.ParcelableWrapper();
        DisplayCutout mDispatchedDisplayCutout = DisplayCutout.NO_CUTOUT;
        final InsetsState mInsetsState = new InsetsState();
        final MergedConfiguration mMergedConfiguration = new MergedConfiguration();

        final WindowManager.LayoutParams mLayout
                = new WindowManager.LayoutParams();
        IWindowSession mSession;
        InputChannel mInputChannel;

        final Object mLock = new Object();
        boolean mOffsetMessageEnqueued;
        @UnsupportedAppUsage(maxTargetSdk = Build.VERSION_CODES.P, trackingBug = 115609023)
        float mPendingXOffset;
        float mPendingYOffset;
        float mPendingXOffsetStep;
        float mPendingYOffsetStep;
        boolean mPendingSync;
        MotionEvent mPendingMove;
        boolean mIsInAmbientMode;

        // Needed for throttling onComputeColors.
        private long mLastColorInvalidation;
        private final Runnable mNotifyColorsChanged = this::notifyColorsChanged;
        private final Supplier<Long> mClockFunction;
        private final Handler mHandler;

        private Display mDisplay;
        private Context mDisplayContext;
        private int mDisplayState;

        SurfaceControl mSurfaceControl = new SurfaceControl();

        final BaseSurfaceHolder mSurfaceHolder = new BaseSurfaceHolder() {
            {
                mRequestedFormat = PixelFormat.RGBX_8888;
            }

            @Override
            public boolean onAllowLockCanvas() {
                return mDrawingAllowed;
            }

            @Override
            public void onRelayoutContainer() {
                Message msg = mCaller.obtainMessage(MSG_UPDATE_SURFACE);
                mCaller.sendMessage(msg);
            }

            @Override
            public void onUpdateSurface() {
                Message msg = mCaller.obtainMessage(MSG_UPDATE_SURFACE);
                mCaller.sendMessage(msg);
            }

            public boolean isCreating() {
                return mIsCreating;
            }

            @Override
            public void setFixedSize(int width, int height) {
                if (!mFixedSizeAllowed) {
                    // Regular apps can't do this.  It can only work for
                    // certain designs of window animations, so you can't
                    // rely on it.
                    throw new UnsupportedOperationException(
                            "Wallpapers currently only support sizing from layout");
                }
                super.setFixedSize(width, height);
            }
            
            public void setKeepScreenOn(boolean screenOn) {
                throw new UnsupportedOperationException(
                        "Wallpapers do not support keep screen on");
            }

            private void prepareToDraw() {
                if (mDisplayState == Display.STATE_DOZE
                        || mDisplayState == Display.STATE_DOZE_SUSPEND) {
                    try {
                        mSession.pokeDrawLock(mWindow);
                    } catch (RemoteException e) {
                        // System server died, can be ignored.
                    }
                }
            }

            @Override
            public Canvas lockCanvas() {
                prepareToDraw();
                return super.lockCanvas();
            }

            @Override
            public Canvas lockCanvas(Rect dirty) {
                prepareToDraw();
                return super.lockCanvas(dirty);
            }

            @Override
            public Canvas lockHardwareCanvas() {
                prepareToDraw();
                return super.lockHardwareCanvas();
            }
        };

        final class WallpaperInputEventReceiver extends InputEventReceiver {
            public WallpaperInputEventReceiver(InputChannel inputChannel, Looper looper) {
                super(inputChannel, looper);
            }

            @Override
            public void onInputEvent(InputEvent event) {
                boolean handled = false;
                try {
                    if (event instanceof MotionEvent
                            && (event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0) {
                        MotionEvent dup = MotionEvent.obtainNoHistory((MotionEvent)event);
                        dispatchPointer(dup);
                        handled = true;
                    }
                } finally {
                    finishInputEvent(event, handled);
                }
            }
        }
        WallpaperInputEventReceiver mInputEventReceiver;

        final BaseIWindow mWindow = new BaseIWindow() {
            @Override
            public void resized(Rect frame, Rect overscanInsets, Rect contentInsets,
                    Rect visibleInsets, Rect stableInsets, Rect outsets, boolean reportDraw,
                    MergedConfiguration mergedConfiguration, Rect backDropRect, boolean forceLayout,
                    boolean alwaysConsumeSystemBars, int displayId,
                    DisplayCutout.ParcelableWrapper displayCutout) {
                Message msg = mCaller.obtainMessageIO(MSG_WINDOW_RESIZED,
                        reportDraw ? 1 : 0, outsets);
                mCaller.sendMessage(msg);
            }

            @Override
            public void moved(int newX, int newY) {
                Message msg = mCaller.obtainMessageII(MSG_WINDOW_MOVED, newX, newY);
                mCaller.sendMessage(msg);
            }

            @Override
            public void dispatchAppVisibility(boolean visible) {
                // We don't do this in preview mode; we'll let the preview
                // activity tell us when to run.
                if (!mIWallpaperEngine.mIsPreview) {
                    Message msg = mCaller.obtainMessageI(MSG_VISIBILITY_CHANGED,
                            visible ? 1 : 0);
                    mCaller.sendMessage(msg);
                }
            }

            @Override
            public void dispatchWallpaperOffsets(float x, float y, float xStep, float yStep,
                    boolean sync) {
                synchronized (mLock) {
                    if (DEBUG) Log.v(TAG, "Dispatch wallpaper offsets: " + x + ", " + y);
                    mPendingXOffset = x;
                    mPendingYOffset = y;
                    mPendingXOffsetStep = xStep;
                    mPendingYOffsetStep = yStep;
                    if (sync) {
                        mPendingSync = true;
                    }
                    if (!mOffsetMessageEnqueued) {
                        mOffsetMessageEnqueued = true;
                        Message msg = mCaller.obtainMessage(MSG_WALLPAPER_OFFSETS);
                        mCaller.sendMessage(msg);
                    }
                }
            }

            @Override
            public void dispatchWallpaperCommand(String action, int x, int y,
                    int z, Bundle extras, boolean sync) {
                synchronized (mLock) {
                    if (DEBUG) Log.v(TAG, "Dispatch wallpaper command: " + x + ", " + y);
                    WallpaperCommand cmd = new WallpaperCommand();
                    cmd.action = action;
                    cmd.x = x;
                    cmd.y = y;
                    cmd.z = z;
                    cmd.extras = extras;
                    cmd.sync = sync;
                    Message msg = mCaller.obtainMessage(MSG_WALLPAPER_COMMAND);
                    msg.obj = cmd;
                    mCaller.sendMessage(msg);
                }
            }
        };

        /**
         * Default constructor
         */
        public Engine() {
            this(SystemClock::elapsedRealtime, Handler.getMain());
        }

        /**
         * Constructor used for test purposes.
         *
         * @param clockFunction Supplies current times in millis.
         * @param handler Used for posting/deferring asynchronous calls.
         * @hide
         */
        @VisibleForTesting
        public Engine(Supplier<Long> clockFunction, Handler handler) {
           mClockFunction = clockFunction;
           mHandler = handler;
        }
        
        /**
         * Provides access to the surface in which this wallpaper is drawn.
         */
        public SurfaceHolder getSurfaceHolder() {
            return mSurfaceHolder;
        }
        
        /**
         * Convenience for {@link WallpaperManager#getDesiredMinimumWidth()
         * WallpaperManager.getDesiredMinimumWidth()}, returning the width
         * that the system would like this wallpaper to run in.
         */
        public int getDesiredMinimumWidth() {
            return mIWallpaperEngine.mReqWidth;
        }
        
        /**
         * Convenience for {@link WallpaperManager#getDesiredMinimumHeight()
         * WallpaperManager.getDesiredMinimumHeight()}, returning the height
         * that the system would like this wallpaper to run in.
         */
        public int getDesiredMinimumHeight() {
            return mIWallpaperEngine.mReqHeight;
        }

        /**
         * Return whether the wallpaper is currently visible to the user,
         * this is the last value supplied to
         * {@link #onVisibilityChanged(boolean)}.
         */
        public boolean isVisible() {
            return mReportedVisible;
        }
        
        /**
         * Returns true if this engine is running in preview mode -- that is,
         * it is being shown to the user before they select it as the actual
         * wallpaper.
         */
        public boolean isPreview() {
            return mIWallpaperEngine.mIsPreview;
        }

        /**
         * Returns true if this engine is running in ambient mode -- that is,
         * it is being shown in low power mode, on always on display.
         * @hide
         */
        @SystemApi
        public boolean isInAmbientMode() {
            return mIsInAmbientMode;
        }
        
        /**
         * Control whether this wallpaper will receive raw touch events
         * from the window manager as the user interacts with the window
         * that is currently displaying the wallpaper.  By default they
         * are turned off.  If enabled, the events will be received in
         * {@link #onTouchEvent(MotionEvent)}.
         */
        public void setTouchEventsEnabled(boolean enabled) {
            mWindowFlags = enabled
                    ? (mWindowFlags&~WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE)
                    : (mWindowFlags|WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE);
            if (mCreated) {
                updateSurface(false, false, false);
            }
        }

        /**
         * Control whether this wallpaper will receive notifications when the wallpaper
         * has been scrolled. By default, wallpapers will receive notifications, although
         * the default static image wallpapers do not. It is a performance optimization to
         * set this to false.
         *
         * @param enabled whether the wallpaper wants to receive offset notifications
         */
        public void setOffsetNotificationsEnabled(boolean enabled) {
            mWindowPrivateFlags = enabled
                    ? (mWindowPrivateFlags |
                        WindowManager.LayoutParams.PRIVATE_FLAG_WANTS_OFFSET_NOTIFICATIONS)
                    : (mWindowPrivateFlags &
                        ~WindowManager.LayoutParams.PRIVATE_FLAG_WANTS_OFFSET_NOTIFICATIONS);
            if (mCreated) {
                updateSurface(false, false, false);
            }
        }

        /** {@hide} */
        @UnsupportedAppUsage
        public void setFixedSizeAllowed(boolean allowed) {
            mFixedSizeAllowed = allowed;
        }

        /**
         * Called once to initialize the engine.  After returning, the
         * engine's surface will be created by the framework.
         */
        public void onCreate(SurfaceHolder surfaceHolder) {
        }

        /**
         * Called right before the engine is going away.  After this the
         * surface will be destroyed and this Engine object is no longer
         * valid.
         */
        public void onDestroy() {
        }

        /**
         * Called to inform you of the wallpaper becoming visible or
         * hidden.  <em>It is very important that a wallpaper only use
         * CPU while it is visible.</em>.
         */
        public void onVisibilityChanged(boolean visible) {
        }

        /**
         * Called with the current insets that are in effect for the wallpaper.
         * This gives you the part of the overall wallpaper surface that will
         * generally be visible to the user (ignoring position offsets applied to it).
         *
         * @param insets Insets to apply.
         */
        public void onApplyWindowInsets(WindowInsets insets) {
        }

        /**
         * Called as the user performs touch-screen interaction with the
         * window that is currently showing this wallpaper.  Note that the
         * events you receive here are driven by the actual application the
         * user is interacting with, so if it is slow you will get fewer
         * move events.
         */
        public void onTouchEvent(MotionEvent event) {
        }

        /**
         * Called to inform you of the wallpaper's offsets changing
         * within its contain, corresponding to the container's
         * call to {@link WallpaperManager#setWallpaperOffsets(IBinder, float, float)
         * WallpaperManager.setWallpaperOffsets()}.
         */
        public void onOffsetsChanged(float xOffset, float yOffset,
                float xOffsetStep, float yOffsetStep,
                int xPixelOffset, int yPixelOffset) {
        }

        /**
         * Process a command that was sent to the wallpaper with
         * {@link WallpaperManager#sendWallpaperCommand}.
         * The default implementation does nothing, and always returns null
         * as the result.
         * 
         * @param action The name of the command to perform.  This tells you
         * what to do and how to interpret the rest of the arguments.
         * @param x Generic integer parameter.
         * @param y Generic integer parameter.
         * @param z Generic integer parameter.
         * @param extras Any additional parameters.
         * @param resultRequested If true, the caller is requesting that
         * a result, appropriate for the command, be returned back.
         * @return If returning a result, create a Bundle and place the
         * result data in to it.  Otherwise return null.
         */
        public Bundle onCommand(String action, int x, int y, int z,
                Bundle extras, boolean resultRequested) {
            return null;
        }

        /**
         * Called when the device enters or exits ambient mode.
         *
         * @param inAmbientMode {@code true} if in ambient mode.
         * @param animationDuration How long the transition animation to change the ambient state
         *                          should run, in milliseconds. If 0 is passed as the argument
         *                          here, the state should be switched immediately.
         *
         * @see #isInAmbientMode()
         * @see WallpaperInfo#supportsAmbientMode()
         * @hide
         */
        @SystemApi
        public void onAmbientModeChanged(boolean inAmbientMode, long animationDuration) {
        }

        /**
         * Called when an application has changed the desired virtual size of
         * the wallpaper.
         */
        public void onDesiredSizeChanged(int desiredWidth, int desiredHeight) {
        }

        /**
         * Convenience for {@link SurfaceHolder.Callback#surfaceChanged
         * SurfaceHolder.Callback.surfaceChanged()}.
         */
        public void onSurfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        }

        /**
         * Convenience for {@link SurfaceHolder.Callback2#surfaceRedrawNeeded
         * SurfaceHolder.Callback.surfaceRedrawNeeded()}.
         */
        public void onSurfaceRedrawNeeded(SurfaceHolder holder) {
        }

        /**
         * Convenience for {@link SurfaceHolder.Callback#surfaceCreated
         * SurfaceHolder.Callback.surfaceCreated()}.
         */
        public void onSurfaceCreated(SurfaceHolder holder) {
        }

        /**
         * Convenience for {@link SurfaceHolder.Callback#surfaceDestroyed
         * SurfaceHolder.Callback.surfaceDestroyed()}.
         */
        public void onSurfaceDestroyed(SurfaceHolder holder) {
        }

        /**
         * Notifies the engine that wallpaper colors changed significantly.
         * This will trigger a {@link #onComputeColors()} call.
         */
        public void notifyColorsChanged() {
            final long now = mClockFunction.get();
            if (now - mLastColorInvalidation < NOTIFY_COLORS_RATE_LIMIT_MS) {
                Log.w(TAG, "This call has been deferred. You should only call "
                        + "notifyColorsChanged() once every "
                        + (NOTIFY_COLORS_RATE_LIMIT_MS / 1000f) + " seconds.");
                if (!mHandler.hasCallbacks(mNotifyColorsChanged)) {
                    mHandler.postDelayed(mNotifyColorsChanged, NOTIFY_COLORS_RATE_LIMIT_MS);
                }
                return;
            }
            mLastColorInvalidation = now;
            mHandler.removeCallbacks(mNotifyColorsChanged);

            try {
                final WallpaperColors newColors = onComputeColors();
                if (mConnection != null) {
                    mConnection.onWallpaperColorsChanged(newColors, mDisplay.getDisplayId());
                } else {
                    Log.w(TAG, "Can't notify system because wallpaper connection "
                            + "was not established.");
                }
            } catch (RemoteException e) {
                Log.w(TAG, "Can't notify system because wallpaper connection was lost.", e);
            }
        }

        /**
         * Called by the system when it needs to know what colors the wallpaper is using.
         * You might return null if no color information is available at the moment.
         * In that case you might want to call {@link #notifyColorsChanged()} when
         * color information becomes available.
         * <p>
         * The simplest way of creating a {@link android.app.WallpaperColors} object is by using
         * {@link android.app.WallpaperColors#fromBitmap(Bitmap)} or
         * {@link android.app.WallpaperColors#fromDrawable(Drawable)}, but you can also specify
         * your main colors by constructing a {@link android.app.WallpaperColors} object manually.
         *
         * @return Wallpaper colors.
         */
        public @Nullable WallpaperColors onComputeColors() {
            return null;
        }

        /**
         * Sets internal engine state. Only for testing.
         * @param created {@code true} or {@code false}.
         * @hide
         */
        @VisibleForTesting
        public void setCreated(boolean created) {
            mCreated = created;
        }

        protected void dump(String prefix, FileDescriptor fd, PrintWriter out, String[] args) {
            out.print(prefix); out.print("mInitializing="); out.print(mInitializing);
                    out.print(" mDestroyed="); out.println(mDestroyed);
            out.print(prefix); out.print("mVisible="); out.print(mVisible);
                    out.print(" mReportedVisible="); out.println(mReportedVisible);
            out.print(prefix); out.print("mDisplay="); out.println(mDisplay);
            out.print(prefix); out.print("mCreated="); out.print(mCreated);
                    out.print(" mSurfaceCreated="); out.print(mSurfaceCreated);
                    out.print(" mIsCreating="); out.print(mIsCreating);
                    out.print(" mDrawingAllowed="); out.println(mDrawingAllowed);
            out.print(prefix); out.print("mWidth="); out.print(mWidth);
                    out.print(" mCurWidth="); out.print(mCurWidth);
                    out.print(" mHeight="); out.print(mHeight);
                    out.print(" mCurHeight="); out.println(mCurHeight);
            out.print(prefix); out.print("mType="); out.print(mType);
                    out.print(" mWindowFlags="); out.print(mWindowFlags);
                    out.print(" mCurWindowFlags="); out.println(mCurWindowFlags);
            out.print(prefix); out.print("mWindowPrivateFlags="); out.print(mWindowPrivateFlags);
                    out.print(" mCurWindowPrivateFlags="); out.println(mCurWindowPrivateFlags);
            out.print(prefix); out.print("mVisibleInsets=");
                    out.print(mVisibleInsets.toShortString());
                    out.print(" mWinFrame="); out.print(mWinFrame.toShortString());
                    out.print(" mContentInsets="); out.println(mContentInsets.toShortString());
            out.print(prefix); out.print("mConfiguration=");
                    out.println(mMergedConfiguration.getMergedConfiguration());
            out.print(prefix); out.print("mLayout="); out.println(mLayout);
            synchronized (mLock) {
                out.print(prefix); out.print("mPendingXOffset="); out.print(mPendingXOffset);
                        out.print(" mPendingXOffset="); out.println(mPendingXOffset);
                out.print(prefix); out.print("mPendingXOffsetStep=");
                        out.print(mPendingXOffsetStep);
                        out.print(" mPendingXOffsetStep="); out.println(mPendingXOffsetStep);
                out.print(prefix); out.print("mOffsetMessageEnqueued=");
                        out.print(mOffsetMessageEnqueued);
                        out.print(" mPendingSync="); out.println(mPendingSync);
                if (mPendingMove != null) {
                    out.print(prefix); out.print("mPendingMove="); out.println(mPendingMove);
                }
            }
        }

        private void dispatchPointer(MotionEvent event) {
            if (event.isTouchEvent()) {
                synchronized (mLock) {
                    if (event.getAction() == MotionEvent.ACTION_MOVE) {
                        mPendingMove = event;
                    } else {
                        mPendingMove = null;
                    }
                }
                Message msg = mCaller.obtainMessageO(MSG_TOUCH_EVENT, event);
                mCaller.sendMessage(msg);
            } else {
                event.recycle();
            }
        }

        void updateSurface(boolean forceRelayout, boolean forceReport, boolean redrawNeeded) {
            if (mDestroyed) {
                Log.w(TAG, "Ignoring updateSurface: destroyed");
            }

            boolean fixedSize = false;
            int myWidth = mSurfaceHolder.getRequestedWidth();
            if (myWidth <= 0) myWidth = ViewGroup.LayoutParams.MATCH_PARENT;
            else fixedSize = true;
            int myHeight = mSurfaceHolder.getRequestedHeight();
            if (myHeight <= 0) myHeight = ViewGroup.LayoutParams.MATCH_PARENT;
            else fixedSize = true;

            final boolean creating = !mCreated;
            final boolean surfaceCreating = !mSurfaceCreated;
            final boolean formatChanged = mFormat != mSurfaceHolder.getRequestedFormat();
            boolean sizeChanged = mWidth != myWidth || mHeight != myHeight;
            boolean insetsChanged = !mCreated;
            final boolean typeChanged = mType != mSurfaceHolder.getRequestedType();
            final boolean flagsChanged = mCurWindowFlags != mWindowFlags ||
                    mCurWindowPrivateFlags != mWindowPrivateFlags;
            if (forceRelayout || creating || surfaceCreating || formatChanged || sizeChanged
                    || typeChanged || flagsChanged || redrawNeeded
                    || !mIWallpaperEngine.mShownReported) {

                if (DEBUG) Log.v(TAG, "Changes: creating=" + creating
                        + " format=" + formatChanged + " size=" + sizeChanged);

                try {
                    mWidth = myWidth;
                    mHeight = myHeight;
                    mFormat = mSurfaceHolder.getRequestedFormat();
                    mType = mSurfaceHolder.getRequestedType();

                    mLayout.x = 0;
                    mLayout.y = 0;

                    if (!fixedSize) {
                        mLayout.width = myWidth;
                        mLayout.height = myHeight;
                    } else {
                        // Force the wallpaper to cover the screen in both dimensions
                        // only internal implementations like ImageWallpaper
                        DisplayInfo displayInfo = new DisplayInfo();
                        mDisplay.getDisplayInfo(displayInfo);
                        final float layoutScale = Math.max(
                                (float) displayInfo.logicalHeight / (float) myHeight,
                                (float) displayInfo.logicalWidth / (float) myWidth);
                        mLayout.height = (int) (myHeight * layoutScale);
                        mLayout.width = (int) (myWidth * layoutScale);
                        mWindowFlags |= WindowManager.LayoutParams.FLAG_SCALED;
                    }

                    mLayout.format = mFormat;
                    
                    mCurWindowFlags = mWindowFlags;
                    mLayout.flags = mWindowFlags
                            | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS
                            | WindowManager.LayoutParams.FLAG_LAYOUT_INSET_DECOR
                            | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN
                            | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
                    mCurWindowPrivateFlags = mWindowPrivateFlags;
                    mLayout.privateFlags = mWindowPrivateFlags;

                    mLayout.memoryType = mType;
                    mLayout.token = mWindowToken;

                    if (!mCreated) {
                        // Retrieve watch round info
                        TypedArray windowStyle = obtainStyledAttributes(
                                com.android.internal.R.styleable.Window);
                        windowStyle.recycle();

                        // Add window
                        mLayout.type = mIWallpaperEngine.mWindowType;
                        mLayout.gravity = Gravity.START|Gravity.TOP;
                        mLayout.setTitle(WallpaperService.this.getClass().getName());
                        mLayout.windowAnimations =
                                com.android.internal.R.style.Animation_Wallpaper;
                        mInputChannel = new InputChannel();

                        if (mSession.addToDisplay(mWindow, mWindow.mSeq, mLayout, View.VISIBLE,
                                mDisplay.getDisplayId(), mWinFrame, mContentInsets, mStableInsets,
                                mOutsets, mDisplayCutout, mInputChannel,
                                mInsetsState) < 0) {
                            Log.w(TAG, "Failed to add window while updating wallpaper surface.");
                            return;
                        }
                        mCreated = true;

                        mInputEventReceiver = new WallpaperInputEventReceiver(
                                mInputChannel, Looper.myLooper());
                    }

                    mSurfaceHolder.mSurfaceLock.lock();
                    mDrawingAllowed = true;

                    if (!fixedSize) {
                        mLayout.surfaceInsets.set(mIWallpaperEngine.mDisplayPadding);
                        mLayout.surfaceInsets.left += mOutsets.left;
                        mLayout.surfaceInsets.top += mOutsets.top;
                        mLayout.surfaceInsets.right += mOutsets.right;
                        mLayout.surfaceInsets.bottom += mOutsets.bottom;
                    } else {
                        mLayout.surfaceInsets.set(0, 0, 0, 0);
                    }
                    final int relayoutResult = mSession.relayout(
                        mWindow, mWindow.mSeq, mLayout, mWidth, mHeight,
                            View.VISIBLE, 0, -1, mWinFrame, mOverscanInsets, mContentInsets,
                            mVisibleInsets, mStableInsets, mOutsets, mBackdropFrame,
                            mDisplayCutout, mMergedConfiguration, mSurfaceControl,
                            mInsetsState);
                    if (mSurfaceControl.isValid()) {
                        mSurfaceHolder.mSurface.copyFrom(mSurfaceControl);
                        mSurfaceControl.release();
                    }

                    if (DEBUG) Log.v(TAG, "New surface: " + mSurfaceHolder.mSurface
                            + ", frame=" + mWinFrame);

                    int w = mWinFrame.width();
                    int h = mWinFrame.height();

                    if (!fixedSize) {
                        final Rect padding = mIWallpaperEngine.mDisplayPadding;
                        w += padding.left + padding.right + mOutsets.left + mOutsets.right;
                        h += padding.top + padding.bottom + mOutsets.top + mOutsets.bottom;
                        mOverscanInsets.left += padding.left;
                        mOverscanInsets.top += padding.top;
                        mOverscanInsets.right += padding.right;
                        mOverscanInsets.bottom += padding.bottom;
                        mContentInsets.left += padding.left;
                        mContentInsets.top += padding.top;
                        mContentInsets.right += padding.right;
                        mContentInsets.bottom += padding.bottom;
                        mStableInsets.left += padding.left;
                        mStableInsets.top += padding.top;
                        mStableInsets.right += padding.right;
                        mStableInsets.bottom += padding.bottom;
                        mDisplayCutout.set(mDisplayCutout.get().inset(-padding.left, -padding.top,
                                -padding.right, -padding.bottom));
                    } else {
                        w = myWidth;
                        h = myHeight;
                    }

                    if (mCurWidth != w) {
                        sizeChanged = true;
                        mCurWidth = w;
                    }
                    if (mCurHeight != h) {
                        sizeChanged = true;
                        mCurHeight = h;
                    }

                    if (DEBUG) {
                        Log.v(TAG, "Wallpaper size has changed: (" + mCurWidth + ", " + mCurHeight);
                    }

                    insetsChanged |= !mDispatchedOverscanInsets.equals(mOverscanInsets);
                    insetsChanged |= !mDispatchedContentInsets.equals(mContentInsets);
                    insetsChanged |= !mDispatchedStableInsets.equals(mStableInsets);
                    insetsChanged |= !mDispatchedOutsets.equals(mOutsets);
                    insetsChanged |= !mDispatchedDisplayCutout.equals(mDisplayCutout.get());

                    mSurfaceHolder.setSurfaceFrameSize(w, h);
                    mSurfaceHolder.mSurfaceLock.unlock();

                    if (!mSurfaceHolder.mSurface.isValid()) {
                        reportSurfaceDestroyed();
                        if (DEBUG) Log.v(TAG, "Layout: Surface destroyed");
                        return;
                    }

                    boolean didSurface = false;

                    try {
                        mSurfaceHolder.ungetCallbacks();

                        if (surfaceCreating) {
                            mIsCreating = true;
                            didSurface = true;
                            if (DEBUG) Log.v(TAG, "onSurfaceCreated("
                                    + mSurfaceHolder + "): " + this);
                            onSurfaceCreated(mSurfaceHolder);
                            SurfaceHolder.Callback callbacks[] = mSurfaceHolder.getCallbacks();
                            if (callbacks != null) {
                                for (SurfaceHolder.Callback c : callbacks) {
                                    c.surfaceCreated(mSurfaceHolder);
                                }
                            }
                        }

                        redrawNeeded |= creating || (relayoutResult
                                & WindowManagerGlobal.RELAYOUT_RES_FIRST_TIME) != 0;

                        if (forceReport || creating || surfaceCreating
                                || formatChanged || sizeChanged) {
                            if (DEBUG) {
                                RuntimeException e = new RuntimeException();
                                e.fillInStackTrace();
                                Log.w(TAG, "forceReport=" + forceReport + " creating=" + creating
                                        + " formatChanged=" + formatChanged
                                        + " sizeChanged=" + sizeChanged, e);
                            }
                            if (DEBUG) Log.v(TAG, "onSurfaceChanged("
                                    + mSurfaceHolder + ", " + mFormat
                                    + ", " + mCurWidth + ", " + mCurHeight
                                    + "): " + this);
                            didSurface = true;
                            onSurfaceChanged(mSurfaceHolder, mFormat,
                                    mCurWidth, mCurHeight);
                            SurfaceHolder.Callback callbacks[] = mSurfaceHolder.getCallbacks();
                            if (callbacks != null) {
                                for (SurfaceHolder.Callback c : callbacks) {
                                    c.surfaceChanged(mSurfaceHolder, mFormat,
                                            mCurWidth, mCurHeight);
                                }
                            }
                        }

                        if (insetsChanged) {
                            mDispatchedOverscanInsets.set(mOverscanInsets);
                            mDispatchedOverscanInsets.left += mOutsets.left;
                            mDispatchedOverscanInsets.top += mOutsets.top;
                            mDispatchedOverscanInsets.right += mOutsets.right;
                            mDispatchedOverscanInsets.bottom += mOutsets.bottom;
                            mDispatchedContentInsets.set(mContentInsets);
                            mDispatchedStableInsets.set(mStableInsets);
                            mDispatchedOutsets.set(mOutsets);
                            mDispatchedDisplayCutout = mDisplayCutout.get();
                            mFinalSystemInsets.set(mDispatchedOverscanInsets);
                            mFinalStableInsets.set(mDispatchedStableInsets);
                            WindowInsets insets = new WindowInsets(mFinalSystemInsets,
                                    mFinalStableInsets,
                                    getResources().getConfiguration().isScreenRound(), false,
                                    mDispatchedDisplayCutout);
                            if (DEBUG) {
                                Log.v(TAG, "dispatching insets=" + insets);
                            }
                            onApplyWindowInsets(insets);
                        }

                        if (redrawNeeded) {
                            onSurfaceRedrawNeeded(mSurfaceHolder);
                            SurfaceHolder.Callback callbacks[] = mSurfaceHolder.getCallbacks();
                            if (callbacks != null) {
                                for (SurfaceHolder.Callback c : callbacks) {
                                    if (c instanceof SurfaceHolder.Callback2) {
                                        ((SurfaceHolder.Callback2)c).surfaceRedrawNeeded(
                                                mSurfaceHolder);
                                    }
                                }
                            }
                        }

                        if (didSurface && !mReportedVisible) {
                            // This wallpaper is currently invisible, but its
                            // surface has changed.  At this point let's tell it
                            // again that it is invisible in case the report about
                            // the surface caused it to start running.  We really
                            // don't want wallpapers running when not visible.
                            if (mIsCreating) {
                                // Some wallpapers will ignore this call if they
                                // had previously been told they were invisble,
                                // so if we are creating a new surface then toggle
                                // the state to get them to notice.
                                if (DEBUG) Log.v(TAG, "onVisibilityChanged(true) at surface: "
                                        + this);
                                onVisibilityChanged(true);
                            }
                            if (DEBUG) Log.v(TAG, "onVisibilityChanged(false) at surface: "
                                        + this);
                            onVisibilityChanged(false);
                        }

                    } finally {
                        mIsCreating = false;
                        mSurfaceCreated = true;
                        if (redrawNeeded) {
                            mSession.finishDrawing(mWindow);
                        }
                        mIWallpaperEngine.reportShown();
                    }
                } catch (RemoteException ex) {
                }
                if (DEBUG) Log.v(
                    TAG, "Layout: x=" + mLayout.x + " y=" + mLayout.y +
                    " w=" + mLayout.width + " h=" + mLayout.height);
            }
        }

        void attach(IWallpaperEngineWrapper wrapper) {
            if (DEBUG) Log.v(TAG, "attach: " + this + " wrapper=" + wrapper);
            if (mDestroyed) {
                return;
            }

            mIWallpaperEngine = wrapper;
            mCaller = wrapper.mCaller;
            mConnection = wrapper.mConnection;
            mWindowToken = wrapper.mWindowToken;
            mSurfaceHolder.setSizeFromLayout();
            mInitializing = true;
            mSession = WindowManagerGlobal.getWindowSession();
            
            mWindow.setSession(mSession);

            mLayout.packageName = getPackageName();
            mIWallpaperEngine.mDisplayManager.registerDisplayListener(mDisplayListener,
                    mCaller.getHandler());
            mDisplay = mIWallpaperEngine.mDisplay;
            mDisplayContext = createDisplayContext(mDisplay);
            mDisplayState = mDisplay.getState();

            if (DEBUG) Log.v(TAG, "onCreate(): " + this);
            onCreate(mSurfaceHolder);

            mInitializing = false;

            mReportedVisible = false;
            updateSurface(false, false, false);
        }

        /**
         * The {@link Context} with resources that match the current display the wallpaper is on.
         * For multiple display environment, multiple engines can be created to render on each
         * display, but these displays may have different densities. Use this context to get the
         * corresponding resources for currently display, avoiding the context of the service.
         * <p>
         * The display context will never be {@code null} after
         * {@link Engine#onCreate(SurfaceHolder)} has been called.
         *
         * @return A {@link Context} for current display.
         */
        @Nullable
        public Context getDisplayContext() {
            return mDisplayContext;
        }

        /**
         * Executes life cycle event and updates internal ambient mode state based on
         * message sent from handler.
         *
         * @param inAmbientMode {@code true} if in ambient mode.
         * @param animationDuration For how long the transition will last, in ms.
         * @hide
         */
        @VisibleForTesting
        public void doAmbientModeChanged(boolean inAmbientMode, long animationDuration) {
            if (!mDestroyed) {
                if (DEBUG) {
                    Log.v(TAG, "onAmbientModeChanged(" + inAmbientMode + ", "
                            + animationDuration + "): " + this);
                }
                mIsInAmbientMode = inAmbientMode;
                if (mCreated) {
                    onAmbientModeChanged(inAmbientMode, animationDuration);
                }
            }
        }

        void doDesiredSizeChanged(int desiredWidth, int desiredHeight) {
            if (!mDestroyed) {
                if (DEBUG) Log.v(TAG, "onDesiredSizeChanged("
                        + desiredWidth + "," + desiredHeight + "): " + this);
                mIWallpaperEngine.mReqWidth = desiredWidth;
                mIWallpaperEngine.mReqHeight = desiredHeight;
                onDesiredSizeChanged(desiredWidth, desiredHeight);
                doOffsetsChanged(true);
            }
        }

        void doDisplayPaddingChanged(Rect padding) {
            if (!mDestroyed) {
                if (DEBUG) Log.v(TAG, "onDisplayPaddingChanged(" + padding + "): " + this);
                if (!mIWallpaperEngine.mDisplayPadding.equals(padding)) {
                    mIWallpaperEngine.mDisplayPadding.set(padding);
                    updateSurface(true, false, false);
                }
            }
        }

        void doVisibilityChanged(boolean visible) {
            if (!mDestroyed) {
                mVisible = visible;
                reportVisibility();
            }
        }

        void reportVisibility() {
            if (!mDestroyed) {
                mDisplayState = mDisplay == null ? Display.STATE_UNKNOWN : mDisplay.getState();
                boolean visible = mVisible && mDisplayState != Display.STATE_OFF;
                if (mReportedVisible != visible) {
                    mReportedVisible = visible;
                    if (DEBUG) Log.v(TAG, "onVisibilityChanged(" + visible
                            + "): " + this);
                    if (visible) {
                        // If becoming visible, in preview mode the surface
                        // may have been destroyed so now we need to make
                        // sure it is re-created.
                        doOffsetsChanged(false);
                        updateSurface(false, false, false);
                    }
                    onVisibilityChanged(visible);
                }
            }
        }
        
        void doOffsetsChanged(boolean always) {
            if (mDestroyed) {
                return;
            }

            if (!always && !mOffsetsChanged) {
                return;
            }

            float xOffset;
            float yOffset;
            float xOffsetStep;
            float yOffsetStep;
            boolean sync;
            synchronized (mLock) {
                xOffset = mPendingXOffset;
                yOffset = mPendingYOffset;
                xOffsetStep = mPendingXOffsetStep;
                yOffsetStep = mPendingYOffsetStep;
                sync = mPendingSync;
                mPendingSync = false;
                mOffsetMessageEnqueued = false;
            }

            if (mSurfaceCreated) {
                if (mReportedVisible) {
                    if (DEBUG) Log.v(TAG, "Offsets change in " + this
                            + ": " + xOffset + "," + yOffset);
                    final int availw = mIWallpaperEngine.mReqWidth-mCurWidth;
                    final int xPixels = availw > 0 ? -(int)(availw*xOffset+.5f) : 0;
                    final int availh = mIWallpaperEngine.mReqHeight-mCurHeight;
                    final int yPixels = availh > 0 ? -(int)(availh*yOffset+.5f) : 0;
                    onOffsetsChanged(xOffset, yOffset, xOffsetStep, yOffsetStep, xPixels, yPixels);
                } else {
                    mOffsetsChanged = true;
                }
            }
            
            if (sync) {
                try {
                    if (DEBUG) Log.v(TAG, "Reporting offsets change complete");
                    mSession.wallpaperOffsetsComplete(mWindow.asBinder());
                } catch (RemoteException e) {
                }
            }
        }
        
        void doCommand(WallpaperCommand cmd) {
            Bundle result;
            if (!mDestroyed) {
                result = onCommand(cmd.action, cmd.x, cmd.y, cmd.z,
                        cmd.extras, cmd.sync);
            } else {
                result = null;
            }
            if (cmd.sync) {
                try {
                    if (DEBUG) Log.v(TAG, "Reporting command complete");
                    mSession.wallpaperCommandComplete(mWindow.asBinder(), result);
                } catch (RemoteException e) {
                }
            }
        }
        
        void reportSurfaceDestroyed() {
            if (mSurfaceCreated) {
                mSurfaceCreated = false;
                mSurfaceHolder.ungetCallbacks();
                SurfaceHolder.Callback callbacks[] = mSurfaceHolder.getCallbacks();
                if (callbacks != null) {
                    for (SurfaceHolder.Callback c : callbacks) {
                        c.surfaceDestroyed(mSurfaceHolder);
                    }
                }
                if (DEBUG) Log.v(TAG, "onSurfaceDestroyed("
                        + mSurfaceHolder + "): " + this);
                onSurfaceDestroyed(mSurfaceHolder);
            }
        }
        
        void detach() {
            if (mDestroyed) {
                return;
            }
            
            mDestroyed = true;

            if (mIWallpaperEngine.mDisplayManager != null) {
                mIWallpaperEngine.mDisplayManager.unregisterDisplayListener(mDisplayListener);
            }

            if (mVisible) {
                mVisible = false;
                if (DEBUG) Log.v(TAG, "onVisibilityChanged(false): " + this);
                onVisibilityChanged(false);
            }
            
            reportSurfaceDestroyed();
            
            if (DEBUG) Log.v(TAG, "onDestroy(): " + this);
            onDestroy();

            if (mCreated) {
                try {
                    if (DEBUG) Log.v(TAG, "Removing window and destroying surface "
                            + mSurfaceHolder.getSurface() + " of: " + this);
                    
                    if (mInputEventReceiver != null) {
                        mInputEventReceiver.dispose();
                        mInputEventReceiver = null;
                    }
                    
                    mSession.remove(mWindow);
                } catch (RemoteException e) {
                }
                mSurfaceHolder.mSurface.release();
                mCreated = false;
                
                // Dispose the input channel after removing the window so the Window Manager
                // doesn't interpret the input channel being closed as an abnormal termination.
                if (mInputChannel != null) {
                    mInputChannel.dispose();
                    mInputChannel = null;
                }
            }
        }

        private final DisplayListener mDisplayListener = new DisplayListener() {
            @Override
            public void onDisplayChanged(int displayId) {
                if (mDisplay.getDisplayId() == displayId) {
                    reportVisibility();
                }
            }

            @Override
            public void onDisplayRemoved(int displayId) {
            }

            @Override
            public void onDisplayAdded(int displayId) {
            }
        };
    }

    class IWallpaperEngineWrapper extends IWallpaperEngine.Stub
            implements HandlerCaller.Callback {
        private final HandlerCaller mCaller;

        final IWallpaperConnection mConnection;
        final IBinder mWindowToken;
        final int mWindowType;
        final boolean mIsPreview;
        boolean mShownReported;
        int mReqWidth;
        int mReqHeight;
        final Rect mDisplayPadding = new Rect();
        final int mDisplayId;
        final DisplayManager mDisplayManager;
        final Display mDisplay;
        private final AtomicBoolean mDetached = new AtomicBoolean();

        Engine mEngine;

        IWallpaperEngineWrapper(WallpaperService context,
                IWallpaperConnection conn, IBinder windowToken,
                int windowType, boolean isPreview, int reqWidth, int reqHeight, Rect padding,
                int displayId) {
            mCaller = new HandlerCaller(context, context.getMainLooper(), this, true);
            mConnection = conn;
            mWindowToken = windowToken;
            mWindowType = windowType;
            mIsPreview = isPreview;
            mReqWidth = reqWidth;
            mReqHeight = reqHeight;
            mDisplayPadding.set(padding);
            mDisplayId = displayId;

            // Create a display context before onCreateEngine.
            mDisplayManager = getSystemService(DisplayManager.class);
            mDisplay = mDisplayManager.getDisplay(mDisplayId);

            if (mDisplay == null) {
                // Ignore this engine.
                throw new IllegalArgumentException("Cannot find display with id" + mDisplayId);
            }
            Message msg = mCaller.obtainMessage(DO_ATTACH);
            mCaller.sendMessage(msg);
        }

        public void setDesiredSize(int width, int height) {
            Message msg = mCaller.obtainMessageII(DO_SET_DESIRED_SIZE, width, height);
            mCaller.sendMessage(msg);
        }

        public void setDisplayPadding(Rect padding) {
            Message msg = mCaller.obtainMessageO(DO_SET_DISPLAY_PADDING, padding);
            mCaller.sendMessage(msg);
        }

        public void setVisibility(boolean visible) {
            Message msg = mCaller.obtainMessageI(MSG_VISIBILITY_CHANGED,
                    visible ? 1 : 0);
            mCaller.sendMessage(msg);
        }

        @Override
        public void setInAmbientMode(boolean inAmbientDisplay, long animationDuration)
                throws RemoteException {
            Message msg = mCaller.obtainMessageIO(DO_IN_AMBIENT_MODE, inAmbientDisplay ? 1 : 0,
                    animationDuration);
            mCaller.sendMessage(msg);
        }

        public void dispatchPointer(MotionEvent event) {
            if (mEngine != null) {
                mEngine.dispatchPointer(event);
            } else {
                event.recycle();
            }
        }

        public void dispatchWallpaperCommand(String action, int x, int y,
                int z, Bundle extras) {
            if (mEngine != null) {
                mEngine.mWindow.dispatchWallpaperCommand(action, x, y, z, extras, false);
            }
        }

        public void reportShown() {
            if (!mShownReported) {
                mShownReported = true;
                try {
                    mConnection.engineShown(this);
                } catch (RemoteException e) {
                    Log.w(TAG, "Wallpaper host disappeared", e);
                    return;
                }
            }
        }

        public void requestWallpaperColors() {
            Message msg = mCaller.obtainMessage(MSG_REQUEST_WALLPAPER_COLORS);
            mCaller.sendMessage(msg);
        }

        public void destroy() {
            Message msg = mCaller.obtainMessage(DO_DETACH);
            mCaller.sendMessage(msg);
        }

        public void detach() {
            mDetached.set(true);
        }

        private void doDetachEngine() {
            mActiveEngines.remove(mEngine);
            mEngine.detach();
        }

        @Override
        public void executeMessage(Message message) {
            if (mDetached.get()) {
                if (mActiveEngines.contains(mEngine)) {
                    doDetachEngine();
                }
                return;
            }
            switch (message.what) {
                case DO_ATTACH: {
                    try {
                        mConnection.attachEngine(this, mDisplayId);
                    } catch (RemoteException e) {
                        Log.w(TAG, "Wallpaper host disappeared", e);
                        return;
                    }
                    Engine engine = onCreateEngine();
                    mEngine = engine;
                    mActiveEngines.add(engine);
                    engine.attach(this);
                    return;
                }
                case DO_DETACH: {
                    doDetachEngine();
                    return;
                }
                case DO_SET_DESIRED_SIZE: {
                    mEngine.doDesiredSizeChanged(message.arg1, message.arg2);
                    return;
                }
                case DO_SET_DISPLAY_PADDING: {
                    mEngine.doDisplayPaddingChanged((Rect) message.obj);
                    return;
                }
                case DO_IN_AMBIENT_MODE: {
                    mEngine.doAmbientModeChanged(message.arg1 != 0, (Long) message.obj);
                    return;
                }
                case MSG_UPDATE_SURFACE:
                    mEngine.updateSurface(true, false, false);
                    break;
                case MSG_VISIBILITY_CHANGED:
                    if (DEBUG) Log.v(TAG, "Visibility change in " + mEngine
                            + ": " + message.arg1);
                    mEngine.doVisibilityChanged(message.arg1 != 0);
                    break;
                case MSG_WALLPAPER_OFFSETS: {
                    mEngine.doOffsetsChanged(true);
                } break;
                case MSG_WALLPAPER_COMMAND: {
                    WallpaperCommand cmd = (WallpaperCommand)message.obj;
                    mEngine.doCommand(cmd);
                } break;
                case MSG_WINDOW_RESIZED: {
                    final boolean reportDraw = message.arg1 != 0;
                    mEngine.mOutsets.set((Rect) message.obj);
                    mEngine.updateSurface(true, false, reportDraw);
                    mEngine.doOffsetsChanged(true);
                } break;
                case MSG_WINDOW_MOVED: {
                    // Do nothing. What does it mean for a Wallpaper to move?
                } break;
                case MSG_TOUCH_EVENT: {
                    boolean skip = false;
                    MotionEvent ev = (MotionEvent)message.obj;
                    if (ev.getAction() == MotionEvent.ACTION_MOVE) {
                        synchronized (mEngine.mLock) {
                            if (mEngine.mPendingMove == ev) {
                                mEngine.mPendingMove = null;
                            } else {
                                // this is not the motion event we are looking for....
                                skip = true;
                            }
                        }
                    }
                    if (!skip) {
                        if (DEBUG) Log.v(TAG, "Delivering touch event: " + ev);
                        mEngine.onTouchEvent(ev);
                    }
                    ev.recycle();
                } break;
                case MSG_REQUEST_WALLPAPER_COLORS: {
                    if (mConnection == null) {
                        break;
                    }
                    try {
                        mConnection.onWallpaperColorsChanged(mEngine.onComputeColors(), mDisplayId);
                    } catch (RemoteException e) {
                        // Connection went away, nothing to do in here.
                    }
                } break;
                default :
                    Log.w(TAG, "Unknown message type " + message.what);
            }
        }
    }

    /**
     * Implements the internal {@link IWallpaperService} interface to convert
     * incoming calls to it back to calls on an {@link WallpaperService}.
     */
    class IWallpaperServiceWrapper extends IWallpaperService.Stub {
        private final WallpaperService mTarget;
        private IWallpaperEngineWrapper mEngineWrapper;

        public IWallpaperServiceWrapper(WallpaperService context) {
            mTarget = context;
        }

        @Override
        public void attach(IWallpaperConnection conn, IBinder windowToken,
                int windowType, boolean isPreview, int reqWidth, int reqHeight, Rect padding,
                int displayId) {
            mEngineWrapper = new IWallpaperEngineWrapper(mTarget, conn, windowToken,
                    windowType, isPreview, reqWidth, reqHeight, padding, displayId);
        }

        @Override
        public void detach() {
            mEngineWrapper.detach();
        }
    }

    @Override
    public void onCreate() {
        super.onCreate();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        for (int i=0; i<mActiveEngines.size(); i++) {
            mActiveEngines.get(i).detach();
        }
        mActiveEngines.clear();
    }

    /**
     * Implement to return the implementation of the internal accessibility
     * service interface.  Subclasses should not override.
     */
    @Override
    public final IBinder onBind(Intent intent) {
        return new IWallpaperServiceWrapper(this);
    }

    /**
     * Must be implemented to return a new instance of the wallpaper's engine.
     * Note that multiple instances may be active at the same time, such as
     * when the wallpaper is currently set as the active wallpaper and the user
     * is in the wallpaper picker viewing a preview of it as well.
     */
    public abstract Engine onCreateEngine();

    @Override
    protected void dump(FileDescriptor fd, PrintWriter out, String[] args) {
        out.print("State of wallpaper "); out.print(this); out.println(":");
        for (int i=0; i<mActiveEngines.size(); i++) {
            Engine engine = mActiveEngines.get(i);
            out.print("  Engine "); out.print(engine); out.println(":");
            engine.dump("    ", fd, out, args);
        }
    }
}
