#include "idle.h"
#include "kernel.h"
#include "memory.h"
#include "process.h"

PCB *current_process = 0;
PCB *idle_process = 0;
Queue ready_queue;

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
    proc->user_context = *uctxt;
    proc->user_context.pc = DoIdle;
    proc->user_context.sp = (void *)(VMEM_1_LIMIT - 4);
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

void
ProcessDestroy(PCB *proc)
{
    if (proc == 0) {
        return;
    }

    helper_retire_pid(proc->pid);
    free(proc->region1_pt);
    free(proc);
}
