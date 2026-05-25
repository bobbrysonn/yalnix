#include "idle.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"

PCB *current_process = 0;
PCB *idle_process = 0;
PCB *init_process = 0;
Queue ready_queue;
unsigned int clock_ticks = 0;

static PCB *process_table[MAX_PROCS];

static KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *unused);
static KernelContext *KCSwitch(KernelContext *kc_in, void *old_pcb_p, void *next_pcb_p);
static int ProcessAllocKernelStack(PCB *proc);
static void ProcessFreeKernelStack(PCB *proc);
static int ProcessCopyRegion1(PCB *dst, PCB *src);
static void ProcessRegister(PCB *proc);
static PCB *ProcessNextReady(void);

void
ProcessInit(void)
{
    int i;

    for (i = 0; i < MAX_PROCS; i++) {
        process_table[i] = 0;
    }

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
    if (proc->pid == ERROR) {
        FrameFree(stack_pfn);
        free(proc->region1_pt);
        free(proc);
        return 0;
    }
    proc->state = PROC_RUNNING;
    proc->kernel_stack_pfns[0] = KERNEL_STACK_BASE >> PAGESHIFT;
    proc->kernel_stack_pfns[1] = (KERNEL_STACK_BASE >> PAGESHIFT) + 1;
    proc->user_context = *uctxt;
    proc->user_context.pc = DoIdle;
    proc->user_context.sp = (void *)(VMEM_1_LIMIT - 4);
    QueueInit(&proc->children);
    QueueEntryInit(&proc->ready_entry, proc);
    QueueEntryInit(&proc->child_entry, proc);
    ProcessRegister(proc);

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
    if (proc->pid == ERROR) {
        ProcessFreeKernelStack(proc);
        free(proc->region1_pt);
        free(proc);
        return 0;
    }
    proc->state = PROC_READY;
    proc->user_context = *uctxt;
    proc->user_stack_low_page = MAX_PT_LEN;
    QueueInit(&proc->children);
    QueueEntryInit(&proc->ready_entry, proc);
    QueueEntryInit(&proc->child_entry, proc);
    ProcessRegister(proc);

    return proc;
}

PCB *
ProcessCreateChild(PCB *parent)
{
    PCB *child;

    child = (PCB *)calloc(1, sizeof(PCB));
    if (child == 0) {
        return 0;
    }

    child->region1_pt = MemoryAllocPageTable();
    if (ProcessAllocKernelStack(child) == ERROR) {
        free(child->region1_pt);
        free(child);
        return 0;
    }

    child->pid = helper_new_pid(child->region1_pt);
    if (child->pid == ERROR) {
        ProcessFreeKernelStack(child);
        free(child->region1_pt);
        free(child);
        return 0;
    }
    child->state = PROC_READY;
    child->user_context = parent->user_context;
    child->kernel_context = parent->kernel_context;
    child->user_brk = parent->user_brk;
    child->user_heap_start = parent->user_heap_start;
    child->user_stack_low_page = parent->user_stack_low_page;
    child->parent = parent;
    QueueInit(&child->children);
    QueueEntryInit(&child->ready_entry, child);
    QueueEntryInit(&child->child_entry, child);

    if (ProcessCopyRegion1(child, parent) == ERROR) {
        ProcessFreeKernelStack(child);
        helper_retire_pid(child->pid);
        free(child->region1_pt);
        free(child);
        return 0;
    }

    QueuePushBack(&parent->children, &child->child_entry);
    ProcessRegister(child);
    return child;
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
    next->state = PROC_RUNNING;

    if (KernelContextSwitch(KCSwitch, old, next) == ERROR) {
        TracePrintf(0, "ProcessSwitch failed\n");
        return ERROR;
    }

    return SUCCESS;
}

void
ProcessMakeReady(PCB *proc)
{
    if (proc == 0 || proc == idle_process || proc->state == PROC_ZOMBIE) {
        return;
    }

    proc->state = PROC_READY;
    if (proc->ready_entry.prev == 0 && proc->ready_entry.next == 0) {
        QueuePushBack(&ready_queue, &proc->ready_entry);
    }
}

void
ProcessSchedule(void)
{
    PCB *old;
    PCB *next;

    ProcessWakeDelayed();

    old = current_process;
    if (old != 0 && old != idle_process && old->state == PROC_RUNNING) {
        ProcessMakeReady(old);
    }

    next = ProcessNextReady();
    if (next == 0) {
        next = idle_process;
    }

    if (next != current_process) {
        ProcessSwitch(next);
    }
}

void
ProcessWakeDelayed(void)
{
    int i;

    for (i = 0; i < MAX_PROCS; i++) {
        if (process_table[i] != 0 &&
            process_table[i]->state == PROC_BLOCKED &&
            process_table[i]->delay_until != 0 &&
            process_table[i]->delay_until <= clock_ticks) {
            process_table[i]->delay_until = 0;
            ProcessMakeReady(process_table[i]);
        }
    }
}

void
ProcessExitCurrent(int status)
{
    PCB *proc = current_process;
    PCB *child;
    QueueEntry *entry;

    if (proc == 0 || proc == idle_process) {
        KernelPanic("idle process exit");
    }
    if (proc == init_process) {
        Halt();
    }

    while (!QueueIsEmpty(&proc->children)) {
        entry = QueuePopFront(&proc->children);
        child = (PCB *)entry->owner;
        child->parent = 0;
    }

    proc->exit_status = status;
    proc->state = PROC_ZOMBIE;

    if (proc->parent != 0 && proc->parent->waiting_for_child) {
        proc->parent->waiting_for_child = 0;
        ProcessMakeReady(proc->parent);
    }

    ProcessSchedule();
    helper_abort("zombie process resumed");
}

int
ProcessGrowStack(PCB *proc, void *addr)
{
    int fault_page;
    int brk_page;
    int page;
    int pfn;

    if (proc == 0 || addr < (void *)VMEM_1_BASE || addr >= (void *)VMEM_1_LIMIT) {
        return ERROR;
    }

    fault_page = ((int)addr - VMEM_1_BASE) >> PAGESHIFT;
    brk_page = (UP_TO_PAGE(proc->user_brk) - VMEM_1_BASE) >> PAGESHIFT;

    if (fault_page >= proc->user_stack_low_page || fault_page <= brk_page) {
        return ERROR;
    }

    for (page = proc->user_stack_low_page - 1; page >= fault_page; page--) {
        pfn = FrameAlloc();
        if (pfn == ERROR) {
            return ERROR;
        }
        if (MemoryMapPage(proc->region1_pt, page, pfn, PROT_READ | PROT_WRITE) == ERROR) {
            FrameFree(pfn);
            return ERROR;
        }
    }

    proc->user_stack_low_page = fault_page;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    return SUCCESS;
}

void
ProcessDestroy(PCB *proc)
{
    QueueEntry *entry;
    PCB *child;

    if (proc == 0) {
        return;
    }

    if (proc->parent != 0 && proc->child_entry.prev != 0) {
        QueueRemove(&proc->parent->children, &proc->child_entry);
    }

    while (!QueueIsEmpty(&proc->children)) {
        entry = QueuePopFront(&proc->children);
        child = (PCB *)entry->owner;
        child->parent = 0;
    }

    if (proc->ready_entry.prev != 0) {
        QueueRemove(&ready_queue, &proc->ready_entry);
    }

    if (proc->pid >= 0 && proc->pid < MAX_PROCS && process_table[proc->pid] == proc) {
        process_table[proc->pid] = 0;
    }

    helper_retire_pid(proc->pid);
    MemoryFreePageTableFrames(proc->region1_pt);
    ProcessFreeKernelStack(proc);
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

static void
ProcessFreeKernelStack(PCB *proc)
{
    int i;

    if (proc == idle_process) {
        return;
    }

    for (i = 0; i < KERNEL_STACK_MAXSIZE / PAGESIZE; i++) {
        if (proc->kernel_stack_pfns[i] != 0) {
            FrameFree(proc->kernel_stack_pfns[i]);
            proc->kernel_stack_pfns[i] = 0;
        }
    }
}

static int
ProcessCopyRegion1(PCB *dst, PCB *src)
{
    int i;
    int pfn;
    int scratch_page = (KERNEL_STACK_BASE >> PAGESHIFT) - 1;
    void *scratch_addr = (void *)(scratch_page << PAGESHIFT);
    void *src_addr;

    for (i = 0; i < MAX_PT_LEN; i++) {
        if (!src->region1_pt[i].valid) {
            continue;
        }

        pfn = FrameAlloc();
        if (pfn == ERROR) {
            MemoryFreePageTableFrames(dst->region1_pt);
            return ERROR;
        }
        if (MemoryMapPage(dst->region1_pt, i, pfn, src->region1_pt[i].prot) == ERROR) {
            FrameFree(pfn);
            MemoryFreePageTableFrames(dst->region1_pt);
            return ERROR;
        }

        MemoryMapPage(kernel_region0_pt, scratch_page, pfn, PROT_READ | PROT_WRITE);
        src_addr = (void *)(VMEM_1_BASE + (i << PAGESHIFT));
        memcpy(scratch_addr, src_addr, PAGESIZE);
    }

    MemoryUnmapPage(kernel_region0_pt, scratch_page);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
    return SUCCESS;
}

static void
ProcessRegister(PCB *proc)
{
    if (proc->pid >= 0 && proc->pid < MAX_PROCS) {
        process_table[proc->pid] = proc;
    }
}

static PCB *
ProcessNextReady(void)
{
    QueueEntry *entry;

    entry = QueuePopFront(&ready_queue);
    if (entry == 0) {
        return 0;
    }

    return (PCB *)entry->owner;
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
