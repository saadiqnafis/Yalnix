#include "process.h"
#include "queue.h"
#include "ykernel.h"

pcb_queue_t *pcb_queue_create(void)
{
  pcb_queue_t *queue = malloc(sizeof(pcb_queue_t));
  if (queue)
  {
    queue->head = queue->tail = NULL;
    queue->size = 0;
  }
  return queue;
}

void pcb_enqueue(pcb_queue_t *queue, pcb_t *pcb)
{
  if (queue == NULL)
  {
    TracePrintf(0, "pcb_enqueue: Queue not initialized!\n");
    Halt();
  }
  if (pcb == NULL)
  {
    TracePrintf(0, "pcb_enqueue: Null PCB!\n");
    Halt();
  }

  pcb->next = NULL;
  pcb->prev = queue->tail;

  if (pcb_queue_is_empty(queue))
  {
    queue->head = pcb;
  }
  else
  {
    queue->tail->next = pcb;
  }

  queue->tail = pcb;
  queue->size++;

  TracePrintf(1, "Enqueued PCB %s (pid %d)\n", pcb->name, pcb->pid);
}

pcb_t *pcb_dequeue(pcb_queue_t *queue)
{
  if (!queue)
  {
    TracePrintf(0, "pcb_dequeue: Queue not initialized!\n");
    Halt();
  }
  pcb_t *pcb = queue->head;
  if (pcb == NULL)
  {
    TracePrintf(0, "pcb_dequeue: Queue is empty!\n");
    return NULL;
  }

  queue->head = pcb->next;
  if (queue->head == NULL)
  {
    queue->tail = NULL;
  }

  queue->size--;
  TracePrintf(1, "Dequeued PCB %s (pid %d)\n", pcb->name, pcb->pid);
  return pcb;
}

int pcb_queue_is_empty(pcb_queue_t *queue)
{
  return queue->size == 0;
}

void pcb_remove(pcb_queue_t *queue, pcb_t *pcb)
{
  if (queue == NULL)
  {
    TracePrintf(0, "pcb_remove: Queue not initialized!\n");
    Halt();
  }
  if (pcb == NULL)
  {
    TracePrintf(0, "pcb_remove: Null PCB!\n");
    Halt();
  }

  if (pcb->prev == NULL)
  {
    queue->head = pcb->next;
  }
  else
  {
    pcb->prev->next = pcb->next;
  }

  if (pcb->next == NULL)
  {
    queue->tail = pcb->prev;
  }
  else
  {
    pcb->next->prev = pcb->prev;
  }

  queue->size--;
}

int pcb_in_queue(pcb_queue_t *queue, pcb_t *pcb)
{
  pcb_t *current = queue->head;
  while (current != NULL)
  {
    if (current == pcb)
    {
      return 1;
    }
    current = current->next;
  }
  return 0;
}