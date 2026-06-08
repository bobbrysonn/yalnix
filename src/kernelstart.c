#include "kernel.h"
#include "idle.h"
#include "ipc.h"
#include "memory.h"
#include "process.h"
#include "trap.h"
#include "tty.h"

int vm_enabled = 0;

int LoadProgram(char *name, char *args[], PCB *proc);

void
KernelPanic(char *msg)
{
    TracePrintf(0, "kernel panic: %s\n", msg);
    helper_abort(msg);
}

void
KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt)
{
    char *default_args[] = { "init", 0 };
    char **init_args = cmd_args;
    char *init_name;
    int rc;

    TracePrintf(KERNEL_TRACE_BOOT, "KernelStart: booting with %u bytes of physical memory\n", pmem_size);

    if (init_args == 0 || init_args[0] == 0) {
        init_args = default_args;
    }
    init_name = init_args[0];

    ProcessInit();
    MemoryInit(pmem_size);
    MemoryBuildKernelRegion0();
    IpcInit();
    TtyInit();
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

    init_process = ProcessCreateInit(uctxt);
    if (init_process == 0) {
        KernelPanic("failed to create init process");
    }
    if (ProcessCloneKernelContext(init_process) == ERROR) {
        KernelPanic("failed to clone init kernel context");
    }

    if (current_process == idle_process) {
        if (ProcessSwitch(init_process) == ERROR) {
            KernelPanic("failed to switch to init");
        }

        *uctxt = idle_process->user_context;
        TracePrintf(KERNEL_TRACE_BOOT, "KernelStart: leaving for idle pid %d\n",
                    idle_process->pid);
        return;
    }

    rc = LoadProgram(init_name, init_args, init_process);
    if (rc != SUCCESS) {
        KernelPanic("failed to load init");
    }

    *uctxt = init_process->user_context;

    TracePrintf(KERNEL_TRACE_BOOT, "KernelStart: leaving for init pid %d\n",
                init_process->pid);
}
