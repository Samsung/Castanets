/*
 * Copyright (C) 2019 The Android Open Source Project
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
package android.telephony.ims;

import android.annotation.WorkerThread;

/**
 * This is a single instance of a message received over RCS.
 *
 * @hide
 */
public class RcsIncomingMessage extends RcsMessage {
    /**
     * @hide
     */
    RcsIncomingMessage(RcsControllerCall rcsControllerCall, int id) {
        super(rcsControllerCall, id);
    }

    /**
     * Sets the timestamp of arrival for this message and persists into storage. The timestamp is
     * defined as milliseconds passed after midnight, January 1, 1970 UTC
     *
     * @param arrivalTimestamp The timestamp to set to.
     * @throws RcsMessageStoreException if the value could not be persisted into storage
     */
    @WorkerThread
    public void setArrivalTimestamp(long arrivalTimestamp) throws RcsMessageStoreException {
        mRcsControllerCall.callWithNoReturn(
                (iRcs, callingPackage) -> iRcs.setMessageArrivalTimestamp(mId, true,
                        arrivalTimestamp, callingPackage));
    }

    /**
     * @return Returns the timestamp of arrival for this message. The timestamp is defined as
     * milliseconds passed after midnight, January 1, 1970 UTC
     * @throws RcsMessageStoreException if the value could not be read from the storage
     */
    @WorkerThread
    public long getArrivalTimestamp() throws RcsMessageStoreException {
        return mRcsControllerCall.call(
                (iRcs, callingPackage) -> iRcs.getMessageArrivalTimestamp(mId, true,
                        callingPackage));
    }

    /**
     * Sets the timestamp of when the user saw this message and persists into storage. The timestamp
     * is defined as milliseconds passed after midnight, January 1, 1970 UTC
     *
     * @param notifiedTimestamp The timestamp to set to.
     * @throws RcsMessageStoreException if the value could not be persisted into storage
     */
    @WorkerThread
    public void setSeenTimestamp(long notifiedTimestamp) throws RcsMessageStoreException {
        mRcsControllerCall.callWithNoReturn(
                (iRcs, callingPackage) -> iRcs.setMessageSeenTimestamp(mId, true, notifiedTimestamp,
                        callingPackage));
    }

    /**
     * @return Returns the timestamp of when the user saw this message. The timestamp is defined as
     * milliseconds passed after midnight, January 1, 1970 UTC
     * @throws RcsMessageStoreException if the value could not be read from the storage
     */
    @WorkerThread
    public long getSeenTimestamp() throws RcsMessageStoreException {
        return mRcsControllerCall.call(
                (iRcs, callingPackage) -> iRcs.getMessageSeenTimestamp(mId, true, callingPackage));
    }

    /**
     * @return Returns the sender of this incoming message.
     * @throws RcsMessageStoreException if the value could not be read from the storage
     */
    @WorkerThread
    public RcsParticipant getSenderParticipant() throws RcsMessageStoreException {
        return new RcsParticipant(
                mRcsControllerCall,
                mRcsControllerCall.call(
                        (iRcs, callingPackage) -> iRcs.getSenderParticipant(mId, callingPackage)));
    }

    /**
     * @return Returns {@code true} as this is an incoming message
     */
    @Override
    public boolean isIncoming() {
        return true;
    }
}
