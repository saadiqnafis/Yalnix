#ifndef KERNEL_H
#define KERNEL_H

#include <sys/types.h>
#include "hardware.h"
#include "synchronization.h"
#include "queue.h"

/*---------------------------------
 * Memory Frame Configuration
 *--------------------------------*/
#define NUM_FRAMES(pmem_size) (pmem_size / PAGESIZE)
#define BIT_VECTOR_SIZE(num_frames) ((num_frames + 7) / 8)

/*---------------------------------
 * Kernel Memory Layout Constants
 *--------------------------------*/
#define KERNEL_TEXT_MAX_PAGE (_first_kernel_data_page - 1)
#define KERNEL_HEAP_MAX_PAGE (KSTACK_START_PAGE - 1)
#define KSTACK_PAGES (KERNEL_STACK_MAXSIZE / PAGESIZE)
#define KSTACK_START_PAGE (KERNEL_STACK_BASE >> PAGESHIFT)
#define SCRATCH_ADDR (KERNEL_STACK_BASE - PAGESIZE) // Temp address to copy the kernel stack to during context switch

/*---------------------------------
 * Virtual Memory Regions
 *--------------------------------*/
#define VMEM_0_PAGES (VMEM_0_SIZE / PAGESIZE)
#define VPN_TO_REGION1_INDEX(vpn) ((vpn) - VMEM_0_PAGES)
#define NUM_PAGES_REGION1 (VMEM_1_SIZE / PAGESIZE)

typedef void (*trap_handler)(UserContext *);

// Memory management
/**
 * GetFrame - Allocates a physical frame from the frame bitmap
 *
 * Searches through the frame bitmap to find a free frame,
 * marks it as used, and returns the frame number.
 *
 * @return Frame number (≥ 0) on success, -1 if no free frames are available
 */
int GetFrame(void);

/**
 * FreeFrame - Marks a physical frame as free in the bitmap
 *
 * @param frame - The frame number to free
 */
void FreeFrame(int frame);

/**
 * AllocateFrame - Marks a specific physical frame as used in the bitmap
 *
 * @param frame - The frame number to mark as used
 */
void AllocateFrame(int frame);

/**
 * IsRegion1Address - Checks if an address is in region 1
 *
 * @param addr - The address to check
 *
 * @return 1 if the address is in region 1, 0 otherwise
 */
int IsRegion1Address(void *addr);

/**
 * IsAddressBelowStackAndAboveBreak - Checks if an address is below the stack and above the break
 *
 * @param addr - The address to check
 *
 * @return 1 if the address is below the stack and above the break, 0 otherwise
 */
int IsAddressBelowStackAndAboveBreak(void *addr);

/**
 * GrowStackToAddress - Grows the stack to an address
 *
 * @param addr - The address to grow the stack to
 *
 * @return SUCCESS if the stack was grown, ERROR otherwise
 */
int GrowStackToAddress(void *addr);

// Process management
/**
 * LoadProgram - Loads a program into a process's address space
 *
 * Loads an executable file into memory, sets up text, data, and stack regions,
 * and prepares arguments for the program.
 *
 * @param name - Path to the executable file
 * @param args - Command line arguments for the program
 * @param proc - Process control block for the process
 *
 * @return SUCCESS on successful load, ERROR if file can't be opened, isn't in proper format, or memory allocation issues, KILL if there's an error reading the file after memory is allocated
 */
int LoadProgram(char *name, char *args[], pcb_t *proc);

/**
 * SetKernelBrk - Sets the kernel's break value
 *
 * Adjusts the kernel heap size by changing the break address.
 * Can increase or decrease the heap size.
 *
 * @param addr - New break address
 *
 * @return 0 on success, ERROR if address is invalid (before original break or overlaps kernel stack), ERROR if out of physical memory when increasing heap
 */
int SetKernelBrk(void *addr);

/**
 * KernelStart - Initializes the kernel and starts the first process
 *
 * Initializes memory management, process queues, trap handlers, and
 * creates the idle and init processes. Sets up virtual memory and
 * loads the initial program.
 *
 * @param cmd_args - Command line arguments for the init process
 * @param pmem_size - Size of physical memory
 * @param uctxt - User context to be filled with the context of the first process
 *
 * Note: Halts the system on critical failures.
 */
void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt);

/**
 * KCSwitch - Kernel context switch function
 *
 * Switches execution from one process to another by saving current kernel context,
 * mapping new process's kernel stack, updating current process, and setting new page tables.
 *
 * @param kc_in - Current kernel context
 * @param curr_pcb_p - Pointer to current process control block
 * @param next_pcb_p - Pointer to next process control block
 *
 * @return Pointer to the kernel context of the next process
 */
KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p);

/**
 * KCCopy - Kernel context copy function for fork
 *
 * Copies the kernel context from parent to child process, including
 * kernel stack contents.
 *
 * @param kc_in - Current kernel context
 * @param new_pcb_p - Pointer to the new process control block
 * @param not_used - Unused parameter
 *
 * @return The original kernel context pointer
 */
KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used);

/**
 * InitializeKernelStack - Initializes the kernel stack
 *
 * @return Pointer to the initialized kernel stack
 *
 * Note: This function is used to initialize the kernel stack for the idle process and uses 1:1 mapping.
 */
pte_t *InitializeKernelStack(void);

/**
 * InitializeChildKernelStack - Initializes the kernel stack for a child process
 *
 * @return Pointer to the initialized kernel stack
 *
 * Note: This function is used to initialize the kernel stack for all processes except the idle process.
 *       Uses regular frame allocation due to VM being enabled.
 */
pte_t *InitializeChildKernelStack(void);

/**
 * MapScratch - Maps the scratch page to a frame, a temporary page used for copying the kernel stack during context switch
 *
 * @param frame - The frame number to map the scratch page to
 */
void MapScratch(int frame);

/**
 * UnmapScratch - Unmaps the scratch page
 */
void UnmapScratch(void);

/**
 * InitializeTrapTable - Initializes the trap interrupt table with the appropriate trap handlers
 */
void InitializeTrapTable(void);

#endif // KERNEL_H