#include "ipc.h"
#include "kernel.h"
#include "process.h"

#define IPC_MAX_OBJECTS 128

typedef enum ipc_type {
    IPC_FREE = 0,
    IPC_PIPE,
    IPC_LOCK,
    IPC_CVAR
} ipc_type_t;

typedef struct pipe_state {
    char *buf;
    int len;
    int cap;
    Queue readers;
} PipeState;

typedef struct lock_state {
    PCB *owner;
    Queue waiters;
} LockState;

typedef struct cvar_state {
    Queue waiters;
} CvarState;

typedef struct ipc_object {
    ipc_type_t type;
    union {
        PipeState pipe;
        LockState lock;
        CvarState cvar;
    } u;
} IpcObject;

static IpcObject objects[IPC_MAX_OBJECTS];

static int AllocObject(ipc_type_t type);
static IpcObject *GetObject(int id, ipc_type_t type);
static void BlockOnQueue(UserContext *uctxt, Queue *queue);
static PCB *WakeOne(Queue *queue);
static void WakeAll(Queue *queue);
static int PipeEnsureCapacity(PipeState *pipe, int needed);
static int LockAcquire(UserContext *uctxt, int lock_id);
static int LockRelease(LockState *lock);

void
IpcInit(void)
{
    int i;

    for (i = 0; i < IPC_MAX_OBJECTS; i++) {
        objects[i].type = IPC_FREE;
    }
}

void
IpcProcessExit(struct pcb *proc)
{
    int i;

    for (i = 0; i < IPC_MAX_OBJECTS; i++) {
        if (objects[i].type == IPC_LOCK && objects[i].u.lock.owner == proc) {
            LockRelease(&objects[i].u.lock);
        }
    }
}

int
SysPipeInit(int *pipe_idp)
{
    int id;
    PipeState *pipe;

    if (pipe_idp == 0) {
        return ERROR;
    }

    id = AllocObject(IPC_PIPE);
    if (id == ERROR) {
        return ERROR;
    }

    pipe = &objects[id].u.pipe;
    pipe->buf = (char *)malloc(PIPE_BUFFER_LEN);
    if (pipe->buf == 0) {
        objects[id].type = IPC_FREE;
        return ERROR;
    }
    pipe->len = 0;
    pipe->cap = PIPE_BUFFER_LEN;
    QueueInit(&pipe->readers);

    *pipe_idp = id;
    return SUCCESS;
}

int
SysPipeRead(UserContext *uctxt, int pipe_id, void *buf, int len)
{
    IpcObject *obj;
    PipeState *pipe;
    int count;

    if (len < 0 || (buf == 0 && len > 0)) {
        return ERROR;
    }
    if (len == 0) {
        return 0;
    }
    if (current_process == 0 || current_process == idle_process) {
        return ERROR;
    }

    obj = GetObject(pipe_id, IPC_PIPE);
    if (obj == 0) {
        return ERROR;
    }
    pipe = &obj->u.pipe;

    while (pipe->len == 0) {
        BlockOnQueue(uctxt, &pipe->readers);
        obj = GetObject(pipe_id, IPC_PIPE);
        if (obj == 0) {
            return ERROR;
        }
        pipe = &obj->u.pipe;
    }

    count = pipe->len;
    if (count > len) {
        count = len;
    }
    memcpy(buf, pipe->buf, count);
    pipe->len -= count;
    if (pipe->len > 0) {
        memcpy(pipe->buf, pipe->buf + count, pipe->len);
    }

    return count;
}

int
SysPipeWrite(int pipe_id, void *buf, int len)
{
    IpcObject *obj;
    PipeState *pipe;

    if (len < 0 || (buf == 0 && len > 0)) {
        return ERROR;
    }
    if (len == 0) {
        return 0;
    }

    obj = GetObject(pipe_id, IPC_PIPE);
    if (obj == 0) {
        return ERROR;
    }
    pipe = &obj->u.pipe;

    if (PipeEnsureCapacity(pipe, pipe->len + len) == ERROR) {
        return ERROR;
    }
    memcpy(pipe->buf + pipe->len, buf, len);
    pipe->len += len;
    WakeAll(&pipe->readers);

    return len;
}

int
SysLockInit(int *lock_idp)
{
    int id;
    LockState *lock;

    if (lock_idp == 0) {
        return ERROR;
    }

    id = AllocObject(IPC_LOCK);
    if (id == ERROR) {
        return ERROR;
    }

    lock = &objects[id].u.lock;
    lock->owner = 0;
    QueueInit(&lock->waiters);
    *lock_idp = id;

    return SUCCESS;
}

int
SysAcquire(UserContext *uctxt, int lock_id)
{
    return LockAcquire(uctxt, lock_id);
}

int
SysRelease(int lock_id)
{
    IpcObject *obj;

    obj = GetObject(lock_id, IPC_LOCK);
    if (obj == 0) {
        return ERROR;
    }
    if (obj->u.lock.owner != current_process) {
        return ERROR;
    }

    return LockRelease(&obj->u.lock);
}

int
SysCvarInit(int *cvar_idp)
{
    int id;

    if (cvar_idp == 0) {
        return ERROR;
    }

    id = AllocObject(IPC_CVAR);
    if (id == ERROR) {
        return ERROR;
    }

    QueueInit(&objects[id].u.cvar.waiters);
    *cvar_idp = id;

    return SUCCESS;
}

int
SysCvarSignal(int cvar_id)
{
    IpcObject *obj;

    obj = GetObject(cvar_id, IPC_CVAR);
    if (obj == 0) {
        return ERROR;
    }
    WakeOne(&obj->u.cvar.waiters);

    return SUCCESS;
}

int
SysCvarBroadcast(int cvar_id)
{
    IpcObject *obj;

    obj = GetObject(cvar_id, IPC_CVAR);
    if (obj == 0) {
        return ERROR;
    }
    WakeAll(&obj->u.cvar.waiters);

    return SUCCESS;
}

int
SysCvarWait(UserContext *uctxt, int cvar_id, int lock_id)
{
    IpcObject *cvar_obj;
    IpcObject *lock_obj;

    cvar_obj = GetObject(cvar_id, IPC_CVAR);
    lock_obj = GetObject(lock_id, IPC_LOCK);
    if (cvar_obj == 0 || lock_obj == 0 ||
        lock_obj->u.lock.owner != current_process) {
        return ERROR;
    }

    if (LockRelease(&lock_obj->u.lock) == ERROR) {
        return ERROR;
    }

    BlockOnQueue(uctxt, &cvar_obj->u.cvar.waiters);
    if (GetObject(cvar_id, IPC_CVAR) == 0) {
        return ERROR;
    }

    return LockAcquire(uctxt, lock_id);
}

int
SysReclaim(int id)
{
    IpcObject *obj;

    if (id < 0 || id >= IPC_MAX_OBJECTS || objects[id].type == IPC_FREE) {
        return ERROR;
    }

    obj = &objects[id];
    if (obj->type == IPC_PIPE) {
        if (!QueueIsEmpty(&obj->u.pipe.readers)) {
            return ERROR;
        }
        free(obj->u.pipe.buf);
        obj->u.pipe.buf = 0;
    } else if (obj->type == IPC_LOCK) {
        if (obj->u.lock.owner != 0 || !QueueIsEmpty(&obj->u.lock.waiters)) {
            return ERROR;
        }
    } else if (obj->type == IPC_CVAR) {
        if (!QueueIsEmpty(&obj->u.cvar.waiters)) {
            return ERROR;
        }
    } else {
        return ERROR;
    }

    obj->type = IPC_FREE;
    return SUCCESS;
}

static int
AllocObject(ipc_type_t type)
{
    int i;

    for (i = 0; i < IPC_MAX_OBJECTS; i++) {
        if (objects[i].type == IPC_FREE) {
            objects[i].type = type;
            return i;
        }
    }

    return ERROR;
}

static IpcObject *
GetObject(int id, ipc_type_t type)
{
    if (id < 0 || id >= IPC_MAX_OBJECTS || objects[id].type != type) {
        return 0;
    }

    return &objects[id];
}

static void
BlockOnQueue(UserContext *uctxt, Queue *queue)
{
    PCB *proc = current_process;

    proc->user_context = *uctxt;
    proc->state = PROC_BLOCKED;
    proc->block_queue = queue;
    QueuePushBack(queue, &proc->block_entry);
    ProcessSchedule();
    *uctxt = proc->user_context;
}

static PCB *
WakeOne(Queue *queue)
{
    QueueEntry *entry;
    PCB *proc;

    entry = QueuePopFront(queue);
    if (entry == 0) {
        return 0;
    }

    proc = (PCB *)entry->owner;
    proc->block_queue = 0;
    proc->user_context.regs[0] = SUCCESS;
    ProcessMakeReady(proc);

    return proc;
}

static void
WakeAll(Queue *queue)
{
    while (!QueueIsEmpty(queue)) {
        WakeOne(queue);
    }
}

static int
PipeEnsureCapacity(PipeState *pipe, int needed)
{
    char *new_buf;
    int new_cap;

    if (needed <= pipe->cap) {
        return SUCCESS;
    }

    new_cap = pipe->cap;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    new_buf = (char *)malloc(new_cap);
    if (new_buf == 0) {
        return ERROR;
    }
    memcpy(new_buf, pipe->buf, pipe->len);
    free(pipe->buf);
    pipe->buf = new_buf;
    pipe->cap = new_cap;

    return SUCCESS;
}

static int
LockAcquire(UserContext *uctxt, int lock_id)
{
    IpcObject *obj;
    LockState *lock;

    obj = GetObject(lock_id, IPC_LOCK);
    if (obj == 0 || current_process == 0 || current_process == idle_process) {
        return ERROR;
    }
    lock = &obj->u.lock;
    if (lock->owner == current_process) {
        return ERROR;
    }

    while (lock->owner != 0) {
        BlockOnQueue(uctxt, &lock->waiters);
        obj = GetObject(lock_id, IPC_LOCK);
        if (obj == 0) {
            return ERROR;
        }
        lock = &obj->u.lock;
        if (lock->owner == current_process) {
            return SUCCESS;
        }
    }

    lock->owner = current_process;
    return SUCCESS;
}

static int
LockRelease(LockState *lock)
{
    PCB *next;

    lock->owner = 0;
    next = WakeOne(&lock->waiters);
    if (next != 0) {
        lock->owner = next;
    }

    return SUCCESS;
}
