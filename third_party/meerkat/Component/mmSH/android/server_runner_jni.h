/*
 * Copyright 2019 Samsung Electronics Co., Ltd
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

#ifndef __INCLUDE_SERVER_RUNNER_JNI_H__
#define __INCLUDE_SERVER_RUNNER_JNI_H__

#include <vector>

std::string Java_getIdToken();

bool Java_verifyIdToken(const char* token);

bool Java_startCastanetsRenderer(std::vector<char*>& argv);

#endif  // __INCLUDE_SERVER_RUNNER_JNI_H__
