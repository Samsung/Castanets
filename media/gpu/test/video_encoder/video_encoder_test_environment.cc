// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_encoder/video_encoder_test_environment.h"

#include <algorithm>
#include <utility>

#include "build/buildflag.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/macros.h"
#include "media/gpu/test/video.h"

namespace media {
namespace test {
namespace {
struct CodecParamToProfile {
  const char* codec;
  const VideoCodecProfile profile;
} kCodecParamToProfile[] = {
    {"h264baseline", H264PROFILE_BASELINE}, {"h264", H264PROFILE_MAIN},
    {"h264main", H264PROFILE_MAIN},         {"vp8", VP8PROFILE_ANY},
    {"vp9", VP9PROFILE_PROFILE0},
};

const std::vector<base::Feature> kEnabledFeaturesForVideoEncoderTest = {
#if BUILDFLAG(USE_VAAPI)
    // TODO(crbug.com/828482): remove once enabled by default.
    media::kVaapiLowPowerEncoderGen9x,
    // TODO(crbug.com/811912): remove once enabled by default.
    kVaapiVP9Encoder,
#endif
};

const std::vector<base::Feature> kDisabledFeaturesForVideoEncoderTest = {
    // FFmpegVideoDecoder is used for vp8 stream whose alpha mode is opaque in
    // chromium browser. However, VpxVideoDecoder will be used to decode any vp8
    // stream for the rightness (b/138840822), and currently be experimented
    // with this feature flag. We disable the feature to use VpxVideoDecoder to
    // decode any vp8 stream in BitstreamValidator.
    kFFmpegDecodeOpaqueVP8,
};
}  // namespace

// static
VideoEncoderTestEnvironment* VideoEncoderTestEnvironment::Create(
    const base::FilePath& video_path,
    const base::FilePath& video_metadata_path,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    const std::string& codec,
    bool output_bitstream) {
  if (video_path.empty()) {
    LOG(ERROR) << "No video specified";
    return nullptr;
  }
  auto video =
      std::make_unique<media::test::Video>(video_path, video_metadata_path);
  if (!video->Load()) {
    LOG(ERROR) << "Failed to load " << video_path;
    return nullptr;
  }

  // If the video file has the .webm format it needs to be decoded first.
  // TODO(b/151134705): Add support to cache decompressed video files.
  if (video->FilePath().MatchesExtension(FILE_PATH_LITERAL(".webm"))) {
    VLOGF(1) << "Test video " << video->FilePath()
             << " is compressed, decoding...";
    if (!video->Decode()) {
      LOG(ERROR) << "Failed to decode " << video->FilePath();
      return nullptr;
    }
  }

  if (video->PixelFormat() == VideoPixelFormat::PIXEL_FORMAT_UNKNOWN) {
    LOG(ERROR) << "Test video " << video->FilePath()
               << " has an invalid video pixel format "
               << VideoPixelFormatToString(video->PixelFormat());
    return nullptr;
  }

  base::Optional<base::FilePath> bitstream_filename;
  if (output_bitstream) {
    base::FilePath::StringPieceType extension =
        codec.find("h264") != std::string::npos ? FILE_PATH_LITERAL("h264")
                                                : FILE_PATH_LITERAL("ivf");
    bitstream_filename = video_path.BaseName().ReplaceExtension(extension);
  }

  const auto* it = std::find_if(
      std::begin(kCodecParamToProfile), std::end(kCodecParamToProfile),
      [codec](const auto& cp) { return cp.codec == codec; });
  if (it == std::end(kCodecParamToProfile)) {
    LOG(ERROR) << "Unknown codec: " << codec;
    return nullptr;
  }

  VideoCodecProfile profile = it->profile;
  return new VideoEncoderTestEnvironment(
      std::move(video), enable_bitstream_validator, output_folder, profile,
      bitstream_filename);
}

VideoEncoderTestEnvironment::VideoEncoderTestEnvironment(
    std::unique_ptr<media::test::Video> video,
    bool enable_bitstream_validator,
    const base::FilePath& output_folder,
    VideoCodecProfile profile,
    const base::Optional<base::FilePath>& bitstream_filename)
    : VideoTestEnvironment(kEnabledFeaturesForVideoEncoderTest,
                           kDisabledFeaturesForVideoEncoderTest),
      video_(std::move(video)),
      enable_bitstream_validator_(enable_bitstream_validator),
      output_folder_(output_folder),
      profile_(profile),
      bitstream_filename_(bitstream_filename),
      gpu_memory_buffer_factory_(
          gpu::GpuMemoryBufferFactory::CreateNativeType(nullptr)) {}

VideoEncoderTestEnvironment::~VideoEncoderTestEnvironment() = default;

media::test::Video* VideoEncoderTestEnvironment::Video() const {
  return video_.get();
}

bool VideoEncoderTestEnvironment::IsBitstreamValidatorEnabled() const {
  return enable_bitstream_validator_;
}

const base::FilePath& VideoEncoderTestEnvironment::OutputFolder() const {
  return output_folder_;
}

VideoCodecProfile VideoEncoderTestEnvironment::Profile() const {
  return profile_;
}

base::Optional<base::FilePath>
VideoEncoderTestEnvironment::OutputBitstreamFilePath() const {
  if (!bitstream_filename_)
    return base::nullopt;

  return output_folder_.Append(GetTestOutputFilePath())
      .Append(*bitstream_filename_);
}

gpu::GpuMemoryBufferFactory*
VideoEncoderTestEnvironment::GetGpuMemoryBufferFactory() const {
  return gpu_memory_buffer_factory_.get();
}

}  // namespace test
}  // namespace media
