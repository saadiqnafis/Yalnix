#include "trap_handler.h"
#include "kernel.h"
#include "hardware.h"
#include "yuser.h"
#include "syscalls.h"
#include "yalnix.h"
#include "process.h"
#include "ykernel.h"
#include "synchronization.h"
#include "tty.h"

void TrapKernelHandler(UserContext *uctxt)
{
  int syscall_number = uctxt->code;

  switch (syscall_number)
  {
  case (YALNIX_FORK):
  {
    TracePrintf(0, "Yalnix Fork Syscall Handler\n");
    pcb_t *current_pcb = GetCurrentProcess();
    memcpy(&current_pcb->user_context, uctxt, sizeof(UserContext));

    int rc = SysFork(uctxt);
    current_pcb = GetCurrentProcess();

    memcpy(uctxt, &current_pcb->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);

    TracePrintf(0, "Fork returned %d\n", rc);
    break;
  }
  case (YALNIX_EXEC):
  {
    TracePrintf(0, "Yalnix Exec Syscall Handler\n");
    pcb_t *current_pcb = GetCurrentProcess();
    char *filename = (char *)uctxt->regs[0];
    char *argvec[GREGS];
    for (int i = 0; i < GREGS; i++)
    {
      argvec[i] = (char *)uctxt->regs[i];
      if (argvec[i] == NULL)
      {
        break;
      }
    }
    int rc = SysExec(filename, argvec);

    if (rc == SUCCESS)
    {
      // Only replace the context if exec succeeded
      memcpy(uctxt, &current_pcb->user_context, sizeof(UserContext));
    }
    else
    {
      // For failure, just set the return value but keep the original context
      uctxt->regs[0] = rc;
    }
    break;
  }
  case (YALNIX_WAIT):
  {
    TracePrintf(0, "Yalnix Wait Syscall Handler\n");
    pcb_t *current_pcb = GetCurrentProcess();
    memcpy(&current_pcb->user_context, uctxt, sizeof(UserContext));
    int *user_status = (int *)uctxt->regs[0];

    if (!IsRegion1Address((void *)user_status))
    {
      TracePrintf(0, "Invalid status pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = SysWait(user_status);
    memcpy(uctxt, &current_pcb->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    TracePrintf(0, "Wait returned %d\n", rc);
    break;
  }
  case (YALNIX_EXIT):
  {
    TracePrintf(0, "Yalnix Exit Syscall Handler\n");
    int status = uctxt->regs[0];
    SysExit(status);
    break;
  }
  case (YALNIX_GETPID):
  {
    TracePrintf(0, "Yalnix GetPID Syscall Handler\n");
    int pid = SysGetPid();
    uctxt->regs[0] = pid;
    break;
  }
  case (YALNIX_BRK):
  {
    TracePrintf(0, "Yalnix Brk Syscall Handler\n");
    void *addr = (void *)uctxt->regs[0];
    int rc = SysBrk(addr);
    if (rc == ERROR)
    {
      TracePrintf(0, "Brk failed\n");
    }
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_DELAY):
  {

    TracePrintf(0, "Yalnix Delay Syscall Handler\n");
    TracePrintf(0, "process %s delaying for %d ticks\n", GetCurrentProcess()->name, uctxt->regs[0]);
    int delay = uctxt->regs[0];
    pcb_t *current = GetCurrentProcess();
    memcpy(&current->user_context, uctxt, sizeof(UserContext));

    int rc = SysDelay(delay);
    if (rc == ERROR)
    {
      TracePrintf(0, "Delay failed\n");
    }

    memcpy(uctxt, &current->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_LOCK_INIT):
  {
    TracePrintf(0, "Yalnix Lock Init Syscall Handler\n");
    int *lock_id = (int *)uctxt->regs[0];

    if (!IsRegion1Address((void *)lock_id) || !IsRegion1Address((void *)lock_id + sizeof(int) - 1))
    {
      TracePrintf(0, "Invalid lock ID pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = LockInit(lock_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_LOCK_ACQUIRE):
  {
    TracePrintf(0, "Yalnix Lock Acquire Syscall Handler\n");
    int lock_id = uctxt->regs[0];
    int rc = Acquire(lock_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_LOCK_RELEASE):
  {
    TracePrintf(0, "Yalnix Lock Release Syscall Handler\n");
    int lock_id = uctxt->regs[0];
    int rc = Release(lock_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_RECLAIM):
  {
    TracePrintf(0, "Yalnix Reclaim Syscall Handler\n");
    int id = uctxt->regs[0];
    int rc = Reclaim(id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_CVAR_INIT):
  {
    TracePrintf(0, "Yalnix Cvar Init Syscall Handler\n");
    int *cvar_id = (int *)uctxt->regs[0];

    if (!IsRegion1Address((void *)cvar_id) || !IsRegion1Address((void *)cvar_id + sizeof(int) - 1))
    {
      TracePrintf(0, "Invalid cvar ID pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = CvarInit(cvar_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_CVAR_WAIT):
  {
    TracePrintf(0, "Yalnix Cvar Wait Syscall Handler\n");
    int cvar_id = uctxt->regs[0];
    int lock_id = uctxt->regs[1];
    pcb_t *current = GetCurrentProcess();
    memcpy(&current->user_context, uctxt, sizeof(UserContext));
    int rc = CvarWait(cvar_id, lock_id);
    memcpy(uctxt, &current->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_CVAR_SIGNAL):
  {
    TracePrintf(0, "Yalnix Cvar Signal Syscall Handler\n");
    int cvar_id = uctxt->regs[0];
    int rc = CvarSignal(cvar_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_CVAR_BROADCAST):
  {
    TracePrintf(0, "Yalnix Cvar Broadcast Syscall Handler\n");
    int cvar_id = uctxt->regs[0];
    int rc = CvarBroadcast(cvar_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_PIPE_INIT):
  {
    TracePrintf(0, "Yalnix Pipe Init Syscall Handler\n");
    int *pipe_id = (int *)uctxt->regs[0];

    if (!IsRegion1Address((void *)pipe_id) || !IsRegion1Address((void *)pipe_id + sizeof(int) - 1))
    {
      TracePrintf(0, "Invalid pipe ID pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = PipeInit(pipe_id);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_PIPE_READ):
  {
    TracePrintf(0, "Yalnix Pipe Read Syscall Handler\n");
    int pipe_id = uctxt->regs[0];
    void *buffer = (void *)uctxt->regs[1];
    int length = uctxt->regs[2];

    if (!IsRegion1Address((void *)buffer) || !IsRegion1Address((void *)buffer + length - 1))
    {
      TracePrintf(0, "Invalid buffer pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = PipeRead(pipe_id, buffer, length);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_PIPE_WRITE):
  {
    TracePrintf(0, "Yalnix Pipe Write Syscall Handler\n");
    int pipe_id = uctxt->regs[0];
    void *buffer = (void *)uctxt->regs[1];
    int length = uctxt->regs[2];

    if (!IsRegion1Address((void *)buffer) || !IsRegion1Address((void *)buffer + length - 1))
    {
      TracePrintf(0, "Invalid buffer pointer not in region 1\n");
      SysExit(ERROR);
    }

    int rc = PipeWrite(pipe_id, buffer, length);
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_TTY_READ):
  {
    TracePrintf(0, "Yalnix TTY Read Syscall Handler\n");
    int terminal = uctxt->regs[0];
    void *buffer = (void *)uctxt->regs[1];
    int length = uctxt->regs[2];

    if (!IsRegion1Address((void *)buffer) || !IsRegion1Address((void *)buffer + length - 1))
    {
      TracePrintf(0, "Invalid buffer pointer not in region 1\n");
      SysExit(ERROR);
    }

    pcb_t *current_pcb = GetCurrentProcess();
    memcpy(&current_pcb->user_context, uctxt, sizeof(UserContext));
    int rc = SysTtyRead(terminal, buffer, length);

    // If we have data in our kernel buffer, copy it to user space now
    if (current_pcb->kernel_read_buffer != NULL && rc > 0)
    {
      // Copy from kernel buffer to user buffer
      memcpy(buffer, current_pcb->kernel_read_buffer, current_pcb->kernel_read_size);

      // Free the temporary buffer
      free(current_pcb->kernel_read_buffer);
      current_pcb->kernel_read_buffer = NULL;
      current_pcb->kernel_read_size = 0;
    }

    memcpy(uctxt, &current_pcb->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    break;
  }
  case (YALNIX_TTY_WRITE):
  {
    TracePrintf(0, "Yalnix TTY Write Syscall Handler\n");
    int terminal = uctxt->regs[0];
    void *buffer = (void *)uctxt->regs[1];
    int length = uctxt->regs[2];

    if (!IsRegion1Address((void *)buffer) || !IsRegion1Address((void *)buffer + length - 1))
    {
      TracePrintf(0, "Invalid buffer pointer not in region 1\n");
      SysExit(ERROR);
    }

    pcb_t *current_pcb = GetCurrentProcess();
    memcpy(&current_pcb->user_context, uctxt, sizeof(UserContext));
    int rc = SysTtyWrite(terminal, buffer, length);
    memcpy(uctxt, &current_pcb->user_context, sizeof(UserContext));
    uctxt->regs[0] = rc;
    break;
  }
  }
}
void TrapClockHandler(UserContext *uctxt)
{
  UpdateDelayedPCB();
  pcb_t *current = GetCurrentProcess();
  memcpy(&current->user_context, uctxt, sizeof(UserContext));

  if (current->pid != idle_pcb->pid)
  {
    pcb_enqueue(ready_processes, current);
  }

  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;

  int rc = KernelContextSwitch(KCSwitch, current, next);

  memcpy(uctxt, &current->user_context, sizeof(UserContext));
}

void TrapIllegalHandler(UserContext *uctxt)
{
  int status = uctxt->regs[0];
  SysExit(status);
}

void TrapMemoryHandler(UserContext *uctxt)
{
  TracePrintf(0, "TrapMemoryHandler\n");
  TracePrintf(0, "The offending address is 0x%lx\n", uctxt->addr);
  TracePrintf(0, "The page is: %d\n", (unsigned int)uctxt->addr >> PAGESHIFT);

  // Check if this is a stack growth request
  if (IsRegion1Address((void *)uctxt->addr) &&
      IsAddressBelowStackAndAboveBreak((void *)uctxt->addr))
  {

    // This is a valid stack growth request
    TracePrintf(0, "Growing stack to address 0x%lx\n", uctxt->addr);
    if (GrowStackToAddress((void *)uctxt->addr) == ERROR)
    {
      // If we can't grow the stack, abort the process
      TracePrintf(0, "Failed to grow stack, aborting current process\n");
      SysExit(ERROR);
    }
  }
  else
  {
    // This is a genuine segmentation fault
    TracePrintf(0, "Segmentation fault, aborting current process\n");
    SysExit(ERROR);
  }
}

void TrapMathHandler(UserContext *uctxt)
{
  SysExit(ERROR);
}

void TrapTtyReceiveHandler(UserContext *uctxt)
{
  int terminal = uctxt->code;
  tty_data_t *tty = &tty_data[terminal];

  // Get the received line
  int len = TtyReceive(terminal, tty->read_buffer + tty->read_buffer_len,
                       TERMINAL_MAX_LINE - tty->read_buffer_len);

  tty->read_buffer_len += len;

  // If there's a process waiting to read, wake it up
  if (tty->read_queue->head != NULL)
  {
    pcb_t *reader = pcb_dequeue(tty->read_queue);

    int bytes_to_copy = (reader->tty_read_len < tty->read_buffer_len) ? reader->tty_read_len : tty->read_buffer_len;

    TracePrintf(0, "TrapTtyReceiveHandler: Copying %d bytes to process %d\n",
                bytes_to_copy, reader->pid);

    // Create a temporary buffer to store the data
    char *temp_buffer = malloc(bytes_to_copy);
    if (temp_buffer != NULL)
    {
      memcpy(temp_buffer, tty->read_buffer, bytes_to_copy);
      reader->kernel_read_buffer = temp_buffer;
      reader->kernel_read_size = bytes_to_copy;
    }
    else
    {
      // Handle allocation failure
      bytes_to_copy = 0;
    }

    // Set return value to number of bytes copied
    reader->user_context.regs[0] = bytes_to_copy;

    // Shift remaining data in the kernel buffer
    if (bytes_to_copy < tty->read_buffer_len)
    {
      memmove(tty->read_buffer,
              tty->read_buffer + bytes_to_copy,
              tty->read_buffer_len - bytes_to_copy);
      tty->read_buffer_len -= bytes_to_copy;
    }
    else
    {
      tty->read_buffer_len = 0;
    }

    reader->state = PCB_STATE_READY;
    pcb_remove(blocked_processes, reader);
    pcb_enqueue(ready_processes, reader);
  }

  TracePrintf(0, "TrapTtyReceiveHandler: After processing, buffer has %d bytes left\n",
              tty->read_buffer_len);

  if (tty->read_buffer_len > 0)
  {
    TracePrintf(0, "TrapTtyReceiveHandler: Remaining data: '");
    for (int i = 0; i < tty->read_buffer_len; i++)
    {
      TracePrintf(0, "%c", tty->read_buffer[i]);
    }
    TracePrintf(0, "'\n");
  }
}

void TrapTtyTransmitHandler(UserContext *uctxt)
{
  int terminal = uctxt->code;
  tty_data_t *tty = &tty_data[terminal];

  TracePrintf(1, "TrapTtyTransmitHandler: Terminal %d transmit complete\n", terminal);

  // If there's more data to write for current writer
  if (tty->write_buffer_position < tty->write_buffer_len)
  {
    // Calculate how much to write next
    int remaining = tty->write_buffer_len - tty->write_buffer_position;
    int to_write = (remaining > TERMINAL_MAX_LINE) ? TERMINAL_MAX_LINE : remaining;

    // Start next write
    TtyTransmit(terminal,
                tty->write_buffer + tty->write_buffer_position,
                to_write);

    tty->write_buffer_position += to_write;
  }
  else
  {
    // Writing is complete, clean up
    TracePrintf(1, "TrapTtyTransmitHandler: Writing complete for terminal %d\n", terminal);

    free(tty->write_buffer);
    tty->write_buffer = NULL;

    // Wake up the writer
    pcb_t *writer = tty->current_writer;
    tty->current_writer = NULL;

    if (writer != NULL)
    {
      TracePrintf(1, "TrapTtyTransmitHandler: Waking writer PID %d\n", writer->pid);

      // Make sure we set the return value correctly before waking
      writer->user_context.regs[0] = writer->tty_write_len;

      writer->state = PCB_STATE_READY;
      pcb_remove(blocked_processes, writer);
      pcb_enqueue(ready_processes, writer);
    }
    else
    {
      TracePrintf(0, "TrapTtyTransmitHandler: Error - No current writer\n");
    }

    // If there are more writers waiting, start the next one
    if (tty->write_queue->head != NULL)
    {
      pcb_t *next_writer = pcb_dequeue(tty->write_queue);
      tty->in_use = 1;

      // Get write request data
      void *buf = (void *)next_writer->user_context.regs[1];
      int len = next_writer->user_context.regs[2];

      // Start a new write operation
      StartTtyWrite(terminal, next_writer, buf, len);
    }
    else
    {
      tty->in_use = 0;
    }
  }
}

void TrapDiskHandler(UserContext *uctxt)
{
  TracePrintf(0, "TrapDiskHandler will be implemented in the future\n");
  SysExit(ERROR);
}

void TrapNotHandled(UserContext *uctxt)
{
  TracePrintf(0, "TrapNotHandled\n");
  SysExit(ERROR);
}
