#include "kernel.h"
#include "tty.h"

typedef struct tty_line {
    QueueEntry entry;
    int len;
    int pos;
    char data[TERMINAL_MAX_LINE];
} TtyLine;

typedef struct tty_state {
    Queue read_queue;
    Queue write_queue;
    Queue input_queue;
    TtyLine *line;
    PCB *writer;
    int busy;
} TtyState;

static TtyState terminals[NUM_TERMINALS];

static int TtyValid(int tty_id);
static int TtyCopyLine(TtyState *tty, PCB *proc);
static TtyLine *TtyNextLine(TtyState *tty);
static void TtyStartWrite(int tty_id);

void
TtyInit(void)
{
    int i;

    for (i = 0; i < NUM_TERMINALS; i++) {
        QueueInit(&terminals[i].read_queue);
        QueueInit(&terminals[i].write_queue);
        QueueInit(&terminals[i].input_queue);
        terminals[i].line = 0;
        terminals[i].writer = 0;
        terminals[i].busy = 0;
    }
}

int
SysTtyRead(UserContext *uctxt, int tty_id, void *buf, int len)
{
    TtyState *tty;
    PCB *proc;
    int rc;

    if (!TtyValid(tty_id) || len < 0 || (buf == 0 && len > 0)) {
        return ERROR;
    }
    if (len == 0) {
        return 0;
    }

    tty = &terminals[tty_id];
    proc = current_process;
    if (proc == 0 || proc == idle_process) {
        return ERROR;
    }

    proc->tty_id = tty_id;
    proc->tty_read_buf = buf;
    proc->tty_read_len = len;
    for (;;) {
        rc = TtyCopyLine(tty, proc);
        if (rc >= 0) {
            return rc;
        }

        proc->user_context = *uctxt;
        proc->state = PROC_BLOCKED;
        proc->block_queue = &tty->read_queue;
        QueuePushBack(&tty->read_queue, &proc->block_entry);
        ProcessSchedule();
        *uctxt = proc->user_context;
    }
}

int
SysTtyWrite(UserContext *uctxt, int tty_id, void *buf, int len)
{
    TtyState *tty;
    PCB *proc;

    if (!TtyValid(tty_id) || len < 0 || (buf == 0 && len > 0)) {
        return ERROR;
    }
    if (len == 0) {
        return 0;
    }

    tty = &terminals[tty_id];
    proc = current_process;
    if (proc == 0 || proc == idle_process) {
        return ERROR;
    }

    proc->tty_write_buf = (char *)malloc(len);
    if (proc->tty_write_buf == 0) {
        return ERROR;
    }
    memcpy(proc->tty_write_buf, buf, len);
    proc->tty_id = tty_id;
    proc->tty_write_len = len;
    proc->tty_write_offset = 0;
    proc->user_context = *uctxt;
    proc->user_context.regs[0] = len;
    proc->state = PROC_BLOCKED;

    if (tty->writer == 0 && tty->busy == 0) {
        tty->writer = proc;
        TtyStartWrite(tty_id);
    } else {
        proc->block_queue = &tty->write_queue;
        QueuePushBack(&tty->write_queue, &proc->block_entry);
    }

    ProcessSchedule();
    *uctxt = proc->user_context;

    free(proc->tty_write_buf);
    proc->tty_write_buf = 0;
    proc->tty_write_len = 0;
    proc->tty_write_offset = 0;
    return len;
}

void
TrapTtyReceive(UserContext *uctxt)
{
    int tty_id = uctxt->code;
    TtyState *tty;
    TtyLine *line;
    QueueEntry *entry;
    PCB *reader;

    if (!TtyValid(tty_id)) {
        return;
    }

    tty = &terminals[tty_id];
    line = (TtyLine *)calloc(1, sizeof(TtyLine));
    if (line == 0) {
        return;
    }
    QueueEntryInit(&line->entry, line);
    line->len = TtyReceive(tty_id, line->data, TERMINAL_MAX_LINE);
    if (line->len < 0) {
        free(line);
        return;
    }
    line->pos = 0;

    if (!QueueIsEmpty(&tty->read_queue)) {
        entry = QueuePopFront(&tty->read_queue);
        reader = (PCB *)entry->owner;
        reader->block_queue = 0;
        reader->tty_read_line = line;
        ProcessMakeReady(reader);
        return;
    }

    QueuePushBack(&tty->input_queue, &line->entry);
}

void
TrapTtyTransmit(UserContext *uctxt)
{
    int tty_id = uctxt->code;
    TtyState *tty;
    PCB *writer;

    if (!TtyValid(tty_id)) {
        return;
    }

    tty = &terminals[tty_id];
    tty->busy = 0;
    writer = tty->writer;
    if (writer == 0) {
        TtyStartWrite(tty_id);
        return;
    }

    if (writer->tty_write_offset >= writer->tty_write_len) {
        writer->user_context.regs[0] = writer->tty_write_len;
        tty->writer = 0;
        ProcessMakeReady(writer);
    }

    TtyStartWrite(tty_id);
}

static int
TtyValid(int tty_id)
{
    return tty_id >= 0 && tty_id < NUM_TERMINALS;
}

static int
TtyCopyLine(TtyState *tty, PCB *proc)
{
    TtyLine *line;
    int count;

    line = (TtyLine *)proc->tty_read_line;
    if (line == 0) {
        line = TtyNextLine(tty);
    }
    if (line == 0) {
        return ERROR;
    }

    count = line->len - line->pos;
    if (count > proc->tty_read_len) {
        count = proc->tty_read_len;
    }
    memcpy(proc->tty_read_buf, line->data + line->pos, count);
    line->pos += count;
    proc->tty_read_line = 0;

    if (line->pos < line->len) {
        tty->line = line;
    } else {
        free(line);
    }

    return count;
}

static TtyLine *
TtyNextLine(TtyState *tty)
{
    QueueEntry *entry;

    if (tty->line != 0) {
        TtyLine *line = tty->line;
        tty->line = 0;
        return line;
    }

    entry = QueuePopFront(&tty->input_queue);
    if (entry == 0) {
        return 0;
    }

    return (TtyLine *)entry->owner;
}

static void
TtyStartWrite(int tty_id)
{
    TtyState *tty = &terminals[tty_id];
    QueueEntry *entry;
    PCB *writer;
    int chunk;

    if (tty->busy) {
        return;
    }

    if (tty->writer == 0) {
        entry = QueuePopFront(&tty->write_queue);
        if (entry == 0) {
            return;
        }
        tty->writer = (PCB *)entry->owner;
        tty->writer->block_queue = 0;
    }

    writer = tty->writer;
    if (writer->tty_write_offset >= writer->tty_write_len) {
        tty->writer = 0;
        ProcessMakeReady(writer);
        TtyStartWrite(tty_id);
        return;
    }

    chunk = writer->tty_write_len - writer->tty_write_offset;
    if (chunk > TERMINAL_MAX_LINE) {
        chunk = TERMINAL_MAX_LINE;
    }

    tty->busy = 1;
    TtyTransmit(tty_id, writer->tty_write_buf + writer->tty_write_offset, chunk);
    writer->tty_write_offset += chunk;
}
