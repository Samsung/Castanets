// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_util.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gpu/config/gpu_info.h"  // nogncheck
#include "gpu/config/vulkan_info.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {
namespace {
uint64_t g_submit_count = 0;
}

bool SubmitSignalVkSemaphores(VkQueue vk_queue,
                              const base::span<VkSemaphore>& vk_semaphores,
                              VkFence vk_fence) {
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.signalSemaphoreCount = vk_semaphores.size();
  submit_info.pSignalSemaphores = vk_semaphores.data();
  const unsigned int submit_count = 1;
  return vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) ==
         VK_SUCCESS;
}

bool SubmitSignalVkSemaphore(VkQueue vk_queue,
                             VkSemaphore vk_semaphore,
                             VkFence vk_fence) {
  return SubmitSignalVkSemaphores(
      vk_queue, base::span<VkSemaphore>(&vk_semaphore, 1), vk_fence);
}

bool SubmitWaitVkSemaphores(VkQueue vk_queue,
                            const base::span<VkSemaphore>& vk_semaphores,
                            VkFence vk_fence) {
  DCHECK(!vk_semaphores.empty());
  std::vector<VkPipelineStageFlags> semaphore_stages(vk_semaphores.size());
  std::fill(semaphore_stages.begin(), semaphore_stages.end(),
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  // Structure specifying a queue submit operation.
  VkSubmitInfo submit_info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  submit_info.waitSemaphoreCount = vk_semaphores.size();
  submit_info.pWaitSemaphores = vk_semaphores.data();
  submit_info.pWaitDstStageMask = semaphore_stages.data();
  const unsigned int submit_count = 1;
  return vkQueueSubmit(vk_queue, submit_count, &submit_info, vk_fence) ==
         VK_SUCCESS;
}

bool SubmitWaitVkSemaphore(VkQueue vk_queue,
                           VkSemaphore vk_semaphore,
                           VkFence vk_fence) {
  return SubmitWaitVkSemaphores(
      vk_queue, base::span<VkSemaphore>(&vk_semaphore, 1), vk_fence);
}

VkSemaphore CreateExternalVkSemaphore(
    VkDevice vk_device,
    VkExternalSemaphoreHandleTypeFlags handle_types) {
  VkExportSemaphoreCreateInfo export_info = {
      VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
  export_info.handleTypes = handle_types;

  VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                    &export_info};

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkResult result =
      vkCreateSemaphore(vk_device, &sem_info, nullptr, &semaphore);

  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkSemaphore: " << result;
    return VK_NULL_HANDLE;
  }

  return semaphore;
}

std::string VkVersionToString(uint32_t version) {
  return base::StringPrintf("%u.%u.%u", VK_VERSION_MAJOR(version),
                            VK_VERSION_MINOR(version),
                            VK_VERSION_PATCH(version));
}

VkResult QueueSubmitHook(VkQueue queue,
                         uint32_t submitCount,
                         const VkSubmitInfo* pSubmits,
                         VkFence fence) {
  g_submit_count++;
  return vkQueueSubmit(queue, submitCount, pSubmits, fence);
}

void ReportQueueSubmitPerSwapBuffers() {
  static uint64_t last_count = 0;
  UMA_HISTOGRAM_CUSTOM_COUNTS("GPU.Vulkan.QueueSubmitPerSwapBuffers",
                              g_submit_count - last_count, 1, 50, 50);
  last_count = g_submit_count;
}

bool CheckVulkanCompabilities(const VulkanInfo& vulkan_info,
                              const GPUInfo& gpu_info) {
// Android uses AHB and SyncFD for interop. They are imported into GL with other
// API.
#if !defined(OS_ANDROID)
#if defined(OS_WIN)
  constexpr char kMemoryObjectExtension[] = "GL_EXT_memory_object_win32";
  constexpr char kSemaphoreExtension[] = "GL_EXT_semaphore_win32";
#elif defined(OS_FUCHSIA)
  constexpr char kMemoryObjectExtension[] = "GL_ANGLE_memory_object_fuchsia";
  constexpr char kSemaphoreExtension[] = "GL_ANGLE_semaphore_fuchsia";
#else
  constexpr char kMemoryObjectExtension[] = "GL_EXT_memory_object_fd";
  constexpr char kSemaphoreExtension[] = "GL_EXT_semaphore_fd";
#endif
  // If both Vulkan and GL are using native GPU (non swiftshader), check
  // necessary extensions for GL and Vulkan interop.
  const auto extensions = gfx::MakeExtensionSet(gpu_info.gl_extensions);
  if (!gfx::HasExtension(extensions, kMemoryObjectExtension) ||
      !gfx::HasExtension(extensions, kSemaphoreExtension)) {
    DLOG(ERROR) << kMemoryObjectExtension << " or " << kSemaphoreExtension
                << " is not supported.";
    return false;
  }
#endif  // !defined(OS_ANDROID)

#if defined(OS_ANDROID)
  if (vulkan_info.physical_devices.empty())
    return false;

  const auto& device_info = vulkan_info.physical_devices.front();
  constexpr uint32_t kVendorARM = 0x13b5;

  // https://crbug.com/1096222: Display problem with Huawei and Honor devices
  // with Mali GPU. The Mali driver version is < 19.0.0.
  if (device_info.properties.vendorID == kVendorARM &&
      device_info.properties.driverVersion < VK_MAKE_VERSION(19, 0, 0)) {
    return false;
  }
#endif  // defined(OS_ANDROID)

  return true;
}

}  // namespace gpu