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

var dispatchStorageEvent = function(key, oldValue, newValue) {
  var evt = document.createEvent("CustomEvent");
  evt.initCustomEvent("storage", true, true, null);
  evt.key = key;
  evt.oldValue = oldValue;
  evt.newValue = newValue;
  evt.storageArea = window.widget.preference;
  document.dispatchEvent(evt);
  for (var i=0; i < window.frames.length; i++) {
    window.frames[i].document.dispatchEvent(evt);
  }
};

var widget_info_ = requireNative('WidgetModule');
var preference_ = widget_info_['preference'];
preference_.__onChanged_WRT__ = dispatchStorageEvent;

function Widget() {
  Object.defineProperties(this, {
    "author": {
      value: widget_info_[
"author"],
      writable: false
    },
    "description": {
      value: widget_info_["description"],
      writable: false
    },
    "name": {
      value: widget_info_["name"],
      writable: false
    },
    "shortName": {
      value: widget_info_["shortName"],
      writable: false
    },
    "version": {
      value: widget_info_["version"],
      writable: false
    },
    "id": {
      value: widget_info_["id"],
      writable: false
    },
    "authorEmail": {
      value: widget_info_["authorEmail"],
      writable: false
    },
    "authorHref": {
      value: widget_info_["authorHref"],
      writable: false
    },
    "height": {
      get: function() {
        return window && window.innerHeight || 0;
      },
      configurable: false
    },
    "width": {
      get: function() {
        return window && window.innerWidth || 0;
      },
      configurable: false
    },
    "preferences": {
      value: preference_,
      writable: false
    }
  });
};

Widget.prototype.toString = function() {
    return "[object Widget]";
};

window.widget = new Widget();
exports = Widget;
