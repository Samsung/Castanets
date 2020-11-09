/*
 * Copyright 2018 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.1 (the License);
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://floralicense.org/license/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "bINIParser.h"

#include <algorithm>
#include <fstream>

#ifdef WIN32
#include <ctype.h>
#endif

using namespace std;

namespace {

inline void ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
    return !isspace(ch);
  }));
}

inline void rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
    return !isspace(ch);
  }).base(), s.end());
}

}  // namespace

namespace mmBase {

CbINIParser::CbINIParser() : parsed_(false) {
}

CbINIParser::~CbINIParser() {
}

int CbINIParser::Parse(const std::string& file_path) {
  if (parsed_)
    return 0;

  int lineno = 0;
  std::ifstream file(file_path.c_str());

  if (!file.good())
    return -1;

  std::string line, section;
  while (std::getline(file, line)) {
    lineno++;

    ltrim(line);
    rtrim(line);

    if (line.empty())
      continue;

    if (line.front() == '#' || line.back() == ';') {
      // Comment line
    } else if (line.front() == '[') {
      if (line.back() != ']')
        return lineno;

      section.clear();
      section = line.substr(1, line.length() - 2);
      ltrim(section);
      rtrim(section);

      if (section.empty())
        return lineno;
    } else {
      auto delim = std::find_if(line.begin(), line.end(), [](int ch) {
          return ch == ':' || ch == '=';
      });

      if (delim == line.end())
        return lineno;

      std::string key(line.begin(), delim);
      rtrim(key);

      std::string value(delim + 1, line.end());
      ltrim(value);

      if (section.empty() || key.empty() || value.empty())
        return lineno;

      std::pair<std::string, std::string> key_pair(section, key);
      if (values_.find(key_pair) != values_.end())
        return lineno;

      values_[key_pair] = value;
    }
  }

  parsed_ = true;
  return 0;
}

std::string CbINIParser::GetAsString(const std::string& section,
                                     const std::string& key,
                                     const std::string& default_value) const {
  INIValueMap::const_iterator iter =
      values_.find(std::make_pair(section, key));

  return iter != values_.end() ? iter->second : default_value;
}

int CbINIParser::GetAsInteger(const std::string& section,
                              const std::string& key,
                              int default_value) const {
  std::string str_value = GetAsString(section, key, std::string());

  return !str_value.empty() ? stoi(str_value) : default_value;
}

double CbINIParser::GetAsDouble(const std::string& section,
                                const std::string& key,
                                double default_value) const {
  std::string str_value = GetAsString(section, key, std::string());

  return !str_value.empty() ? stod(str_value) : default_value;
}

bool CbINIParser::GetAsBoolean(const std::string& section,
                               const std::string& key,
                               bool default_value) const {
  std::string str_value = GetAsString(section, key, std::string());

  if (!str_value.empty()) {
    std::transform(
        str_value.begin(), str_value.end(), str_value.begin(), ::tolower);

    if (str_value == "true" || str_value == "on")
      return true;
    else if (str_value == "false" || str_value == "off")
      return false;
  }

  return default_value;
}

}  // namespace mmBase
