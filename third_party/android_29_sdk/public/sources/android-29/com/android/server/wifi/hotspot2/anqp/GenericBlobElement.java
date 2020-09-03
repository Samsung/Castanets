package com.android.server.wifi.hotspot2.anqp;

import com.android.server.wifi.hotspot2.Utils;

import java.nio.ByteBuffer;

/**
 * ANQP Element to hold a raw, unparsed, octet blob
 */
public class GenericBlobElement extends ANQPElement {
    private final byte[] mData;

    public GenericBlobElement(Constants.ANQPElementType infoID, ByteBuffer payload) {
        super(infoID);
        mData = new byte[payload.remaining()];
        payload.get(mData);
    }

    public byte[] getData() {
        return mData;
    }

    @Override
    public String toString() {
        return "Element ID " + getID() + ": " + Utils.toHexString(mData);
    }
}
