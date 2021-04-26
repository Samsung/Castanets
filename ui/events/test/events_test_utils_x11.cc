// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/events_test_utils_x11.h"

#include <stddef.h>
#include <xcb/xproto.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/xinput.h"

namespace {

// Converts ui::EventType to state for X*Events.
unsigned int XEventState(int flags) {
  return ((flags & ui::EF_SHIFT_DOWN) ? ShiftMask : 0) |
         ((flags & ui::EF_CAPS_LOCK_ON) ? LockMask : 0) |
         ((flags & ui::EF_CONTROL_DOWN) ? ControlMask : 0) |
         ((flags & ui::EF_ALT_DOWN) ? Mod1Mask : 0) |
         ((flags & ui::EF_NUM_LOCK_ON) ? Mod2Mask : 0) |
         ((flags & ui::EF_MOD3_DOWN) ? Mod3Mask : 0) |
         ((flags & ui::EF_COMMAND_DOWN) ? Mod4Mask : 0) |
         ((flags & ui::EF_ALTGR_DOWN) ? Mod5Mask : 0) |
         ((flags & ui::EF_LEFT_MOUSE_BUTTON) ? Button1Mask : 0) |
         ((flags & ui::EF_MIDDLE_MOUSE_BUTTON) ? Button2Mask : 0) |
         ((flags & ui::EF_RIGHT_MOUSE_BUTTON) ? Button3Mask : 0);
}

// Converts EventType to XKeyEvent type.
int XKeyEventType(ui::EventType type) {
  switch (type) {
    case ui::ET_KEY_PRESSED:
      return x11::KeyEvent::Press;
    case ui::ET_KEY_RELEASED:
      return x11::KeyEvent::Release;
    default:
      return 0;
  }
}

// Converts EventType to XI2 event type.
int XIKeyEventType(ui::EventType type) {
  switch (type) {
    case ui::ET_KEY_PRESSED:
      return XI_KeyPress;
    case ui::ET_KEY_RELEASED:
      return XI_KeyRelease;
    default:
      return 0;
  }
}

int XIButtonEventType(ui::EventType type) {
  switch (type) {
    case ui::ET_MOUSEWHEEL:
    case ui::ET_MOUSE_PRESSED:
      // The button release X events for mouse wheels are dropped by Aura.
      return XI_ButtonPress;
    case ui::ET_MOUSE_RELEASED:
      return XI_ButtonRelease;
    default:
      NOTREACHED();
      return 0;
  }
}

// Converts Aura event type and flag to X button event.
unsigned int XButtonEventButton(ui::EventType type, int flags) {
  // Aura events don't keep track of mouse wheel button, so just return
  // the first mouse wheel button.
  if (type == ui::ET_MOUSEWHEEL)
    return 4;

  if (flags & ui::EF_LEFT_MOUSE_BUTTON)
    return 1;
  if (flags & ui::EF_MIDDLE_MOUSE_BUTTON)
    return 2;
  if (flags & ui::EF_RIGHT_MOUSE_BUTTON)
    return 3;

  return 0;
}

void InitValuatorsForXIDeviceEvent(XIDeviceEvent* xiev,
                                   x11::Input::DeviceEvent* devev) {
  int valuator_count = ui::DeviceDataManagerX11::DT_LAST_ENTRY;
  xiev->valuators.mask_len = (valuator_count / 8) + 1;
  xiev->valuators.mask = new unsigned char[xiev->valuators.mask_len];
  memset(xiev->valuators.mask, 0, xiev->valuators.mask_len);
  xiev->valuators.values = new double[valuator_count];

  devev->valuator_mask.resize((xiev->valuators.mask_len + 3) / 4);
  devev->axisvalues.resize(valuator_count);
}

x11::Event CreateXInput2Event(int deviceid,
                              int evtype,
                              int tracking_id,
                              const gfx::Point& location) {
  XEvent xlib_event;
  memset(&xlib_event, 0, sizeof(xlib_event));
  xlib_event.type = x11::GeGenericEvent::opcode;
  xlib_event.xcookie.data = new XIDeviceEvent;
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xlib_event.xcookie.data);
  memset(xiev, 0, sizeof(XIDeviceEvent));
  xiev->deviceid = deviceid;
  xiev->sourceid = deviceid;
  xiev->evtype = evtype;
  xiev->detail = tracking_id;
  xiev->event_x = location.x();
  xiev->event_y = location.y();
  xiev->event = DefaultRootWindow(gfx::GetXDisplay());
  if (evtype == XI_ButtonPress || evtype == XI_ButtonRelease) {
    xiev->buttons.mask_len = 8;
    xiev->buttons.mask = new unsigned char[xiev->buttons.mask_len];
    memset(xiev->buttons.mask, 0, xiev->buttons.mask_len);
  }

  x11::Input::DeviceEvent event;
  event.deviceid = static_cast<x11::Input::DeviceId>(deviceid);
  event.sourceid = static_cast<x11::Input::DeviceId>(deviceid);
  event.opcode = static_cast<x11::Input::DeviceEvent::Opcode>(evtype);
  event.detail = tracking_id;
  event.event_x = {location.x(), 0};
  event.event_y = {location.y(), 0};
  event.event = static_cast<x11::Window>(DefaultRootWindow(gfx::GetXDisplay()));
  event.button_mask = {0, 0};

  return x11::Event(&xlib_event, std::move(event));
}

}  // namespace

namespace ui {

ScopedXI2Event::ScopedXI2Event() = default;
ScopedXI2Event::~ScopedXI2Event() = default;

void ScopedXI2Event::InitKeyEvent(EventType type,
                                  KeyboardCode key_code,
                                  int flags) {
  auto* connection = x11::Connection::Get();
  xcb_generic_event_t ge;
  memset(&ge, 0, sizeof(ge));
  auto* key = reinterpret_cast<xcb_key_press_event_t*>(&ge);
  key->response_type = XKeyEventType(type);
  CHECK_NE(0, key->response_type);
  key->sequence = 0;
  key->time = 0;
  key->event = 0;
  key->root = 0;
  key->child = 0;
  key->event_x = 0;
  key->event_y = 0;
  key->root_x = 0;
  key->root_y = 0;
  key->state = XEventState(flags);
  key->detail =
      XKeyCodeForWindowsKeyCode(key_code, flags, connection->display());
  key->same_screen = 1;

  x11::Event x11_event(&ge, connection);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitMotionEvent(const gfx::Point& location,
                                     const gfx::Point& root_location,
                                     int flags) {
  auto* connection = x11::Connection::Get();
  xcb_generic_event_t ge;
  memset(&ge, 0, sizeof(ge));
  auto* motion = reinterpret_cast<xcb_motion_notify_event_t*>(&ge);
  motion->response_type = MotionNotify;
  motion->sequence = 0;
  motion->time = 0;
  motion->event = 0;
  motion->root = 0;
  motion->child = 0;
  motion->event_x = location.x();
  motion->event_y = location.y();
  motion->root_x = root_location.x();
  motion->root_y = root_location.y();
  motion->state = XEventState(flags);
  motion->same_screen = 1;

  x11::Event x11_event(&ge, connection);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitButtonEvent(EventType type,
                                     const gfx::Point& location,
                                     int flags) {
  auto* connection = x11::Connection::Get();
  xcb_generic_event_t ge;
  memset(&ge, 0, sizeof(ge));
  auto* button = reinterpret_cast<xcb_button_press_event_t*>(&ge);
  button->response_type = (type == ui::ET_MOUSE_PRESSED)
                              ? x11::ButtonEvent::Press
                              : x11::ButtonEvent::Release;
  button->sequence = 0;
  button->time = 0;
  button->event = 0;
  button->root = 0;
  button->event_x = location.x();
  button->event_y = location.y();
  button->root_x = location.x();
  button->root_y = location.y();
  button->state = 0;
  button->detail = XButtonEventButton(type, flags);
  button->same_screen = 1;

  x11::Event x11_event(&ge, connection);
  event_ = std::move(x11_event);
}

void ScopedXI2Event::InitGenericKeyEvent(int deviceid,
                                         int sourceid,
                                         EventType type,
                                         KeyboardCode key_code,
                                         int flags) {
  event_ = CreateXInput2Event(deviceid, XIKeyEventType(type), 0, gfx::Point());
  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(event_.xlib_event().xcookie.data);
  CHECK_NE(0, xievent->evtype);
  XDisplay* display = gfx::GetXDisplay();
  event_.xlib_event().xgeneric.display = display;
  xievent->display = display;
  xievent->mods.effective = XEventState(flags);
  xievent->detail = XKeyCodeForWindowsKeyCode(key_code, flags, display);
  xievent->sourceid = sourceid;

  auto* dev_event = event_.As<x11::Input::DeviceEvent>();
  dev_event->mods.effective = XEventState(flags);
  dev_event->detail = XKeyCodeForWindowsKeyCode(key_code, flags, display);
  dev_event->sourceid = static_cast<x11::Input::DeviceId>(sourceid);
}

void ScopedXI2Event::InitGenericButtonEvent(int deviceid,
                                            EventType type,
                                            const gfx::Point& location,
                                            int flags) {
  event_ =
      CreateXInput2Event(deviceid, XIButtonEventType(type), 0, gfx::Point());
  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(event_.xlib_event().xcookie.data);
  xievent->mods.effective = XEventState(flags);
  xievent->detail = XButtonEventButton(type, flags);
  xievent->event_x = location.x();
  xievent->event_y = location.y();
  XISetMask(xievent->buttons.mask, xievent->detail);

  auto* dev_event = event_.As<x11::Input::DeviceEvent>();
  dev_event->mods.effective = XEventState(flags);
  dev_event->detail = XButtonEventButton(type, flags);
  dev_event->event_x = {location.x(), 0};
  dev_event->event_y = {location.y(), 0};
  XISetMask(dev_event->button_mask.data(), xievent->detail);

  // Setup an empty valuator list for generic button events.
  SetUpValuators(std::vector<Valuator>());
}

void ScopedXI2Event::InitGenericMouseWheelEvent(int deviceid,
                                                int wheel_delta,
                                                int flags) {
  InitGenericButtonEvent(deviceid, ui::ET_MOUSEWHEEL, gfx::Point(), flags);
  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(event_.xlib_event().xcookie.data);
  xievent->detail = wheel_delta > 0 ? 4 : 5;

  event_.As<x11::Input::DeviceEvent>()->detail = xievent->detail;
}

void ScopedXI2Event::InitScrollEvent(int deviceid,
                                     int x_offset,
                                     int y_offset,
                                     int x_offset_ordinal,
                                     int y_offset_ordinal,
                                     int finger_count) {
  event_ = CreateXInput2Event(deviceid, XI_Motion, 0, gfx::Point());

  Valuator valuators[] = {
      Valuator(DeviceDataManagerX11::DT_CMT_SCROLL_X, x_offset),
      Valuator(DeviceDataManagerX11::DT_CMT_SCROLL_Y, y_offset),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_X, x_offset_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_Y, y_offset_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_FINGER_COUNT, finger_count)};
  SetUpValuators(
      std::vector<Valuator>(valuators, valuators + base::size(valuators)));
}

void ScopedXI2Event::InitFlingScrollEvent(int deviceid,
                                          int x_velocity,
                                          int y_velocity,
                                          int x_velocity_ordinal,
                                          int y_velocity_ordinal,
                                          bool is_cancel) {
  event_ = CreateXInput2Event(deviceid, XI_Motion, deviceid, gfx::Point());

  Valuator valuators[] = {
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_STATE, is_cancel ? 1 : 0),
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_Y, y_velocity),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_Y, y_velocity_ordinal),
      Valuator(DeviceDataManagerX11::DT_CMT_FLING_X, x_velocity),
      Valuator(DeviceDataManagerX11::DT_CMT_ORDINAL_X, x_velocity_ordinal)};

  SetUpValuators(
      std::vector<Valuator>(valuators, valuators + base::size(valuators)));
}

void ScopedXI2Event::InitTouchEvent(int deviceid,
                                    int evtype,
                                    int tracking_id,
                                    const gfx::Point& location,
                                    const std::vector<Valuator>& valuators) {
  event_ = CreateXInput2Event(deviceid, evtype, tracking_id, location);

  // If a timestamp was specified, setup the event.
  for (size_t i = 0; i < valuators.size(); ++i) {
    if (valuators[i].data_type ==
        DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP) {
      SetUpValuators(valuators);
      return;
    }
  }

  // No timestamp was specified. Use |ui::EventTimeForNow()|.
  std::vector<Valuator> valuators_with_time = valuators;
  valuators_with_time.emplace_back(
      DeviceDataManagerX11::DT_TOUCH_RAW_TIMESTAMP,
      (ui::EventTimeForNow() - base::TimeTicks()).InMicroseconds());
  SetUpValuators(valuators_with_time);
}

void ScopedXI2Event::SetUpValuators(const std::vector<Valuator>& valuators) {
  auto* devev = event_.As<x11::Input::DeviceEvent>();
  CHECK(devev);
  CHECK_EQ(x11::GeGenericEvent::opcode, event_.xlib_event().type);
  XIDeviceEvent* xiev =
      static_cast<XIDeviceEvent*>(event_.xlib_event().xcookie.data);
  InitValuatorsForXIDeviceEvent(xiev, devev);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  for (auto valuator : valuators) {
    manager->SetValuatorDataForTest(xiev, devev, valuator.data_type,
                                    valuator.value);
  }
}

void SetUpTouchPadForTest(int deviceid) {
  std::vector<int> device_list;
  device_list.push_back(deviceid);

  TouchFactory::GetInstance()->SetPointerDeviceForTest(device_list);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(std::vector<int>(), device_list,
                                std::vector<int>());
}

void SetUpTouchDevicesForTest(const std::vector<int>& devices) {
  TouchFactory::GetInstance()->SetTouchDeviceForTest(devices);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(devices, std::vector<int>(),
                                std::vector<int>());
}

void SetUpPointerDevicesForTest(const std::vector<int>& devices) {
  TouchFactory::GetInstance()->SetPointerDeviceForTest(devices);
  ui::DeviceDataManagerX11* manager = ui::DeviceDataManagerX11::GetInstance();
  manager->SetDeviceListForTest(std::vector<int>(), std::vector<int>(),
                                devices);
}

}  // namespace ui
