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

#include "third_party/wrt/common/string_utils.h"

// #include <uuid/uuid.h>
// #include <glib.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "base/guid.h"

namespace common {
namespace utils {

std::string GenerateUUID() {
  return base::GenerateGUID();
}

bool StartsWith(const std::string& str, const std::string& sub) {
  if (sub.size() > str.size())
    return false;
  return std::equal(sub.begin(), sub.end(), str.begin());
}

bool EndsWith(const std::string& str, const std::string& sub) {
  if (sub.size() > str.size())
    return false;
  return std::equal(sub.rbegin(), sub.rend(), str.rbegin());
}

std::string ReplaceAll(const std::string& replace,
                       const std::string& from,
                       const std::string& to) {
  std::string str = replace;
  size_t pos = str.find(from);
  while (pos != std::string::npos) {
    str.replace(pos, from.length(), to);
    pos = str.find(from, pos + to.length());
  }
  return str;
}

std::string GetCurrentMilliSeconds() {
  std::ostringstream ss;
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  ss << (spec.tv_sec % 10000) << "." << std::setw(3) << std::setfill('0')
     << (round(spec.tv_nsec / 1.0e6));
  return ss.str();
}

bool SplitString(const std::string& str,
                 std::string* part_1,
                 std::string* part_2,
                 const char delim) {
  if (part_1 == nullptr || part_2 == nullptr)
    return false;

  size_t pos = str.find(delim);
  if (pos == std::string::npos)
    return false;

  *part_1 = str.substr(0, pos);
  *part_2 = str.substr(pos + 1);
  return true;
}
/*
std::string UrlDecode(const std::string& url) {
  std::unique_ptr<char, decltype(std::free)*> decoded_str{
      g_uri_unescape_string(url.c_str(), NULL), std::free};
  return decoded_str.get() != nullptr ? std::string(decoded_str.get()) : url;
}

std::string UrlEncode(const std::string& url) {
  std::unique_ptr<char, decltype(std::free)*> encoded_str{
      g_uri_escape_string(url.c_str(), NULL, TRUE), std::free};
  return encoded_str.get() != nullptr ? std::string(encoded_str.get()) : url;
}

std::string Base64Encode(const unsigned char* data, size_t len) {
  gchar* encoded = g_base64_encode(data, len);
  std::unique_ptr<gchar, decltype(g_free)*> encoded_ptr{encoded, g_free};
  return std::string(encoded);
}
*/
}  // namespace utils
}  // namespace common
