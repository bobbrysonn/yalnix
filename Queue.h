#ifndef YALNIX_QUEUE_H
#define YALNIX_QUEUE_H

typedef struct queue_entry {
    struct queue_entry *prev;
    struct queue_entry *next;
    void *owner;
} QueueEntry;

typedef struct queue {
    QueueEntry head;
    int length;
} Queue;

void QueueInit(Queue *queue);
void QueueEntryInit(QueueEntry *entry, void *owner);
int QueueIsEmpty(Queue *queue);
void QueuePushBack(Queue *queue, QueueEntry *entry);
QueueEntry *QueuePopFront(Queue *queue);
void QueueRemove(Queue *queue, QueueEntry *entry);

#endif
