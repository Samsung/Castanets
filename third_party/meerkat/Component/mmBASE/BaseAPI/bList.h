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

#ifndef __INCLUDE_COMMON_LIST_H__
#define __INCLUDE_COMMON_LIST_H__

#include <stdio.h>

namespace mmBase {

typedef void* HCListTemple;
template <typename TYPE>

class CbList {
 protected:
  struct CNode {
    CNode* pNext;
    CNode* pPrev;
    TYPE* data;
  };

 public:
  // Construction
  CbList() {
    m_pNodeHead = NULL;
    m_pNodeTail = NULL;
    m_pEnumNodeCurrent = NULL;
    m_nCount = 0;
  }

  virtual ~CbList() {}

  // add before head or after tail
  void AddHead(TYPE* newElement) {
    if (m_pNodeHead == NULL) {
      m_pNodeHead = new CNode;
      m_pNodeHead->pPrev = NULL;
      m_pNodeHead->pNext = NULL;
      m_pNodeHead->data = newElement;
      m_pNodeTail = m_pNodeHead;
    } else {
      CNode* pTempNode = new CNode;
      pTempNode->pPrev = NULL;
      pTempNode->pNext = m_pNodeHead;
      pTempNode->data = newElement;

      m_pNodeHead->pPrev = pTempNode;
      m_pNodeHead = pTempNode;
    }
    m_nCount++;
  }

  TYPE* GetHead(void) {
    if (m_pNodeHead == NULL) {
      printf("ASSERT!! No head exist\n");
      return NULL;
    } else
      return m_pNodeHead->data;
  }

  void AddTail(TYPE* newElement) {
    if (m_pNodeHead == NULL) {
      m_pNodeHead = new CNode;
      m_pNodeHead->pPrev = NULL;
      m_pNodeHead->pNext = NULL;
      m_pNodeHead->data = newElement;
      m_pNodeTail = m_pNodeHead;
    } else {
      CNode* pTempNode;
      pTempNode = new CNode;
      pTempNode->pPrev = m_pNodeTail;
      pTempNode->pNext = NULL;
      pTempNode->data = newElement;
      m_pNodeTail->pNext = pTempNode;
      m_pNodeTail = pTempNode;
    }
    m_nCount++;
  }

  TYPE* GetTail(void) {
    if (m_pNodeTail == NULL) {
      printf("ASSERT!! No Tail exist\n");
      return NULL;
    } else
      return m_pNodeTail->data;
  }

  void AddAt(int pos, TYPE* newElement) {
    if (pos > m_nCount) {
      printf("Assersion Failed!! total count:%d vs try to set %d\n", m_nCount,
             pos);
      return;
    } else if (pos == m_nCount) {
      AddTail(newElement);
    } else if (pos == 0) {
      AddHead(newElement);
    } else {
      if (m_pNodeHead == NULL) {
        if (pos != 0) {
          printf(
              "Assersion Failed!! Only AddAt(0) is possible when Head is NULL "
              "--%d\n",
              pos);
          return;
        }
        m_pNodeHead = new CNode;
        m_pNodeHead->pPrev = NULL;
        m_pNodeHead->pNext = NULL;
        m_pNodeHead->data = newElement;
        m_pNodeTail = m_pNodeHead;
      } else {
        CNode* pTravNode = m_pNodeHead;
        for (int i = 0; i < pos; i++) {
          pTravNode = pTravNode->pNext;
        }
        CNode* pTempNode = new CNode;
        pTempNode->pPrev = m_pNodeTail;
        pTempNode->pNext = NULL;
        pTempNode->data = newElement;
        pTempNode->pPrev = pTravNode;
        pTempNode->pNext =
            pTravNode->pNext;  // pTravNode -> pNext == NULL skkim
        pTravNode->pNext->pPrev = pTempNode;  // kill skkim
        pTravNode->pNext = pTempNode;
        m_nCount++;
      }
    }
  }

  TYPE* GetAt(int pos) {
    if (pos >= m_nCount) {
      printf("Assersion Failed!! total count:%d vs try to get %d\n", m_nCount,
             pos);
      return NULL;
    }
    CNode* pTemp = m_pNodeHead;
    for (int i = 0; i < pos; i++) {
      pTemp = pTemp->pNext;
    }
    return pTemp->data;
  }

  int DelAt(int pos) {
    if (pos >= m_nCount) {
      printf("Assersion Failed!! total count:%d vs try to del %d\n", m_nCount,
             pos);
      return -1;
    }
    CNode* pTemp = m_pNodeHead;
    for (int i = 0; i < pos; i++) {
      pTemp = pTemp->pNext;
    }

    if (pTemp == m_pNodeHead)  // head를 지우는 경우
    {
      if (pTemp->pNext == NULL)  //총 1개에서 1개를 지우는 경우
      {
        m_pNodeHead->pPrev = NULL;
        delete m_pNodeHead->data;
        delete m_pNodeHead;
        m_pNodeHead = m_pNodeTail = NULL;
      } else {
        m_pNodeHead = pTemp->pNext;
        m_pNodeHead->pPrev = NULL;
        delete pTemp->data;
        delete pTemp;
      }
    } else if (pTemp == m_pNodeTail) {
      if (pTemp->pPrev != NULL) {
        m_pNodeTail = pTemp->pPrev;
        pTemp->pPrev->pNext = NULL;
        delete pTemp->data;
        delete pTemp;
      }
    } else {
      pTemp->pPrev->pNext = pTemp->pNext;
      pTemp->pNext->pPrev = pTemp->pPrev;
      delete pTemp->data;
      delete pTemp;
    }

    return --m_nCount;
  }

  void RemoveAll() {
    CNode* pTemp;
    for (int i = 0; i < m_nCount; i++) {
      pTemp = m_pNodeTail;
      if (pTemp->pPrev == NULL) {
        delete pTemp->data;
        delete pTemp;
        break;
      }
      m_pNodeTail = pTemp->pPrev;
      delete pTemp->data;
      delete pTemp;
    }
    m_nCount = 0;
    m_pNodeHead = m_pNodeTail = NULL;
  }

  TYPE* FindFirstNode() {
    m_pEnumNodeCurrent = m_pNodeHead;
    if (m_pNodeHead == NULL) {
      printf("ASSERT!! No head exist\n");
      return NULL;
    } else {
      return m_pNodeHead->data;
    }
  }

  HCListTemple FindFirstNode(TYPE** ppdata) {
    CNode* pCurrentPos;

    *ppdata = NULL;

    pCurrentPos = m_pNodeHead;
    if (pCurrentPos == NULL) {
      printf("ASSERT!! No head exist\n");
      *ppdata = NULL;
      // return NULL;
    } else {
      *ppdata = m_pNodeHead->data;
    }

    return (HCListTemple)pCurrentPos;
  }

  TYPE* FindNextNode() {
    if (m_pEnumNodeCurrent == NULL) {
      // printf("ASSERT!! No More Node Exist\n");
      return NULL;
    } else {
      m_pEnumNodeCurrent = m_pEnumNodeCurrent->pNext;
      if (!m_pEnumNodeCurrent) {
        // printf("ASSERT!! No More Node Exist\n");
        return NULL;
      } else {
        return m_pEnumNodeCurrent->data;
      }
    }
  }

  HCListTemple FindNextNode(HCListTemple hNode, TYPE** ppdata) {
    CNode* pCurrentPos;

    pCurrentPos = (CNode*)hNode;

    *ppdata = NULL;

    if (pCurrentPos == NULL) {
      // printf("ASSERT!! No More Node Exist\n");
      return NULL;
    } else {
      pCurrentPos = pCurrentPos->pNext;
      if (!pCurrentPos) {
        // printf("ASSERT!! No More Node Exist\n");
        return NULL;
      } else {
        *ppdata = pCurrentPos->data;
      }
    }

    return (HCListTemple)pCurrentPos;
  }

  void FindCloseNode() { m_pEnumNodeCurrent = NULL; }

  int GetCount() { return m_nCount; }

  // Implementation
 protected:
  CNode* m_pNodeHead;
  CNode* m_pNodeTail;

  CNode* m_pEnumNodeCurrent;

  int m_nCount;
};
}

#endif
