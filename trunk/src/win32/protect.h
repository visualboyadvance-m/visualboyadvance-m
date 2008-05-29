#ifndef ___PROTECT_H_
#define ___PROTECT_H_

#if defined(__cplusplus)
extern "C" {
#endif

//Returns 0 on success, 1 on failure, and <0 when an error occured
//Note, can only be called once per execution
int ExecutableValid(const char *executable_filename);
char *unprotect_buffer(unsigned char *buffer, size_t buffer_len);

#define SET_FN_PTR(func, num)             \
 static __inline void *get_##func(void) { \
  int  i, j = num / 4;                    \
  long ptr = (long)func + num;            \
  for (i = 0;  i < 2;  i++) { ptr -= j; } \
  return (void *)(ptr - (j * 2));         \
 }

#define GET_FN_PTR(func) get_##func()

#if defined(__cplusplus)
}
#endif

#endif
