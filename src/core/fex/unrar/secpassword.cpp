#include "rar.hpp"

// When we call memset in end of function to clean local variables
// for security reason, compiler optimizer can remove such call.
// So we use our own function for this purpose.
void cleandata(void *data,size_t size)
{
#if defined(_WIN_ALL) && defined(_MSC_VER)
  SecureZeroMemory(data,size);
#else
  // 'volatile' is required. Otherwise optimizers can remove this function
  // if cleaning local variables, which are not used after that.
  volatile byte *d = (volatile byte *)data;
  for (size_t i=0;i<size;i++)
    d[i]=0;
#endif
}

