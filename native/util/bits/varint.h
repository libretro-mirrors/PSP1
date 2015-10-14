#ifndef _UTIL_BITS_VARINT
#define _UTIL_BITS_VARINT

#include "base/basictypes.h"

namespace varint {

void Encode32(uint32_t value, char **dest);

}  // namespace varint

#endif  // _UTIL_BITS_VARINT
