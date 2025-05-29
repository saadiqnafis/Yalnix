#include "synchronization.h"
#include "kernel.h"
#include "hardware.h"
#include "yuser.h"
#include "ykernel.h"
#include "yalnix.h"
#include "syscalls.h"
#include "process.h"

lock_list_t *global_locks;
cond_list_t *global_condvars;
pipe_list_t *global_pipes;
int next_sync_id = 1;

void InitSyncLists()
{
  global_locks = (lock_list_t *)malloc(sizeof(lock_list_t));
  if (global_locks == NULL)
  {
    TracePrintf(0, "Failed to allocate memory for global_locks\n");
    Halt();
  }
  global_locks->head = NULL;
  global_locks->tail = NULL;
  global_locks->size = 0;

  global_condvars = (cond_list_t *)malloc(sizeof(cond_list_t));
  if (global_condvars == NULL)
  {
    TracePrintf(0, "Failed to allocate memory for global_condvars\n");
    Halt();
  }
  global_condvars->head = NULL;
  global_condvars->tail = NULL;
  global_condvars->size = 0;

  global_pipes = (pipe_list_t *)malloc(sizeof(pipe_list_t));
  if (global_pipes == NULL)
  {
    TracePrintf(0, "Failed to allocate memory for global_pipes\n");
    Halt();
  }
  global_pipes->head = NULL;
  global_pipes->tail = NULL;
  global_pipes->size = 0;
}

int LockInit(int *lock_idp)
{
  if (lock_idp == NULL)
  {
    return ERROR;
  }
  lock_t *lock = malloc(sizeof(lock_t));
  if (lock == NULL)
  {
    return ERROR;
  }
  lock->is_locked = 0;
  lock->owner = NULL;
  lock->wait_queue = pcb_queue_create();
  lock->id = LOCK_ID_FLAG | next_sync_id++;
  lock->next = NULL;
  lock->prev = NULL;

  if (global_locks->head == NULL)
  {
    global_locks->head = lock;
    global_locks->tail = lock;
  }
  else
  {
    global_locks->tail->next = lock;
    lock->prev = global_locks->tail;
    global_locks->tail = lock;
  }
  global_locks->size++;
  *lock_idp = lock->id;

  TracePrintf(0, "Lock initialized with id %d\n", lock->id);
  return SUCCESS;
}

int Acquire(int lock_id)
{
  if (lock_id <= 0)
  {
    return ERROR;
  }

  lock_t *current = FindLock(lock_id);

  if (current == NULL)
  {
    return ERROR;
  }

  pcb_t *pcb = GetCurrentProcess();
  if (current->is_locked)
  {
    pcb_enqueue(current->wait_queue, pcb);
    pcb->state = PCB_STATE_BLOCKED;
    pcb_enqueue(blocked_processes, pcb);

    pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;

    int rc = KernelContextSwitch(KCSwitch, pcb, next);
    if (rc == -1)
    {
      TracePrintf(0, "KernelContextSwitch failed when acquiring lock\n");
      Halt();
    }

    TracePrintf(0, "Lock acquired by process %d after waiting\n", pcb->pid);
    return SUCCESS;
  }

  current->is_locked = 1;
  current->owner = pcb;
  TracePrintf(0, "Lock acquired by process %d\n", pcb->pid);
  return SUCCESS;
}

int Release(int lock_id)
{
  if (!IS_LOCK(lock_id) || lock_id <= 0)
  {
    return ERROR;
  }

  pcb_t *pcb = GetCurrentProcess();
  if (pcb == NULL)
  {
    TracePrintf(0, "GetCurrentProcess returned NULL\n");
    return ERROR;
  }

  lock_t *current = FindLock(lock_id);

  if (current == NULL)
  {
    TracePrintf(0, "Lock not found\n");
    return ERROR;
  }

  if (current->owner != pcb)
  {
    TracePrintf(0, "Process %d is not the owner of lock %d\n", pcb->pid, lock_id);
    return ERROR;
  }

  current->is_locked = 0;
  current->owner = NULL;

  if (current->wait_queue->head != NULL)
  {
    pcb_t *next = pcb_dequeue(current->wait_queue);
    next->state = PCB_STATE_READY;
    pcb_remove(blocked_processes, next);
    pcb_enqueue(ready_processes, next);

    current->is_locked = 1;
    current->owner = next;
    TracePrintf(0, "Lock %d transferred from process %d to process %d\n", lock_id, pcb->pid, next->pid);
  }
  else
  {
    TracePrintf(0, "Lock %d released by process %d with no waiters\n", lock_id, pcb->pid);
  }

  TracePrintf(0, "Lock released by process %d\n", pcb->pid);
  return SUCCESS;
}

int CvarInit(int *cvar_idp)
{
  if (cvar_idp == NULL)
  {
    TracePrintf(0, "cvar_idp is NULL\n");
    return ERROR;
  }

  cond_t *condvar = malloc(sizeof(cond_t));
  if (condvar == NULL)
  {
    TracePrintf(0, "Failed to allocate memory for condvar\n");
    return ERROR;
  }
  condvar->id = CONDVAR_ID_FLAG | next_sync_id++;
  condvar->wait_queue = pcb_queue_create();
  condvar->next = NULL;
  condvar->prev = NULL;

  if (global_condvars->head == NULL)
  {
    global_condvars->head = condvar;
    global_condvars->tail = condvar;
  }
  else
  {
    global_condvars->tail->next = condvar;
    condvar->prev = global_condvars->tail;
    global_condvars->tail = condvar;
  }
  global_condvars->size++;
  *cvar_idp = condvar->id;

  TracePrintf(0, "Condition variable initialized with id %d\n", condvar->id);
  return SUCCESS;
}

int CvarWait(int cvar_id, int lock_id)
{
  if (cvar_id <= 0 || lock_id <= 0)
  {
    return ERROR;
  }

  Release(lock_id);
  pcb_t *pcb = GetCurrentProcess();
  pcb->state = PCB_STATE_BLOCKED;

  cond_t *condvar = FindCondvar(cvar_id);
  if (condvar == NULL)
  {
    return ERROR;
  }

  pcb_enqueue(condvar->wait_queue, pcb);
  pcb_enqueue(blocked_processes, pcb);

  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;

  int rc = KernelContextSwitch(KCSwitch, pcb, next);
  if (rc == -1)
  {
    TracePrintf(0, "KernelContextSwitch failed when waiting for condition variable\n");
    Halt();
  }

  Acquire(lock_id);

  TracePrintf(0, "Process %d waiting on condition variable %d has been resumed\n", pcb->pid, cvar_id);
  return SUCCESS;
}

int CvarSignal(int cvar_id)
{
  if (cvar_id <= 0)
  {
    return ERROR;
  }

  cond_t *condvar = FindCondvar(cvar_id);

  if (condvar == NULL)
  {
    return ERROR;
  }

  if (condvar->wait_queue->head != NULL)
  {
    pcb_t *next = pcb_dequeue(condvar->wait_queue);
    next->state = PCB_STATE_READY;
    pcb_remove(blocked_processes, next);
    pcb_enqueue(ready_processes, next);

    TracePrintf(0, "Process %d has been resumed from condition variable %d\n", next->pid, cvar_id);
  }
  else
  {
    TracePrintf(0, "Condition variable %d has no waiters\n", cvar_id);
  }

  TracePrintf(0, "Condition variable %d signaled\n", cvar_id);
  return SUCCESS;
}

int CvarBroadcast(int cvar_id)
{
  if (cvar_id <= 0)
  {
    return ERROR;
  }

  cond_t *condvar = FindCondvar(cvar_id);
  if (condvar == NULL)
  {
    return ERROR;
  }
  for (pcb_t *pcb = condvar->wait_queue->head; pcb != NULL; pcb = pcb->next)
  {
    pcb->state = PCB_STATE_READY;
    pcb_remove(blocked_processes, pcb);
    pcb_enqueue(ready_processes, pcb);
    TracePrintf(0, "Process %d has been resumed from condition variable %d\n", pcb->pid, cvar_id);
  }
  TracePrintf(0, "Condition variable %d broadcasted\n", cvar_id);
  return SUCCESS;
}

int Reclaim(int id)
{
  if (id <= 0)
  {
    return ERROR;
  }
  if (IS_LOCK(id))
  {
    return ReclaimLockHelper(global_locks->head, id);
  }
  else if (IS_CONDVAR(id))
  {
    return ReclaimCondvarHelper(global_condvars->head, id);
  }
  else if (IS_PIPE(id))
  {
    return ReclaimPipeHelper(global_pipes->head, id);
  }

  TracePrintf(0, "Invalid ID %d, cannot reclaim\n", id);
  return ERROR;
}

int PipeInit(int *pipe_idp)
{
  if (pipe_idp == NULL)
  {
    TracePrintf(0, "PipeInit: pipe_idp is NULL\n");
    return ERROR;
  }

  pipe_t *pipe = malloc(sizeof(pipe_t));
  if (pipe == NULL)
  {
    TracePrintf(0, "PipeInit: Failed to allocate memory for pipe\n");
    return ERROR;
  }

  pipe->id = PIPE_ID_FLAG | next_sync_id++;
  pipe->read_queue = pcb_queue_create();
  if (pipe->read_queue == NULL)
  {
    free(pipe);
    return ERROR;
  }

  // Create write queue
  pipe->write_queue = malloc(sizeof(write_queue_t));
  if (pipe->write_queue == NULL)
  {
    free(pipe->read_queue);
    free(pipe);
    return ERROR;
  }
  pipe->write_queue->head = NULL;
  pipe->write_queue->tail = NULL;
  pipe->write_queue->size = 0;

  // CRITICAL: Initialize buffer correctly
  pipe->bytes_available = 0;
  pipe->read_index = 0;
  pipe->write_index = 0;

  // Also initialize the buffer itself (optional but good practice)
  memset(pipe->buffer, 0, PIPE_BUFFER_LEN);

  pipe->next = NULL;
  pipe->prev = NULL;

  // Add to global list
  if (global_pipes->head == NULL)
  {
    global_pipes->head = pipe;
    global_pipes->tail = pipe;
  }
  else
  {
    global_pipes->tail->next = pipe;
    pipe->prev = global_pipes->tail;
    global_pipes->tail = pipe;
  }
  global_pipes->size++;
  *pipe_idp = pipe->id;

  TracePrintf(2, "PipeInit: Pipe initialized with id %d\n", pipe->id);
  return SUCCESS;
}

int PipeRead(int pipe_id, void *buffer, int length)
{
  if (pipe_id <= 0 || buffer == NULL || length <= 0)
  {
    TracePrintf(0, "PipeRead: Invalid arguments\n");
    return ERROR;
  }

  pipe_t *pipe = FindPipe(pipe_id);
  if (pipe == NULL)
  {
    TracePrintf(0, "PipeRead: Pipe %d not found\n", pipe_id);
    return ERROR;
  }

  pcb_t *pcb = GetCurrentProcess();
  TracePrintf(3, "PipeRead: Process %d attempting to read %d bytes (available: %d)\n",
              pcb->pid, length, pipe->bytes_available);

  // Block if pipe is empty
  if (pipe->bytes_available == 0)
  {
    TracePrintf(2, "PipeRead: Pipe empty, blocking reader (pid %d)\n", pcb->pid);
    pcb_enqueue(pipe->read_queue, pcb);
    pcb->state = PCB_STATE_BLOCKED;
    pcb_enqueue(blocked_processes, pcb);

    pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
    int rc = KernelContextSwitch(KCSwitch, pcb, next);
    if (rc == -1)
    {
      TracePrintf(0, "PipeRead: KernelContextSwitch failed\n");
      Halt();
    }

    TracePrintf(2, "PipeRead: Process %d resumed after blocking\n", pcb->pid);
  }

  // Double-check we have data after being woken up
  if (pipe->bytes_available == 0)
  {
    TracePrintf(0, "PipeRead: ERROR - Woken up but pipe is still empty!\n");
    return ERROR;
  }

  // Determine how many bytes to read
  int bytes_to_read = (length < pipe->bytes_available) ? length : pipe->bytes_available;

  // Read data from the circular buffer
  char *dst = (char *)buffer;
  for (int i = 0; i < bytes_to_read; i++)
  {
    dst[i] = pipe->buffer[pipe->read_index];
    pipe->read_index = (pipe->read_index + 1) % PIPE_BUFFER_LEN;
  }

  TracePrintf(3, "PipeRead: Read %d bytes, indexes: read=%d, write=%d\n",
              bytes_to_read, pipe->read_index, pipe->write_index);
  pipe->bytes_available -= bytes_to_read;

  // Check if we can now satisfy any waiting writers
  while (pipe->write_queue->head != NULL)
  {
    write_request_t *request = pipe->write_queue->head;

    // Check if there's enough space for this write
    if (pipe->bytes_available + request->length <= PIPE_BUFFER_LEN)
    {
      // Remove from queue
      pipe->write_queue->head = request->next;
      if (pipe->write_queue->head == NULL)
      {
        pipe->write_queue->tail = NULL;
      }
      else
      {
        pipe->write_queue->head->prev = NULL;
      }
      pipe->write_queue->size--;

      // Perform the write
      char *src = (char *)request->buffer;
      for (int i = 0; i < request->length; i++)
      {
        pipe->buffer[pipe->write_index] = src[i];
        pipe->write_index = (pipe->write_index + 1) % PIPE_BUFFER_LEN;
      }

      pipe->bytes_available += request->length;
      TracePrintf(3, "PipeRead: Performed delayed write of %d bytes\n", request->length);

      // Wake up the writer
      pcb_t *writer = request->pcb;
      writer->state = PCB_STATE_READY;
      pcb_remove(blocked_processes, writer);
      pcb_enqueue(ready_processes, writer);

      TracePrintf(2, "PipeRead: Woke up process %d after writing to pipe %d\n", writer->pid, pipe_id);

      // Free the request
      free(request);
    }
    else
    {
      // Can't satisfy this request yet
      break;
    }
  }

  TracePrintf(2, "PipeRead: Successfully read %d bytes from pipe %d, %d bytes remaining\n",
              bytes_to_read, pipe_id, pipe->bytes_available);
  return bytes_to_read;
}

int PipeWrite(int pipe_id, void *buffer, int length)
{
  if (pipe_id <= 0 || buffer == NULL || length <= 0)
  {
    return ERROR;
  }

  pipe_t *pipe = FindPipe(pipe_id);
  if (pipe == NULL)
  {
    return ERROR;
  }

  // Calculate how much we can write immediately
  int space_available = PIPE_BUFFER_LEN - pipe->bytes_available;
  int bytes_to_write = (length <= space_available) ? length : space_available;

  // Debug print
  TracePrintf(2, "PipeWrite: Available space=%d, writing %d/%d bytes immediately\n",
              space_available, bytes_to_write, length);

  // Write what we can now
  char *src = (char *)buffer;
  for (int i = 0; i < bytes_to_write; i++)
  {
    pipe->buffer[pipe->write_index] = src[i];
    pipe->write_index = (pipe->write_index + 1) % PIPE_BUFFER_LEN;
  }

  pipe->bytes_available += bytes_to_write;

  // Wake up readers if we have data
  if (pipe->read_queue->head != NULL)
  {
    pcb_t *reader = pcb_dequeue(pipe->read_queue);
    reader->state = PCB_STATE_READY;
    pcb_remove(blocked_processes, reader);
    pcb_enqueue(ready_processes, reader);
    TracePrintf(2, "PipeWrite: Woke up reader process\n");
  }

  // If we wrote everything, we're done
  if (bytes_to_write == length)
  {
    return length;
  }

  // We need to wait to write the rest
  pcb_t *pcb = GetCurrentProcess();

  // Create a write request for the remaining bytes
  write_request_t *request = malloc(sizeof(write_request_t));
  if (request == NULL)
  {
    return bytes_to_write; // Return what we could write
  }

  // Make a copy of the remaining data to avoid pointer issues
  void *remaining_data = malloc(length - bytes_to_write);
  if (remaining_data == NULL)
  {
    free(request);
    return bytes_to_write;
  }
  memcpy(remaining_data, (char *)buffer + bytes_to_write, length - bytes_to_write);

  request->pcb = pcb;
  request->buffer = remaining_data; // Use our copy
  request->length = length - bytes_to_write;
  request->next = NULL;
  request->prev = NULL;

  // Add to write queue
  if (pipe->write_queue->head == NULL)
  {
    pipe->write_queue->head = request;
    pipe->write_queue->tail = request;
  }
  else
  {
    pipe->write_queue->tail->next = request;
    request->prev = pipe->write_queue->tail;
    pipe->write_queue->tail = request;
  }
  pipe->write_queue->size++;

  // Block until more space is available
  pcb->state = PCB_STATE_BLOCKED;
  pcb_enqueue(blocked_processes, pcb);

  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
  int rc = KernelContextSwitch(KCSwitch, pcb, next);
  if (rc == -1)
  {
    TracePrintf(0, "PipeWrite: KernelContextSwitch failed\n");
    Halt();
  }

  return length;
}

int ReclaimLockHelper(lock_t *lock, int id)
{
  while (lock != NULL)
  {
    if (lock->id == id)
    {
      break;
    }
    lock = lock->next;
  }

  if (lock == NULL)
  {
    TracePrintf(0, "Lock %d not found, cannot reclaim\n", id);
    return ERROR;
  }

  if (lock->is_locked)
  {
    TracePrintf(0, "Lock %d is locked, cannot reclaim\n", id);
    return ERROR;
  }

  if (lock->prev == NULL)
  {
    global_locks->head = lock->next;
  }
  else
  {
    lock->prev->next = lock->next;
  }

  if (lock->next == NULL)
  {
    global_locks->tail = lock->prev;
  }
  else
  {
    lock->next->prev = lock->prev;
  }

  free(lock->wait_queue);
  free(lock);
  global_locks->size--;

  return SUCCESS;
}

int ReclaimCondvarHelper(cond_t *condvar, int id)
{

  while (condvar != NULL)
  {
    if (condvar->id == id)
    {
      break;
    }
    condvar = condvar->next;
  }

  if (condvar == NULL)
  {
    TracePrintf(0, "Condition variable %d not found, cannot reclaim\n", id);
    return ERROR;
  }

  if (condvar->prev == NULL)
  {
    global_condvars->head = condvar->next;
  }
  else
  {
    condvar->prev->next = condvar->next;
  }

  if (condvar->next == NULL)
  {
    global_condvars->tail = condvar->prev;
  }
  else
  {
    condvar->next->prev = condvar->prev;
  }

  free(condvar->wait_queue);
  free(condvar);
  global_condvars->size--;

  return SUCCESS;
}

lock_t *FindLock(int id)
{
  lock_t *lock = global_locks->head;
  while (lock != NULL)
  {
    if (lock->id == id)
    {
      break;
    }
    lock = lock->next;
  }
  return lock;
}

cond_t *FindCondvar(int cvar_id)
{
  cond_t *condvar = global_condvars->head;
  while (condvar != NULL)
  {
    if (condvar->id == cvar_id)
    {
      break;
    }
    condvar = condvar->next;
  }
  return condvar;
}

pipe_t *FindPipe(int pipe_id)
{
  pipe_t *pipe = global_pipes->head;
  while (pipe != NULL)
  {
    if (pipe->id == pipe_id)
    {
      break;
    }
    pipe = pipe->next;
  }
  return pipe;
}

int ReclaimPipeHelper(pipe_t *pipe, int id)
{
  while (pipe != NULL)
  {
    if (pipe->id == id)
    {
      break;
    }
    pipe = pipe->next;
  }

  if (pipe == NULL)
  {
    TracePrintf(0, "Pipe %d not found, cannot reclaim\n", id);
    return ERROR;
  }

  // Free any waiting writers
  while (pipe->write_queue->head != NULL)
  {
    write_request_t *request = pipe->write_queue->head;
    pipe->write_queue->head = request->next;
    free(request);
  }
  free(pipe->write_queue);

  // Free read queue
  free(pipe->read_queue);

  // Remove from linked list
  if (pipe->prev == NULL)
  {
    global_pipes->head = pipe->next;
  }
  else
  {
    pipe->prev->next = pipe->next;
  }

  if (pipe->next == NULL)
  {
    global_pipes->tail = pipe->prev;
  }
  else
  {
    pipe->next->prev = pipe->prev;
  }

  free(pipe);
  global_pipes->size--;

  return SUCCESS;
}