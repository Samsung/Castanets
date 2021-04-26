// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.share_sheet;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anySet;
import static org.mockito.Matchers.any;

import android.app.Activity;
import android.support.test.rule.ActivityTestRule;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.DummyUiActivity;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests {@link ShareSheetCoordinator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public final class ShareSheetCoordinatorTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Rule
    public ActivityTestRule<DummyUiActivity> mActivityTestRule =
            new ActivityTestRule<>(DummyUiActivity.class);

    @Rule
    public TestRule mFeatureProcessor = new Features.JUnitProcessor();

    @Mock
    private ShareSheetPropertyModelBuilder mPropertyModelBuilder;

    @Mock
    private BottomSheetController mController;

    private ShareSheetCoordinator mShareSheetCoordinator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        PropertyModel testModel1 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel1")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, false)
                                           .build();
        PropertyModel testModel2 = new PropertyModel.Builder(ShareSheetItemViewProperties.ALL_KEYS)
                                           .with(ShareSheetItemViewProperties.ICON, null)
                                           .with(ShareSheetItemViewProperties.LABEL, "testModel2")
                                           .with(ShareSheetItemViewProperties.CLICK_LISTENER, null)
                                           .with(ShareSheetItemViewProperties.IS_FIRST_PARTY, false)
                                           .build();

        ArrayList<PropertyModel> thirdPartyPropertyModels =
                new ArrayList<>(Arrays.asList(testModel1, testModel2));
        Mockito.when(mPropertyModelBuilder.selectThirdPartyApps(
                             any(), anySet(), any(), anyBoolean(), anyLong()))
                .thenReturn(thirdPartyPropertyModels);

        mShareSheetCoordinator =
                new ShareSheetCoordinator(mController, null, mPropertyModelBuilder, null, null);
    }

    @Test
    @MediumTest
    public void disableFirstPartyFeatures() {
        mShareSheetCoordinator.disableFirstPartyFeaturesForTesting();
        Activity activity = mActivityTestRule.getActivity();

        List<PropertyModel> propertyModels =
                mShareSheetCoordinator.createTopRowPropertyModels(activity,
                        /*shareParams=*/null, /*chromeShareExtras=*/null,
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES);
        assertEquals("Property model list should be empty.", 0, propertyModels.size());
    }

    @Test
    @MediumTest
    public void testCreateBottomRowPropertyModels() {
        Activity activity = mActivityTestRule.getActivity();

        List<PropertyModel> propertyModels =
                mShareSheetCoordinator.createBottomRowPropertyModels(activity, /*shareParams=*/null,
                        ShareSheetPropertyModelBuilder.ALL_CONTENT_TYPES, /*saveLastUsed=*/false);
        assertEquals("Incorrect number of property models.", 3, propertyModels.size());
        assertEquals("First property model isn't testModel1.", "testModel1",
                propertyModels.get(0).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("First property model is marked as first party.", false,
                propertyModels.get(0).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        assertEquals("Second property model isn't testModel2.", "testModel2",
                propertyModels.get(1).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Second property model is marked as first party.", false,
                propertyModels.get(1).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
        assertEquals("Third property model isn't More.",
                activity.getResources().getString(R.string.sharing_more_icon_label),
                propertyModels.get(2).get(ShareSheetItemViewProperties.LABEL));
        assertEquals("Third property model isn't marked as first party.", true,
                propertyModels.get(2).get(ShareSheetItemViewProperties.IS_FIRST_PARTY));
    }
}
