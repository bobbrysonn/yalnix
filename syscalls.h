#ifndef YALNIX_SYSCALLS_H
#define YALNIX_SYSCALLS_H

#include <ykernel.h>

void SyscallDispatch(UserContext *uctxt);
int SysFork(void);
int SysExec(char *filename, char **argvec);
void SysExit(int status);
int SysWait(UserContext *uctxt, int *status_ptr);
int SysGetPid(void);
int SysBrk(void *addr);
int SysDelay(UserContext *uctxt, int clock_ticks);

#endif
