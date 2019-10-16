#ifndef CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PARAM_TRAITS_H_
#define CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PARAM_TRAITS_H_

#include "base/pickle.h"
#include "content/common/content_export.h"
#include "ipc/ipc_param_traits.h"
#include "media/base/ranges.h"

namespace IPC {

template <>
struct CONTENT_EXPORT ParamTraits<media::Ranges<base::TimeDelta>> {
  typedef media::Ranges<base::TimeDelta> param_type;
  static void Write(base::Pickle* pickle, const param_type& ptype);
  static bool Read(const base::Pickle* pickle,
                   base::PickleIterator* iter,
                   param_type* ptype);
  static void Log(const param_type& ptype, std::string* str);
};

}  // namespace IPC

#endif  // CONTENT_COMMON_MEDIA_CASTANETS_MEDIA_PARAM_TRAITS_H_
