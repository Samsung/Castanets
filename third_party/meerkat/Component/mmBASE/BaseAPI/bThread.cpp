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

#include "bThread.h"

using namespace mmBase;
/**
 * @brief         생성자
 * @remarks       생성자
 */
CbThread::CbThread() {
  m_bThreading = false;
  m_bRun = false;
  memset(m_szThreadName, 0, 64);
  strcpy(m_szThreadName, "Annonymous");
}

CbThread::CbThread(const CHAR* pszName) {
  m_bThreading = false;
  m_bRun = false;
  memset(m_szThreadName, 0, 64);
  strcpy(m_szThreadName, pszName);
}

/**
 * @brief         소멸자.
 * @remarks       소멸자.
 */
CbThread::~CbThread() {}

/**
 * @brief         entryPoint.
 * @remarks       thread entry point (static thread method에서 member access를
 * 위한 this pointer 전달)
 * @param         pthis    this pointer
 */
void* CbThread::entryPoint(void* pthis) {
  CbThread* pThread = (CbThread*)pthis;
  pThread->runMainLoop(pThread->argument());
  return NULL;
}

/**
 * @brief         runMainLoop.
 * @remarks       runMainLoop
 * @param         args    thread argument
 */
void CbThread::runMainLoop(void* args) {
  Begin();
  MainLoop(args);
  Endup();
}

/**
 * @brief         Begin
 * @remarks       Begin
 */
void CbThread::Begin(void) {
  DPRINT(COMM, DEBUG_INFO, "Start Thread [%s] Loop\n", m_szThreadName);
}

/**
 * @brief         MainLoop
 * @remarks       MainLoop
 * @param         args    thread argument
 */
void CbThread::MainLoop(void* args) {}

/**
 * @brief         Endup
 * @remarks       Endup
 */
void CbThread::Endup(void) {
  DPRINT(COMM, DEBUG_INFO, "Finish Thread [%s] Loop\n", m_szThreadName);
}

/**
 * @brief         StartMainLoop
 * @remarks       StartMainLoop
 * @param         args    thread argument
 */
int CbThread::StartMainLoop(void* args) {
  m_bRun = true;
  argument(args);
  m_hMainThread =
      __OSAL_Create_Thread((void*)CbThread::entryPoint, (void*)this);
  m_bThreading = true;
  return 0;
}

/**
 * @brief         StopMainLoop
 * @remarks       StopMainLoop
 */
void CbThread::StopMainLoop(void) {
  if (m_bThreading) {
    m_bRun = false;
    __OSAL_Join_Thread(m_hMainThread, 3000);
    m_bThreading = false;
  }
}
