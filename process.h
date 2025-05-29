#ifndef PROCESS_H
#define PROCESS_H

#include "hardware.h"
#include "queue.h"

/**
 * Process Control Block states
 */
typedef enum pcb_state
{
  PCB_STATE_RUNNING, // Process is currently running
  PCB_STATE_READY,   // Process is ready to run
  PCB_STATE_BLOCKED, // Process is blocked, waiting for a resource
  PCB_STATE_DEFUNCT, // Process has exited but not yet cleaned up (zombie)
  PCB_STATE_ORPHAN,  // Process whose parent has exited
} pcb_state_t;

typedef struct pcb pcb_t;

/**
 * Process Control Block structure - represents a process in the system
 */
struct pcb
{
  int pid;           // Process ID
  pcb_state_t state; // Current process state

  pte_t *page_table;   // Region 1 page table
  pte_t *kernel_stack; // Kernel stack page table entries
  void *brk;           // Current break pointer for heap management

  UserContext user_context;     // User-level register state
  KernelContext kernel_context; // Kernel-level register state

  pcb_t *next;           // Next PCB in queue
  pcb_t *prev;           // Previous PCB in queue
  pcb_t *parent;         // Parent process
  pcb_queue_t *children; // List of child processes

  int delay_ticks; // Remaining clock ticks for delayed processes
  int exit_status; // Exit status code

  void *tty_read_buf;  // Buffer for TTY read operations
  int tty_read_len;    // Length of TTY read buffer
  void *tty_write_buf; // Buffer for TTY write operations
  int tty_write_len;   // Length of TTY write buffer

  char *kernel_read_buffer; // Buffer for kernel read operations
  int kernel_read_size;     // Size of kernel read buffer

  char *name; // Process name
};

// Global process queues and current process
extern pcb_t *idle_pcb;                       // The idle process
extern pcb_queue_t *ready_processes;          // Queue of processes ready to run
extern pcb_queue_t *blocked_processes;        // Queue of blocked processes
extern pcb_queue_t *defunct_processes;        // Queue of defunct (zombie) processes
extern pcb_queue_t *waiting_parent_processes; // Queue of processes waiting for their children

static pcb_t *current_process; // Currently running process

/**
 * CreatePCB - Creates a new process control block
 *
 * Allocates and initializes a new PCB with default values,
 * including a new page table for region 1 and a children queue.
 *
 * @param name - Name of the process
 *
 * @return Pointer to the newly created PCB on success,
 *         NULL if memory allocation fails
 */
pcb_t *CreatePCB(char *name);

/**
 * GetCurrentProcess - Returns the currently running process
 *
 * @return Pointer to the currently running process's PCB
 */
pcb_t *GetCurrentProcess();

/**
 * InitializeProcessQueues - Initializes the global process queues
 *
 * Creates the ready, blocked, defunct, and waiting parent queues.
 * Halts the system if any queue creation fails.
 */
void InitializeProcessQueues();

/**
 * SetCurrentProcess - Sets the currently running process
 *
 * Updates the current_process pointer and marks the process
 * as running.
 *
 * @param process - Pointer to the PCB to set as current
 */
void SetCurrentProcess(pcb_t *process);

/**
 * DestroyPCB - Cleans up and frees a process control block
 *
 * Frees all resources associated with a PCB, including page tables,
 * kernel stack, and frames. Updates parent-child relationships and
 * removes the PCB from any queues it might be in.
 *
 * @param pcb - Pointer to the PCB to destroy
 */
void DestroyPCB(pcb_t *pcb);

/**
 * UpdateDelayedPCB - Updates delay counters for blocked processes
 *
 * Decrements the delay_ticks counter for all processes in the
 * blocked queue. Moves processes to the ready queue when their
 * delay reaches zero.
 */
void UpdateDelayedPCB();

/**
 * PrintPageTable - Prints the contents of a process's page table
 *
 * Debug function that prints the validity, frame number, and
 * protection bits for each page in a process's page table.
 *
 * @param pcb - Pointer to the PCB whose page table to print
 */
void PrintPageTable(pcb_t *pcb);

/**
 * CopyPageTable - Copies a page table from parent to child process
 *
 * Creates copies of all valid pages from the parent's address space
 * to the child's address space, using temporary mapping through the
 * scratch page for copying page contents.
 *
 * @param parent - Pointer to the parent PCB
 * @param child - Pointer to the child PCB
 *
 * Note: This function allocates new frames for the child process.
 */
void CopyPageTable(pcb_t *parent, pcb_t *child);

#endif // PROCESS_H