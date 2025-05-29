#ifndef SYNCHRONIZATION_H
#define SYNCHRONIZATION_H

#include "queue.h"
#include "yalnix.h"

/**
 * Lock structure - represents a mutual exclusion lock
 */
typedef struct lock
{
  int is_locked;           // 1 if locked, 0 if unlocked
  int id;                  // Unique identifier for this lock
  pcb_t *owner;            // Process that currently holds the lock
  pcb_queue_t *wait_queue; // Queue of processes waiting to acquire the lock
  struct lock *next;       // Next lock in the global list
  struct lock *prev;       // Previous lock in the global list
} lock_t;

/**
 * Condition Variable structure - for thread synchronization
 */
typedef struct cond
{
  int id;                  // Unique identifier for this condition variable
  pcb_queue_t *wait_queue; // Queue of processes waiting on this condition
  struct cond *next;       // Next condition variable in the global list
  struct cond *prev;       // Previous condition variable in the global list
} cond_t;

/**
 * Lock List structure - maintains a linked list of all locks
 */
typedef struct lock_list
{
  lock_t *head; // First lock in the list
  lock_t *tail; // Last lock in the list
  int size;     // Number of locks in the list
} lock_list_t;

/**
 * Condition Variable List structure - maintains a linked list of all condition variables
 */
typedef struct cond_list
{
  cond_t *head; // First condition variable in the list
  cond_t *tail; // Last condition variable in the list
  int size;     // Number of condition variables in the list
} cond_list_t;

/**
 * Write Request structure - for pipe write operations
 */
typedef struct write_request
{
  pcb_t *pcb;                 // Process making the write request
  void *buffer;               // Buffer containing data to write
  int length;                 // Length of data to write
  struct write_request *next; // Next write request in queue
  struct write_request *prev; // Previous write request in queue
} write_request_t;

/**
 * Write Queue structure - queue of pending write requests for a pipe
 */
typedef struct write_queue
{
  write_request_t *head; // First write request in queue
  write_request_t *tail; // Last write request in queue
  int size;              // Number of write requests in queue
} write_queue_t;

/**
 * Pipe structure - for interprocess communication
 */
typedef struct pipe
{
  int id; // Unique identifier for this pipe

  pcb_queue_t *read_queue;    // Queue of processes waiting to read
  write_queue_t *write_queue; // Queue of processes waiting to write

  char buffer[PIPE_BUFFER_LEN]; // Circular buffer for pipe data
  int read_index;               // Current read position in buffer
  int write_index;              // Current write position in buffer
  int bytes_available;          // Number of bytes available to read

  struct pipe *next; // Next pipe in the global list
  struct pipe *prev; // Previous pipe in the global list
} pipe_t;

/**
 * Pipe List structure - maintains a linked list of all pipes
 */
typedef struct pipe_list
{
  pipe_t *head; // First pipe in the list
  pipe_t *tail; // Last pipe in the list
  int size;     // Number of pipes in the list
} pipe_list_t;

// Type identification flags for synchronization objects
#define LOCK_ID_FLAG 0x10000    // Bit 16 set for locks
#define CONDVAR_ID_FLAG 0x20000 // Bit 17 set for condition variables
#define PIPE_ID_FLAG 0x30000    // Bits 16-17 set for pipes
#define TYPE_MASK 0xF0000       // Mask to extract type
#define ID_MASK 0x0FFFF         // Mask to extract actual ID (supports 65535 IDs per type)

// Helper macros for ID manipulation
#define GET_RAW_ID(id) ((id) & ID_MASK)
#define GET_TYPE(id) ((id) & TYPE_MASK)
#define IS_LOCK(id) (GET_TYPE(id) == LOCK_ID_FLAG)
#define IS_CONDVAR(id) (GET_TYPE(id) == CONDVAR_ID_FLAG)
#define IS_PIPE(id) (GET_TYPE(id) == PIPE_ID_FLAG)

/**
 * InitSyncLists - Initialize the synchronization subsystem
 *
 * Allocates and initializes the global lists for locks, condition
 * variables, and pipes.
 *
 * Note: Halts the system if memory allocation fails
 */
void InitSyncLists();

/**
 * LockInit - Initialize a new lock
 *
 * Creates a new lock and assigns it a unique ID.
 *
 * @param lock_idp - Pointer to store the lock ID
 *
 * @return SUCCESS on successful initialization, ERROR if lock_idp is NULL or memory allocation fails
 */
int LockInit(int *lock_idp);

/**
 * Acquire - Acquire a lock
 *
 * Attempts to acquire the specified lock. If the lock is already held,
 * blocks the calling process until the lock becomes available.
 *
 * @param lock_id - ID of the lock to acquire
 *
 * @return SUCCESS on successful acquisition,
 *         ERROR if lock_id is invalid or the lock doesn't exist,
 *         will halt the system if context switch fails
 */
int Acquire(int lock_id);

/**
 * Release - Release a lock
 *
 * Releases a previously acquired lock and wakes up the next
 * waiting process, if any.
 *
 * @param lock_id - ID of the lock to release
 *
 * @return SUCCESS on successful release,
 *         ERROR if lock_id is invalid, the lock doesn't exist, or the current process is not the owner of the lock
 */
int Release(int lock_id);

/**
 * CvarInit - Initialize a new condition variable
 *
 * Creates a new condition variable and assigns it a unique ID.
 *
 * @param cvar_id - Pointer to store the condition variable ID
 *
 * @return SUCCESS on successful initialization, ERROR if cvar_id is NULL or memory allocation fails
 */
int CvarInit(int *cvar_id);

/**
 * CvarWait - Wait on a condition variable
 *
 * Atomically releases the specified lock and blocks the calling process
 * until the condition variable is signaled. The lock is reacquired before
 * returning.
 *
 * @param cvar_id - ID of the condition variable to wait on
 * @param lock_id - ID of the lock to release while waiting
 *
 * @return SUCCESS on successful wait and wake-up,
 *         ERROR if cvar_id or lock_id is invalid or doesn't exist,
 *         will halt the system if context switch fails
 */
int CvarWait(int cvar_id, int lock_id);

/**
 * CvarSignal - Signal a condition variable
 *
 * Wakes up one process waiting on the specified condition variable.
 * If no processes are waiting, this is a no-op.
 *
 * @param cvar_id - ID of the condition variable to signal
 *
 * @return SUCCESS on successful signal,
 *         ERROR if cvar_id is invalid or the condition variable doesn't exist
 */
int CvarSignal(int cvar_id);

/**
 * CvarBroadcast - Broadcast to a condition variable
 *
 * Wakes up all processes waiting on the specified condition variable.
 * If no processes are waiting, this is a no-op.
 *
 * @param cvar_id - ID of the condition variable to broadcast to
 *
 * @return SUCCESS on successful broadcast,
 *         ERROR if cvar_id is invalid or the condition variable doesn't exist
 */
int CvarBroadcast(int cvar_id);

/**
 * Reclaim - Reclaim a synchronization object
 *
 * Deallocates the specified synchronization object (lock, condition variable,
 * or pipe) and removes it from the global list.
 *
 * @param resource_id - ID of the synchronization object to reclaim
 *
 * @return SUCCESS on successful reclamation,
 *         ERROR if resource_id is invalid or the object doesn't exist,
 *         ERROR for locks if the lock is currently held by a process
 */
int Reclaim(int resource_id);

/**
 * ReclaimLockHelper - Helper function to reclaim a lock
 *
 * Removes a lock from the global lock list and frees its resources.
 *
 * @param lock - Pointer to the first lock in the list to search
 * @param id - ID of the lock to reclaim
 *
 * @return SUCCESS on successful reclamation,
 *         ERROR if the lock doesn't exist or is currently locked
 */
int ReclaimLockHelper(lock_t *lock, int id);

/**
 * ReclaimCondvarHelper - Helper function to reclaim a condition variable
 *
 * Removes a condition variable from the global condition variable list
 * and frees its resources.
 *
 * @param condvar - Pointer to the first condition variable in the list to search
 * @param id - ID of the condition variable to reclaim
 *
 * @return SUCCESS on successful reclamation,
 *         ERROR if the condition variable doesn't exist
 */
int ReclaimCondvarHelper(cond_t *condvar, int id);

/**
 * ReclaimPipeHelper - Helper function to reclaim a pipe
 *
 * Removes a pipe from the global pipe list, frees any pending
 * write requests, and frees all pipe resources.
 *
 * @param pipe - Pointer to the first pipe in the list to search
 * @param id - ID of the pipe to reclaim
 *
 * @return SUCCESS on successful reclamation,
 *         ERROR if the pipe doesn't exist
 */
int ReclaimPipeHelper(pipe_t *pipe, int id);

/**
 * FindLock - Find a lock by ID
 *
 * Searches the global lock list for a lock with the specified ID.
 *
 * @param lock_id - ID of the lock to find
 *
 * @return Pointer to the lock if found, NULL if not found
 */
lock_t *FindLock(int lock_id);

/**
 * FindCondvar - Find a condition variable by ID
 *
 * Searches the global condition variable list for a condition variable
 * with the specified ID.
 *
 * @param condvar_id - ID of the condition variable to find
 *
 * @return Pointer to the condition variable if found, NULL if not found
 */
cond_t *FindCondvar(int condvar_id);

/**
 * FindPipe - Find a pipe by ID
 *
 * Searches the global pipe list for a pipe with the specified ID.
 *
 * @param pipe_id - ID of the pipe to find
 *
 * @return Pointer to the pipe if found, NULL if not found
 */
pipe_t *FindPipe(int pipe_id);

#endif // SYNCHRONIZATION_H
