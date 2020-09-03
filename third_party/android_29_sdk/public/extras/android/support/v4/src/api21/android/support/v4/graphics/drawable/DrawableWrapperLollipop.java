/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.support.v4.graphics.drawable;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Outline;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableContainer;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;

class DrawableWrapperLollipop extends DrawableWrapperKitKat {

    DrawableWrapperLollipop(Drawable drawable) {
        super(drawable);
    }

    DrawableWrapperLollipop(DrawableWrapperState state, Resources resources) {
        super(state, resources);
    }

    @Override
    public void setHotspot(float x, float y) {
        mDrawable.setHotspot(x, y);
    }

    @Override
    public void setHotspotBounds(int left, int top, int right, int bottom) {
        mDrawable.setHotspotBounds(left, top, right, bottom);
    }

    @Override
    public void getOutline(Outline outline) {
        mDrawable.getOutline(outline);
    }

    @Override
    public Rect getDirtyBounds() {
        return mDrawable.getDirtyBounds();
    }

    @Override
    public void setTintList(ColorStateList tint) {
        if (isCompatTintEnabled()) {
            setCompatTintList(tint);
        } else {
            mDrawable.setTintList(tint);
        }
    }

    @Override
    public void setTint(int tintColor) {
        if (isCompatTintEnabled()) {
            setCompatTint(tintColor);
        } else {
            mDrawable.setTint(tintColor);
        }
    }

    @Override
    public void setTintMode(PorterDuff.Mode tintMode) {
        if (isCompatTintEnabled()) {
            setCompatTintMode(tintMode);
        } else {
            mDrawable.setTintMode(tintMode);
        }
    }

    @Override
    public boolean setState(int[] stateSet) {
        if (super.setState(stateSet)) {
            // Manually invalidate because the framework doesn't currently force an invalidation
            // on a state change
            invalidateSelf();
            return true;
        }
        return false;
    }

    @Override
    protected boolean isCompatTintEnabled() {
        if (Build.VERSION.SDK_INT == 21) {
            final Drawable drawable = mDrawable;
            return drawable instanceof GradientDrawable || drawable instanceof DrawableContainer
                    || drawable instanceof InsetDrawable;
        }
        return false;
    }

    @NonNull
    @Override
    DrawableWrapperState mutateConstantState() {
        return new DrawableWrapperStateLollipop(mState, null);
    }

    private static class DrawableWrapperStateLollipop extends DrawableWrapperState {
        DrawableWrapperStateLollipop(@Nullable DrawableWrapperState orig,
                @Nullable Resources res) {
            super(orig, res);
        }

        @Override
        public Drawable newDrawable(@Nullable Resources res) {
            return new DrawableWrapperLollipop(this, res);
        }
    }
}
