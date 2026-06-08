#include "kernel.h"
#include "process.h"
#include "syscalls.h"
#include "trap.h"
#include "tty.h"

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
    trap_vector[TRAP_MEMORY] = TrapMemory;
    trap_vector[TRAP_ILLEGAL] = TrapAbort;
    trap_vector[TRAP_MATH] = TrapAbort;
    trap_vector[TRAP_TTY_RECEIVE] = TrapTtyReceive;
    trap_vector[TRAP_TTY_TRANSMIT] = TrapTtyTransmit;
    trap_vector[TRAP_DISK] = TrapDisk;
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

    ProcessSchedule();

    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
}

void
TrapKernel(UserContext *uctxt)
{
    PCB *caller = current_process;

    if (current_process != 0) {
        current_process->user_context = *uctxt;
    }

    TracePrintf(1, "TRAP_KERNEL code=0x%x\n", uctxt->code);
    SyscallDispatch(uctxt);

    if (caller == current_process && current_process != 0) {
        current_process->user_context = *uctxt;
    }

    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
}

void
TrapMemory(UserContext *uctxt)
{
    if (current_process != 0) {
        current_process->user_context = *uctxt;
    }

    if (ProcessGrowStack(current_process, uctxt->addr) == SUCCESS) {
        *uctxt = current_process->user_context;
        return;
    }

    TracePrintf(0, "pid %d memory trap addr=%p code=0x%x\n",
                current_process != 0 ? current_process->pid : -1,
                uctxt->addr, uctxt->code);
    ProcessExitCurrent(ERROR);
    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
}

void
TrapAbort(UserContext *uctxt)
{
    if (current_process != 0) {
        current_process->user_context = *uctxt;
    }

    TracePrintf(0, "pid %d aborting on trap vector=%d code=0x%x pc=%p\n",
                current_process != 0 ? current_process->pid : -1,
                uctxt->vector, uctxt->code, uctxt->pc);
    ProcessExitCurrent(ERROR);
    if (current_process != 0) {
        *uctxt = current_process->user_context;
    }
}

void
TrapDisk(UserContext *uctxt)
{
    TracePrintf(1, "TRAP_DISK code=0x%x\n", uctxt->code);
}

void
TrapUnhandled(UserContext *uctxt)
{
    TracePrintf(0, "unhandled trap vector=%d code=0x%x addr=%p pc=%p sp=%p\n",
                uctxt->vector, uctxt->code, uctxt->addr, uctxt->pc, uctxt->sp);
}
