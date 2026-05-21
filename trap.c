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
    if (current_process != 0) {
        current_process->user_context = *uctxt;
    }

    clock_ticks++;
    TracePrintf(1, "TRAP_CLOCK pc=%p sp=%p\n", uctxt->pc, uctxt->sp);

    if (init_process != 0 && init_process->state == PROC_BLOCKED &&
        init_process->delay_until <= clock_ticks) {
        init_process->state = PROC_READY;
    }

    if (current_process == idle_process) {
        if (init_process != 0 && init_process->state == PROC_READY) {
            ProcessSwitch(init_process);
        }
    } else if (current_process == init_process && init_process->state == PROC_RUNNING) {
        ProcessSwitch(idle_process);
    }

    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
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
