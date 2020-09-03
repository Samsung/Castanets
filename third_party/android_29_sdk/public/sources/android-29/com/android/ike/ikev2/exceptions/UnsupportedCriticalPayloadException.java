/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.ike.ikev2.exceptions;

import com.android.ike.ikev2.message.IkeNotifyPayload;

import java.util.List;

/**
 * This exception is thrown when payload type is not supported and critical bit is set
 *
 * <p>Include UNSUPPORTED_CRITICAL_PAYLOAD Notify payload in a response message containing the
 * payload type for each payload.
 *
 * @see <a href="https://tools.ietf.org/html/rfc7296#section-2.5">RFC 7296, Internet Key Exchange
 *     Protocol Version 2 (IKEv2)</a>
 */
public final class UnsupportedCriticalPayloadException extends IkeException {

    public final List<Integer> payloadTypeList;

    /**
     * Construct an instance of UnsupportedCriticalPayloadException
     *
     * @param payloadList the list of all unsupported critical payload types
     */
    public UnsupportedCriticalPayloadException(List<Integer> payloadList) {
        super(IkeNotifyPayload.NOTIFY_TYPE_UNSUPPORTED_CRITICAL_PAYLOAD);
        payloadTypeList = payloadList;
    }
}
