// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_requestor.h"

#include <stddef.h>
#include <xcb/xcb.h>

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/x/selection_utils.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

class SelectionRequestorTest : public testing::Test {
 public:
  SelectionRequestorTest() : x_display_(gfx::GetXDisplay()) {}

  ~SelectionRequestorTest() override = default;

  // Responds to the SelectionRequestor's XConvertSelection() request by
  // - Setting the property passed into the XConvertSelection() request to
  //   |value|.
  // - Sending a SelectionNotify event.
  void SendSelectionNotify(x11::Atom selection,
                           x11::Atom target,
                           const std::string& value) {
    ui::SetStringProperty(x_window_, requestor_->x_property_,
                          gfx::GetAtom("STRING"), value);

    xcb_generic_event_t ge;
    memset(&ge, 0, sizeof(ge));
    auto* event = reinterpret_cast<xcb_selection_notify_event_t*>(&ge);
    event->response_type = x11::SelectionNotifyEvent::opcode;
    event->sequence = 0;
    event->requestor = static_cast<uint32_t>(x_window_);
    event->selection = static_cast<uint32_t>(selection);
    event->target = static_cast<uint32_t>(target);
    event->property = static_cast<uint32_t>(requestor_->x_property_);
    event->time = x11::CurrentTime;

    x11::Event xev(&ge, x11::Connection::Get());
    requestor_->OnSelectionNotify(*xev.As<x11::SelectionNotifyEvent>());
  }

 protected:
  void SetUp() override {
    // Make X11 synchronous for our display connection.
    XSynchronize(x_display_, x11::True);

    // Create a window for the selection requestor to use.
    x_window_ = static_cast<x11::Window>(XCreateWindow(
        x_display_, DefaultRootWindow(x_display_), 0, 0, 10,
        10,  // x, y, width, height
        0,   // border width
        static_cast<int>(x11::WindowClass::CopyFromParent),  // depth
        static_cast<int>(x11::WindowClass::InputOnly),
        nullptr,  // visual
        0, nullptr));

    event_source_ = PlatformEventSource::CreateDefault();
    CHECK(PlatformEventSource::GetInstance());
    requestor_ = std::make_unique<SelectionRequestor>(x_window_, nullptr);
  }

  void TearDown() override {
    requestor_.reset();
    event_source_.reset();
    XDestroyWindow(x_display_, static_cast<uint32_t>(x_window_));
    XSynchronize(x_display_, x11::False);
  }

  Display* x_display_;

  // |requestor_|'s window.
  x11::Window x_window_ = x11::Window::None;

  std::unique_ptr<PlatformEventSource> event_source_;
  std::unique_ptr<SelectionRequestor> requestor_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};

 private:
  DISALLOW_COPY_AND_ASSIGN(SelectionRequestorTest);
};

namespace {

// Converts |selection| to |target| and checks the returned values.
void PerformBlockingConvertSelection(SelectionRequestor* requestor,
                                     x11::Atom selection,
                                     x11::Atom target,
                                     const std::string& expected_data) {
  std::vector<uint8_t> out_data;
  x11::Atom out_type = x11::Atom::None;
  EXPECT_TRUE(requestor->PerformBlockingConvertSelection(selection, target,
                                                         &out_data, &out_type));
  EXPECT_EQ(expected_data.size(), out_data.size());
  EXPECT_EQ(expected_data, ui::RefCountedMemoryToString(
                               base::RefCountedBytes::TakeVector(&out_data)));
  EXPECT_EQ(gfx::GetAtom("STRING"), out_type);
}

}  // namespace

// Test that SelectionRequestor correctly handles receiving a request while it
// is processing another request.
TEST_F(SelectionRequestorTest, NestedRequests) {
  // Assume that |selection| will have no owner. If there is an owner, the owner
  // will set the property passed into the XConvertSelection() request which is
  // undesirable.
  x11::Atom selection = gfx::GetAtom("FAKE_SELECTION");

  x11::Atom target1 = gfx::GetAtom("TARGET1");
  x11::Atom target2 = gfx::GetAtom("TARGET2");

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PerformBlockingConvertSelection,
                                base::Unretained(requestor_.get()), selection,
                                target2, "Data2"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target1, "Data1"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionRequestorTest::SendSelectionNotify,
                     base::Unretained(this), selection, target2, "Data2"));
  PerformBlockingConvertSelection(requestor_.get(), selection, target1,
                                  "Data1");
}

}  // namespace ui
