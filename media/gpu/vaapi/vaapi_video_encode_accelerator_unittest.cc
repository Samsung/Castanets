// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include <memory>
#include <numeric>
#include <vector>

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RunClosure;
using ::testing::_;
using ::testing::Return;
using ::testing::WithArgs;

namespace media {
namespace {

constexpr gfx::Size kDefaultEncodeSize(1280, 720);
constexpr uint32_t kDefaultBitrateBps = 4 * 1000 * 1000;
constexpr uint32_t kDefaultFramerate = 30;
constexpr size_t kMaxNumOfRefFrames = 3u;
const VideoEncodeAccelerator::Config kDefaultVideoEncodeAcceleratorConfig(
    PIXEL_FORMAT_I420,
    kDefaultEncodeSize,
    VP9PROFILE_PROFILE0,
    kDefaultBitrateBps,
    kDefaultFramerate);

MATCHER_P2(MatchesAcceleratedVideoEncoderConfig,
           max_ref_frames,
           bitrate_control,
           "") {
  return arg.max_num_ref_frames == max_ref_frames &&
         arg.bitrate_control == bitrate_control;
}

MATCHER_P2(MatchesBitstreamBufferMetadata, payload_size_bytes, key_frame, "") {
  return arg.payload_size_bytes == payload_size_bytes &&
         arg.key_frame == key_frame;
}

class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;
  virtual ~MockVideoEncodeAcceleratorClient() = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyError, void(VideoEncodeAccelerator::Error));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const VideoEncoderInfo&));
};

class MockVaapiWrapper : public VaapiWrapper {
 public:
  MockVaapiWrapper(CodecMode mode) : VaapiWrapper(mode) {}
  MOCK_METHOD2(GetVAEncMaxNumOfRefFrames, bool(VideoCodecProfile, size_t*));
  MOCK_METHOD5(CreateContextAndSurfaces,
               bool(unsigned int,
                    const gfx::Size&,
                    SurfaceUsageHint,
                    size_t,
                    std::vector<VASurfaceID>*));
  MOCK_METHOD2(CreateVABuffer, bool(size_t, VABufferID*));
  MOCK_METHOD2(GetEncodedChunkSize, uint64_t(VABufferID, VASurfaceID));
  MOCK_METHOD5(DownloadFromVABuffer,
               bool(VABufferID, VASurfaceID, uint8_t*, size_t, size_t*));
  MOCK_METHOD3(UploadVideoFrameToSurface,
               bool(const VideoFrame&, VASurfaceID, const gfx::Size&));
  MOCK_METHOD1(ExecuteAndDestroyPendingBuffers, bool(VASurfaceID));
  MOCK_METHOD1(DestroyVABuffer, void(VABufferID));
  MOCK_METHOD0(DestroyContext, void());
  MOCK_METHOD1(DestroySurfaces, void(std::vector<VASurfaceID> va_surface_ids));

 private:
  ~MockVaapiWrapper() override = default;
};

class MockAcceleratedVideoEncoder : public AcceleratedVideoEncoder {
 public:
  MOCK_METHOD2(Initialize,
               bool(const VideoEncodeAccelerator::Config&,
                    const AcceleratedVideoEncoder::Config&));
  MOCK_CONST_METHOD0(GetCodedSize, gfx::Size());
  MOCK_CONST_METHOD0(GetBitstreamBufferSize, size_t());
  MOCK_CONST_METHOD0(GetMaxNumOfRefFrames, size_t());
  MOCK_METHOD1(PrepareEncodeJob, bool(EncodeJob*));
  MOCK_METHOD1(BitrateControlUpdate, void(uint64_t));
  bool UpdateRates(const VideoBitrateAllocation&, uint32_t) override {
    return false;
  }
  ScalingSettings GetScalingSettings() const override {
    return ScalingSettings();
  }
};
}  // namespace

struct VaapiVideoEncodeAcceleratorTestParam;

class VaapiVideoEncodeAcceleratorTest
    : public ::testing::TestWithParam<VaapiVideoEncodeAcceleratorTestParam> {
 protected:
  VaapiVideoEncodeAcceleratorTest() = default;
  ~VaapiVideoEncodeAcceleratorTest() override = default;

  void SetUp() override {
    mock_vaapi_wrapper_ =
        base::MakeRefCounted<MockVaapiWrapper>(VaapiWrapper::kEncode);
    encoder_.reset(new VaapiVideoEncodeAccelerator);
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    vaapi_encoder->vaapi_wrapper_ = mock_vaapi_wrapper_;
    vaapi_encoder->encoder_ = std::make_unique<MockAcceleratedVideoEncoder>();
    mock_encoder_ = reinterpret_cast<MockAcceleratedVideoEncoder*>(
        vaapi_encoder->encoder_.get());
  }

  void SetDefaultMocksBehavior(const VideoEncodeAccelerator::Config& config) {
    ASSERT_TRUE(mock_vaapi_wrapper_);
    ASSERT_TRUE(mock_encoder_);

    ON_CALL(*mock_vaapi_wrapper_, GetVAEncMaxNumOfRefFrames)
        .WillByDefault(WithArgs<1>([](size_t* max_ref_frames) {
          *max_ref_frames = kMaxNumOfRefFrames;
          return true;
        }));

    ON_CALL(*mock_encoder_, GetBitstreamBufferSize)
        .WillByDefault(Return(config.input_visible_size.GetArea()));
    ON_CALL(*mock_encoder_, GetCodedSize())
        .WillByDefault(Return(config.input_visible_size));
    ON_CALL(*mock_encoder_, GetMaxNumOfRefFrames())
        .WillByDefault(Return(kMaxNumOfRefFrames));
  }

  bool InitializeVideoEncodeAccelerator(
      const VideoEncodeAccelerator::Config& config) {
    VideoEncodeAccelerator::SupportedProfile profile(config.output_profile,
                                                     config.input_visible_size);
    auto* vaapi_encoder =
        reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder_.get());
    vaapi_encoder->supported_profiles_for_testing_.push_back(profile);
    vaapi_encoder->aligned_va_surface_size_ = config.input_visible_size;
    if (config.input_visible_size.IsEmpty())
      return false;
    return encoder_->Initialize(config, &client_);
  }

  void InitializeSequenceForVP9(const VideoEncodeAccelerator::Config& config) {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    ::testing::InSequence s;
    constexpr auto kBitrateControl =
        AcceleratedVideoEncoder::BitrateControl::kConstantQuantizationParameter;
    EXPECT_CALL(*mock_encoder_,
                Initialize(_, MatchesAcceleratedVideoEncoderConfig(
                                  kMaxNumOfRefFrames, kBitrateControl)))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_vaapi_wrapper_,
                CreateContextAndSurfaces(
                    _, kDefaultEncodeSize,
                    VaapiWrapper::SurfaceUsageHint::kVideoEncoder, _, _))
        .WillOnce(WithArgs<3, 4>(
            [&surfaces = this->va_surfaces_](
                size_t num_surfaces, std::vector<VASurfaceID>* va_surface_ids) {
              surfaces.resize(num_surfaces);
              std::iota(surfaces.begin(), surfaces.end(), 0);
              *va_surface_ids = surfaces;
              return true;
            }));
    EXPECT_CALL(client_, RequireBitstreamBuffers(_, kDefaultEncodeSize, _))
        .WillOnce(WithArgs<2>([this, &quit_closure](size_t output_buffer_size) {
          this->output_buffer_size_ = output_buffer_size;
          quit_closure.Run();
        }));
    ASSERT_TRUE(InitializeVideoEncodeAccelerator(config));
    run_loop.Run();
  }

  void EncodeSequenceForVP9() {
    base::RunLoop run_loop;
    base::Closure quit_closure = run_loop.QuitClosure();
    ::testing::InSequence s;

    constexpr VABufferID kCodedBufferId = 123;
    EXPECT_CALL(*mock_vaapi_wrapper_, CreateVABuffer(output_buffer_size_, _))
        .WillOnce(WithArgs<1>([](VABufferID* va_buffer_id) {
          *va_buffer_id = kCodedBufferId;
          return true;
        }));

    ASSERT_FALSE(va_surfaces_.empty());
    const VASurfaceID kInputSurfaceId = va_surfaces_.back();
    EXPECT_CALL(*mock_encoder_, PrepareEncodeJob(_))
        .WillOnce(WithArgs<0>(
            [encoder = encoder_.get(), kCodedBufferId,
             kInputSurfaceId](AcceleratedVideoEncoder::EncodeJob* job) {
              job->AddPostExecuteCallback(base::BindOnce(
                  &VaapiVideoEncodeAccelerator::NotifyEncodedChunkSize,
                  base::Unretained(
                      reinterpret_cast<VaapiVideoEncodeAccelerator*>(encoder)),
                  kCodedBufferId, kInputSurfaceId));
              return true;
            }));
    EXPECT_CALL(
        *mock_vaapi_wrapper_,
        UploadVideoFrameToSurface(_, kInputSurfaceId, kDefaultEncodeSize))
        .WillOnce(Return(true));
    EXPECT_CALL(*mock_vaapi_wrapper_,
                ExecuteAndDestroyPendingBuffers(kInputSurfaceId))
        .WillOnce(Return(true));

    constexpr uint64_t kEncodedChunkSize = 1234;
    ASSERT_LE(kEncodedChunkSize, output_buffer_size_);
    EXPECT_CALL(*mock_vaapi_wrapper_,
                GetEncodedChunkSize(kCodedBufferId, kInputSurfaceId))
        .WillOnce(Return(kEncodedChunkSize));
    EXPECT_CALL(*mock_encoder_, BitrateControlUpdate(kEncodedChunkSize))
        .WillOnce(Return());
    EXPECT_CALL(*mock_vaapi_wrapper_,
                DownloadFromVABuffer(kCodedBufferId, kInputSurfaceId, _,
                                     output_buffer_size_, _))
        .WillOnce(WithArgs<4>([](size_t* coded_data_size) {
          *coded_data_size = kEncodedChunkSize;
          return true;
        }));
    EXPECT_CALL(*mock_vaapi_wrapper_, DestroyVABuffer(kCodedBufferId))
        .WillOnce(Return());

    constexpr int32_t kBitstreamId = 12;
    EXPECT_CALL(client_, BitstreamBufferReady(kBitstreamId,
                                              MatchesBitstreamBufferMetadata(
                                                  kEncodedChunkSize, false)))
        .WillOnce(RunClosure(quit_closure));

    auto region = base::UnsafeSharedMemoryRegion::Create(output_buffer_size_);
    ASSERT_TRUE(region.IsValid());
    encoder_->UseOutputBitstreamBuffer(
        BitstreamBuffer(kBitstreamId, std::move(region), output_buffer_size_));

    auto frame = VideoFrame::CreateFrame(PIXEL_FORMAT_I420, kDefaultEncodeSize,
                                         gfx::Rect(kDefaultEncodeSize),
                                         kDefaultEncodeSize, base::TimeDelta());
    ASSERT_TRUE(frame);
    encoder_->Encode(std::move(frame), false /* force_keyframe */);
    run_loop.Run();
  }

  size_t output_buffer_size_ = 0;
  std::vector<VASurfaceID> va_surfaces_;
  base::test::TaskEnvironment task_environment_;
  MockVideoEncodeAcceleratorClient client_;
  std::unique_ptr<VideoEncodeAccelerator> encoder_;
  scoped_refptr<MockVaapiWrapper> mock_vaapi_wrapper_;
  MockAcceleratedVideoEncoder* mock_encoder_ = nullptr;
};

struct VaapiVideoEncodeAcceleratorTestParam {
  uint8_t num_of_spatial_layers = 0;
  uint8_t num_of_temporal_layers = 0;
} kTestCases[]{
    {1u, 1u},  // Single spatial layer, single temporal layer.
    {1u, 3u},  // Single spatial layer, multiple temporal layers.
    {3u, 1u},  // Multiple spatial layers, single temporal layer.
    {3u, 3u},  // Multiple spatial layers, multiple temporal layers.
};

TEST_P(VaapiVideoEncodeAcceleratorTest,
       InitializeVP9WithMultipleSpatialLayers) {
  const uint8_t num_of_spatial_layers = GetParam().num_of_spatial_layers;
  if (num_of_spatial_layers <= 1)
    GTEST_SKIP() << "Test only meant for multiple spatial layers configuration";

  VideoEncodeAccelerator::Config config = kDefaultVideoEncodeAcceleratorConfig;
  const uint8_t num_of_temporal_layers = GetParam().num_of_temporal_layers;
  constexpr int kDenom[] = {4, 2, 1};
  for (uint8_t i = 0; i < num_of_spatial_layers; ++i) {
    VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
    const int denom = kDenom[i];
    spatial_layer.width = kDefaultEncodeSize.width() / denom;
    spatial_layer.height = kDefaultEncodeSize.height() / denom;
    spatial_layer.bitrate_bps = kDefaultBitrateBps / denom;
    spatial_layer.framerate = kDefaultFramerate;
    spatial_layer.max_qp = 30;
    spatial_layer.num_of_temporal_layers = num_of_temporal_layers;
    config.spatial_layers.push_back(spatial_layer);
  }

  EXPECT_FALSE(InitializeVideoEncodeAccelerator(config));
}

TEST_P(VaapiVideoEncodeAcceleratorTest, EncodeVP9WithSingleSpatialLayer) {
  if (GetParam().num_of_spatial_layers > 1u)
    GTEST_SKIP() << "Test only meant for single spatial layer";

  VideoEncodeAccelerator::Config config = kDefaultVideoEncodeAcceleratorConfig;
  VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
  spatial_layer.width = kDefaultEncodeSize.width();
  spatial_layer.height = kDefaultEncodeSize.height();
  spatial_layer.bitrate_bps = kDefaultBitrateBps;
  spatial_layer.framerate = kDefaultFramerate;
  spatial_layer.max_qp = 30;
  spatial_layer.num_of_temporal_layers = GetParam().num_of_temporal_layers;
  config.spatial_layers.push_back(spatial_layer);
  SetDefaultMocksBehavior(config);

  InitializeSequenceForVP9(config);
  EncodeSequenceForVP9();
}

INSTANTIATE_TEST_SUITE_P(,
                         VaapiVideoEncodeAcceleratorTest,
                         ::testing::ValuesIn(kTestCases));
}  // namespace media
