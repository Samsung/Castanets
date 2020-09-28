package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.chrome.R;

public class CastanetsFragment extends Fragment implements FirstRunFragment {
    public static class Page implements FirstRunPage<CastanetsFragment> {
        @Override
        public boolean shouldSkipPageOnCreate() {
            return FirstRunStatus.shouldSkipWelcomePage();
        }

        @Override
        public CastanetsFragment instantiateFragment() {
            return new CastanetsFragment();
        }
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.castanets_welcome, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
    }
}
