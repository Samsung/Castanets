package com.android.server.location;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.platform.test.annotations.Presubmit;

import com.android.server.location.GnssBatchingProvider.GnssBatchingProviderNative;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;

/**
 * Unit tests for {@link GnssBatchingProvider}.
 */
@RunWith(RobolectricTestRunner.class)
@Presubmit
public class GnssBatchingProviderTest {

    private static final long PERIOD_NANOS = (long) 1e9;
    private static final boolean WAKE_ON_FIFO_FULL = true;
    private static final int BATCH_SIZE = 3;
    @Mock
    private GnssBatchingProviderNative mMockNative;
    private GnssBatchingProvider mTestProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mMockNative.initBatching()).thenReturn(true);
        when(mMockNative.startBatch(anyLong(), anyBoolean())).thenReturn(true);
        when(mMockNative.stopBatch()).thenReturn(true);
        when(mMockNative.getBatchSize()).thenReturn(BATCH_SIZE);
        mTestProvider = new GnssBatchingProvider(mMockNative);
        mTestProvider.enable();
        mTestProvider.start(PERIOD_NANOS, WAKE_ON_FIFO_FULL);
    }

    @Test
    public void start_nativeStarted() {
        verify(mMockNative).startBatch(eq(PERIOD_NANOS), eq(WAKE_ON_FIFO_FULL));
    }

    @Test
    public void stop_nativeStopped() {
        mTestProvider.stop();
        verify(mMockNative).stopBatch();
    }

    @Test
    public void flush_nativeFlushed() {
        mTestProvider.flush();
        verify(mMockNative).flushBatch();
    }

    @Test
    public void getBatchSize_nativeGetBatchSize() {
        assertThat(mTestProvider.getBatchSize()).isEqualTo(BATCH_SIZE);
    }

    @Test
    public void started_resume_started() {
        mTestProvider.resumeIfStarted();
        verify(mMockNative, times(2)).startBatch(eq(PERIOD_NANOS), eq(WAKE_ON_FIFO_FULL));
    }

    @Test
    public void stopped_resume_notStarted() {
        mTestProvider.stop();
        mTestProvider.resumeIfStarted();
        verify(mMockNative, times(1)).startBatch(eq(PERIOD_NANOS), eq(WAKE_ON_FIFO_FULL));
    }
}
