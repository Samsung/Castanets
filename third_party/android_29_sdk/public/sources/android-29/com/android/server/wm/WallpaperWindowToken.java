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
 * limitations under the License
 */

package com.android.server.wm;

import static android.view.WindowManager.LayoutParams.TYPE_WALLPAPER;
import static com.android.server.wm.WindowManagerDebugConfig.DEBUG_LAYERS;
import static com.android.server.wm.WindowManagerDebugConfig.DEBUG_WALLPAPER_LIGHT;
import static com.android.server.wm.WindowManagerDebugConfig.TAG_WITH_CLASS_NAME;
import static com.android.server.wm.WindowManagerDebugConfig.TAG_WM;

import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Slog;
import android.view.DisplayInfo;
import android.view.animation.Animation;

/**
 * A token that represents a set of wallpaper windows.
 */
class WallpaperWindowToken extends WindowToken {

    private static final String TAG = TAG_WITH_CLASS_NAME ? "WallpaperWindowToken" : TAG_WM;

    WallpaperWindowToken(WindowManagerService service, IBinder token, boolean explicit,
            DisplayContent dc, boolean ownerCanManageAppTokens) {
        super(service, token, TYPE_WALLPAPER, explicit, dc, ownerCanManageAppTokens);
        dc.mWallpaperController.addWallpaperToken(this);
    }

    @Override
    void setExiting() {
        super.setExiting();
        mDisplayContent.mWallpaperController.removeWallpaperToken(this);
    }

    void hideWallpaperToken(boolean wasDeferred, String reason) {
        for (int j = mChildren.size() - 1; j >= 0; j--) {
            final WindowState wallpaper = mChildren.get(j);
            wallpaper.hideWallpaperWindow(wasDeferred, reason);
        }
        setHidden(true);
    }

    void sendWindowWallpaperCommand(
            String action, int x, int y, int z, Bundle extras, boolean sync) {
        for (int wallpaperNdx = mChildren.size() - 1; wallpaperNdx >= 0; wallpaperNdx--) {
            final WindowState wallpaper = mChildren.get(wallpaperNdx);
            try {
                wallpaper.mClient.dispatchWallpaperCommand(action, x, y, z, extras, sync);
                // We only want to be synchronous with one wallpaper.
                sync = false;
            } catch (RemoteException e) {
            }
        }
    }

    void updateWallpaperOffset(int dw, int dh, boolean sync) {
        final WallpaperController wallpaperController = mDisplayContent.mWallpaperController;
        for (int wallpaperNdx = mChildren.size() - 1; wallpaperNdx >= 0; wallpaperNdx--) {
            final WindowState wallpaper = mChildren.get(wallpaperNdx);
            if (wallpaperController.updateWallpaperOffset(wallpaper, dw, dh, sync)) {
                // We only want to be synchronous with one wallpaper.
                sync = false;
            }
        }
    }

    void updateWallpaperVisibility(boolean visible) {
        final DisplayInfo displayInfo = mDisplayContent.getDisplayInfo();
        final int dw = displayInfo.logicalWidth;
        final int dh = displayInfo.logicalHeight;

        if (isHidden() == visible) {
            setHidden(!visible);

            // Need to do a layout to ensure the wallpaper now has the correct size.
            mDisplayContent.setLayoutNeeded();
        }

        final WallpaperController wallpaperController = mDisplayContent.mWallpaperController;
        for (int wallpaperNdx = mChildren.size() - 1; wallpaperNdx >= 0; wallpaperNdx--) {
            final WindowState wallpaper = mChildren.get(wallpaperNdx);
            if (visible) {
                wallpaperController.updateWallpaperOffset(wallpaper, dw, dh, false);
            }

            wallpaper.dispatchWallpaperVisibility(visible);
        }
    }

    /**
     * Starts {@param anim} on all children.
     */
    void startAnimation(Animation anim) {
        for (int ndx = mChildren.size() - 1; ndx >= 0; ndx--) {
            final WindowState windowState = mChildren.get(ndx);
            windowState.startAnimation(anim);
        }
    }

    void updateWallpaperWindows(boolean visible) {

        if (isHidden() == visible) {
            if (DEBUG_WALLPAPER_LIGHT) Slog.d(TAG,
                    "Wallpaper token " + token + " hidden=" + !visible);
            setHidden(!visible);
            // Need to do a layout to ensure the wallpaper now has the correct size.
            mDisplayContent.setLayoutNeeded();
        }

        final DisplayInfo displayInfo = mDisplayContent.getDisplayInfo();
        final int dw = displayInfo.logicalWidth;
        final int dh = displayInfo.logicalHeight;
        final WallpaperController wallpaperController = mDisplayContent.mWallpaperController;
        for (int wallpaperNdx = mChildren.size() - 1; wallpaperNdx >= 0; wallpaperNdx--) {
            final WindowState wallpaper = mChildren.get(wallpaperNdx);

            if (visible) {
                wallpaperController.updateWallpaperOffset(wallpaper, dw, dh, false);
            }

            // First, make sure the client has the current visibility state.
            wallpaper.dispatchWallpaperVisibility(visible);

            if (DEBUG_LAYERS || DEBUG_WALLPAPER_LIGHT) Slog.v(TAG, "adjustWallpaper win "
                    + wallpaper);
        }
    }

    boolean hasVisibleNotDrawnWallpaper() {
        for (int j = mChildren.size() - 1; j >= 0; --j) {
            final WindowState wallpaper = mChildren.get(j);
            if (wallpaper.hasVisibleNotDrawnWallpaper()) {
                return true;
            }
        }
        return false;
    }

    @Override
    public String toString() {
        if (stringName == null) {
            StringBuilder sb = new StringBuilder();
            sb.append("WallpaperWindowToken{");
            sb.append(Integer.toHexString(System.identityHashCode(this)));
            sb.append(" token="); sb.append(token); sb.append('}');
            stringName = sb.toString();
        }
        return stringName;
    }
}
