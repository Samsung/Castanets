// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include <limits>

#include "base/logging.h"
#include "base/time/time.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_frame.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fetch/readable_stream_bytes_consumer.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"

namespace blink {

// static
ImageDecoderExternal* ImageDecoderExternal::Create(
    ScriptState* script_state,
    const ImageDecoderInit* init,
    ExceptionState& exception_state) {
  return MakeGarbageCollected<ImageDecoderExternal>(script_state, init,
                                                    exception_state);
}

ImageDecoderExternal::DecodeRequest::DecodeRequest(
    ScriptPromiseResolver* resolver,
    uint32_t frame_index,
    bool complete_frames_only)
    : resolver(resolver),
      frame_index(frame_index),
      complete_frames_only(complete_frames_only) {}

void ImageDecoderExternal::DecodeRequest::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
}

// static
bool ImageDecoderExternal::canDecodeType(String type) {
  return type.ContainsOnlyASCIIOrEmpty() &&
         IsSupportedImageMimeType(type.Ascii());
}

ImageDecoderExternal::ImageDecoderExternal(ScriptState* script_state,
                                           const ImageDecoderInit* init,
                                           ExceptionState& exception_state)
    : script_state_(script_state) {
  // |data| is a required field.
  DCHECK(init->hasData());
  DCHECK(!init->data().IsNull());

  options_ =
      init->hasOptions() ? init->options() : ImageBitmapOptions::Create();

  mime_type_ = init->type();
  if (!canDecodeType(mime_type_)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Unsupported image format.");
    return;
  }

  if (init->data().IsReadableStream()) {
    consumer_ = MakeGarbageCollected<ReadableStreamBytesConsumer>(
        script_state, init->data().GetAsReadableStream(), exception_state);
    if (exception_state.HadException())
      return;

    stream_buffer_ = WTF::SharedBuffer::Create();
    CreateImageDecoder();

    // We need one initial call to OnStateChange() to start reading, but
    // thereafter calls will be driven by the ReadableStreamBytesConsumer.
    consumer_->SetClient(this);
    OnStateChange();
    return;
  }

  // Since we don't make a copy of buffer passed in, we must retain a reference.
  init_data_ = init;

  DOMArrayPiece buffer;
  if (init->data().IsArrayBuffer()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBuffer());
  } else if (init->data().IsArrayBufferView()) {
    buffer = DOMArrayPiece(init->data().GetAsArrayBufferView().View());
  } else {
    NOTREACHED();
    return;
  }

  // TODO: Data is owned by the caller who may be free to manipulate it. We will
  // probably need to make a copy to our own internal data or neuter the buffers
  // as seen by JS.
  segment_reader_ = SegmentReader::CreateFromSkData(
      SkData::MakeWithoutCopy(buffer.Data(), buffer.ByteLengthAsSizeT()));
  if (!segment_reader_) {
    exception_state.ThrowDOMException(DOMExceptionCode::kConstraintError,
                                      "Failed to read image data");
    return;
  }

  data_complete_ = true;

  CreateImageDecoder();
  MaybeUpdateMetadata();
  if (decoder_->Failed()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Image decoding failed");
    return;
  }
}

ImageDecoderExternal::~ImageDecoderExternal() {
  DVLOG(1) << __func__;
}

ScriptPromise ImageDecoderExternal::decode(uint32_t frame_index,
                                           bool complete_frames_only) {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();
  pending_decodes_.push_back(MakeGarbageCollected<DecodeRequest>(
      resolver, frame_index, complete_frames_only));
  MaybeSatisfyPendingDecodes();
  return promise;
}

ScriptPromise ImageDecoderExternal::decodeMetadata() {
  DVLOG(1) << __func__;

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  auto promise = resolver->Promise();
  pending_metadata_decodes_.push_back(resolver);
  MaybeSatisfyPendingMetadataDecodes();
  return promise;
}

uint32_t ImageDecoderExternal::frameCount() const {
  return frame_count_;
}

String ImageDecoderExternal::type() const {
  return mime_type_;
}

uint32_t ImageDecoderExternal::repetitionCount() const {
  return repetition_count_;
}

bool ImageDecoderExternal::complete() const {
  return data_complete_;
}

void ImageDecoderExternal::OnStateChange() {
  const char* buffer;
  size_t available;
  while (!data_complete_) {
    auto result = consumer_->BeginRead(&buffer, &available);
    if (result == BytesConsumer::Result::kShouldWait)
      return;

    if (result == BytesConsumer::Result::kOk) {
      if (available > 0)
        stream_buffer_->Append(buffer, SafeCast<wtf_size_t>(available));
      result = consumer_->EndRead(available);
    }

    if (result == BytesConsumer::Result::kError) {
      data_complete_ = true;
      return;
    }

    data_complete_ = result == BytesConsumer::Result::kDone;
    decoder_->SetData(stream_buffer_, data_complete_);

    MaybeUpdateMetadata();
    MaybeSatisfyPendingDecodes();
  }
}

String ImageDecoderExternal::DebugName() const {
  return "ImageDecoderExternal";
}

void ImageDecoderExternal::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(consumer_);
  visitor->Trace(pending_decodes_);
  visitor->Trace(pending_metadata_decodes_);
  visitor->Trace(init_data_);
  visitor->Trace(options_);
  ScriptWrappable::Trace(visitor);
}

void ImageDecoderExternal::CreateImageDecoder() {
  DCHECK(!decoder_);

  // TODO: We should probably call ImageDecoder::SetMemoryAllocator() so that
  // we can recycle frame buffers for decoded images.

  constexpr char kNoneOption[] = "none";

  auto color_behavior = ColorBehavior::Tag();
  if (options_->colorSpaceConversion() == kNoneOption)
    color_behavior = ColorBehavior::Ignore();

  auto premultiply_alpha = ImageDecoder::kAlphaPremultiplied;
  if (options_->premultiplyAlpha() == kNoneOption)
    premultiply_alpha = ImageDecoder::kAlphaNotPremultiplied;

  // TODO: Is it okay to use resize size like this?
  auto desired_size = SkISize::MakeEmpty();
  if (options_->hasResizeWidth() && options_->hasResizeHeight()) {
    desired_size =
        SkISize::Make(options_->resizeWidth(), options_->resizeHeight());
  }

  if (stream_buffer_) {
    if (!segment_reader_)
      segment_reader_ = SegmentReader::CreateFromSharedBuffer(stream_buffer_);
  } else {
    DCHECK(data_complete_);
  }

  DCHECK(canDecodeType(mime_type_));
  decoder_ = ImageDecoder::CreateByMimeType(
      mime_type_, segment_reader_, data_complete_, premultiply_alpha,
      ImageDecoder::kHighBitDepthToHalfFloat, color_behavior,
      ImageDecoder::OverrideAllowDecodeToYuv::kDeny, desired_size);

  // CreateByImageType() can't fail if we use a supported image type. Which we
  // DCHECK above via canDecodeType().
  DCHECK(decoder_);
}

void ImageDecoderExternal::MaybeSatisfyPendingDecodes() {
  DCHECK(decoder_);
  for (auto& request : pending_decodes_) {
    if (!data_complete_) {
      // We can't fulfill this promise at this time.
      if (request->frame_index >= frame_count_)
        continue;
    } else if (request->frame_index >= frame_count_) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Frame index out of range"));
      continue;
    }

    auto* image = decoder_->DecodeFrameBufferAtIndex(request->frame_index);
    if (decoder_->Failed() || !image) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kConstraintError, "Failed to decode frame"));
      continue;
    }

    // Only satisfy fully complete decode requests.
    const bool is_complete = image->GetStatus() == ImageFrame::kFrameComplete;
    if (!is_complete && request->complete_frames_only)
      continue;

    if (!is_complete && image->GetStatus() != ImageFrame::kFramePartial)
      continue;

    // Prefer FinalizePixelsAndGetImage() since that will mark the underlying
    // bitmap as immutable, which allows copies to be avoided.
    auto sk_image = is_complete ? image->FinalizePixelsAndGetImage()
                                : SkImage::MakeFromBitmap(image->Bitmap());
    if (!sk_image) {
      request->complete = true;
      // TODO: Include frameIndex in rejection?
      request->resolver->Reject(MakeGarbageCollected<DOMException>(
          DOMExceptionCode::kOperationError, "Failed decode frame"));
      continue;
    }

    if (!is_complete) {
      auto generation_id = image->Bitmap().getGenerationID();
      auto it = incomplete_frames_.find(request->frame_index);
      if (it == incomplete_frames_.end()) {
        incomplete_frames_.Set(request->frame_index, generation_id);
      } else {
        // Don't fulfill the promise until a new bitmap is seen.
        if (it->value == generation_id)
          continue;

        it->value = generation_id;
      }
    } else {
      incomplete_frames_.erase(request->frame_index);
    }

    auto* result = ImageFrameExternal::Create();
    result->setImage(MakeGarbageCollected<ImageBitmap>(
        UnacceleratedStaticBitmapImage::Create(std::move(sk_image),
                                               decoder_->Orientation()),
        base::nullopt, options_));
    result->setDuration(
        decoder_->FrameDurationAtIndex(request->frame_index).InMicroseconds());
    result->setOrientation(decoder_->Orientation().Orientation());
    result->setComplete(is_complete);
    request->complete = true;
    request->resolver->Resolve(result);
  }

  auto* new_end =
      std::remove_if(pending_decodes_.begin(), pending_decodes_.end(),
                     [](const auto& request) { return request->complete; });
  pending_decodes_.Shrink(
      static_cast<wtf_size_t>(new_end - pending_decodes_.begin()));
}

void ImageDecoderExternal::MaybeSatisfyPendingMetadataDecodes() {
  DCHECK(decoder_);
  DCHECK(decoder_->Failed() || decoder_->IsDecodedSizeAvailable());
  for (auto& resolver : pending_metadata_decodes_)
    resolver->Resolve();
  pending_metadata_decodes_.clear();
}

void ImageDecoderExternal::MaybeUpdateMetadata() {
  const size_t decoded_frame_count = decoder_->FrameCount();
  if (decoder_->Failed()) {
    MaybeSatisfyPendingMetadataDecodes();
    return;
  }

  // Since we always create the decoder at construction, we need to wait until
  // at least the size is available before signaling that metadata has been
  // retrieved.
  if (!decoder_->IsSizeAvailable())
    return;

  frame_count_ = static_cast<uint32_t>(decoded_frame_count);

  // The internal value has some magic negative numbers; for external purposes
  // we want to only surface positive repetition counts. The rest is up to the
  // client.
  const int decoded_repetition_count = decoder_->RepetitionCount();
  if (decoded_repetition_count > 0)
    repetition_count_ = decoded_repetition_count;

  MaybeSatisfyPendingMetadataDecodes();
}

}  // namespace blink
