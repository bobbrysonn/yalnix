#ifndef YALNIX_TTY_H
#define YALNIX_TTY_H

#include <ykernel.h>
#include "process.h"

void TtyInit(void);
int SysTtyRead(UserContext *uctxt, int tty_id, void *buf, int len);
int SysTtyWrite(UserContext *uctxt, int tty_id, void *buf, int len);
void TrapTtyReceive(UserContext *uctxt);
void TrapTtyTransmit(UserContext *uctxt);

#endif
