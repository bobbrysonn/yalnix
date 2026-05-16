#ifndef YALNIX_TRAP_H
#define YALNIX_TRAP_H

#include <ykernel.h>

typedef void (*TrapHandler)(UserContext *);

void TrapInit(void);
void TrapClock(UserContext *uctxt);
void TrapKernel(UserContext *uctxt);
void TrapUnhandled(UserContext *uctxt);

#endif
