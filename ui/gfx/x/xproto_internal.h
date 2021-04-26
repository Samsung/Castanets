// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_XPROTO_INTERNAL_H_
#define UI_GFX_X_XPROTO_INTERNAL_H_

#ifndef IS_X11_IMPL
#error "This file should only be included by generated xprotos"
#endif

#include <X11/Xlib-xcb.h>
#include <stdint.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xcbext.h>

#include <bitset>
#include <limits>
#include <type_traits>

#include "base/component_export.h"
#include "base/logging.h"
#include "base/optional.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

template <class Reply>
class Future;

using WriteBuffer = std::vector<uint8_t>;

template <typename T, typename Enable = void>
struct EnumBase {
  using type = T;
};

template <typename T>
struct EnumBase<T, typename std::enable_if_t<std::is_enum<T>::value>> {
  using type = typename std::underlying_type<T>::type;
};

template <typename T>
using EnumBaseType = typename EnumBase<T>::type;

struct ReadBuffer {
  const uint8_t* data = nullptr;
  size_t offset = 0;
};

template <typename T>
void VerifyAlignment(T* t, size_t offset) {
  // On the wire, X11 types are always aligned to their size.  This is a sanity
  // check to ensure padding etc are working properly.
  if (sizeof(T) == 2 || sizeof(T) == 4 || sizeof(T) == 8)
    DCHECK_EQ(offset % sizeof(*t), 0UL);
}

template <typename T>
void Write(const T* t, WriteBuffer* buf) {
  static_assert(std::is_trivially_copyable<T>::value, "");
  VerifyAlignment(t, buf->size());
  const uint8_t* start = reinterpret_cast<const uint8_t*>(t);
  std::copy(start, start + sizeof(*t), std::back_inserter(*buf));
}

template <typename T>
void Read(T* t, ReadBuffer* buf) {
  static_assert(std::is_trivially_copyable<T>::value, "");
  VerifyAlignment(t, buf->offset);
  memcpy(t, buf->data + buf->offset, sizeof(*t));
  buf->offset += sizeof(*t);
}

inline void Pad(WriteBuffer* buf, size_t amount) {
  buf->resize(buf->size() + amount, '\0');
}

inline void Pad(ReadBuffer* buf, size_t amount) {
  buf->offset += amount;
}

inline void Align(WriteBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->size() % align)) % align);
}

inline void Align(ReadBuffer* buf, size_t align) {
  Pad(buf, (align - (buf->offset % align)) % align);
}

template <typename Reply>
Future<Reply> SendRequest(x11::Connection* connection, WriteBuffer* buf) {
  // Clang crashes when the value of |is_void| is inlined below,
  // so keep this variable outside of |xpr|.
  constexpr bool is_void = std::is_void<Reply>::value;
  xcb_protocol_request_t xpr{
      .ext = nullptr,
      .isvoid = is_void,
  };

  struct RequestHeader {
    uint8_t major_opcode;
    uint8_t minor_opcode;
    uint16_t length;
  };

  struct ExtendedRequestHeader {
    RequestHeader header;
    uint32_t long_length;
  };
  static_assert(sizeof(ExtendedRequestHeader) == 8, "");

  auto* old_header = reinterpret_cast<RequestHeader*>(buf->data());
  ExtendedRequestHeader new_header{*old_header, 0};

  // Requests are always a multiple of 4 bytes on the wire.  Because of this,
  // the length field represents the size in chunks of 4 bytes.
  DCHECK_EQ(buf->size() % 4, 0UL);
  size_t size32 = buf->size() / 4;

  struct iovec io[4];
  memset(&io, 0, sizeof(io));
  if (size32 < connection->setup().maximum_request_length) {
    xpr.count = 1;
    old_header->length = size32;
    io[2].iov_base = buf->data();
    io[2].iov_len = buf->size();
  } else if (size32 < connection->extended_max_request_length()) {
    xpr.count = 2;
    DCHECK_EQ(new_header.header.length, 0U);
    new_header.long_length = size32 + 1;
    io[2].iov_base = &new_header;
    io[2].iov_len = sizeof(ExtendedRequestHeader);
    io[3].iov_base = buf->data() + sizeof(RequestHeader);
    io[3].iov_len = buf->size() - sizeof(RequestHeader);
  } else {
    LOG(ERROR) << "Cannot send request of length " << buf->size();
    return {nullptr, base::nullopt};
  }

  xcb_connection_t* conn = connection->XcbConnection();
  auto flags = XCB_REQUEST_CHECKED | XCB_REQUEST_RAW;
  auto sequence = xcb_send_request(conn, flags, &io[2], &xpr);
  if (xcb_connection_has_error(conn))
    return {nullptr, base::nullopt};
  return {connection, sequence};
}

// Helper function for xcbproto popcount.  Given an integral type, returns the
// number of 1 bits present.
template <typename T>
size_t PopCount(T t) {
  return std::bitset<sizeof(T) * 8>(static_cast<EnumBaseType<T>>(t)).count();
}

// Helper function for xcbproto sumof.  Given a function |f| and a container
// |t|, maps the elements uisng |f| and reduces by summing the results.
template <typename F, typename T>
auto SumOf(F&& f, T& t) {
  decltype(f(t[0])) sum = 0;
  for (auto& v : t)
    sum += f(v);
  return sum;
}

// Helper function for xcbproto case.  Checks for equality between |t| and |s|.
template <typename T, typename S>
bool CaseEq(T t, S s) {
  return t == static_cast<decltype(t)>(s);
}

// Helper function for xcbproto bitcase expressions.  Checks if the bitmasks |t|
// and |s| have any intersection.
template <typename T, typename S>
bool CaseAnd(T t, S s) {
  return static_cast<EnumBaseType<T>>(t) & static_cast<EnumBaseType<T>>(s);
}

// Helper function for xcbproto & expressions.  Computes |t| & |s|.
template <typename T, typename S>
auto BitAnd(T t, S s) {
  return static_cast<EnumBaseType<T>>(t) & static_cast<EnumBaseType<T>>(s);
}

// Helper function for xcbproto ~ expressions.
template <typename T>
auto BitNot(T t) {
  return ~static_cast<EnumBaseType<T>>(t);
}

// Helper function for generating switch values.  |switch_var| is the value to
// modify.  |enum_val| is the value to set |switch_var| to if this is a regular
// case, or the bit to be set in |switch_var| if this is a bit case.  This
// function is a no-op when |condition| is false.
template <typename T>
auto SwitchVar(T enum_val, bool condition, bool is_bitcase, T* switch_var) {
  using EnumInt = EnumBaseType<T>;
  if (!condition)
    return;
  EnumInt switch_int = static_cast<EnumInt>(*switch_var);
  if (is_bitcase) {
    *switch_var = static_cast<T>(switch_int | static_cast<EnumInt>(enum_val));
  } else {
    DCHECK(!switch_int);
    *switch_var = enum_val;
  }
}

template <typename T>
std::unique_ptr<T> MakeExtension(Connection* connection,
                                 Future<QueryExtensionReply> future) {
  auto reply = future.Sync();
  return std::make_unique<T>(connection,
                             reply ? *reply.reply : QueryExtensionReply{});
}

}  // namespace x11

#endif  //  UI_GFX_X_XPROTO_INTERNAL_H_
