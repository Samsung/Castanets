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

#ifndef XWALK_EXTENSIONS_COMMON_CONSTANTS_H_
#define XWALK_EXTENSIONS_COMMON_CONSTANTS_H_

namespace extensions {

extern const char kMethodGetExtensions[];
extern const char kMethodCreateInstance[];
extern const char kMethodDestroyInstance[];
extern const char kMethodSendSyncMessage[];
extern const char kMethodPostMessage[];
extern const char kMethodGetAPIScript[];
extern const char kMethodPostMessageToJS[];
extern const char kMethodLoadUserExtensions[];

}  // namespace extensions

#endif  // XWALK_EXTENSIONS_COMMON_CONSTANTS_H_
