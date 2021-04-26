// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.os.Handler;
import android.os.Message;
import android.util.SparseArray;
import android.view.View;

import org.junit.Assert;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.CalledByNativeJavaTest;
import org.chromium.base.annotations.NativeJavaTestFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.ToolbarDataProvider;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Tests for {@link AutocompleteMediator}.
 */
public class AutocompleteMediatorUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;

    @Mock
    AutocompleteDelegate mAutocompleteDelegate;

    @Mock
    UrlBarEditingTextStateProvider mTextStateProvider;

    @Mock
    SuggestionProcessor mMockProcessor;

    @Mock
    HeaderProcessor mMockHeaderProcessor;

    @Mock
    AutocompleteController mAutocompleteController;

    @Mock
    ToolbarDataProvider mToolbarDataProvider;

    @Mock
    Handler mHandler;

    private PropertyModel mListModel;
    private AutocompleteMediator mMediator;
    private List<OmniboxSuggestion> mSuggestionsList;
    private ModelList mSuggestionModels;

    @CalledByNative
    private AutocompleteMediatorUnitTest() {}

    @CalledByNative
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSuggestionModels = new ModelList();
        mListModel = new PropertyModel(SuggestionListProperties.ALL_KEYS);
        mListModel.set(SuggestionListProperties.SUGGESTION_MODELS, mSuggestionModels);

        mMediator = new AutocompleteMediator(ContextUtils.getApplicationContext(),
                mAutocompleteDelegate, mTextStateProvider, mAutocompleteController, mListModel,
                mHandler);
        mMediator.setToolbarDataProvider(mToolbarDataProvider);
        mMediator.getDropdownItemViewInfoListBuilderForTest().registerSuggestionProcessor(
                mMockProcessor);
        mMediator.getDropdownItemViewInfoListBuilderForTest().setHeaderProcessorForTest(
                mMockHeaderProcessor);
        mMediator.setSuggestionVisibilityState(
                AutocompleteMediator.SuggestionVisibilityState.ALLOWED);

        when(mMockProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockProcessor.getMinimumViewHeight()).thenReturn(SUGGESTION_MIN_HEIGHT);
        when(mMockProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mMockHeaderProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockHeaderProcessor.getMinimumViewHeight()).thenReturn(HEADER_MIN_HEIGHT);
        when(mMockHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        when(mHandler.sendMessageAtTime(any(Message.class), anyLong()))
                .thenAnswer(new Answer<Void>() {
                    @Override
                    public Void answer(InvocationOnMock invocation) {
                        ((Message) invocation.getArguments()[0]).getCallback().run();
                        return null;
                    }
                });

        mSuggestionsList = buildDummySuggestionsList(10, "Suggestion");
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(true);
    }

    /**
     * Build a fake suggestions list with elements named 'Suggestion #', where '#' is the suggestion
     * index (1-based).
     *
     * @return List of suggestions.
     */
    private List<OmniboxSuggestion> buildDummySuggestionsList(int count, String prefix) {
        List<OmniboxSuggestion> list = new ArrayList<>();
        for (int index = 0; index < count; ++index) {
            list.add(OmniboxSuggestionBuilderForTest
                             .searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                             .setDisplayText(prefix + (index + 1))
                             .build());
        }

        return list;
    }

    /**
     * Build a fake group headers map with elements named 'Header #', where '#' is the group header
     * index (1-based) and 'Header' is the supplied prefix. Each header has a corresponding key
     * computed as baseKey + #.
     *
     * @param count Number of group headers to build.
     * @param baseKey Key of the first group header.
     * @param prefix Name prefix for each group.
     * @return Map of group headers (populated in random order).
     */
    private SparseArray<String> buildDummyGroupHeaders(int count, int baseKey, String prefix) {
        SparseArray<String> headers = new SparseArray<>(count);
        for (int index = 0; index < count; index++) {
            headers.put(baseKey + index, prefix + " " + (index + 1));
        }

        return headers;
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void updateSuggestionsList_notEffectiveWhenDisabled() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 2;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");

        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        Assert.assertTrue(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithNullList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(null, null), "");

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_worksWithEmptyList() {
        mMediator.onNativeInitialized();

        final int maximumListHeight = SUGGESTION_MIN_HEIGHT * 7;

        mMediator.onSuggestionDropdownHeightChanged(maximumListHeight);
        mMediator.onSuggestionsReceived(new AutocompleteResult(new ArrayList<>(), null), "");

        Assert.assertEquals(0, mSuggestionModels.size());
        Assert.assertFalse(mListModel.get(SuggestionListProperties.VISIBLE));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_defersKeyboardPopupWhenHaveLotsOfSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void
    updateSuggestionsList_showsKeyboardWhenHaveFewSuggestionsToShow() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(
                        mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), null),
                "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_scrolEventsWithConcealedItemsTogglesKeyboardVisibility() {
        mMediator.onNativeInitialized();

        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 2) * SUGGESTION_MIN_HEIGHT;

        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // With fully concealed elements, scroll should trigger keyboard hide.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownScroll();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(false));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(true));

        // Pretend that the user scrolled back to top with an overscroll.
        // This should bring back the soft keyboard.
        reset(mAutocompleteDelegate);
        mMediator.onSuggestionDropdownOverscrolledToTop();
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));
        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_updateHeightWhenHardwareKeyboardIsConnected() {
        // Simulates behavior of physical keyboard being attached to the device.
        // In this scenario, requesting keyboard to come up will not result with an actual
        // keyboard showing on the screen. As a result, the updated height should be used
        // when estimating presence of fully concealed items on the suggestions list.
        //
        // Attaching and detaching physical keyboard will affect the space on the screen, but since
        // the list of suggestions does not change, we are keeping them in exactly the same order
        // (and keep the grouping prior to the change).
        // The grouping is only affected, when the new list is provided (as a result of user's
        // input).
        mMediator.onNativeInitialized();

        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // This will request keyboard to show up upon receiving next suggestions list.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(true);
        // Report the height of the suggestions list, indicating that the keyboard is not visible.
        // In both cases, the updated suggestions list height should be used to estimate presence of
        // fully concealed items on the suggestions list.
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertFalse(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // Build separate list of suggestions so that these are accepted as a new set.
        // We want to follow the same restrictions as the original list (specifically: have a
        // resulting list of suggestions taller than the space in dropdown view), so make sure
        // the list sizes are same.
        List<OmniboxSuggestion> newList =
                buildDummySuggestionsList(mSuggestionsList.size(), "SuggestionB");
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(newList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_rejectsHeightUpdatesWhenKeyboardIsHidden() {
        // Simulates scenario where we receive dropdown height update after software keyboard is
        // explicitly hidden. In this scenario the updates should be rejected when estimating
        // presence of fully concealed items on the suggestions list.
        mMediator.onNativeInitialized();

        final int heightOfOAllSuggestions = mSuggestionsList.size() * SUGGESTION_MIN_HEIGHT;
        final int heightWithOneConcealedItem =
                (mSuggestionsList.size() - 1) * SUGGESTION_MIN_HEIGHT;

        // Report height change with keyboard visible
        mMediator.onSuggestionDropdownHeightChanged(heightWithOneConcealedItem);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());

        // "Hide keyboard", report larger area and re-evaluate the results. We should see no
        // difference, as the logic should only evaluate presence of items concealed when keyboard
        // is active.
        when(mAutocompleteDelegate.isKeyboardActive()).thenReturn(false);
        mMediator.onSuggestionDropdownHeightChanged(heightOfOAllSuggestions);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertTrue(
                mMediator.getDropdownItemViewInfoListBuilderForTest().hasFullyConcealedElements());
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @NativeJavaTestFeatures.Enable(ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP)
    public void updateSuggestionsList_retainsKeyboardOnScrollWithFewSuggestions() {
        mMediator.onNativeInitialized();
        mMediator.signalPendingKeyboardShowDecision();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(
                        mSuggestionsList.subList(0, MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW), null),
                "");
        verify(mAutocompleteDelegate, times(1)).setKeyboardVisibility(eq(true));

        // Should perform no action.
        mMediator.onSuggestionDropdownScroll();
        // Should perform no action.
        mMediator.onSuggestionDropdownScroll();

        verify(mAutocompleteDelegate, never()).setKeyboardVisibility(eq(false));
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_emptyTextTriggersZeroSuggest() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("", "");
        verify(mAutocompleteController)
                .startZeroSuggest(profile, "", url, pageClassification, title);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_nonEmptyTextTriggersSuggestions() {
        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        verify(mAutocompleteController)
                .start(profile, url, pageClassification, "test", 4, false, null, false);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onTextChanged_cancelsPendingRequests() {
        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.shouldAutocomplete()).thenReturn(true);
        when(mTextStateProvider.getSelectionStart()).thenReturn(4);
        when(mTextStateProvider.getSelectionEnd()).thenReturn(4);

        mMediator.onNativeInitialized();
        mMediator.onTextChanged("test", "testing");
        mMediator.onTextChanged("nottest", "nottesting");
        verify(mAutocompleteController)
                .start(profile, url, pageClassification, "nottest", 4, false, null, false);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onSuggestionsReceived_sendsOnSuggestionsChanged() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete");
        verify(mAutocompleteDelegate).onSuggestionsChanged("inline_autocomplete");

        // Ensure duplicate requests are suppressed.
        mMediator.onSuggestionsReceived(
                new AutocompleteResult(mSuggestionsList, null), "inline_autocomplete2");
        verifyNoMoreInteractions(mAutocompleteDelegate);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_beforeInitialization() {
        mMediator.onNativeInitialized();
        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void setLayoutDirection_afterInitialization() {
        mMediator.onNativeInitialized();
        mMediator.onSuggestionDropdownHeightChanged(Integer.MAX_VALUE);
        mMediator.onSuggestionsReceived(new AutocompleteResult(mSuggestionsList, null), "");
        Assert.assertEquals(mSuggestionsList.size(), mSuggestionModels.size());

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_RTL,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }

        mMediator.setLayoutDirection(View.LAYOUT_DIRECTION_LTR);
        for (int i = 0; i < mSuggestionModels.size(); i++) {
            Assert.assertEquals(i + "th model does not have the expected layout direction.",
                    View.LAYOUT_DIRECTION_LTR,
                    mSuggestionModels.get(i).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
        }
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onUrlFocusChange_triggersZeroSuggest_nativeInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn(url);

        mMediator.onNativeInitialized();
        mMediator.onUrlFocusChange(true);
        verify(mAutocompleteController)
                .startZeroSuggest(profile, url, url, pageClassification, title);
    }

    @CalledByNativeJavaTest
    @NativeJavaTestFeatures.Disable({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_DEFERRED_KEYBOARD_POPUP})
    public void onUrlFocusChange_triggersZeroSuggest_nativeNotInitialized() {
        when(mAutocompleteDelegate.isUrlBarFocused()).thenReturn(true);
        when(mAutocompleteDelegate.didFocusUrlFromFakebox()).thenReturn(false);

        Profile profile = Mockito.mock(Profile.class);
        String url = "http://www.example.com";
        String title = "Title";
        int pageClassification = 2;
        when(mToolbarDataProvider.getProfile()).thenReturn(profile);
        when(mToolbarDataProvider.getCurrentUrl()).thenReturn(url);
        when(mToolbarDataProvider.getTitle()).thenReturn(title);
        when(mToolbarDataProvider.hasTab()).thenReturn(true);
        when(mToolbarDataProvider.getPageClassification(false)).thenReturn(pageClassification);

        when(mTextStateProvider.getTextWithAutocomplete()).thenReturn("");

        // Signal focus prior to initializing native.
        mMediator.onUrlFocusChange(true);

        // Initialize native and ensure zero suggest is triggered.
        mMediator.onNativeInitialized();
        verify(mAutocompleteController)
                .startZeroSuggest(profile, "", url, pageClassification, title);
    }
}
