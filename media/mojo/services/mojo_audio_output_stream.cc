// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/sync_socket.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(CASTANETS)
#include "base/base_switches.h"
#include "base/command_line.h"
#include "mojo/public/cpp/platform/tcp_platform_handle_utils.h"
#endif

namespace media {

MojoAudioOutputStream::MojoAudioOutputStream(
    CreateDelegateCallback create_delegate_callback,
    StreamCreatedCallback stream_created_callback,
    DeleterCallback deleter_callback)
    : stream_created_callback_(std::move(stream_created_callback)),
      deleter_callback_(std::move(deleter_callback)),
      binding_(this),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(deleter_callback_);
  delegate_ = std::move(create_delegate_callback).Run(this);
  if (!delegate_) {
    // Failed to initialize the stream. We cannot call |deleter_callback_| yet,
    // since construction isn't done.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioOutputStream::OnStreamError,
                       weak_factory_.GetWeakPtr(), /* not used */ 0));
  }
}

MojoAudioOutputStream::~MojoAudioOutputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioOutputStream::Play() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPlayStream();
}

void MojoAudioOutputStream::Pause() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnPauseStream();
}

void MojoAudioOutputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (volume < 0 || volume > 1) {
    LOG(ERROR) << "MojoAudioOutputStream::SetVolume(" << volume
               << ") out of range.";
    OnStreamError(/*not used*/ 0);
    return;
  }
  delegate_->OnSetVolume(volume);
}

#if defined(CASTANETS)
void MojoAudioOutputStream::RequestTCPConnect(
    uint16_t assigned_port,
    RequestTCPConnectCallback callback) {
  base::ScopedFD socket_handle;

  // If no port number was assigned, this process is the TCP server.
  if (!assigned_port) {
    // Create a server TCP socket and get the random port number.
    mojo::PlatformHandle server_handle =
        mojo::CreateTCPServerHandle(0, &assigned_port);

    // Ack with the new port number.
    std::move(callback).Run(assigned_port);

    // Accept the connection with the TCP client.
    mojo::TCPServerAcceptConnection(server_handle.GetFD().get(),
                                    &socket_handle);
  } else {
    std::move(callback).Run(assigned_port);

    std::string server_address =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kServerAddress);
    // Create a TCP client socket.
    mojo::PlatformHandle tcp_client_handle =
        mojo::CreateTCPClientHandle(assigned_port, server_address);
    if (!tcp_client_handle.is_valid()) {
      LOG(ERROR) << __func__ << " tcp_client_handle is not valid.";
      return;
    }
    socket_handle = tcp_client_handle.TakeFD();
  }

  delegate_->OnTCPConnected(socket_handle.release());
}
#endif

void MojoAudioOutputStream::OnStreamCreated(
    int stream_id,
    base::UnsafeSharedMemoryRegion shared_memory_region,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(foreign_socket);

  if (!shared_memory_region.IsValid()) {
    OnStreamError(/*not used*/ 0);
    return;
  }

  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket->Release());

  DCHECK(socket_handle.is_valid());

  mojom::AudioOutputStreamPtr stream;
  binding_.Bind(mojo::MakeRequest(&stream));
  // |this| owns |binding_| so unretained is safe.
  binding_.set_connection_error_handler(base::BindOnce(
      &MojoAudioOutputStream::StreamConnectionLost, base::Unretained(this)));

  std::move(stream_created_callback_)
      .Run(std::move(stream), {base::in_place, std::move(shared_memory_region),
                               std::move(socket_handle)});
}

void MojoAudioOutputStream::OnStreamError(int stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(/*had_error*/ true);  // Deletes |this|.
}

void MojoAudioOutputStream::StreamConnectionLost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(/*had_error*/ false);  // Deletes |this|.
}

}  // namespace media
