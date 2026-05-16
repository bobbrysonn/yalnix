#include "kernel.h"
#include "idle.h"
#include "memory.h"
#include "process.h"
#include "trap.h"

int vm_enabled = 0;

void
KernelPanic(char *msg)
{
    TracePrintf(0, "kernel panic: %s\n", msg);
    helper_abort(msg);
}

void
KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt)
{
    (void)cmd_args;

    TracePrintf(KERNEL_TRACE_BOOT, "KernelStart: booting with %u bytes of physical memory\n", pmem_size);

    ProcessInit();
    MemoryInit(pmem_size);
    MemoryBuildKernelRegion0();
    TrapInit();

    idle_process = ProcessCreateIdle(uctxt);
    if (idle_process == 0) {
        KernelPanic("failed to create idle process");
    }

    ProcessSetCurrent(idle_process);
    MemoryInstallPageTables(idle_process->region1_pt);

    WriteRegister(REG_VM_ENABLE, 1);
    vm_enabled = 1;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    idle_process->user_context.pc = DoIdle;
    idle_process->user_context.sp = (void *)(VMEM_1_LIMIT - 4);
    *uctxt = idle_process->user_context;

    TracePrintf(KERNEL_TRACE_BOOT, "KernelStart: leaving for idle pid %d\n", idle_process->pid);
}
