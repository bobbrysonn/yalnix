#ifndef YALNIX_SYSCALLS_H
#define YALNIX_SYSCALLS_H

#include <ykernel.h>

void SyscallDispatch(UserContext *uctxt);
int SysGetPid(void);
int SysBrk(void *addr);
int SysDelay(int clock_ticks);

#endif
