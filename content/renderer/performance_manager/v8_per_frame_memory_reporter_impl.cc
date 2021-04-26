// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/performance_manager/v8_per_frame_memory_reporter_impl.h"

#include "base/containers/flat_map.h"
#include "content/public/common/isolated_world_ids.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "v8/include/v8.h"

namespace performance_manager {

namespace {

class FrameAssociatedMeasurementDelegate : public v8::MeasureMemoryDelegate {
 public:
  using GetPerFrameV8MemoryUsageDataCallback =
      mojom::V8PerFrameMemoryReporter::GetPerFrameV8MemoryUsageDataCallback;

  explicit FrameAssociatedMeasurementDelegate(
      GetPerFrameV8MemoryUsageDataCallback&& callback)
      : callback_(std::move(callback)) {}

  ~FrameAssociatedMeasurementDelegate() override {
    if (callback_) {
      std::move(callback_).Run(mojom::PerProcessV8MemoryUsageData::New());
    }
  }

 private:
  bool ShouldMeasure(v8::Local<v8::Context> context) override {
    // Measure all contexts.
    return true;
  }

  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override {
    mojom::PerProcessV8MemoryUsageDataPtr result =
        mojom::PerProcessV8MemoryUsageData::New();

    result->unassociated_bytes_used = unattributed_size_in_bytes;

    // Keep track of the per-frame data throughout this loop.
    base::flat_map<blink::WebLocalFrame*, mojom::PerFrameV8MemoryUsageDataPtr>
        frames;
    for (const auto& context_and_size : context_sizes_in_bytes) {
      const v8::Local<v8::Context>& context = context_and_size.first;
      const size_t size = context_and_size.second;

      blink::WebLocalFrame* frame =
          blink::WebLocalFrame::FrameForContext(context);

      if (!frame) {
        // TODO(crbug.com/1080672): It would be prefereable to count the
        // V8SchemaRegistry context's overhead with unassociated_bytes, but at
        // present there isn't a public API that allows this distinction.
        ++(result->num_unassociated_contexts);
        result->unassociated_context_bytes_used += size;
      } else {
        mojom::PerFrameV8MemoryUsageData* per_frame_resources =
            frames[frame].get();
        if (!per_frame_resources) {
#if DCHECK_IS_ON()
          // Check that the frame token didn't already occur.
          for (const auto& entry : frames) {
            // This frame was already added to the map by frames[frame] above.
            if (frame == entry.first)
              continue;
            DCHECK_NE(entry.first->GetFrameToken(), frame->GetFrameToken());
          }
#endif
          auto new_resources = mojom::PerFrameV8MemoryUsageData::New();
          new_resources->frame_token = frame->GetFrameToken();
          per_frame_resources = new_resources.get();
          frames[frame] = std::move(new_resources);
        }

        mojom::V8IsolatedWorldMemoryUsagePtr isolated_world_usage =
            mojom::V8IsolatedWorldMemoryUsage::New();
        isolated_world_usage->bytes_used = size;
        int32_t world_id = frame->GetScriptContextWorldId(context);

        if (world_id != content::ISOLATED_WORLD_ID_GLOBAL) {
          isolated_world_usage->stable_id =
              frame->GetIsolatedWorldStableId(context).Utf8();
          isolated_world_usage->human_readable_name =
              frame->GetIsolatedWorldHumanReadableName(context).Utf8();
        }

        DCHECK(
            !base::Contains(per_frame_resources->associated_bytes, world_id));
        per_frame_resources->associated_bytes[world_id] =
            std::move(isolated_world_usage);
      }
    }
    // Move the per-frame memory values to the result.
    for (auto& entry : frames)
      result->associated_memory.push_back(std::move(entry.second));

    std::move(callback_).Run(std::move(result));
  }

  GetPerFrameV8MemoryUsageDataCallback callback_;
};

}  // namespace

// static
void V8PerFrameMemoryReporterImpl::Create(
    mojo::PendingReceiver<mojom::V8PerFrameMemoryReporter> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<V8PerFrameMemoryReporterImpl>(),
                              std::move(receiver));
}

void V8PerFrameMemoryReporterImpl::GetPerFrameV8MemoryUsageData(
    GetPerFrameV8MemoryUsageDataCallback callback) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (!isolate) {
    std::move(callback).Run(mojom::PerProcessV8MemoryUsageData::New());
  } else {
    std::unique_ptr<FrameAssociatedMeasurementDelegate> delegate =
        std::make_unique<FrameAssociatedMeasurementDelegate>(
            std::move(callback));

    isolate->MeasureMemory(std::move(delegate));
  }
}

}  // namespace performance_manager
