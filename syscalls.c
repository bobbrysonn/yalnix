#include "process.h"
#include "syscalls.h"

void
SyscallDispatch(UserContext *uctxt)
{
    switch (uctxt->code) {
    case YALNIX_GETPID:
        uctxt->regs[0] = SysGetPid();
        break;
    case YALNIX_BRK:
        uctxt->regs[0] = SysBrk((void *)uctxt->regs[0]);
        break;
    case YALNIX_DELAY:
        uctxt->regs[0] = SysDelay((int)uctxt->regs[0]);
        break;
    default:
        TracePrintf(1, "unsupported syscall code=0x%x\n", uctxt->code);
        uctxt->regs[0] = ERROR;
        break;
    }
}

int
SysGetPid(void)
{
    PCB *proc = ProcessCurrent();

    if (proc == 0) {
        return ERROR;
    }

    return proc->pid;
}

int
SysBrk(void *addr)
{
    TracePrintf(1, "Brk stub addr=%p\n", addr);
    return ERROR;
}

int
SysDelay(int clock_ticks)
{
    if (clock_ticks < 0) {
        return ERROR;
    }
    if (clock_ticks == 0) {
        return SUCCESS;
    }

    TracePrintf(1, "Delay stub ticks=%d\n", clock_ticks);
    return ERROR;
}
