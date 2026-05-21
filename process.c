#include "idle.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"

PCB *current_process = 0;
PCB *idle_process = 0;
PCB *init_process = 0;
Queue ready_queue;
unsigned int clock_ticks = 0;

static KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *unused);
static KernelContext *KCSwitch(KernelContext *kc_in, void *old_pcb_p, void *next_pcb_p);
static int ProcessAllocKernelStack(PCB *proc);

void
ProcessInit(void)
{
    QueueInit(&ready_queue);
}

PCB *
ProcessCreateIdle(UserContext *uctxt)
{
    PCB *proc;
    int stack_page;
    int stack_pfn;

    proc = (PCB *)calloc(1, sizeof(PCB));
    if (proc == 0) {
        return 0;
    }

    proc->region1_pt = MemoryAllocPageTable();
    stack_page = MAX_PT_LEN - 1;
    stack_pfn = FrameAlloc();
    if (stack_pfn == ERROR) {
        free(proc->region1_pt);
        free(proc);
        return 0;
    }

    MemoryMapPage(proc->region1_pt, stack_page, stack_pfn, PROT_READ | PROT_WRITE);

    proc->pid = helper_new_pid(proc->region1_pt);
    proc->state = PROC_RUNNING;
    proc->kernel_stack_pfns[0] = KERNEL_STACK_BASE >> PAGESHIFT;
    proc->kernel_stack_pfns[1] = (KERNEL_STACK_BASE >> PAGESHIFT) + 1;
    proc->user_context = *uctxt;
    proc->user_context.pc = DoIdle;
    proc->user_context.sp = (void *)(VMEM_1_LIMIT - 4);
    QueueInit(&proc->children);
    QueueEntryInit(&proc->queue_entry, proc);

    return proc;
}

PCB *
ProcessCreateInit(UserContext *uctxt)
{
    PCB *proc;

    proc = (PCB *)calloc(1, sizeof(PCB));
    if (proc == 0) {
        return 0;
    }

    proc->region1_pt = MemoryAllocPageTable();
    if (ProcessAllocKernelStack(proc) == ERROR) {
        free(proc->region1_pt);
        free(proc);
        return 0;
    }

    proc->pid = helper_new_pid(proc->region1_pt);
    proc->state = PROC_READY;
    proc->user_context = *uctxt;
    proc->user_stack_low_page = MAX_PT_LEN;
    QueueInit(&proc->children);
    QueueEntryInit(&proc->queue_entry, proc);

    return proc;
}

void
ProcessSetCurrent(PCB *proc)
{
    current_process = proc;
}

PCB *
ProcessCurrent(void)
{
    return current_process;
}

int
ProcessCloneKernelContext(PCB *proc)
{
    if (KernelContextSwitch(KCCopy, proc, 0) == ERROR) {
        TracePrintf(0, "ProcessCloneKernelContext failed\n");
        return ERROR;
    }

    return SUCCESS;
}

int
ProcessSwitch(PCB *next)
{
    PCB *old;

    if (next == 0 || next == current_process) {
        return SUCCESS;
    }

    old = current_process;
    if (old != 0 && old->state == PROC_RUNNING) {
        old->state = PROC_READY;
    }
    next->state = PROC_RUNNING;

    if (KernelContextSwitch(KCSwitch, old, next) == ERROR) {
        TracePrintf(0, "ProcessSwitch failed\n");
        return ERROR;
    }

    return SUCCESS;
}

void
ProcessDestroy(PCB *proc)
{
    if (proc == 0) {
        return;
    }

    helper_retire_pid(proc->pid);
    MemoryFreePageTableFrames(proc->region1_pt);
    free(proc->region1_pt);
    free(proc);
}

static int
ProcessAllocKernelStack(PCB *proc)
{
    int i;
    int pfn;

    for (i = 0; i < KERNEL_STACK_MAXSIZE / PAGESIZE; i++) {
        pfn = FrameAlloc();
        if (pfn == ERROR) {
            while (--i >= 0) {
                FrameFree(proc->kernel_stack_pfns[i]);
            }
            return ERROR;
        }
        proc->kernel_stack_pfns[i] = pfn;
    }

    return SUCCESS;
}

static KernelContext *
KCCopy(KernelContext *kc_in, void *new_pcb_p, void *unused)
{
    PCB *proc = (PCB *)new_pcb_p;
    int i;
    int scratch_page = (KERNEL_STACK_BASE >> PAGESHIFT) - 1;
    void *scratch_addr = (void *)(scratch_page << PAGESHIFT);
    void *src_addr;

    (void)unused;

    proc->kernel_context = *kc_in;

    for (i = 0; i < KERNEL_STACK_MAXSIZE / PAGESIZE; i++) {
        MemoryMapPage(kernel_region0_pt, scratch_page, proc->kernel_stack_pfns[i],
                      PROT_READ | PROT_WRITE);
        src_addr = (void *)(KERNEL_STACK_BASE + (i * PAGESIZE));
        memcpy(scratch_addr, src_addr, PAGESIZE);
    }

    MemoryUnmapPage(kernel_region0_pt, scratch_page);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

    return kc_in;
}

static KernelContext *
KCSwitch(KernelContext *kc_in, void *old_pcb_p, void *next_pcb_p)
{
    PCB *old = (PCB *)old_pcb_p;
    PCB *next = (PCB *)next_pcb_p;
    int i;
    int stack_page = KERNEL_STACK_BASE >> PAGESHIFT;

    if (old != 0) {
        old->kernel_context = *kc_in;
    }

    for (i = 0; i < KERNEL_STACK_MAXSIZE / PAGESIZE; i++) {
        kernel_region0_pt[stack_page + i].valid = 1;
        kernel_region0_pt[stack_page + i].prot = PROT_READ | PROT_WRITE;
        kernel_region0_pt[stack_page + i].pfn = next->kernel_stack_pfns[i];
    }

    current_process = next;
    MemoryInstallPageTables(next->region1_pt);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    return &next->kernel_context;
}
