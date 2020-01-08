// Copyright (c) 2013 Intel Corporation. All rights reserved.
// Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/wrt/xwalk_extensions/renderer/xwalk_v8tools_module.h"

#include "base/logging.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

void ForceSetPropertyCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() != 3 || !info[0]->IsObject() || !info[1]->IsString()) {
    return;
  }
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  auto key = info[1]->ToString(context).FromMaybe(v8::Local<v8::String>());
  auto maybe =
      info[0].As<v8::Object>()->DefineOwnProperty(context, key, info[2]);
  if (!maybe.FromMaybe(false))
    LOG(ERROR) << "Fail to set property";
}

// ================
// lifecycleTracker
// ================
struct LifecycleTrackerWrapper {
  v8::Global<v8::Object> handle;
  v8::Global<v8::Function> destructor;
};

void LifecycleTrackerCleanup(
    const v8::WeakCallbackInfo<LifecycleTrackerWrapper>& data) {
  LifecycleTrackerWrapper* wrapper = data.GetParameter();

  if (!wrapper->destructor.IsEmpty()) {
    v8::HandleScope handle_scope(data.GetIsolate());
    v8::Local<v8::Context> context = v8::Context::New(data.GetIsolate());
    v8::Context::Scope scope(context);

    v8::Local<v8::Function> destructor =
      wrapper->destructor.Get(data.GetIsolate());

    v8::MicrotasksScope microtasks(
        data.GetIsolate(), v8::MicrotasksScope::kDoNotRunMicrotasks);

    v8::TryCatch try_catch(data.GetIsolate());
    destructor->Call(context->Global(), 0, nullptr);

    if (try_catch.HasCaught()) {
      LOG(WARNING) << "Exception when running LifecycleTracker destructor";
    }
  }
}

void LifecycleTracker(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(info.GetIsolate());

  v8::Local<v8::Object> tracker = v8::Object::New(isolate);
  LifecycleTrackerWrapper* wrapper = new LifecycleTrackerWrapper;
  wrapper->handle.Reset(isolate, tracker);
  wrapper->handle.SetWeak(wrapper, LifecycleTrackerCleanup,
                          v8::WeakCallbackType::kParameter);
  info.GetReturnValue().Set(wrapper->handle);
}

}  // namespace

XWalkV8ToolsModule::XWalkV8ToolsModule() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  auto object_template = v8::ObjectTemplate::New(isolate);

  // TODO(cmarcelo): Use Template::Set() function that takes isolate, once we
  // update the Chromium (and V8) version.
  object_template->Set(v8::String::NewFromUtf8(isolate, "forceSetProperty"),
                       v8::FunctionTemplate::New(
                          isolate, ForceSetPropertyCallback));
  object_template->Set(v8::String::NewFromUtf8(isolate, "lifecycleTracker"),
                       v8::FunctionTemplate::New(isolate, LifecycleTracker));

  object_template_.Reset(isolate, object_template);
}

XWalkV8ToolsModule::~XWalkV8ToolsModule() {
  object_template_.Reset();
}

v8::Handle<v8::Object> XWalkV8ToolsModule::NewInstance() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Handle<v8::ObjectTemplate> object_template =
      v8::Local<v8::ObjectTemplate>::New(isolate, object_template_);
  return handle_scope.Escape(object_template->NewInstance());
}

}  // namespace extensions
