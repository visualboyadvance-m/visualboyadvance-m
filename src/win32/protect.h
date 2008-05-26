#ifndef ___PROTECT_H_
#define ___PROTECT_H_

#if defined(__cplusplus)
extern "C" {
#endif

//Returns 0 on success, 1 on failure, and <0 when an error occured
//Note, can only be called once per execution
int ExecutableValid(const char *executable_filename);
void unprotect_buf(char *buffer, size_t buffer_len)

#if defined(__cplusplus)
}
#endif

#endif
