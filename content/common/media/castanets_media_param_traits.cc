// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/media/castanets_media_param_traits.h"

#include "base/strings/stringprintf.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

void ParamTraits<media::Ranges<base::TimeDelta>>::Write(
    base::Pickle* pickle,
    const media::Ranges<base::TimeDelta>& range) {
  WriteParam(pickle, static_cast<int>(range.size()));
  for (size_t i = 0; i < range.size(); ++i) {
    WriteParam(pickle, range.start(i));
    WriteParam(pickle, range.end(i));
  }
}

bool ParamTraits<media::Ranges<base::TimeDelta>>::Read(
    const base::Pickle* pickle,
    base::PickleIterator* iter,
    media::Ranges<base::TimeDelta>* range) {
  int size = 0;

  // ReadLength() checks for < 0 itself.
  if (!iter->ReadLength(&size))
    return false;
  for (int i = 0; i < size; i++) {
    base::TimeDelta start, end;
    if (!ReadParam(pickle, iter, &start) || !ReadParam(pickle, iter, &end))
      return false;
    range->Add(start, end);
  }
  return true;
}

void ParamTraits<media::Ranges<base::TimeDelta>>::Log(
    const media::Ranges<base::TimeDelta>& pickle,
    std::string* str) {
  str->append("TimeRanges:[");
  for (size_t i = 0u; i < pickle.size(); ++i) {
    str->append(base::StringPrintf("{%zu:{%lf,%lf}}, ", i,
                                   pickle.start(i).InSecondsF(),
                                   pickle.end(i).InSecondsF()));
  }
  str->append("]");
}

}  // namespace IPC
