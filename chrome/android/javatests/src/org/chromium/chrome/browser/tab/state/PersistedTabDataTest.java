// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.support.test.annotation.UiThreadTest;
import android.support.test.filters.SmallTest;
import android.support.test.rule.UiThreadTestRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Test relating to {@link PersistedTabData}
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PersistedTabDataTest {
    private static final int INITIAL_VALUE = 42;
    private static final int CHANGED_VALUE = 51;

    @Rule
    public UiThreadTestRule mRule = new UiThreadTestRule();

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
    }

    @SmallTest
    @UiThreadTest
    @Test
    public void testCacheCallbacks() throws InterruptedException {
        Tab tab = new MockTab(1, false);
        MockPersistedTabData mockPersistedTabData = new MockPersistedTabData(tab, INITIAL_VALUE);
        mockPersistedTabData.save();
        // 1
        MockPersistedTabData.from(tab, (res) -> {
            Assert.assertEquals(INITIAL_VALUE, res.getField());
            tab.getUserDataHost().getUserData(MockPersistedTabData.class).setField(CHANGED_VALUE);
            // Caching callbacks means 2) shouldn't overwrite CHANGED_VALUE
            // back to INITIAL_VALUE in the callback.
            MockPersistedTabData.from(
                    tab, (ares) -> { Assert.assertEquals(CHANGED_VALUE, ares.getField()); });
        });
        // 2
        MockPersistedTabData.from(tab, (res) -> {
            Assert.assertEquals(CHANGED_VALUE, res.getField());
            mockPersistedTabData.delete();
        });
    }
}
