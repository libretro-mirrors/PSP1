#include "util/bits/varint.h"

namespace varint {

void Encode32(uint32_t value, char **dest) {
  // Simple varint
  char *p = *dest;
  while (value > 127) {
    *p++ = (value & 127);
    value >>= 7;
  }
  *p++ = value | 0x80;
  *dest = p;
}

}  // namespace varint


