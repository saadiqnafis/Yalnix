#ifndef SYSCALLS_H
#define SYSCALLS_H

/**
 * SysFork - Creates a new child process that is a copy of the current process
 *
 * Allocates resources for a new process, copies the memory and context from
 * the parent, and performs the necessary context switching.
 *
 * @param uctxt - Pointer to the parent's UserContext
 *
 * @return In the parent: PID of the new child process,
 *         In the child: 0,
 *         Will halt the system if context switch fails
 */
int SysFork(UserContext *uctxt);

/**
 * SysExec - Replaces current process with a new program
 *
 * Loads the specified program into the current process's address space,
 * replacing its code and data with the new program.
 *
 * @param filename - Path to the executable file
 * @param argvec - Array of argument strings for the new program
 *
 * @return SUCCESS on successful execution,
 *         ERROR if loading the program fails
 */
int SysExec(char *filename, char *argvec[]);

/**
 * SysExit - Terminates the current process
 *
 * Cleans up the process's resources, notifies the parent if it's waiting,
 * and switches to another process.
 *
 * @param status - Exit status code to be reported to the parent
 *
 * Note: If PID 1 (init) exits, the system halts
 */
void SysExit(int status);

/**
 * SysWait - Waits for a child process to terminate
 *
 * Blocks the calling process until a child process terminates.
 * If a child has already terminated, returns immediately.
 *
 * @param status_ptr - Pointer to store the child's exit status
 *
 * @return PID of the terminated child process on success,
 *         ERROR if the calling process has no children
 */
int SysWait(int *status_ptr);

/**
 * SysGetPid - Returns the process ID of the current process
 *
 * @return The PID of the calling process
 */
int SysGetPid(void);

/**
 * SysBrk - Changes the heap size for the current process
 *
 * Expands or contracts the heap by allocating or freeing
 * memory pages as needed.
 *
 * @param addr - New break address (end of heap)
 *
 * @return 0 on success,
 *         ERROR if addr is invalid (NULL, out of valid range),
 *         ERROR if memory allocation fails when expanding
 */
int SysBrk(void *addr);

/**
 * SysDelay - Blocks the current process for a specified time
 *
 * Puts the calling process to sleep for the specified number
 * of clock ticks, then returns.
 *
 * @param clock_ticks - Number of clock ticks to delay
 *
 * @return 0 on success, ERROR if clock_ticks is negative
 */
int SysDelay(int clock_ticks);

#endif // SYSCALLS_H
