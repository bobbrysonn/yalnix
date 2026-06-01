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
    void *user_brk;
    void *user_heap_start;
    int user_stack_low_page;
    unsigned int delay_until;
    int exit_status;
    int waiting_for_child;
    int tty_id;
    void *tty_read_buf;
    int tty_read_len;
    void *tty_read_line;
    char *tty_write_buf;
    int tty_write_len;
    int tty_write_offset;
    struct pcb *parent;
    Queue children;
    QueueEntry ready_entry;
    QueueEntry block_entry;
    Queue *block_queue;
    QueueEntry child_entry;
} PCB;

extern PCB *current_process;
extern PCB *idle_process;
extern PCB *init_process;
extern Queue ready_queue;
extern unsigned int clock_ticks;

void ProcessInit(void);
PCB *ProcessCreateIdle(UserContext *uctxt);
PCB *ProcessCreateInit(UserContext *uctxt);
PCB *ProcessCreateChild(PCB *parent);
void ProcessSetCurrent(PCB *proc);
PCB *ProcessCurrent(void);
int ProcessCloneKernelContext(PCB *proc);
int ProcessSwitch(PCB *next);
void ProcessMakeReady(PCB *proc);
void ProcessSchedule(void);
void ProcessWakeDelayed(void);
void ProcessExitCurrent(int status);
int ProcessGrowStack(PCB *proc, void *addr);
void ProcessDestroy(PCB *proc);

#endif
