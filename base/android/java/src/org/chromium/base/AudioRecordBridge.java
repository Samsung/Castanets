package org.chromium.base;

import java.nio.ByteBuffer;

public abstract class AudioRecordBridge {
    public static AudioRecordBridge sAudioRecord;

    public static ByteBuffer sAudioBuffer;

    public AudioRecordBridge() {
        sAudioRecord = this;
    }

    public abstract void OnData(int bytesRead);

    public static AudioRecordBridge getAudioRecord() {
        return sAudioRecord;
    }
}
