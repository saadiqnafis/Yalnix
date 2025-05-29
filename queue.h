#ifndef QUEUE_H
#define QUEUE_H

typedef struct pcb pcb_t;

/**
 * Process Control Block Queue structure
 *
 * Implements a doubly-linked list of PCBs with head and tail pointers
 * for efficient queue operations.
 */
typedef struct
{
  pcb_t *head; // Pointer to the first PCB in the queue
  pcb_t *tail; // Pointer to the last PCB in the queue
  int size;    // Number of PCBs in the queue
} pcb_queue_t;

/**
 * pcb_queue_create - Creates a new PCB queue
 *
 * Allocates and initializes an empty PCB queue.
 *
 * @return Pointer to the newly created queue on success, NULL if memory allocation fails
 */
pcb_queue_t *pcb_queue_create(void);

/**
 * pcb_enqueue - Adds a PCB to the end of a queue
 *
 * Adds the specified PCB to the end of the queue and updates
 * the queue's head, tail, and size.
 *
 * @param queue - Pointer to the queue to add to
 * @param pcb - Pointer to the PCB to add
 *
 * Note: Halts the system if queue or pcb is NULL
 */
void pcb_enqueue(pcb_queue_t *queue, pcb_t *pcb);

/**
 * pcb_dequeue - Removes and returns the first PCB in a queue
 *
 * Removes the PCB at the head of the queue and updates
 * the queue's head, tail, and size.
 *
 * @param queue - Pointer to the queue to remove from
 *
 * @return Pointer to the removed PCB, NULL if the queue is empty
 *
 * Note: Halts the system if queue is NULL
 */
pcb_t *pcb_dequeue(pcb_queue_t *queue);

/**
 * pcb_queue_is_empty - Checks if a queue is empty
 *
 * @param queue - Pointer to the queue to check
 *
 * @return 1 (true) if the queue is empty, 0 (false) if the queue contains at least one PCB
 */
int pcb_queue_is_empty(pcb_queue_t *queue);

/**
 * pcb_remove - Removes a specific PCB from a queue
 *
 * Removes the specified PCB from anywhere in the queue and updates
 * the queue's head, tail, and size appropriately.
 *
 * @param queue - Pointer to the queue to remove from
 * @param pcb - Pointer to the PCB to remove
 *
 * Note: Halts the system if queue or pcb is NULL
 */
void pcb_remove(pcb_queue_t *queue, pcb_t *pcb);

/**
 * pcb_in_queue - Checks if a PCB is in a queue
 *
 * Searches the queue for the specified PCB.
 *
 * @param queue - Pointer to the queue to search
 * @param pcb - Pointer to the PCB to search for
 *
 * @return 1 (true) if the PCB is in the queue, 0 (false) if the PCB is not in the queue
 */
int pcb_in_queue(pcb_queue_t *queue, pcb_t *pcb);

#endif // QUEUE_H
