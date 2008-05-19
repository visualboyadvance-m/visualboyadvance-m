//#ifdef PROTECT_H
//#define PROTECT_H

#if defined(__cplusplus)
extern "C" {
#endif

//Returns 0 on success, 1 on failure, and <0 when an error occured
//Note, can only be called once per execution
int ExecutableValid(const char *executable_filename);

#if defined(__cplusplus)
}
#endif

//#endif
