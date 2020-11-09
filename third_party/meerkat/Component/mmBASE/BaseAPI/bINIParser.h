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

#ifndef __INCLUDE_COMMON_INIPARSER_H__
#define __INCLUDE_COMMON_INIPARSER_H__

#include <map>
#include <string>

namespace mmBase {

class CbINIParser {
 public:
  CbINIParser();
  virtual ~CbINIParser();

  virtual int Parse(const std::string& file_path);

  std::string GetAsString(const std::string& section,
                          const std::string& key,
                          const std::string& default_value) const;

  int GetAsInteger(const std::string& section,
                   const std::string& key,
                   int default_value) const;

  double GetAsDouble(const std::string& section,
                     const std::string& key,
                     double default_value) const;

  bool GetAsBoolean(const std::string& section,
                    const std::string& key,
                    bool default_value) const;

 private:
  typedef std::map<std::pair<std::string, std::string>, std::string>
      INIValueMap;
  INIValueMap values_;
  bool parsed_;
};

}  // namespace mmBase

#endif  // __INCLUDE_COMMON_INIPARSER_H__
