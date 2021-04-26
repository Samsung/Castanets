// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/cast_audio_input_stream.h"

#include "base/logging.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"
#include "media/audio/audio_manager_base.h"

namespace chromecast {
namespace media {

CastAudioInputStream::CastAudioInputStream(
    ::media::AudioManagerBase* audio_manager,
    const ::media::AudioParameters& audio_params,
    const std::string& device_id)
    : audio_manager_(audio_manager), audio_params_(audio_params) {
  DETACH_FROM_THREAD(audio_thread_checker_);
  LOG(INFO) << __func__ << " " << this
            << " created from device_id = " << device_id
            << " with audio_params = {" << audio_params_.AsHumanReadableString()
            << "}.";
}

CastAudioInputStream::~CastAudioInputStream() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
}

bool CastAudioInputStream::Open() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(!capture_service_receiver_);
  LOG(INFO) << __func__ << " " << this << ".";

  // Sanity check the audio parameters.
  ::media::AudioParameters::Format format = audio_params_.format();
  DCHECK((format == ::media::AudioParameters::AUDIO_PCM_LINEAR) ||
         (format == ::media::AudioParameters::AUDIO_PCM_LOW_LATENCY));
  ::media::ChannelLayout channel_layout = audio_params_.channel_layout();
  if ((channel_layout != ::media::CHANNEL_LAYOUT_MONO) &&
      (channel_layout != ::media::CHANNEL_LAYOUT_STEREO)) {
    LOG(WARNING) << "Unsupported channel layout: " << channel_layout;
    return false;
  }
  DCHECK_GE(audio_params_.channels(), 1);
  DCHECK_LE(audio_params_.channels(), 2);

  audio_bus_ = ::media::AudioBus::Create(audio_params_.channels(),
                                         audio_params_.frames_per_buffer());
  capture_service_receiver_ = std::make_unique<CaptureServiceReceiver>(
      capture_service::StreamInfo{
          capture_service::StreamType::kSoftwareEchoCancelled,
          capture_service::AudioCodec::kPcm, audio_params_.channels(),
          // Format doesn't matter in the request.
          capture_service::SampleFormat::LAST_FORMAT,
          audio_params_.sample_rate(), audio_params_.frames_per_buffer()},
      this);
  return true;
}

void CastAudioInputStream::Start(AudioInputCallback* input_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(capture_service_receiver_);
  DCHECK(!input_callback_);
  DCHECK(input_callback);
  LOG(INFO) << __func__ << " " << this << ".";
  input_callback_ = input_callback;
  capture_service_receiver_->Start();
}

void CastAudioInputStream::Stop() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  DCHECK(capture_service_receiver_);
  LOG(INFO) << __func__ << " " << this << ".";
  capture_service_receiver_->Stop();
  input_callback_ = nullptr;
}

void CastAudioInputStream::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(audio_thread_checker_);
  LOG(INFO) << __func__ << " " << this << ".";
  capture_service_receiver_.reset();
  audio_bus_.reset();
  if (audio_manager_) {
    audio_manager_->ReleaseInputStream(this);
  }
}

double CastAudioInputStream::GetMaxVolume() {
  return 1.0;
}

void CastAudioInputStream::SetVolume(double volume) {}

double CastAudioInputStream::GetVolume() {
  return 1.0;
}

bool CastAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool CastAudioInputStream::GetAutomaticGainControl() {
  return false;
}

bool CastAudioInputStream::IsMuted() {
  return false;
}

void CastAudioInputStream::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  // Not supported. Do nothing.
}

bool CastAudioInputStream::OnCaptureData(const char* data, size_t size) {
  capture_service::PacketInfo info;
  if (!capture_service::ReadPcmAudioMessage(data, size, &info,
                                            audio_bus_.get())) {
    return false;
  }

  DCHECK(input_callback_);
  input_callback_->OnData(
      audio_bus_.get(),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(info.timestamp_us),
      /* volume */ 1.0);
  return true;
}

void CastAudioInputStream::OnCaptureError() {
  DCHECK(input_callback_);
  input_callback_->OnError();
}

}  // namespace media
}  // namespace chromecast
