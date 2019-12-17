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

#ifndef XWALK_COMMON_STRING_UTILS_H_
#define XWALK_COMMON_STRING_UTILS_H_

#include <string>
#include <vector>

namespace common {
namespace utils {

std::string GenerateUUID();
bool StartsWith(const std::string& str, const std::string& sub);
bool EndsWith(const std::string& str, const std::string& sub);
std::string ReplaceAll(const std::string& replace,
                       const std::string& from,
                       const std::string& to);
std::string GetCurrentMilliSeconds();
bool SplitString(const std::string& str,
                 std::string* part_1,
                 std::string* part_2,
                 const char delim);
/*
std::string UrlEncode(const std::string& url);
std::string UrlDecode(const std::string& url);
std::string Base64Encode(const unsigned char* data, size_t len);
*/

}  // namespace utils
}  // namespace common

#endif  // XWALK_COMMON_STRING_UTILS_H_
