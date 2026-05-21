#include "memory.h"
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
    PCB *proc = ProcessCurrent();
    int old_page;
    int new_page;
    int page;
    int pfn;

    if (proc == 0 || proc->user_heap_start == 0 ||
        addr < proc->user_heap_start || addr >= (void *)VMEM_1_LIMIT) {
        return ERROR;
    }

    old_page = (UP_TO_PAGE(proc->user_brk) - VMEM_1_BASE) >> PAGESHIFT;
    new_page = (UP_TO_PAGE(addr) - VMEM_1_BASE) >> PAGESHIFT;

    if (new_page >= proc->user_stack_low_page) {
        return ERROR;
    }

    for (page = old_page; page < new_page; page++) {
        pfn = FrameAlloc();
        if (pfn == ERROR) {
            while (--page >= old_page) {
                FrameFree(proc->region1_pt[page].pfn);
                MemoryUnmapPage(proc->region1_pt, page);
            }
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
            return ERROR;
        }
        if (MemoryMapPage(proc->region1_pt, page, pfn, PROT_READ | PROT_WRITE) == ERROR) {
            FrameFree(pfn);
            while (--page >= old_page) {
                FrameFree(proc->region1_pt[page].pfn);
                MemoryUnmapPage(proc->region1_pt, page);
            }
            WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
            return ERROR;
        }
    }

    for (page = new_page; page < old_page; page++) {
        if (proc->region1_pt[page].valid) {
            FrameFree(proc->region1_pt[page].pfn);
            MemoryUnmapPage(proc->region1_pt, page);
        }
    }

    proc->user_brk = (void *)UP_TO_PAGE(addr);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    return SUCCESS;
}

int
SysDelay(int ticks)
{
    PCB *proc;

    if (ticks < 0) {
        return ERROR;
    }
    if (ticks == 0) {
        return SUCCESS;
    }

    proc = ProcessCurrent();
    if (proc == 0 || proc == idle_process) {
        return ERROR;
    }

    proc->delay_until = clock_ticks + (unsigned int)ticks;
    proc->state = PROC_BLOCKED;
    if (ProcessSwitch(idle_process) == ERROR) {
        return ERROR;
    }

    return SUCCESS;
}
