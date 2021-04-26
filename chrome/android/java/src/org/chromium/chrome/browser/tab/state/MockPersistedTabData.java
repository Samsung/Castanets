// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;

import java.nio.ByteBuffer;

/**
 * MockPersistedTabData object used for testing
 */
public class MockPersistedTabData extends PersistedTabData {
    private int mField;

    /**
     * @param tab   tab associated with {@link MockPersistedTabData}
     * @param field field stored in {@link MockPersistedTabData}
     */
    public MockPersistedTabData(Tab tab, int field) {
        super(tab,
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .storage,
                PersistedTabDataConfiguration.get(MockPersistedTabData.class, tab.isIncognito())
                        .id);
        mField = field;
    }

    private MockPersistedTabData(Tab tab, byte[] data, PersistedTabDataStorage storage, String id) {
        super(tab, data, storage, id);
    }

    /**
     * Acquire {@link MockPersistedTabData} from storage or create it and
     * associate with {@link Tab}
     * @param tab      {@link Tab} {@link MockPersistedTabData} will be associated with
     * @param callback callback {@link MockPersistedTabData} will be passed back in
     */
    public static void from(Tab tab, Callback<MockPersistedTabData> callback) {
        PersistedTabData.from(tab,
                (data, storage, id)
                        -> { return new MockPersistedTabData(tab, data, storage, id); },
                ()
                        -> {
                    return null; /** Currently unused */
                },
                MockPersistedTabData.class, callback);
    }

    /**
     * @return field stored in {@link MockPersistedTabData}
     */
    public int getField() {
        return mField;
    }

    /**
     * Sets field
     * @param field new value of field
     */
    public void setField(int field) {
        mField = field;
        save();
    }

    @Override
    public byte[] serialize() {
        return ByteBuffer.allocate(4).putInt(mField).array();
    }

    @Override
    public void deserialize(byte[] data) {
        mField = ByteBuffer.wrap(data).getInt();
    }

    @Override
    public void destroy() {}
}
