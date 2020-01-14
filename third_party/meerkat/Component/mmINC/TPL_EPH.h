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

#ifndef __INCLUDE_EXCEPTION_HANDLER_H__
#define __INCLUDE_EXCEPTION_HANDLER_H__

#include <exception>
#include <iostream>
#include <string>

namespace mmEXH {
class e_InvaildAccess : public std::exception {
 public:
  void what() { std::cout << "Access Exception" << std::endl; }
};

class e_InvalidAlloc : public std::exception {
 public:
  void what() { std::cout << "Allocation Exception" << std::endl; }
};
}  // namespace mmEXH

#endif
