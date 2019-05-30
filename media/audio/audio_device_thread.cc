// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_device_thread.h"

#include <limits>
#if defined(CASTANETS) && !defined(OS_WIN)
#include <sys/socket.h>
#endif

#include "base/logging.h"
#include "base/sys_info.h"

namespace {

base::ThreadPriority GetAudioThreadPriority() {
#if defined(OS_CHROMEOS)
  // On Chrome OS, there are priority inversion issues with having realtime
  // threads on systems with only two cores, see crbug.com/710245.
  return base::SysInfo::NumberOfProcessors() > 2
             ? base::ThreadPriority::REALTIME_AUDIO
             : base::ThreadPriority::NORMAL;
#else
  return base::ThreadPriority::REALTIME_AUDIO;
#endif
}

}  // namespace

namespace media {

// AudioDeviceThread::Callback implementation

AudioDeviceThread::Callback::Callback(const AudioParameters& audio_parameters,
                                      base::SharedMemoryHandle memory,
                                      uint32_t segment_length,
                                      uint32_t total_segments)
    : audio_parameters_(audio_parameters),
      memory_length_(
          base::CheckMul(segment_length, total_segments).ValueOrDie()),
      total_segments_(total_segments),
      segment_length_(segment_length),
      // CHECK that the shared memory is large enough. The memory allocated
      // must be at least as large as expected.
      shared_memory_((CHECK(memory_length_ <= memory.GetSize()), memory),
                     false) {
  CHECK_GT(total_segments_, 0u);
  thread_checker_.DetachFromThread();
}

AudioDeviceThread::Callback::~Callback() {}

void AudioDeviceThread::Callback::InitializeOnAudioThread() {
  // Normally this function is called before the thread checker is used
  // elsewhere, but it's not guaranteed. DCHECK to ensure it was not used on
  // another thread before we get here.
  DCHECK(thread_checker_.CalledOnValidThread())
      << "Thread checker was attached on the wrong thread";
  DCHECK(!shared_memory_.memory());
  MapSharedMemory();
  CHECK(shared_memory_.memory());
}

// AudioDeviceThread implementation

AudioDeviceThread::AudioDeviceThread(Callback* callback,
                                     base::SyncSocket::Handle socket,
                                     const char* thread_name)
    : callback_(callback), thread_name_(thread_name), socket_(socket) {
#if defined(CASTANETS) && !defined(OS_WIN)
  client_handle_ =
      mojo::edk::CreateTCPClientHandle(mojo::edk::kCastanetsAudioSyncPort);
  if (!client_handle_.is_valid()) {
    LOG(ERROR) << "client_handle is not valid. " << __FUNCTION__;
    return;
  }
#endif

  CHECK(base::PlatformThread::CreateWithPriority(0, this, &thread_handle_,
                                                 GetAudioThreadPriority()));
  DCHECK(!thread_handle_.is_null());
}

AudioDeviceThread::~AudioDeviceThread() {
#if defined(CASTANETS) && !defined(OS_WIN)
  close(client_handle_.get().handle);
#else
  socket_.Shutdown();
#endif
  if (thread_handle_.is_null())
    return;
  base::PlatformThread::Join(thread_handle_);
}

void AudioDeviceThread::ThreadMain() {
  base::PlatformThread::SetName(thread_name_);
  callback_->InitializeOnAudioThread();

  uint32_t buffer_index = 0;
#if defined(CASTANETS) && !defined(NETWORK_SHARED_MEMORY)
  size_t buffer_size = callback_->shared_memory()->handle().GetSize();
  uint8_t* buffer_data = new uint8_t[buffer_size];
#endif

  while (true) {
    uint32_t pending_data = 0;
#if defined(CASTANETS)
#if defined(OS_WIN)
    size_t bytes_read = 0;
#else
#if !defined(NETWORK_SHARED_MEMORY)
    // Receive AudioOutputBuffer data to know delay time.
    size_t buffer_bytes_read = HANDLE_EINTR(recv(
         client_handle_.get().handle, buffer_data, buffer_size, MSG_WAITALL));
    if (buffer_bytes_read != buffer_size)
      break;
    memcpy(callback_->shared_memory()->memory(), buffer_data, buffer_size);
#endif

    // Receive pending data.
    size_t bytes_read = HANDLE_EINTR(recv(
        client_handle_.get().handle, &pending_data, sizeof(pending_data), 0));
#endif
#else
    size_t bytes_read = socket_.Receive(&pending_data, sizeof(pending_data));
#endif
    if (bytes_read != sizeof(pending_data))
      break;

    // std::numeric_limits<uint32_t>::max() is a special signal which is
    // returned after the browser stops the output device in response to a
    // renderer side request.
    //
    // Avoid running Process() for the paused signal, we still need to update
    // the buffer index for synchronized buffers though.
    //
    // See comments in AudioOutputController::DoPause() for details on why.
    if (pending_data != std::numeric_limits<uint32_t>::max())
      callback_->Process(pending_data);

#if defined(CASTANETS) && !defined(NETWORK_SHARED_MEMORY)
    // Send decoded audio data to browser process via socket.
    uint8_t* data = static_cast<uint8_t*>(callback_->shared_memory()->memory());
    uint32_t data_size = callback_->shared_memory()->mapped_size();
    size_t bytes_data_sent = HANDLE_EINTR(send(client_handle_.get().handle, data, data_size, MSG_MORE));
    if (bytes_data_sent != data_size)
      break;
#endif

    // The usage of synchronized buffers differs between input and output cases.
    //
    // Input: Let the other end know that we have read data, so that it can
    // verify it doesn't overwrite any data before read. The |buffer_index|
    // value is not used. For more details, see AudioInputSyncWriter::Write().
    //
    // Output: Let the other end know which buffer we just filled. The
    // |buffer_index| is used to ensure the other end is getting the buffer it
    // expects. For more details on how this works see
    // AudioSyncReader::WaitUntilDataIsReady().
    ++buffer_index;
#if defined(NETWORK_SHARED_MEMORY)
    fdatasync(callback_->shared_memory()->handle().GetHandle());
#endif
#if defined(CASTANETS)
#if defined(OS_WIN)
    size_t bytes_sent = 0;
#else
    // Send a buffer index to browser process via socket.
    size_t bytes_sent =
        HANDLE_EINTR(send(client_handle_.get().handle, &buffer_index,
                          sizeof(buffer_index), MSG_NOSIGNAL));
#endif
#else
    size_t bytes_sent = socket_.Send(&buffer_index, sizeof(buffer_index));
#endif
    if (bytes_sent != sizeof(buffer_index))
      break;
  }
#if defined(CASTANETS) && !defined(NETWORK_SHARED_MEMORY)
  delete []buffer_data;
#endif
}

}  // namespace media.
