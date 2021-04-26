// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter.h"

#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace viz {

OutputPresenter::Image::Image() = default;

OutputPresenter::Image::~Image() {
  // TODO(vasilyt): As we are going to delete image anyway we should be able
  // to abort write to avoid unnecessary flush to submit semaphores.
  if (scoped_skia_write_access_) {
    EndWriteSkia();
  }
  DCHECK(!scoped_skia_write_access_);
}

bool OutputPresenter::Image::Initialize(
    gpu::SharedImageFactory* factory,
    gpu::SharedImageRepresentationFactory* representation_factory,
    const gpu::Mailbox& mailbox,
    SkiaOutputSurfaceDependency* deps) {
  skia_representation_ = representation_factory->ProduceSkia(
      mailbox, deps->GetSharedContextState());
  if (!skia_representation_) {
    DLOG(ERROR) << "ProduceSkia() failed.";
    return false;
  }

  // Initialize |shared_image_deleter_| to make sure the shared image backing
  // will be released with the Image.
  shared_image_deleter_.ReplaceClosure(base::BindOnce(
      base::IgnoreResult(&gpu::SharedImageFactory::DestroySharedImage),
      base::Unretained(factory), mailbox));

  return true;
}

void OutputPresenter::Image::BeginWriteSkia() {
  DCHECK(!scoped_skia_write_access_);
  DCHECK(!present_count());
  DCHECK(end_semaphores_.empty());

  std::vector<GrBackendSemaphore> begin_semaphores;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(0 /* flags */,
                               SkSurfaceProps::kLegacyFontHost_InitType);

  // Buffer queue is internal to GPU proc and handles texture initialization,
  // so allow uncleared access.
  // TODO(vasilyt): Props and MSAA
  scoped_skia_write_access_ = skia_representation_->BeginScopedWriteAccess(
      0 /* final_msaa_count */, surface_props, &begin_semaphores,
      &end_semaphores_,
      gpu::SharedImageRepresentation::AllowUnclearedAccess::kYes);
  DCHECK(scoped_skia_write_access_);
  if (!begin_semaphores.empty()) {
    scoped_skia_write_access_->surface()->wait(begin_semaphores.size(),
                                               begin_semaphores.data());
  }
}

SkSurface* OutputPresenter::Image::sk_surface() {
  return scoped_skia_write_access_ ? scoped_skia_write_access_->surface()
                                   : nullptr;
}

std::vector<GrBackendSemaphore>
OutputPresenter::Image::TakeEndWriteSkiaSemaphores() {
  std::vector<GrBackendSemaphore> result;
  result.swap(end_semaphores_);
  return result;
}

void OutputPresenter::Image::EndWriteSkia() {
  // The Flush now takes place in finishPaintCurrentBuffer on the CPU side.
  // check if end_semaphores is not empty then flash here
  DCHECK(scoped_skia_write_access_);
  if (!end_semaphores_.empty()) {
    GrFlushInfo flush_info = {
        .fFlags = kNone_GrFlushFlags,
        .fNumSemaphores = end_semaphores_.size(),
        .fSignalSemaphores = end_semaphores_.data(),
    };
    scoped_skia_write_access_->surface()->flush(
        SkSurface::BackendSurfaceAccess::kNoAccess, flush_info);
    DCHECK(scoped_skia_write_access_->surface()->getContext());
    scoped_skia_write_access_->surface()->getContext()->submit();
  }
  scoped_skia_write_access_.reset();
  end_semaphores_.clear();

  // SkiaRenderer always draws the full frame.
  skia_representation_->SetCleared();
}

OutputPresenter::OverlayData::OverlayData(
    std::unique_ptr<gpu::SharedImageRepresentationOverlay> representation,
    std::unique_ptr<gpu::SharedImageRepresentationOverlay::ScopedReadAccess>
        scoped_read_access)
    : representation_(std::move(representation)),
      scoped_read_access_(std::move(scoped_read_access)) {}
OutputPresenter::OverlayData::OverlayData(OverlayData&&) = default;
OutputPresenter::OverlayData::~OverlayData() = default;
OutputPresenter::OverlayData& OutputPresenter::OverlayData::operator=(
    OverlayData&&) = default;

}  // namespace viz
