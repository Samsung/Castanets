/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd All Rights Reserved
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "third_party/wrt/xwalk_extensions/renderer/object_tools_module.h"

#include <string>

#include "base/logging.h"
#include "v8/include/v8.h"

namespace extensions {

namespace {

const char* kCreateObjectCode =
    "(function(object) {"
    "  var newobject = Object.create(object);"
    "  Object.getOwnPropertyNames(object).forEach(function(name) {"
    "    if (object[name] instanceof Function) {"
    "      newobject[name] = object[name];"
    "    }"
    "  });"
    "  newobject['origin_prototype'] = {};"
    "  Object.getOwnPropertyNames(object.prototype).forEach(function(name) {"
    "    if (object.prototype[name] instanceof Function) {"
    "      newobject['origin_prototype'][name] = object.prototype[name];"
    "    }"
    "  });"
    "  return function() {"
    "    return newobject;"
    "  };"
    "}(Object));";

v8::Handle<v8::Value> RunString(const std::string& code) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Handle<v8::String> v8_code(
      v8::String::NewFromUtf8(isolate, code.c_str()));

  v8::TryCatch try_catch(isolate);
  try_catch.SetVerbose(true);

  auto context = isolate->GetCurrentContext();
  auto script = v8::Script::Compile(context, v8_code);
  if (script.IsEmpty() || try_catch.HasCaught()) {
    v8::String::Utf8Value exception(isolate, try_catch.Exception());
    LOG(ERROR) << "Error occurred(script compile):" << *exception;
    return handle_scope.Escape(
        v8::Local<v8::Primitive>(v8::Undefined(isolate)));
  }

  auto result = script.ToLocalChecked()->Run(context);
  if (result.IsEmpty() || try_catch.HasCaught()) {
    v8::String::Utf8Value exception(isolate, try_catch.Exception());
    LOG(ERROR) << "Error occurred(script run):" << *exception;
    return handle_scope.Escape(
        v8::Local<v8::Primitive>(v8::Undefined(isolate)));
  }
  return handle_scope.Escape(result.ToLocalChecked());
}

}  //  namespace


ObjectToolsModule::ObjectToolsModule() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);

  v8::Handle<v8::Value> result = RunString(kCreateObjectCode);
  if (!result->IsFunction()) {
    LOG(ERROR) << "Couldn't load Object Create function";
    return;
  }
  v8::Handle<v8::Function> create_function =
      v8::Handle<v8::Function>::Cast(result);

  create_function_.Reset(isolate, create_function);
}

ObjectToolsModule::~ObjectToolsModule() {
  create_function_.Reset();
}

v8::Handle<v8::Object> ObjectToolsModule::NewInstance() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  if (create_function_.IsEmpty()) {
    return v8::Object::New(isolate);
  }
  v8::Handle<v8::Function> function =
      v8::Local<v8::Function>::New(isolate, create_function_);

  v8::Handle<v8::Context> context = v8::Context::New(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Handle<v8::Value> ret = function->Call(context->Global(), 0, NULL);
  if (try_catch.HasCaught()) {
    LOG(ERROR) << "Exception when running create function: ";
    return v8::Object::New(isolate);
  }
  return v8::Handle<v8::Object>::Cast(ret);
}

}  // namespace extensions

