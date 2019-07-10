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

#ifndef __INCLUDE_TUN_DRIVER_H__
#define __INCLUDE_TUN_DRIVER_H__

class CTunDrv {
 public:
  CTunDrv();
  virtual ~CTunDrv();

 public:  // public method
  int Open(char* dev, char* pb_addr);
  int Close(int fd);
  int Read(int fd, char* buf, int len);
  int Write(int fd, char* buf, int len);

 private:  // private method
};

#endif  //__INCLUDE_TUN_DRIVER_H__
