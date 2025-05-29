#ifndef _TTY_H
#define _TTY_H

#include "hardware.h"
#include "process.h"

/**
 * TTY Terminal Data Structure
 *
 * Contains state information for each terminal device, including
 * read/write queues, buffers, and status information.
 */
typedef struct tty_data
{
  pcb_queue_t *read_queue;   // Queue of processes waiting to read
  pcb_queue_t *write_queue;  // Queue of processes waiting to write
  char *read_buffer;         // Buffer holding read data
  int read_buffer_len;       // Number of bytes in read buffer
  char *write_buffer;        // Buffer holding data to be written
  int write_buffer_len;      // Total length of write buffer
  int write_buffer_position; // Current position in write buffer
  pcb_t *current_writer;     // Process currently writing to terminal
  int in_use;                // Flag indicating if terminal is busy
} tty_data_t;

/**
 * InitTTY - Initialize the TTY subsystem
 *
 * Creates and initializes data structures for all terminals.
 * Allocates read buffers and creates read/write queues.
 *
 * Note: Halts the system if memory allocation fails
 */
void InitTTY(void);

/**
 * StartTtyWrite - Begin writing to a terminal
 *
 * Starts a terminal write operation by copying data from user space
 * to kernel space and initiating the first TtyTransmit operation.
 *
 * Parameters:
 *   terminal - Terminal ID (0 to NUM_TERMINALS-1)
 *   writer - Pointer to PCB of the process performing the write
 *   buf - Buffer containing data to write
 *   len - Number of bytes to write
 *
 * Note: On error, marks the writer process as ready and enqueues it
 *       with ERROR set as the return value
 */
void StartTtyWrite(int terminal, pcb_t *writer, void *buf, int len);

/**
 * SysTtyRead - System call to read from a terminal
 *
 * Reads up to 'len' bytes from the specified terminal into 'buf'.
 * If data is available in the read buffer, returns immediately.
 * Otherwise, blocks the calling process until data becomes available.
 *
 * Parameters:
 *   tty_id - Terminal ID (0 to NUM_TERMINALS-1)
 *   buf - Buffer to store read data
 *   len - Maximum number of bytes to read
 *
 * Returns:
 *   - Number of bytes read on success
 *   - ERROR if tty_id is invalid, buf is NULL, or len <= 0
 */
int SysTtyRead(int tty_id, void *buf, int len);

/**
 * SysTtyWrite - System call to write to a terminal
 *
 * Writes 'len' bytes from 'buf' to the specified terminal.
 * If the terminal is busy, blocks the calling process until
 * the terminal becomes available.
 *
 * Parameters:
 *   tty_id - Terminal ID (0 to NUM_TERMINALS-1)
 *   buf - Buffer containing data to write
 *   len - Number of bytes to write
 *
 * Returns:
 *   - Number of bytes written on success (always 'len' if successful)
 *   - ERROR if tty_id is invalid, buf is NULL, or len <= 0
 */
int SysTtyWrite(int tty_id, void *buf, int len);

// External global array of TTY data structures
extern tty_data_t tty_data[NUM_TERMINALS];

#endif /* _TTY_H */
