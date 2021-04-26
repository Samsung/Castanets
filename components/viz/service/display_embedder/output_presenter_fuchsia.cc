// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/output_presenter_fuchsia.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/inspect/cpp/component.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/service/display_embedder/skia_output_surface_dependency.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/ipc/common/gpu_client_ids.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "ui/ozone/public/platform_window_surface.h"

namespace viz {

namespace {

void GrSemaphoresToZxEvents(gpu::VulkanImplementation* vulkan_implementation,
                            VkDevice vk_device,
                            const std::vector<GrBackendSemaphore>& semaphores,
                            std::vector<zx::event>* events) {
  for (auto& semaphore : semaphores) {
    gpu::SemaphoreHandle handle = vulkan_implementation->GetSemaphoreHandle(
        vk_device, semaphore.vkSemaphore());
    DCHECK(handle.is_valid());
    events->push_back(handle.TakeHandle());
  }
}

class PresenterImageFuchsia : public OutputPresenter::Image {
 public:
  explicit PresenterImageFuchsia(uint32_t image_id);
  ~PresenterImageFuchsia() override;

  void BeginPresent() final;
  void EndPresent() final;
  int present_count() const final;

  uint32_t image_id() const { return image_id_; }

  void TakeSemaphores(std::vector<GrBackendSemaphore>* read_begin_semaphores,
                      std::vector<GrBackendSemaphore>* read_end_semaphores);

 private:
  const uint32_t image_id_;

  int present_count_ = 0;

  std::unique_ptr<gpu::SharedImageRepresentationSkia::ScopedReadAccess>
      read_access_;

  std::vector<GrBackendSemaphore> read_begin_semaphores_;
  std::vector<GrBackendSemaphore> read_end_semaphores_;
};

PresenterImageFuchsia::PresenterImageFuchsia(uint32_t image_id)
    : image_id_(image_id) {}

PresenterImageFuchsia::~PresenterImageFuchsia() {
  DCHECK(read_begin_semaphores_.empty());
  DCHECK(read_end_semaphores_.empty());
}

void PresenterImageFuchsia::BeginPresent() {
  ++present_count_;

  if (present_count_ == 1) {
    DCHECK(!read_access_);
    DCHECK(read_begin_semaphores_.empty());
    DCHECK(read_end_semaphores_.empty());
    read_access_ = skia_representation()->BeginScopedReadAccess(
        &read_begin_semaphores_, &read_end_semaphores_);
  }
}

void PresenterImageFuchsia::EndPresent() {
  DCHECK(present_count_);
  --present_count_;
  if (!present_count_)
    read_access_.reset();
}

int PresenterImageFuchsia::present_count() const {
  return present_count_;
}

void PresenterImageFuchsia::TakeSemaphores(
    std::vector<GrBackendSemaphore>* read_begin_semaphores,
    std::vector<GrBackendSemaphore>* read_end_semaphores) {
  DCHECK(read_begin_semaphores->empty());
  std::swap(*read_begin_semaphores, read_begin_semaphores_);

  DCHECK(read_end_semaphores->empty());
  std::swap(*read_end_semaphores, read_end_semaphores_);
}

}  // namespace

OutputPresenterFuchsia::PendingFrame::PendingFrame() = default;
OutputPresenterFuchsia::PendingFrame::~PendingFrame() = default;

OutputPresenterFuchsia::PendingFrame::PendingFrame(PendingFrame&&) = default;
OutputPresenterFuchsia::PendingFrame&
OutputPresenterFuchsia::PendingFrame::operator=(PendingFrame&&) = default;

// static
std::unique_ptr<OutputPresenterFuchsia> OutputPresenterFuchsia::Create(
    ui::PlatformWindowSurface* window_surface,
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker) {
  auto* inspector = base::ComponentInspectorForProcess();

  if (!base::FeatureList::IsEnabled(
          features::kUseSkiaOutputDeviceBufferQueue)) {
    inspector->root().CreateString("output_presenter", "swapchain", inspector);
    return {};
  }

  inspector->root().CreateString("output_presenter",
                                 "SkiaOutputDeviceBufferQueue", inspector);

  // SetTextureToNewImagePipe() will call ScenicSession::Present() to send
  // CreateImagePipe2Cmd creation command, but it will be processed only after
  // vsync, which will delay buffer allocation of buffers in AllocateImages(),
  // but that shouldn't cause any issues.
  fuchsia::images::ImagePipe2Ptr image_pipe;
  if (!window_surface->SetTextureToNewImagePipe(image_pipe.NewRequest()))
    return {};

  return std::make_unique<OutputPresenterFuchsia>(std::move(image_pipe), deps,
                                                  memory_tracker);
}

OutputPresenterFuchsia::OutputPresenterFuchsia(
    fuchsia::images::ImagePipe2Ptr image_pipe,
    SkiaOutputSurfaceDependency* deps,
    gpu::MemoryTracker* memory_tracker)
    : image_pipe_(std::move(image_pipe)),
      dependency_(deps),
      shared_image_factory_(deps->GetGpuPreferences(),
                            deps->GetGpuDriverBugWorkarounds(),
                            deps->GetGpuFeatureInfo(),
                            deps->GetSharedContextState().get(),
                            deps->GetMailboxManager(),
                            deps->GetSharedImageManager(),
                            deps->GetGpuImageFactory(),
                            memory_tracker,
                            true),
      shared_image_representation_factory_(deps->GetSharedImageManager(),
                                           memory_tracker) {
  sysmem_allocator_ = base::ComponentContextForProcess()
                          ->svc()
                          ->Connect<fuchsia::sysmem::Allocator>();

  image_pipe_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ImagePipe disconnected";

    for (auto& frame : pending_frames_) {
      std::move(frame.completion_callback)
          .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    }
    pending_frames_.clear();
  });
}

OutputPresenterFuchsia::~OutputPresenterFuchsia() {}

void OutputPresenterFuchsia::InitializeCapabilities(
    OutputSurface::Capabilities* capabilities) {
  // We expect origin of buffers is at top left.
  capabilities->output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities->supports_post_sub_buffer = false;
  capabilities->supports_commit_overlay_planes = false;

  capabilities->sk_color_type = kRGBA_8888_SkColorType;
  capabilities->gr_backend_format =
      dependency_->GetSharedContextState()->gr_context()->defaultBackendFormat(
          capabilities->sk_color_type, GrRenderable::kYes);
}

bool OutputPresenterFuchsia::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     gfx::BufferFormat format,
                                     gfx::OverlayTransform transform) {
  if (!image_pipe_)
    return false;

  frame_size_ = size;

  return true;
}

std::vector<std::unique_ptr<OutputPresenter::Image>>
OutputPresenterFuchsia::AllocateImages(gfx::ColorSpace color_space,
                                       gfx::Size image_size,
                                       size_t num_images) {
  if (!image_pipe_)
    return {};

  // If we already allocated buffer collection then it needs to be released.
  if (last_buffer_collection_id_) {
    // If there are pending frames for the old buffer collection then remove the
    // collection only after that frame is presented. Otherwise remove it now.
    if (!pending_frames_.empty() &&
        pending_frames_.back().buffer_collection_id ==
            last_buffer_collection_id_) {
      DCHECK(!pending_frames_.back().remove_buffer_collection);
      pending_frames_.back().remove_buffer_collection = true;
    } else {
      image_pipe_->RemoveBufferCollection(last_buffer_collection_id_);
    }
  }

  buffer_collection_.reset();

  // Create buffer collection with 2 extra tokens: one for Vulkan and one for
  // the ImagePipe.
  fuchsia::sysmem::BufferCollectionTokenPtr collection_token;
  sysmem_allocator_->AllocateSharedCollection(collection_token.NewRequest());

  fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
      token_for_scenic;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              token_for_scenic.NewRequest());

  fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
      token_for_vulkan;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              token_for_vulkan.NewRequest());

  fuchsia::sysmem::BufferCollectionSyncPtr collection;
  sysmem_allocator_->BindSharedCollection(std::move(collection_token),
                                          collection.NewRequest());

  zx_status_t status = collection->Sync();
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection.Sync()";
    return {};
  }

  auto* vulkan =
      dependency_->GetVulkanContextProvider()->GetVulkanImplementation();

  // Set constraints for the new collection.
  fuchsia::sysmem::BufferCollectionConstraints constraints;
  constraints.min_buffer_count = num_images;
  constraints.usage.none = fuchsia::sysmem::noneUsage;
  constraints.image_format_constraints_count = 1;
  constraints.image_format_constraints[0].pixel_format.type =
      fuchsia::sysmem::PixelFormatType::R8G8B8A8;
  constraints.image_format_constraints[0].min_coded_width = frame_size_.width();
  constraints.image_format_constraints[0].min_coded_height =
      frame_size_.height();
  constraints.image_format_constraints[0].color_spaces_count = 1;
  constraints.image_format_constraints[0].color_space[0].type =
      fuchsia::sysmem::ColorSpaceType::SRGB;
  collection->SetConstraints(true, constraints);

  // Register the new buffer collection with the ImagePipe.
  last_buffer_collection_id_++;
  image_pipe_->AddBufferCollection(last_buffer_collection_id_,
                                   std::move(token_for_scenic));

  // Register the new buffer collection with Vulkan.
  gfx::SysmemBufferCollectionId buffer_collection_id =
      gfx::SysmemBufferCollectionId::Create();

  VkDevice vk_device = dependency_->GetVulkanContextProvider()
                           ->GetDeviceQueue()
                           ->GetVulkanDevice();
  buffer_collection_ = vulkan->RegisterSysmemBufferCollection(
      vk_device, buffer_collection_id, token_for_vulkan.TakeChannel(),
      buffer_format_, gfx::BufferUsage::SCANOUT);

  // Wait for the images to be allocated.
  zx_status_t wait_status;
  fuchsia::sysmem::BufferCollectionInfo_2 buffers_info;
  status = collection->WaitForBuffersAllocated(&wait_status, &buffers_info);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection failed";
    return {};
  }

  if (wait_status != ZX_OK) {
    ZX_DLOG(ERROR, wait_status)
        << "Sysmem buffer collection allocation failed.";
    return {};
  }

  DCHECK_GE(buffers_info.buffer_count, num_images);

  // We no longer need the BufferCollection connection. Close it to ensure
  // ImagePipe can still use the collection after BufferCollection connection
  // is dropped below.
  collection->Close();

  // Create PresenterImageFuchsia for each buffer in the collection.
  uint32_t image_usage = gpu::SHARED_IMAGE_USAGE_RASTER;
  if (vulkan->enforce_protected_memory())
    image_usage |= gpu::SHARED_IMAGE_USAGE_PROTECTED;

  std::vector<std::unique_ptr<OutputPresenter::Image>> images;
  images.reserve(num_images);

  fuchsia::sysmem::ImageFormat_2 image_format;
  image_format.coded_width = frame_size_.width();
  image_format.coded_height = frame_size_.height();

  // Create an image for each buffer in the collection.
  for (size_t i = 0; i < num_images; ++i) {
    last_image_id_++;
    image_pipe_->AddImage(last_image_id_, last_buffer_collection_id_, i,
                          image_format);

    gfx::GpuMemoryBufferHandle gmb_handle;
    gmb_handle.type = gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
    gmb_handle.native_pixmap_handle.buffer_collection_id = buffer_collection_id;
    gmb_handle.native_pixmap_handle.buffer_index = i;

    auto mailbox = gpu::Mailbox::GenerateForSharedImage();
    if (!shared_image_factory_.CreateSharedImage(
            mailbox, gpu::kInProcessCommandBufferClientId,
            std::move(gmb_handle), buffer_format_, gpu::kNullSurfaceHandle,
            frame_size_, color_space, image_usage)) {
      return {};
    }

    auto image = std::make_unique<PresenterImageFuchsia>(last_image_id_);
    if (!image->Initialize(&shared_image_factory_,
                           &shared_image_representation_factory_, mailbox,
                           dependency_)) {
      return {};
    }
    images.push_back(std::move(image));
  }

  return images;
}

void OutputPresenterFuchsia::SwapBuffers(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  if (!image_pipe_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    return;
  }

  // SwapBuffer() should be called only after SchedulePrimaryPlane().
  DCHECK(next_frame_);

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "viz", "OutputPresenterFuchsia::PresentQueue", TRACE_ID_LOCAL(this),
      "image_id", next_frame_->image_id);

  next_frame_->completion_callback = std::move(completion_callback);
  next_frame_->presentation_callback = std::move(presentation_callback);

  pending_frames_.push_back(std::move(next_frame_.value()));
  next_frame_.reset();

  if (!present_is_pending_)
    PresentNextFrame();
}

void OutputPresenterFuchsia::PostSubBuffer(
    const gfx::Rect& rect,
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // Sub buffer presentation is not supported.
  NOTREACHED();
}

void OutputPresenterFuchsia::CommitOverlayPlanes(
    SwapCompletionCallback completion_callback,
    BufferPresentedCallback presentation_callback) {
  // Overlays are not supported yet.
  NOTREACHED();
}

void OutputPresenterFuchsia::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane,
    Image* image,
    bool is_submitted) {
  auto* image_fuchsia = static_cast<PresenterImageFuchsia*>(image);

  DCHECK(!next_frame_);
  next_frame_ = PendingFrame();
  next_frame_->image_id = image_fuchsia->image_id();
  next_frame_->buffer_collection_id = last_buffer_collection_id_;

  // Take semaphores for the image and covert them to zx::events that are later
  // passed to ImagePipe::PresentImage().
  std::vector<GrBackendSemaphore> read_begin_semaphores;
  std::vector<GrBackendSemaphore> read_end_semaphores;
  image_fuchsia->TakeSemaphores(&read_begin_semaphores, &read_end_semaphores);

  auto* vulkan_context_provider = dependency_->GetVulkanContextProvider();
  auto* vulkan_implementation =
      vulkan_context_provider->GetVulkanImplementation();
  VkDevice vk_device =
      vulkan_context_provider->GetDeviceQueue()->GetVulkanDevice();

  GrSemaphoresToZxEvents(vulkan_implementation, vk_device,
                         read_begin_semaphores, &(next_frame_->acquire_fences));
  GrSemaphoresToZxEvents(vulkan_implementation, vk_device, read_end_semaphores,
                         &(next_frame_->release_fences));

  // Destroy |read_begin_semaphores|, but not |read_end_semaphores|, since
  // SharedImageRepresentationSkia::BeginScopedReadAccess() keeps ownership of
  // the end_semaphores.
  for (auto& semaphore : read_begin_semaphores) {
    vkDestroySemaphore(vk_device, semaphore.vkSemaphore(), nullptr);
  }
}

std::vector<OutputPresenter::OverlayData>
OutputPresenterFuchsia::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  // Overlays are not supported yet.
  NOTREACHED();
  return {};
}

void OutputPresenterFuchsia::PresentNextFrame() {
  DCHECK(!present_is_pending_);
  DCHECK(!pending_frames_.empty());

  TRACE_EVENT_NESTABLE_ASYNC_END1("viz", "OutputPresenterFuchsia::PresentQueue",
                                  TRACE_ID_LOCAL(this), "image_id",
                                  pending_frames_.front().image_id);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "viz", "OutputPresenterFuchsia::PresentFrame", TRACE_ID_LOCAL(this),
      "image_id", pending_frames_.front().image_id);

  present_is_pending_ = true;
  uint64_t target_presentation_time = zx_clock_get_monotonic();
  image_pipe_->PresentImage(
      pending_frames_.front().image_id, target_presentation_time,
      std::move(pending_frames_.front().acquire_fences),
      std::move(pending_frames_.front().release_fences),
      fit::bind_member(this, &OutputPresenterFuchsia::OnPresentComplete));
}

void OutputPresenterFuchsia::OnPresentComplete(
    fuchsia::images::PresentationInfo presentation_info) {
  DCHECK(present_is_pending_);
  present_is_pending_ = false;

  TRACE_EVENT_NESTABLE_ASYNC_END1("viz", "OutputPresenterFuchsia::PresentFrame",
                                  TRACE_ID_LOCAL(this), "image_id",
                                  pending_frames_.front().image_id);

  std::move(pending_frames_.front().completion_callback)
      .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_ACK));
  std::move(pending_frames_.front().presentation_callback)
      .Run(gfx::PresentationFeedback(
          base::TimeTicks::FromZxTime(presentation_info.presentation_time),
          base::TimeDelta::FromZxDuration(
              presentation_info.presentation_interval),
          gfx::PresentationFeedback::kVSync));

  if (pending_frames_.front().remove_buffer_collection) {
    image_pipe_->RemoveBufferCollection(
        pending_frames_.front().buffer_collection_id);
  }

  pending_frames_.pop_front();
  if (!pending_frames_.empty())
    PresentNextFrame();
}

}  // namespace viz
