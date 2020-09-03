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
 * limitations under the License.
 */

package android.support.v4.graphics;

import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.test.AndroidTestCase;
import android.test.suitebuilder.annotation.SmallTest;

import static org.mockito.Mockito.*;

public class DrawableCompatTest extends AndroidTestCase {

    @SmallTest
    public void testDrawableUnwrap() {
        final Drawable original = new GradientDrawable();
        final Drawable wrappedDrawable = DrawableCompat.wrap(original);
        assertSame(original, DrawableCompat.unwrap(wrappedDrawable));
    }

    @SmallTest
    public void testDrawableChangeBoundsCopy() {
        final Rect bounds = new Rect(0, 0, 10, 10);

        final Drawable drawable = new GradientDrawable();

        final Drawable wrapper = DrawableCompat.wrap(drawable);
        wrapper.setBounds(bounds);

        // Assert that the bounds were given to the original drawable
        assertEquals(bounds, drawable.getBounds());
    }

    @SmallTest
    public void testDrawableWrapOnlyWrapsOnce() {
        final Drawable wrappedDrawable = DrawableCompat.wrap(new GradientDrawable());
        assertSame(wrappedDrawable, DrawableCompat.wrap(wrappedDrawable));
    }

    @SmallTest
    public void testWrapMutatedDrawableHasConstantState() {
        // First create a Drawable, and mutated it so that it has a constant state
        Drawable drawable = new GradientDrawable();
        drawable = drawable.mutate();
        assertNotNull(drawable.getConstantState());

        // Now wrap and assert that the wrapper also returns a constant state
        final Drawable wrapper = DrawableCompat.wrap(drawable);
        assertNotNull(wrapper.getConstantState());
    }

    @SmallTest
    public void testWrappedDrawableHasCallbackSet() {
        // First create a Drawable
        final Drawable drawable = new GradientDrawable();

        // Now wrap it and set a mock as the wrapper's callback
        final Drawable wrapper = DrawableCompat.wrap(drawable);
        final Drawable.Callback mockCallback = mock(Drawable.Callback.class);
        wrapper.setCallback(mockCallback);

        // Now make the wrapped drawable invalidate itself
        drawable.invalidateSelf();

        // ...and verify that the wrapper calls to be invalidated
        verify(mockCallback, times(1)).invalidateDrawable(wrapper);
    }

}