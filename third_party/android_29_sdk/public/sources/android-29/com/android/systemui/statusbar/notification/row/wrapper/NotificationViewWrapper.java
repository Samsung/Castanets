/*
 * Copyright (C) 2014 The Android Open Source Project
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

package com.android.systemui.statusbar.notification.row.wrapper;

import android.annotation.ColorInt;
import android.app.Notification;
import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.Paint;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.NotificationHeaderView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.graphics.ColorUtils;
import com.android.internal.util.ContrastColorUtil;
import com.android.systemui.statusbar.CrossFadeHelper;
import com.android.systemui.statusbar.TransformableView;
import com.android.systemui.statusbar.notification.TransformState;
import com.android.systemui.statusbar.notification.row.ExpandableNotificationRow;

/**
 * Wraps the actual notification content view; used to implement behaviors which are different for
 * the individual templates and custom views.
 */
public abstract class NotificationViewWrapper implements TransformableView {

    protected final View mView;
    protected final ExpandableNotificationRow mRow;

    protected int mBackgroundColor = 0;

    public static NotificationViewWrapper wrap(Context ctx, View v, ExpandableNotificationRow row) {
        if (v.getId() == com.android.internal.R.id.status_bar_latest_event_content) {
            if ("bigPicture".equals(v.getTag())) {
                return new NotificationBigPictureTemplateViewWrapper(ctx, v, row);
            } else if ("bigText".equals(v.getTag())) {
                return new NotificationBigTextTemplateViewWrapper(ctx, v, row);
            } else if ("media".equals(v.getTag()) || "bigMediaNarrow".equals(v.getTag())) {
                return new NotificationMediaTemplateViewWrapper(ctx, v, row);
            } else if ("messaging".equals(v.getTag())) {
                return new NotificationMessagingTemplateViewWrapper(ctx, v, row);
            }
            Class<? extends Notification.Style> style =
                    row.getEntry().notification.getNotification().getNotificationStyle();
            if (Notification.DecoratedCustomViewStyle.class.equals(style)) {
                return new NotificationDecoratedCustomViewWrapper(ctx, v, row);
            }
            return new NotificationTemplateViewWrapper(ctx, v, row);
        } else if (v instanceof NotificationHeaderView) {
            return new NotificationHeaderViewWrapper(ctx, v, row);
        } else {
            return new NotificationCustomViewWrapper(ctx, v, row);
        }
    }

    protected NotificationViewWrapper(Context ctx, View view, ExpandableNotificationRow row) {
        mView = view;
        mRow = row;
        onReinflated();
    }

    /**
     * Notifies this wrapper that the content of the view might have changed.
     * @param row the row this wrapper is attached to
     */
    public void onContentUpdated(ExpandableNotificationRow row) {
    }

    public void onReinflated() {
        if (shouldClearBackgroundOnReapply()) {
            mBackgroundColor = 0;
        }
        int backgroundColor = getBackgroundColor(mView);
        if (backgroundColor != Color.TRANSPARENT) {
            mBackgroundColor = backgroundColor;
            mView.setBackground(new ColorDrawable(Color.TRANSPARENT));
        }
    }

    protected boolean needsInversion(int defaultBackgroundColor, View view) {
        if (view == null) {
            return false;
        }

        Configuration configuration = mView.getResources().getConfiguration();
        boolean nightMode = (configuration.uiMode & Configuration.UI_MODE_NIGHT_MASK)
                == Configuration.UI_MODE_NIGHT_YES;
        if (!nightMode) {
            return false;
        }

        // Apps targeting Q should fix their dark mode bugs.
        if (mRow.getEntry().targetSdk >= Build.VERSION_CODES.Q) {
            return false;
        }

        int background = getBackgroundColor(view);
        if (background == Color.TRANSPARENT) {
            background = defaultBackgroundColor;
        }
        if (background == Color.TRANSPARENT) {
            background = resolveBackgroundColor();
        }

        float[] hsl = new float[] {0f, 0f, 0f};
        ColorUtils.colorToHSL(background, hsl);

        // Notifications with colored backgrounds should not be inverted
        if (hsl[1] != 0) {
            return false;
        }

        // Invert white or light gray backgrounds.
        boolean isLightGrayOrWhite = hsl[1] == 0 && hsl[2] > 0.5;
        if (isLightGrayOrWhite) {
            return true;
        }

        // Now let's check if there's unprotected text somewhere, and invert if we find it.
        if (view instanceof ViewGroup) {
            return childrenNeedInversion(background, (ViewGroup) view);
        } else {
            return false;
        }
    }

    @VisibleForTesting
    boolean childrenNeedInversion(@ColorInt int parentBackground, ViewGroup viewGroup) {
        if (viewGroup == null) {
            return false;
        }

        int backgroundColor = getBackgroundColor(viewGroup);
        if (Color.alpha(backgroundColor) != 255) {
            backgroundColor = ContrastColorUtil.compositeColors(backgroundColor, parentBackground);
            backgroundColor = ColorUtils.setAlphaComponent(backgroundColor, 255);
        }
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);
            if (child instanceof TextView) {
                int foreground = ((TextView) child).getCurrentTextColor();
                if (ColorUtils.calculateContrast(foreground, backgroundColor) < 3) {
                    return true;
                }
            } else if (child instanceof ViewGroup) {
                if (childrenNeedInversion(backgroundColor, (ViewGroup) child)) {
                    return true;
                }
            }
        }

        return false;
    }

    protected int getBackgroundColor(View view) {
        if (view == null) {
            return Color.TRANSPARENT;
        }
        Drawable background = view.getBackground();
        if (background instanceof ColorDrawable) {
            return ((ColorDrawable) background).getColor();
        }
        return Color.TRANSPARENT;
    }

    protected void invertViewLuminosity(View view) {
        Paint paint = new Paint();
        ColorMatrix matrix = new ColorMatrix();
        ColorMatrix tmp = new ColorMatrix();
        // Inversion should happen on Y'UV space to conserve the colors and
        // only affect the luminosity.
        matrix.setRGB2YUV();
        tmp.set(new float[]{
                -1f, 0f, 0f, 0f, 255f,
                0f, 1f, 0f, 0f, 0f,
                0f, 0f, 1f, 0f, 0f,
                0f, 0f, 0f, 1f, 0f
        });
        matrix.postConcat(tmp);
        tmp.setYUV2RGB();
        matrix.postConcat(tmp);
        paint.setColorFilter(new ColorMatrixColorFilter(matrix));
        view.setLayerType(View.LAYER_TYPE_HARDWARE, paint);
    }

    protected boolean shouldClearBackgroundOnReapply() {
        return true;
    }

    /**
     * Update the appearance of the expand button.
     *
     * @param expandable should this view be expandable
     * @param onClickListener the listener to invoke when the expand affordance is clicked on
     */
    public void updateExpandability(boolean expandable, View.OnClickListener onClickListener) {}

    /**
     * @return the notification header if it exists
     */
    public NotificationHeaderView getNotificationHeader() {
        return null;
    }

    public int getHeaderTranslation() {
        return 0;
    }

    @Override
    public TransformState getCurrentState(int fadingView) {
        return null;
    }

    @Override
    public void transformTo(TransformableView notification, Runnable endRunnable) {
        // By default we are fading out completely
        CrossFadeHelper.fadeOut(mView, endRunnable);
    }

    @Override
    public void transformTo(TransformableView notification, float transformationAmount) {
        CrossFadeHelper.fadeOut(mView, transformationAmount);
    }

    @Override
    public void transformFrom(TransformableView notification) {
        // By default we are fading in completely
        CrossFadeHelper.fadeIn(mView);
    }

    @Override
    public void transformFrom(TransformableView notification, float transformationAmount) {
        CrossFadeHelper.fadeIn(mView, transformationAmount);
    }

    @Override
    public void setVisible(boolean visible) {
        mView.animate().cancel();
        mView.setVisibility(visible ? View.VISIBLE : View.INVISIBLE);
    }

    public int getCustomBackgroundColor() {
        // Parent notifications should always use the normal background color
        return mRow.isSummaryWithChildren() ? 0 : mBackgroundColor;
    }

    protected int resolveBackgroundColor() {
        int customBackgroundColor = getCustomBackgroundColor();
        if (customBackgroundColor != 0) {
            return customBackgroundColor;
        }
        return mView.getContext().getColor(
                com.android.internal.R.color.notification_material_background_color);
    }

    public void setLegacy(boolean legacy) {
    }

    public void setContentHeight(int contentHeight, int minHeightHint) {
    }

    public void setRemoteInputVisible(boolean visible) {
    }

    public void setIsChildInGroup(boolean isChildInGroup) {
    }

    public boolean isDimmable() {
        return true;
    }

    public boolean disallowSingleClick(float x, float y) {
        return false;
    }

    public int getMinLayoutHeight() {
        return 0;
    }

    public boolean shouldClipToRounding(boolean topRounded, boolean bottomRounded) {
        return false;
    }

    public void setHeaderVisibleAmount(float headerVisibleAmount) {
    }

    /**
     * Get the extra height that needs to be added to this view, such that it can be measured
     * normally.
     */
    public int getExtraMeasureHeight() {
        return 0;
    }
}
