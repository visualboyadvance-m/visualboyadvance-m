#ifndef CSTDINT_H
#define CSTDINT_H

#if defined(__has_include)
#   if __has_include(<cstdint>)
#       include <cstdint>
// necessary on Mac OS X Lion 10.7 or any clang <= 3.0
#   elif __has_include(<tr1/cstdint>)
#       include <tr1/cstdint>
#   else
//      throw error
#       include <cstdint>
#   endif
#else
#   include <cstdint>
#endif

#endif // CSTDINT_H
