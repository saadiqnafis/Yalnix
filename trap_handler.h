#ifndef TRAP_HANDLER_H
#define TRAP_HANDLER_H

#include "hardware.h"

/**
 * TrapKernelHandler - Handles kernel trap (system call) requests
 *
 * Examines the system call code in the UserContext and dispatches to the
 * appropriate system call implementation. Updates the UserContext with
 * return values from the system call for return to user mode.
 *
 * @param uctxt - Pointer to the user context at the time of the trap
 */
void TrapKernelHandler(UserContext *uctxt);

/**
 * TrapClockHandler - Handles clock/timer interrupts
 *
 * Updates delayed processes, saves the current process's context, and
 * performs a context switch to give another process a chance to run.
 * If no other process is ready, switches to the idle process.
 *
 * @param uctxt - Pointer to the user context at the time of the interrupt
 */
void TrapClockHandler(UserContext *uctxt);

/**
 * TrapIllegalHandler - Handles illegal instruction traps
 *
 * Terminates the current process with the status code stored
 * in register 0 of the user context.
 *
 * @param uctxt - Pointer to the user context at the time of the trap
 */
void TrapIllegalHandler(UserContext *uctxt);

/**
 * TrapMemoryHandler - Handles memory access violations
 *
 * Prints diagnostic information about the memory access violation
 * (segmentation fault) and halts the system.
 *
 * @param uctxt - Pointer to the user context at the time of the trap
 *
 * Note: This handler does not return - it halts the system
 */
void TrapMemoryHandler(UserContext *uctxt);

/**
 * TrapMathHandler - Handles math exceptions (e.g., divide by zero)
 *
 * Terminates the current process with an ERROR status.
 *
 * @param uctxt - Pointer to the user context at the time of the trap
 */
void TrapMathHandler(UserContext *uctxt);

/**
 * TrapTtyReceiveHandler - Handles TTY receive interrupts
 *
 * Processes data received from a terminal, copies it to the kernel buffer,
 * and wakes up any process waiting to read from that terminal. If no
 * process is waiting, the data remains in the buffer for future reads.
 *
 * @param uctxt - Pointer to the user context at the time of the interrupt
 *           (contains the terminal ID in the code field)
 */
void TrapTtyReceiveHandler(UserContext *uctxt);

/**
 * TrapTtyTransmitHandler - Handles TTY transmit complete interrupts
 *
 * Continues transmission of remaining data if a write operation is not
 * complete, or wakesup the writer process and starts the next queued
 * write operation if available.
 *
 * @param uctxt - Pointer to the user context at the time of the interrupt
 *           (contains the terminal ID in the code field)
 */
void TrapTtyTransmitHandler(UserContext *uctxt);

/**
 * TrapDiskHandler - Handles disk interrupts
 *
 * Processes disk operation completion interrupts.
 *
 * @param uctxt - Pointer to the user context at the time of the interrupt
 *
 * Note: Currently not implemented, will do so in the future
 */
void TrapDiskHandler(UserContext *uctxt);

/**
 * TrapNotHandled - Default handler for unhandled trap types
 *
 * Placeholder handler for trap types that don't have specific handlers.
 *
 * @param uctxt - Pointer to the user context at the time of the trap
 */
void TrapNotHandled(UserContext *uctxt);

#endif // TRAP_HANDLER_H