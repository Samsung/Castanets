// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/invitation.h"

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/platform_handle.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if defined(CASTANETS)
#include "base/distributed_chromium_util.h"
#endif

namespace mojo {

namespace {

static constexpr base::StringPiece kIsolatedPipeName = {"\0\0\0\0", 4};

void ProcessHandleToMojoProcessHandle(base::ProcessHandle target_process,
                                      MojoPlatformProcessHandle* handle) {
  handle->struct_size = sizeof(*handle);
#if defined(OS_WIN)
  handle->value =
      static_cast<uint64_t>(reinterpret_cast<uintptr_t>(target_process));
#else
  handle->value = static_cast<uint64_t>(target_process);
#endif
}

void PlatformHandleToTransportEndpoint(
    PlatformHandle platform_handle,
    MojoPlatformHandle* endpoint_handle,
    MojoInvitationTransportEndpoint* endpoint) {
  PlatformHandle::ToMojoPlatformHandle(std::move(platform_handle),
                                       endpoint_handle);
  CHECK_NE(endpoint_handle->type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  endpoint->struct_size = sizeof(*endpoint);
  endpoint->num_platform_handles = 1;
  endpoint->platform_handles = endpoint_handle;
}

void RunErrorCallback(uintptr_t context,
                      const MojoProcessErrorDetails* details) {
  auto* callback = reinterpret_cast<ProcessErrorCallback*>(context);
  std::string error_message;
  if (details->error_message) {
    error_message =
        std::string(details->error_message, details->error_message_length - 1);
    callback->Run(error_message);
  } else if (details->flags & MOJO_PROCESS_ERROR_FLAG_DISCONNECTED) {
    delete callback;
  }
}

void SendInvitation(ScopedInvitationHandle invitation,
                    base::ProcessHandle target_process,
                    PlatformHandle endpoint_handle,
                    MojoInvitationTransportType transport_type,
                    MojoSendInvitationFlags flags,
                    const ProcessErrorCallback& error_callback,
#if defined(CASTANETS)
                    base::StringPiece isolated_connection_name,
                    base::RepeatingCallback<void()> tcp_success_callback = {},
                    bool secure_connection = false,
                    base::StringPiece tcp_address = base::StringPiece(),
                    uint16_t tcp_port = 0) {
#else
                    base::StringPiece isolated_connection_name) {
#endif
  MojoPlatformProcessHandle process_handle;
  ProcessHandleToMojoProcessHandle(target_process, &process_handle);

  MojoPlatformHandle platform_handle;
  MojoInvitationTransportEndpoint endpoint;
  PlatformHandleToTransportEndpoint(std::move(endpoint_handle),
                                    &platform_handle, &endpoint);
  endpoint.type = transport_type;

  MojoProcessErrorHandler error_handler = nullptr;
  uintptr_t error_handler_context = 0;
  if (error_callback) {
    error_handler = &RunErrorCallback;

    // NOTE: The allocated callback is effectively owned by the error handler,
    // which will delete it on the final invocation for this context (i.e.
    // process disconnection).
    error_handler_context =
        reinterpret_cast<uintptr_t>(new ProcessErrorCallback(error_callback));
  }

  MojoSendInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = flags;
  if (flags & MOJO_SEND_INVITATION_FLAG_ISOLATED) {
    options.isolated_connection_name = isolated_connection_name.data();
    options.isolated_connection_name_length =
        static_cast<uint32_t>(isolated_connection_name.size());
  }
  MojoResult result;
#if defined(CASTANETS)
  if (base::Castanets::IsEnabled()) {
    options.tcp_address = tcp_address.data();
    options.tcp_address_length = static_cast<uint32_t>(tcp_address.size());
    options.tcp_port = tcp_port;
    options.secure_connection = secure_connection;
    endpoint.secure_connection = secure_connection;
    result = MojoSendInvitation(invitation.get().value(), &process_handle,
                                &endpoint, error_handler, error_handler_context,
                                &options, tcp_success_callback);
  } else
#endif
  {
    result =
        MojoSendInvitation(invitation.get().value(), &process_handle, &endpoint,
                           error_handler, error_handler_context, &options);
  }

  // If successful, the invitation handle is already closed for us.
  if (result == MOJO_RESULT_OK)
    ignore_result(invitation.release());
}

#if defined(CASTANETS)
void RetryInvitation(base::ProcessHandle old_process,
                     base::ProcessHandle process,
                     PlatformHandle endpoint_handle,
                     MojoInvitationTransportType transport_type) {
  MojoPlatformProcessHandle old_process_handle, process_handle;
  ProcessHandleToMojoProcessHandle(old_process, &old_process_handle);
  ProcessHandleToMojoProcessHandle(process, &process_handle);

  MojoPlatformHandle platform_handle;
  MojoInvitationTransportEndpoint endpoint;
  PlatformHandleToTransportEndpoint(std::move(endpoint_handle),
                                    &platform_handle, &endpoint);
  endpoint.type = transport_type;

  MojoRetryInvitation(&old_process_handle, &process_handle, &endpoint);
}
#endif
}  // namespace

OutgoingInvitation::OutgoingInvitation() {
  MojoHandle invitation_handle;
  MojoResult result = MojoCreateInvitation(nullptr, &invitation_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  handle_.reset(InvitationHandle(invitation_handle));
}

OutgoingInvitation::OutgoingInvitation(OutgoingInvitation&& other) = default;

OutgoingInvitation::~OutgoingInvitation() = default;

OutgoingInvitation& OutgoingInvitation::operator=(OutgoingInvitation&& other) =
    default;

ScopedMessagePipeHandle OutgoingInvitation::AttachMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  MojoHandle message_pipe_handle;
  MojoResult result = MojoAttachMessagePipeToInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle OutgoingInvitation::AttachMessagePipe(uint64_t name) {
  return AttachMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

ScopedMessagePipeHandle OutgoingInvitation::ExtractMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  MojoHandle message_pipe_handle;
  MojoResult result = MojoExtractMessagePipeFromInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle OutgoingInvitation::ExtractMessagePipe(uint64_t name) {
  return ExtractMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelEndpoint channel_endpoint,
                              const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "");
}

// static
void OutgoingInvitation::Send(OutgoingInvitation invitation,
                              base::ProcessHandle target_process,
                              PlatformChannelServerEndpoint server_endpoint,
                              const ProcessErrorCallback& error_callback) {
  SendInvitation(std::move(invitation.handle_), target_process,
                 server_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER,
                 MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "");
}

#if defined(CASTANETS)
// static
void OutgoingInvitation::SendTcpSocket(
    OutgoingInvitation invitation,
    base::ProcessHandle target_process,
    PlatformHandle platform_handle,
    const ProcessErrorCallback& error_callback,
    base::RepeatingCallback<void()> tcp_success_callback,
    bool secure_connection,
    std::string address,
    uint16_t tcp_port) {
  SendInvitation(
      std::move(invitation.handle_), target_process, std::move(platform_handle),
      address.empty() ? MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER
                      : MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_TCP_CLIENT,
      MOJO_SEND_INVITATION_FLAG_NONE, error_callback, "", tcp_success_callback,
      secure_connection, base::StringPiece(address), tcp_port);
}

// static
void OutgoingInvitation::Retry(base::ProcessHandle old_process,
                               base::ProcessHandle process,
                               PlatformChannelEndpoint channel_endpoint) {
  RetryInvitation(old_process, process, channel_endpoint.TakePlatformHandle(),
                  MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL);
}
#endif
// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelEndpoint channel_endpoint,
    base::StringPiece connection_name) {
  mojo::OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
  SendInvitation(std::move(invitation.handle_), base::kNullProcessHandle,
                 channel_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL,
                 MOJO_SEND_INVITATION_FLAG_ISOLATED, ProcessErrorCallback(),
                 connection_name);
  return pipe;
}

// static
ScopedMessagePipeHandle OutgoingInvitation::SendIsolated(
    PlatformChannelServerEndpoint server_endpoint,
    base::StringPiece connection_name) {
  mojo::OutgoingInvitation invitation;
  ScopedMessagePipeHandle pipe =
      invitation.AttachMessagePipe(kIsolatedPipeName);
  SendInvitation(std::move(invitation.handle_), base::kNullProcessHandle,
                 server_endpoint.TakePlatformHandle(),
                 MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER,
                 MOJO_SEND_INVITATION_FLAG_ISOLATED, ProcessErrorCallback(),
                 connection_name);
  return pipe;
}

IncomingInvitation::IncomingInvitation() = default;

IncomingInvitation::IncomingInvitation(IncomingInvitation&& other) = default;

IncomingInvitation::IncomingInvitation(ScopedInvitationHandle handle)
    : handle_(std::move(handle)) {}

IncomingInvitation::~IncomingInvitation() = default;

IncomingInvitation& IncomingInvitation::operator=(IncomingInvitation&& other) =
    default;

// static
IncomingInvitation IncomingInvitation::Accept(
    PlatformChannelEndpoint channel_endpoint) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                       &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, nullptr, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return IncomingInvitation();

  return IncomingInvitation(
      ScopedInvitationHandle(InvitationHandle(invitation_handle)));
}

// static
ScopedMessagePipeHandle IncomingInvitation::AcceptIsolated(
    PlatformChannelEndpoint channel_endpoint) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(channel_endpoint.TakePlatformHandle(),
                                       &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type = MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;

  MojoAcceptInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_ACCEPT_INVITATION_FLAG_ISOLATED;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, &options, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return ScopedMessagePipeHandle();

  IncomingInvitation invitation{
      ScopedInvitationHandle(InvitationHandle(invitation_handle))};
  return invitation.ExtractMessagePipe(kIsolatedPipeName);
}

#if defined(CASTANETS)
IncomingInvitation IncomingInvitation::AcceptTcpSocket(PlatformHandle handle,
                                                       std::string address,
                                                       uint16_t port,
                                                       bool secure_connection) {
  MojoPlatformHandle endpoint_handle;
  PlatformHandle::ToMojoPlatformHandle(std::move(handle), &endpoint_handle);
  CHECK_NE(endpoint_handle.type, MOJO_PLATFORM_HANDLE_TYPE_INVALID);

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.type =
      (address.empty()) ? MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_SERVER
                        : MOJO_INVITATION_TRANSPORT_TYPE_CHANNEL_TCP_CLIENT;
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;
  transport_endpoint.tcp_address = address.c_str();
  transport_endpoint.tcp_address_length = address.length();
  transport_endpoint.tcp_port = port;
  transport_endpoint.secure_connection = secure_connection;

  MojoHandle invitation_handle;
  MojoResult result =
      MojoAcceptInvitation(&transport_endpoint, nullptr, &invitation_handle);
  if (result != MOJO_RESULT_OK)
    return IncomingInvitation();

  return IncomingInvitation(
      ScopedInvitationHandle(InvitationHandle(invitation_handle)));
}
#endif

ScopedMessagePipeHandle IncomingInvitation::ExtractMessagePipe(
    base::StringPiece name) {
  DCHECK(!name.empty());
  DCHECK(base::IsValueInRangeForNumericType<uint32_t>(name.size()));
  DCHECK(handle_.is_valid());
  MojoHandle message_pipe_handle;
  MojoResult result = MojoExtractMessagePipeFromInvitation(
      handle_.get().value(), name.data(), static_cast<uint32_t>(name.size()),
      nullptr, &message_pipe_handle);
  DCHECK_EQ(MOJO_RESULT_OK, result);
  return ScopedMessagePipeHandle(MessagePipeHandle(message_pipe_handle));
}

ScopedMessagePipeHandle IncomingInvitation::ExtractMessagePipe(uint64_t name) {
  return ExtractMessagePipe(
      base::StringPiece(reinterpret_cast<const char*>(&name), sizeof(name)));
}

}  // namespace mojo
