#include "memory.h"
#include "process.h"
#include "syscalls.h"

int LoadProgram(char *name, char *args[], PCB *proc);

static int CopyExecArgs(char *filename, char **argvec, char **name_out, char ***args_out);
static void FreeExecArgs(char *name, char **args);
static char *KernelStrDup(char *src);

void
SyscallDispatch(UserContext *uctxt)
{
    switch (uctxt->code) {
    case YALNIX_FORK:
        uctxt->regs[0] = SysFork();
        break;
    case YALNIX_EXEC:
        if (SysExec((char *)uctxt->regs[0], (char **)uctxt->regs[1]) == SUCCESS) {
            *uctxt = current_process->user_context;
        } else {
            uctxt->regs[0] = ERROR;
        }
        break;
    case YALNIX_EXIT:
        SysExit((int)uctxt->regs[0]);
        break;
    case YALNIX_WAIT:
        uctxt->regs[0] = SysWait(uctxt, (int *)uctxt->regs[0]);
        break;
    case YALNIX_GETPID:
        uctxt->regs[0] = SysGetPid();
        break;
    case YALNIX_BRK:
        uctxt->regs[0] = SysBrk((void *)uctxt->regs[0]);
        break;
    case YALNIX_DELAY:
        uctxt->regs[0] = SysDelay(uctxt, (int)uctxt->regs[0]);
        break;
    default:
        TracePrintf(1, "unsupported syscall code=0x%x\n", uctxt->code);
        uctxt->regs[0] = ERROR;
        break;
    }
}

int
SysFork(void)
{
    PCB *parent = ProcessCurrent();
    PCB *child;

    if (parent == 0 || parent == idle_process) {
        return ERROR;
    }

    child = ProcessCreateChild(parent);
    if (child == 0) {
        return ERROR;
    }

    child->user_context.regs[0] = 0;
    if (ProcessCloneKernelContext(child) == ERROR) {
        ProcessDestroy(child);
        return ERROR;
    }

    if (ProcessCurrent() == child) {
        return 0;
    }

    ProcessMakeReady(child);
    return child->pid;
}

int
SysExec(char *filename, char **argvec)
{
    PCB *proc = ProcessCurrent();
    char *name;
    char **args;
    int rc;

    if (proc == 0 || proc == idle_process) {
        return ERROR;
    }

    if (CopyExecArgs(filename, argvec, &name, &args) == ERROR) {
        return ERROR;
    }

    rc = LoadProgram(name, args, proc);
    FreeExecArgs(name, args);
    if (rc != SUCCESS) {
        if (rc == KILL) {
            ProcessExitCurrent(ERROR);
        }
        return ERROR;
    }

    return SUCCESS;
}

void
SysExit(int status)
{
    ProcessExitCurrent(status);
    helper_abort("SysExit returned");
}

int
SysWait(UserContext *uctxt, int *status_ptr)
{
    PCB *proc = ProcessCurrent();
    QueueEntry *entry;
    PCB *child;
    int pid;
    int status;

    if (proc == 0 || proc == idle_process) {
        return ERROR;
    }

    for (;;) {
        for (entry = proc->children.head.next;
             entry != &proc->children.head;
             entry = entry->next) {
            child = (PCB *)entry->owner;
            if (child->state == PROC_ZOMBIE) {
                pid = child->pid;
                status = child->exit_status;
                if (status_ptr != 0) {
                    *status_ptr = status;
                }
                ProcessDestroy(child);
                return pid;
            }
        }

        if (QueueIsEmpty(&proc->children)) {
            return ERROR;
        }

        proc->waiting_for_child = 1;
        proc->user_context = *uctxt;
        proc->state = PROC_BLOCKED;
        ProcessSchedule();
        *uctxt = proc->user_context;
        proc->waiting_for_child = 0;
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

static int
CopyExecArgs(char *filename, char **argvec, char **name_out, char ***args_out)
{
    char **args;
    int argc;
    int i;

    if (filename == 0) {
        return ERROR;
    }

    *name_out = KernelStrDup(filename);
    if (*name_out == 0) {
        return ERROR;
    }

    if (argvec == 0) {
        args = (char **)calloc(2, sizeof(char *));
        if (args == 0) {
            free(*name_out);
            return ERROR;
        }
        args[0] = KernelStrDup(filename);
        if (args[0] == 0) {
            free(args);
            free(*name_out);
            return ERROR;
        }
        *args_out = args;
        return SUCCESS;
    }

    for (argc = 0; argvec[argc] != 0 && argc < 64; argc++) {
    }
    if (argc == 64) {
        free(*name_out);
        return ERROR;
    }

    args = (char **)calloc(argc + 1, sizeof(char *));
    if (args == 0) {
        free(*name_out);
        return ERROR;
    }

    for (i = 0; i < argc; i++) {
        args[i] = KernelStrDup(argvec[i]);
        if (args[i] == 0) {
            FreeExecArgs(*name_out, args);
            return ERROR;
        }
    }

    *args_out = args;
    return SUCCESS;
}

static void
FreeExecArgs(char *name, char **args)
{
    int i;

    if (args != 0) {
        for (i = 0; args[i] != 0; i++) {
            free(args[i]);
        }
        free(args);
    }
    free(name);
}

static char *
KernelStrDup(char *src)
{
    char *dst;
    int len;

    if (src == 0) {
        return 0;
    }

    len = strlen(src) + 1;
    dst = (char *)malloc(len);
    if (dst == 0) {
        return 0;
    }

    memcpy(dst, src, len);
    return dst;
}

int
SysDelay(UserContext *uctxt, int ticks)
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
    proc->user_context = *uctxt;
    proc->user_context.regs[0] = SUCCESS;
    proc->state = PROC_BLOCKED;
    ProcessSchedule();

    *uctxt = proc->user_context;
    return SUCCESS;
}
