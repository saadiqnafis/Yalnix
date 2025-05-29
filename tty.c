#include "tty.h"
#include "kernel.h"
#include "hardware.h"
#include "process.h"
#include "ylib.h"
#include "yalnix.h"

// Global array of TTY data structures
tty_data_t tty_data[NUM_TERMINALS];

void InitTTY(void)
{
  TracePrintf(1, "InitTTY: Initializing TTY subsystem\n");
  for (int i = 0; i < NUM_TERMINALS; i++)
  {
    tty_data[i].read_queue = pcb_queue_create();
    tty_data[i].write_queue = pcb_queue_create();
    tty_data[i].read_buffer = malloc(TERMINAL_MAX_LINE);
    if (tty_data[i].read_buffer == NULL)
    {
      TracePrintf(0, "InitTTY: Failed to allocate read buffer for terminal %d\n", i);
      Halt();
    }
    tty_data[i].read_buffer_len = 0;
    tty_data[i].write_buffer = NULL;
    tty_data[i].write_buffer_len = 0;
    tty_data[i].write_buffer_position = 0;
    tty_data[i].current_writer = NULL;
    tty_data[i].in_use = 0;
    TracePrintf(1, "InitTTY: Terminal %d initialized\n", i);
  }
  TracePrintf(1, "InitTTY: TTY subsystem initialized successfully\n");
}

void StartTtyWrite(int terminal, pcb_t *writer, void *buf, int len)
{
  TracePrintf(1, "StartTtyWrite: Terminal %d, Writer PID %d, Length %d\n",
              terminal, writer->pid, len);

  tty_data_t *tty = &tty_data[terminal];

  // Allocate and copy user buffer to kernel buffer
  tty->write_buffer = malloc(len);
  if (tty->write_buffer == NULL)
  {
    TracePrintf(0, "StartTtyWrite: Failed to allocate write buffer\n");
    writer->user_context.regs[0] = ERROR;
    writer->state = PCB_STATE_READY;
    pcb_enqueue(ready_processes, writer);
    return;
  }

  // Copy user buffer to kernel buffer
  memcpy(tty->write_buffer, buf, len);
  tty->write_buffer_len = len;
  tty->write_buffer_position = 0;
  tty->current_writer = writer;

  // Start the first transmit
  int to_write = (len > TERMINAL_MAX_LINE) ? TERMINAL_MAX_LINE : len;
  TracePrintf(1, "StartTtyWrite: Beginning first chunk of %d bytes\n", to_write);
  TtyTransmit(terminal, tty->write_buffer, to_write);
  tty->write_buffer_position = to_write;
}

int SysTtyRead(int tty_id, void *buf, int len)
{
  TracePrintf(1, "SysTtyRead: Terminal %d, Buffer %p, Length %d\n", tty_id, buf, len);

  if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0)
  {
    TracePrintf(0, "SysTtyRead: Invalid arguments\n");
    return ERROR;
  }

  tty_data_t *tty = &tty_data[tty_id];
  pcb_t *pcb = GetCurrentProcess();

  // If there's data available, return it immediately
  if (tty->read_buffer_len > 0)
  {
    int bytes_to_copy = (len < tty->read_buffer_len) ? len : tty->read_buffer_len;
    TracePrintf(1, "SysTtyRead: Data available - copying %d bytes immediately\n",
                bytes_to_copy);

    // First, make a copy in kernel space
    char *temp_buffer = malloc(bytes_to_copy);
    if (temp_buffer == NULL)
    {
      TracePrintf(0, "SysTtyRead: Failed to allocate temporary buffer\n");
      return ERROR;
    }

    memcpy(temp_buffer, tty->read_buffer, bytes_to_copy);
    pcb->kernel_read_buffer = temp_buffer;
    pcb->kernel_read_size = bytes_to_copy;

    // If we didn't consume all data, shift remaining data to beginning of buffer
    if (bytes_to_copy < tty->read_buffer_len)
    {
      TracePrintf(1, "SysTtyRead: Shifting remaining %d bytes in buffer\n",
                  tty->read_buffer_len - bytes_to_copy);

      memmove(tty->read_buffer,
              tty->read_buffer + bytes_to_copy,
              tty->read_buffer_len - bytes_to_copy);
    }

    tty->read_buffer_len -= bytes_to_copy;
    TracePrintf(1, "SysTtyRead: Returning %d bytes, %d bytes left in buffer\n",
                bytes_to_copy, tty->read_buffer_len);

    return bytes_to_copy;
  }

  // No data available, need to block
  TracePrintf(1, "SysTtyRead: No data available, blocking PID %d\n", pcb->pid);

  // Save the buffer and length in PCB for when we wake up
  pcb->tty_read_buf = buf;
  pcb->tty_read_len = len;

  // Add to read queue
  pcb_enqueue(tty->read_queue, pcb);

  // Block the process
  pcb->state = PCB_STATE_BLOCKED;
  pcb_enqueue(blocked_processes, pcb);

  // Switch to next process
  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
  TracePrintf(1, "SysTtyRead: Switching to process %d\n", next->pid);

  KernelContextSwitch(KCSwitch, pcb, next);

  // When we wake up, check if there was an error or how many bytes were read
  TracePrintf(1, "SysTtyRead: Process %d woken up\n", pcb->pid);

  // The return value should be stored in reg[0] by the trap handler
  return pcb->user_context.regs[0];
}

int SysTtyWrite(int tty_id, void *buf, int len)
{
  TracePrintf(1, "SysTtyWrite: Terminal %d, Buffer %p, Length %d\n", tty_id, buf, len);

  if (tty_id < 0 || tty_id >= NUM_TERMINALS || buf == NULL || len <= 0)
  {
    TracePrintf(0, "SysTtyWrite: Invalid arguments\n");
    return ERROR;
  }

  tty_data_t *tty = &tty_data[tty_id];
  pcb_t *pcb = GetCurrentProcess();

  // Save the buffer and length in PCB for use when dequeued
  pcb->tty_write_buf = buf;
  pcb->tty_write_len = len;

  // If terminal is free, start writing immediately
  if (!tty->in_use)
  {
    TracePrintf(1, "SysTtyWrite: Terminal %d is free, starting write\n", tty_id);
    tty->in_use = 1;
    StartTtyWrite(tty_id, pcb, buf, len);
  }
  else
  {
    // Terminal busy, add to queue
    TracePrintf(1, "SysTtyWrite: Terminal %d is busy, queueing PID %d\n",
                tty_id, pcb->pid);
    pcb_enqueue(tty->write_queue, pcb);
  }

  pcb->state = PCB_STATE_BLOCKED;
  pcb_enqueue(blocked_processes, pcb);

  // Switch to next process
  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;

  KernelContextSwitch(KCSwitch, pcb, next);

  // When we wake up, write is complete
  TracePrintf(1, "SysTtyWrite: Process %d woken up, write complete\n", pcb->pid);
  return len;
}
