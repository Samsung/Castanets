package org.chromium.media;

import android.annotation.SuppressLint;
import android.graphics.ImageFormat;
import android.graphics.PixelFormat;
import android.media.Image;
import android.util.Log;

import java.io.Serializable;
import java.nio.ByteBuffer;

@SuppressLint("NewApi")
public class ImageWrapper implements Serializable {
    private static final String TAG = "[Service_Offloading] ImageWrapper";

    private static final int NUM_OF_PLANES_FOR_RGBA8888 = 1;
    private static final int NUM_OF_PLANES_FOR_YUV420 = 3;

    public ImageWrapper(Image image) {
        super();
        format = image.getFormat();

        switch (image.getFormat()) {
            case PixelFormat.RGBA_8888:
                if (image.getPlanes().length != NUM_OF_PLANES_FOR_RGBA8888) {
                    Log.e(TAG,
                            "Unexpected image planes for RGBA_8888 format: "
                                    + image.getPlanes().length);
                    throw new IllegalStateException();
                }
                ScreenCaptureService.sVideoBuffer[0].rewind();
                ScreenCaptureService.sVideoBuffer[0].put(image.getPlanes()[0].getBuffer());
                rowStride[0] = image.getPlanes()[0].getRowStride();
                break;
            case ImageFormat.YUV_420_888:
                if (image.getPlanes().length != NUM_OF_PLANES_FOR_YUV420) {
                    Log.e(TAG,
                            "Unexpected image planes for YUV_420_888 format: "
                                    + image.getPlanes().length);
                    throw new IllegalStateException();
                }
                for (int i = 0; i < NUM_OF_PLANES_FOR_YUV420; i++) {
                    ScreenCaptureService.sVideoBuffer[i].rewind();
                    ScreenCaptureService.sVideoBuffer[i].put(image.getPlanes()[i].getBuffer());
                }
                rowStride[0] = image.getPlanes()[0].getRowStride();
                rowStride[1] = image.getPlanes()[1].getRowStride();
                pixelStride = image.getPlanes()[1].getPixelStride();
                break;
            default:
                Log.e(TAG, "Unexpected image format: " + image.getFormat());
                throw new IllegalStateException();
        }

        rectLeft = image.getCropRect().left;
        rectTop = image.getCropRect().top;
        rectWidth = image.getCropRect().width();
        rectHeight = image.getCropRect().height();
        timestamp = image.getTimestamp();
        }
        public int format;

        public int[] rowStride = new int[2];
        public int pixelStride;

        public int rectLeft;
        public int rectTop;
        public int rectWidth;
        public int rectHeight;
        public long timestamp;
}
