#ifndef YALNIX_PROCESS_H
#define YALNIX_PROCESS_H

#include <ykernel.h>
#include "Queue.h"

typedef enum proc_state {
    PROC_EMPTY = 0,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_ZOMBIE
} proc_state_t;

typedef struct pcb {
    int pid;
    proc_state_t state;
    pte_t *region1_pt;
    UserContext user_context;
    KernelContext kernel_context;
    unsigned int kernel_stack_pfns[KERNEL_STACK_MAXSIZE / PAGESIZE];
    struct pcb *parent;
    Queue children;
    QueueEntry queue_entry;
} PCB;

extern PCB *current_process;
extern PCB *idle_process;
extern Queue ready_queue;

void ProcessInit(void);
PCB *ProcessCreateIdle(UserContext *uctxt);
void ProcessSetCurrent(PCB *proc);
PCB *ProcessCurrent(void);
void ProcessDestroy(PCB *proc);

#endif
