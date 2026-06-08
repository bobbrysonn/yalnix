#include "Queue.h"

void
QueueInit(Queue *queue)
{
    queue->head.prev = &queue->head;
    queue->head.next = &queue->head;
    queue->head.owner = 0;
    queue->length = 0;
}

void
QueueEntryInit(QueueEntry *entry, void *owner)
{
    entry->prev = 0;
    entry->next = 0;
    entry->owner = owner;
}

int
QueueIsEmpty(Queue *queue)
{
    return queue->length == 0;
}

void
QueuePushBack(Queue *queue, QueueEntry *entry)
{
    entry->prev = queue->head.prev;
    entry->next = &queue->head;
    queue->head.prev->next = entry;
    queue->head.prev = entry;
    queue->length++;
}

QueueEntry *
QueuePopFront(Queue *queue)
{
    QueueEntry *entry;

    if (QueueIsEmpty(queue)) {
        return 0;
    }

    entry = queue->head.next;
    QueueRemove(queue, entry);
    return entry;
}

void
QueueRemove(Queue *queue, QueueEntry *entry)
{
    if (entry->prev == 0 || entry->next == 0) {
        return;
    }

    entry->prev->next = entry->next;
    entry->next->prev = entry->prev;
    entry->prev = 0;
    entry->next = 0;
    queue->length--;
}
