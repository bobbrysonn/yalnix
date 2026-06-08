#ifndef YALNIX_IPC_H
#define YALNIX_IPC_H

#include <ykernel.h>

struct pcb;

void IpcInit(void);
void IpcProcessExit(struct pcb *proc);
int SysPipeInit(int *pipe_idp);
int SysPipeRead(UserContext *uctxt, int pipe_id, void *buf, int len);
int SysPipeWrite(int pipe_id, void *buf, int len);
int SysLockInit(int *lock_idp);
int SysAcquire(UserContext *uctxt, int lock_id);
int SysRelease(int lock_id);
int SysCvarInit(int *cvar_idp);
int SysCvarSignal(int cvar_id);
int SysCvarBroadcast(int cvar_id);
int SysCvarWait(UserContext *uctxt, int cvar_id, int lock_id);
int SysReclaim(int id);

#endif
