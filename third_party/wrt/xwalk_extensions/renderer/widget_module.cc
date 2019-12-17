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

#include "third_party/wrt/xwalk_extensions/renderer/widget_module.h"

#include <algorithm>
#include <vector>

#include "base/logging.h"
#include "v8/include/v8.h"
// #include "third_party/wrt/common/app_db.h"
// #include "third_party/wrt/src/common/application_data.h"

namespace extensions {

namespace {
const char* kOnchangedEventHandler = "__onChanged_WRT__";
const char* kKeyKey = "key";
const char* kGetItemKey = "getItem";
const char* kSetItemKey = "setItem";
const char* kRemoveItemKey = "removeItem";
const char* kLengthKey = "length";
const char* kClearKey = "clear";
const int kKeyLengthLimit = 80;
const int kValueLengthLimit = 8192;

std::vector<const char*> kExcludeList = {
  kOnchangedEventHandler,
  kKeyKey,
  kGetItemKey,
  kSetItemKey,
  kRemoveItemKey,
  kLengthKey,
  kClearKey};

void DispatchEvent(const v8::Local<v8::Object>& This,
                   v8::Local<v8::Value> key,
                   v8::Local<v8::Value> oldvalue,
                   v8::Local<v8::Value> newvalue) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  v8::Handle<v8::Value> function =
      This->Get(v8::String::NewFromUtf8(isolate, kOnchangedEventHandler));

  if (function.IsEmpty() || !function->IsFunction()) {
    LOG(INFO) << "onChanged function not set";
    return;
  }

  v8::Handle<v8::Context> context = v8::Context::New(isolate);

  const int argc = 3;
  v8::Handle<v8::Value> argv[argc] = {
    key,
    oldvalue,
    newvalue
  };

  v8::TryCatch try_catch(isolate);
  v8::Handle<v8::Function>::Cast(function)->Call(
      context->Global(), argc, argv);
  if (try_catch.HasCaught())
    LOG(INFO) << "Exception when running onChanged callback";
}

v8::Handle<v8::Object> MakeException(int code,
                                   std::string name,
                                   std::string message) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);
  v8::Local<v8::Object> error = v8::Object::New(isolate);

  error->Set(
      v8::String::NewFromUtf8(isolate, "code"),
      v8::Number::New(isolate, code));
  error->Set(
      v8::String::NewFromUtf8(isolate, "name"),
      v8::String::NewFromUtf8(isolate, name.c_str()));
  error->Set(
      v8::String::NewFromUtf8(isolate, "message"),
      v8::String::NewFromUtf8(isolate, message.c_str()));

  return handle_scope.Escape(error);
}

void KeyFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  int idx = info[0].As<v8::Int32>()->Value();
  auto widget = WidgetPreferenceDB::GetInstance();
  std::string keyname;
  if (widget->Key(idx, &keyname)) {
    info.GetReturnValue().Set(
        v8::String::NewFromUtf8(isolate, keyname.c_str()));
  } else {
    info.GetReturnValue().SetNull();
  }
}

void GetItemFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  std::string key = *v8::String::Utf8Value(isolate, info[0].As<v8::String>());
  auto widget = WidgetPreferenceDB::GetInstance();
  std::string valuestr;
  if (widget->GetItem(key, &valuestr)) {
    info.GetReturnValue().Set(
        v8::String::NewFromUtf8(isolate, valuestr.c_str()));
  } else {
    info.GetReturnValue().SetNull();
  }
}

void SetItemFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  std::string key = *v8::String::Utf8Value(isolate, info[0].As<v8::String>());
  std::string value = *v8::String::Utf8Value(
      isolate, info[1].As<v8::String>());
  auto widget = WidgetPreferenceDB::GetInstance();

  v8::Local<v8::Value> oldvalue = v8::Null(isolate);
  std::string oldvaluestr;
  if (widget->GetItem(key, &oldvaluestr)) {
    oldvalue = v8::String::NewFromUtf8(isolate, oldvaluestr.c_str());
  }

  if (widget->SetItem(key, value)) {
    DispatchEvent(info.This(),
                  info[0],
                  oldvalue,
                  info[1]);
  } else {
    info.GetReturnValue().Set(isolate->ThrowException(MakeException(
        7, "NoModificationAllowedError", "Read only data")));
  }
}

void RemoveItemFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  std::string key = *v8::String::Utf8Value(isolate, info[0].As<v8::String>());
  auto widget = WidgetPreferenceDB::GetInstance();

  if (!widget->HasItem(key)) {
    return;
  }

  v8::Local<v8::Value> oldvalue = v8::Null(isolate);
  std::string oldvaluestr;
  if (widget->GetItem(key, &oldvaluestr)) {
    oldvalue = v8::String::NewFromUtf8(isolate, oldvaluestr.c_str());
  }

  if (widget->RemoveItem(key)) {
    DispatchEvent(info.This(),
                  info[0],
                  oldvalue,
                  v8::Null(isolate));
  } else {
    info.GetReturnValue().Set(isolate->ThrowException(MakeException(
        7, "NoModificationAllowedError", "Read only data")));
  }
}

void ClearFunction(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  auto widget = WidgetPreferenceDB::GetInstance();
  widget->Clear();
  DispatchEvent(info.This(),
                v8::Null(isolate),
                v8::Null(isolate),
                v8::Null(isolate));
}

}  // namespace

WidgetModule::WidgetModule() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::ObjectTemplate>
      preference_object_template = v8::ObjectTemplate::New(isolate);

  auto getter = [](
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    auto widget = WidgetPreferenceDB::GetInstance();
    std::string key = *v8::String::Utf8Value(isolate, property);

    if (key == kLengthKey) {
      info.GetReturnValue().Set(widget->Length());
      return;
    }

    if (std::find(kExcludeList.begin(), kExcludeList.end(), key)
        != kExcludeList.end()) {
      return;
    }

    if (!widget->HasItem(key)) {
      return;
    }

    std::string value;
    if (widget->GetItem(key, &value)) {
      info.GetReturnValue().Set(
          v8::String::NewFromUtf8(isolate, value.c_str()));
    } else {
      info.GetReturnValue().SetNull();
    }
  };

  auto setter = [](
      v8::Local<v8::Name> property,
      v8::Local<v8::Value> value,
      const v8::PropertyCallbackInfo<v8::Value>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    auto widget = WidgetPreferenceDB::GetInstance();
    std::string key = *v8::String::Utf8Value(isolate, property);
    std::string nvalue = *v8::String::Utf8Value(isolate, value->ToString());

    if (std::find(kExcludeList.begin(), kExcludeList.end(), key)
        != kExcludeList.end()) {
      return;
    }

    v8::Local<v8::Value> oldvalue = v8::Null(isolate);
    std::string oldvaluestr;
    if (widget->GetItem(key, &oldvaluestr)) {
      oldvalue = v8::String::NewFromUtf8(isolate, oldvaluestr.c_str());
    }
    if (widget->SetItem(key, nvalue)) {
      info.GetReturnValue().Set(value);
      DispatchEvent(info.This(),
                    property,
                    oldvalue,
                    value);
    }
  };

  auto deleter = [](
      v8::Local<v8::Name> property,
      const v8::PropertyCallbackInfo<v8::Boolean>& info) {
    v8::Isolate* isolate = info.GetIsolate();
    auto widget = WidgetPreferenceDB::GetInstance();
    std::string key = *v8::String::Utf8Value(isolate, property);
    if (!widget->HasItem(key)) {
      info.GetReturnValue().Set(false);
      return;
    }

    v8::Local<v8::Value> oldvalue = v8::Null(isolate);
    std::string oldvaluestr;
    if (widget->GetItem(key, &oldvaluestr)) {
      oldvalue = v8::String::NewFromUtf8(isolate, oldvaluestr.c_str());
    }

    if (widget->RemoveItem(key)) {
      info.GetReturnValue().Set(true);
      DispatchEvent(info.This(),
                    property,
                    oldvalue,
                    v8::Null(isolate));
    } else {
      info.GetReturnValue().Set(false);
    }
  };

  preference_object_template->SetHandler(v8::NamedPropertyHandlerConfiguration(
      getter, setter, nullptr, deleter, nullptr));

  preference_object_template->Set(
      v8::String::NewFromUtf8(isolate, kKeyKey),
      v8::FunctionTemplate::New(isolate, KeyFunction));

  preference_object_template->Set(
      v8::String::NewFromUtf8(isolate, kGetItemKey),
      v8::FunctionTemplate::New(isolate, GetItemFunction));

  preference_object_template->Set(
      v8::String::NewFromUtf8(isolate, kSetItemKey),
      v8::FunctionTemplate::New(isolate, SetItemFunction));

  preference_object_template->Set(
      v8::String::NewFromUtf8(isolate, kRemoveItemKey),
      v8::FunctionTemplate::New(isolate, RemoveItemFunction));

  preference_object_template->Set(
      v8::String::NewFromUtf8(isolate, kClearKey),
      v8::FunctionTemplate::New(isolate, ClearFunction));


  preference_object_template_.Reset(isolate, preference_object_template);
}

WidgetModule::~WidgetModule() {
}

v8::Handle<v8::Object> WidgetModule::NewInstance() {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::EscapableHandleScope handle_scope(isolate);

  v8::Local<v8::Object> widget = v8::Object::New(isolate);
  v8::Handle<v8::ObjectTemplate> object_template =
      v8::Local<v8::ObjectTemplate>::New(isolate, preference_object_template_);

  auto widgetdb = WidgetPreferenceDB::GetInstance();
  widgetdb->InitializeDB();

  widget->Set(
      v8::String::NewFromUtf8(isolate, "preference"),
      object_template->NewInstance());

  widget->Set(
      v8::String::NewFromUtf8(isolate, "author"),
      v8::String::NewFromUtf8(isolate, widgetdb->author().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "description"),
      v8::String::NewFromUtf8(isolate, widgetdb->description().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "name"),
      v8::String::NewFromUtf8(isolate, widgetdb->name().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "shortName"),
      v8::String::NewFromUtf8(isolate, widgetdb->shortName().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "version"),
      v8::String::NewFromUtf8(isolate, widgetdb->version().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "id"),
      v8::String::NewFromUtf8(isolate, widgetdb->id().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "authorEmail"),
      v8::String::NewFromUtf8(isolate, widgetdb->authorEmail().c_str()));
  widget->Set(
      v8::String::NewFromUtf8(isolate, "authorHref"),
      v8::String::NewFromUtf8(isolate, widgetdb->authorHref().c_str()));

  return handle_scope.Escape(widget);
}


namespace {
  const char* kDbInitedCheckKey = "__WRT_DB_INITED__";
  const char* kDBPublicSection = "public";
  const char* kDBPrivateSection = "private";
  const char* kReadOnlyPrefix = "_READONLY_KEY_";
}  // namespace


WidgetPreferenceDB* WidgetPreferenceDB::GetInstance() {
  static WidgetPreferenceDB instance;
  return &instance;
}

WidgetPreferenceDB::WidgetPreferenceDB()
    // : locale_manager_(nullptr)
{
}
WidgetPreferenceDB::~WidgetPreferenceDB() {
}
/*
void WidgetPreferenceDB::Initialize(common::LocaleManager* locale_manager) {
  locale_manager_ = locale_manager;
}
*/
void WidgetPreferenceDB::InitializeDB() {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  if (db->HasKey(kDBPrivateSection, kDbInitedCheckKey)) {
    return;
  }

  auto& app_data = wrt::ApplicationData::GetInstance();
  auto& preferences = app_data.widget_info().preferences();

  for (const auto& pref : preferences) {
    if (pref->Name().empty())
      continue;

    // check size limit
    std::string key = pref->Name();
    std::string value = pref->Value();
    if (key.length() > kKeyLengthLimit) {
      key.resize(kKeyLengthLimit);
    }

    if (db->HasKey(kDBPublicSection, key))
      continue;

    // check size limit
    if (value.length() > kValueLengthLimit) {
      value.resize(kValueLengthLimit);
    }

    db->Set(kDBPublicSection,
            key,
            value);
    if (pref->ReadOnly()) {
      db->Set(kDBPrivateSection,
              kReadOnlyPrefix + key, "true");
    }
  }
  db->Set(kDBPrivateSection, kDbInitedCheckKey, "true");
  */
}

int WidgetPreferenceDB::Length() {
  // common::AppDB* db = common::AppDB::GetInstance();
  std::list<std::string> list;
  // db->GetKeys(kDBPublicSection, &list);
  return list.size();
}

bool WidgetPreferenceDB::Key(int idx, std::string* key) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  std::list<std::string> list;
  db->GetKeys(kDBPublicSection, &list);

  auto it = list.begin();
  for ( ; it != list.end() && idx >= 0; ++it) {
    if (idx == 0) {
      *key = *it;
      return true;
    }
    idx--;
  }
  */
  return false;
}

bool WidgetPreferenceDB::GetItem(const std::string& key, std::string* value) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  if (!db->HasKey(kDBPublicSection, key))
    return false;
  *value = db->Get(kDBPublicSection, key);
  */
  return true;
}

bool WidgetPreferenceDB::SetItem(const std::string& key,
                                 const std::string& value) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  if (db->HasKey(kDBPrivateSection, kReadOnlyPrefix + key))
    return false;
  db->Set(kDBPublicSection, key, value);
  */
  return true;
}

bool WidgetPreferenceDB::RemoveItem(const std::string& key) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  if (!db->HasKey(kDBPublicSection, key))
    return false;
  if (db->HasKey(kDBPrivateSection, kReadOnlyPrefix + key))
    return false;
  db->Remove(kDBPublicSection, key);
  */
  return true;
}

bool WidgetPreferenceDB::HasItem(const std::string& key) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  return db->HasKey(kDBPublicSection, key);
  */
  return true;
}

void WidgetPreferenceDB::Clear() {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  std::list<std::string> list;
  db->GetKeys(kDBPublicSection, &list);
  auto it = list.begin();
  for ( ; it != list.end(); ++it) {
    if (db->HasKey(kDBPrivateSection, kReadOnlyPrefix + *it))
      continue;
    db->Remove(kDBPublicSection, *it);
  }
  */
}

void WidgetPreferenceDB::GetKeys(std::list<std::string>* keys) {
  /*
  common::AppDB* db = common::AppDB::GetInstance();
  db->GetKeys(kDBPublicSection, keys);
  */
}

std::string WidgetPreferenceDB::author() {
  /*
  auto& app_data = wrt::ApplicationData::GetInstance();
  return app_data.widget_info().author();
  */
  return std::string();
}

std::string WidgetPreferenceDB::description() {
  /*
  if (!locale_manager_)
    return std::string();
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return locale_manager_->GetLocalizedString(widget_info.description_set());
  */
  return std::string();
}

std::string WidgetPreferenceDB::name() {
  /*
  if (!locale_manager_)
    return std::string();
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return locale_manager_->GetLocalizedString(widget_info.name_set());
  */
  return std::string();
}

std::string WidgetPreferenceDB::shortName() {
  /*
  if (!locale_manager_)
    return std::string();
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return locale_manager_->GetLocalizedString(widget_info.short_name_set());
  */
  return std::string();
}

std::string WidgetPreferenceDB::version() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.version();
  */
  return std::string();
}

std::string WidgetPreferenceDB::id() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.id();
  */
  return std::string();
}

std::string WidgetPreferenceDB::authorEmail() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.author_email();
  */
  return std::string();
}

std::string WidgetPreferenceDB::authorHref() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.author_href();
  */
  return std::string();
}

unsigned int WidgetPreferenceDB::height() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.height();
  */
  return 0;
}

unsigned int WidgetPreferenceDB::width() {
  /*
  auto& widget_info = wrt::ApplicationData::GetInstance().widget_info();
  return widget_info.width();
  */
  return 0;
}

}  // namespace extensions
