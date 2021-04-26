// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "components/exo/surface.h"
#include "components/exo/wayland/server_util.h"

namespace exo {
namespace wayland {

WaylandDisplayOutput::WaylandDisplayOutput(int64_t id) : id_(id) {}

WaylandDisplayOutput::~WaylandDisplayOutput() {
  // Empty the output_ids_ so that Unregister will be no op.
  auto ids = std::move(output_ids_);
  for (auto pair : ids)
    wl_resource_destroy(pair.second);

  if (global_)
    wl_global_destroy(global_);
}

int64_t WaylandDisplayOutput::id() const {
  return id_;
}

void WaylandDisplayOutput::set_global(wl_global* global) {
  global_ = global;
}

void WaylandDisplayOutput::UnregisterOutput(wl_resource* output_resource) {
  base::EraseIf(output_ids_, [output_resource](auto& pair) {
    return pair.second == output_resource;
  });
}

void WaylandDisplayOutput::RegisterOutput(wl_resource* output_resource) {
  auto* client = wl_resource_get_client(output_resource);
  output_ids_.insert(std::make_pair(client, output_resource));
}

wl_resource* WaylandDisplayOutput::GetOutputResourceForClient(
    wl_client* client) {
  auto iter = output_ids_.find(client);
  if (iter == output_ids_.end())
    return nullptr;
  return iter->second;
}

}  // namespace wayland
}  // namespace exo
