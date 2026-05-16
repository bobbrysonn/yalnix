#include "kernel.h"
#include "process.h"
#include "syscalls.h"
#include "trap.h"

static TrapHandler trap_vector[TRAP_VECTOR_SIZE];

void
TrapInit(void)
{
    int i;

    for (i = 0; i < TRAP_VECTOR_SIZE; i++) {
        trap_vector[i] = TrapUnhandled;
    }

    trap_vector[TRAP_CLOCK] = TrapClock;
    trap_vector[TRAP_KERNEL] = TrapKernel;
    WriteRegister(REG_VECTOR_BASE, (unsigned int)trap_vector);
}

void
TrapClock(UserContext *uctxt)
{
    TracePrintf(1, "TRAP_CLOCK pc=%p sp=%p\n", uctxt->pc, uctxt->sp);

}

void
TrapKernel(UserContext *uctxt)
{
    if (current_process != 0) {
        current_process->user_context = *uctxt;
    }

    TracePrintf(1, "TRAP_KERNEL code=0x%x\n", uctxt->code);
    SyscallDispatch(uctxt);

    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
}

void
TrapUnhandled(UserContext *uctxt)
{
    TracePrintf(0, "unhandled trap vector=%d code=0x%x addr=%p pc=%p sp=%p\n",
                uctxt->vector, uctxt->code, uctxt->addr, uctxt->pc, uctxt->sp);
}
