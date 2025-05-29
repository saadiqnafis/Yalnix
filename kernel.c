#include "kernel.h"
#include "ykernel.h"
#include "syscalls.h"
#include "yalnix.h"
#include "hardware.h"
#include "trap_handler.h"
#include "unistd.h"
#include "load_info.h"
#include "queue.h"
#include "process.h"
#include "load_info.h"
#include <fcntl.h>
#include "unistd.h"
#include "synchronization.h"
#include "tty.h"

/*---------------------------------
 * Memory Management Variables
 *--------------------------------*/
static unsigned char *frame_bitmap;
static pte_t *page_table_region0;
static trap_handler trap_table[TRAP_VECTOR_SIZE];
static int bit_vector_size;
static int current_kernel_brk_page;
static int is_vm_enabled = 0;
static int switch_flag = 0;

void DoIdle()
{
  while (1)
  {
    TracePrintf(0, "Idle\n");
    Pause();
  }
}

int GetFrame()
{
  for (int i = 0; i < bit_vector_size; i++)
  {
    // Check each of the 8 bits in the i-th byte.
    for (int j = 0; j < 8; j++)
    {
      // Check if the j-th bit is free.
      if ((frame_bitmap[i] & (1 << j)) == 0)
      {
        // Mark the bit as used.
        frame_bitmap[i] |= (1 << j);
        // Return the corresponding frame number.
        TracePrintf(0, "Getting free frame %d\n", i * 8 + j);
        return i * 8 + j;
      }
    }
  }
  return -1;
}

void FreeFrame(int frame)
{
  int byte = frame / 8;
  int bit = frame % 8;
  frame_bitmap[byte] &= ~(1 << bit);
  TracePrintf(0, "Freeing frame %d\n", frame);
}

void AllocateFrame(int frame)
{
  int byte = frame / 8;
  int bit = frame % 8;
  frame_bitmap[byte] |= (1 << bit);
}

int IsRegion1Address(void *addr)
{
  // Region 1 starts at VMEM_1_BASE and extends to VMEM_1_LIMIT
  return ((unsigned long)addr >= VMEM_1_BASE &&
          (unsigned long)addr < VMEM_1_LIMIT);
}

int IsAddressBelowStackAndAboveBreak(void *addr)
{
  pcb_t *current_pcb = GetCurrentProcess();

  // Calculate the lowest address that's currently part of the stack
  int lowest_stack_page = -1;
  for (int i = NUM_PAGES_REGION1 - 1; i >= 0; i--)
  {
    if (!current_pcb->page_table[i].valid)
    {
      break;
    }
    lowest_stack_page = i;
  }

  unsigned int stack_bottom = VMEM_1_BASE + (lowest_stack_page << PAGESHIFT);
  // Check if the address is below the stack bottom but above brk
  return ((unsigned long)addr < stack_bottom &&
          (unsigned long)addr > (unsigned long)current_pcb->brk);
}

int GrowStackToAddress(void *addr)
{
  pcb_t *current_pcb = GetCurrentProcess();

  // Calculate the page index for the target address (in Region 1)
  unsigned int addr_val = (unsigned long)addr;
  int target_page = (addr_val - VMEM_1_BASE) >> PAGESHIFT;

  // Find the lowest page of the current stack
  int lowest_stack_page = -1;
  for (int i = NUM_PAGES_REGION1 - 1; i >= 0; i--)
  {
    if (!current_pcb->page_table[i].valid)
    {
      break;
    }
    lowest_stack_page = i;
  }

  if (lowest_stack_page == -1)
  {
    // No stack found - should never happen
    return ERROR;
  }

  // Allocate pages from target_page up to lowest_stack_page-1
  for (int i = target_page; i < lowest_stack_page; i++)
  {
    int frame = GetFrame();
    if (frame == -1)
    {
      // Out of physical memory
      TracePrintf(0, "GrowStackToAddress: Out of physical memory\n");
      return ERROR;
    }

    // Map the new frame to the stack page
    current_pcb->page_table[i].valid = 1;
    current_pcb->page_table[i].pfn = frame;
    current_pcb->page_table[i].prot = PROT_READ | PROT_WRITE;

    // Zero out the new page for security
    MapScratch(frame);
    memset((void *)SCRATCH_ADDR, 0, PAGESIZE);
    UnmapScratch();

    // Flush TLB for this address
    WriteRegister(REG_TLB_FLUSH, VMEM_1_BASE + (i << PAGESHIFT));

    TracePrintf(0, "GrowStackToAddress: Allocated page %d (frame %d) for stack growth\n",
                i, frame);
  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_0);
  return SUCCESS;
}

pte_t *InitializeKernelStack()
{
  pte_t *kernel_stack = (pte_t *)malloc(KSTACK_PAGES * sizeof(pte_t));
  if (kernel_stack == NULL)
  {
    TracePrintf(0, "Failed to allocate kernel stack\n");
    Halt();
  }

  for (int j = 0; j < KSTACK_PAGES; j++)
  {
    int vpage = KSTACK_START_PAGE + j;
    kernel_stack[j].valid = 1;
    kernel_stack[j].pfn = vpage;
    kernel_stack[j].prot = PROT_READ | PROT_WRITE;

    // Also mark this frame as used in the frame_bitmap:
    AllocateFrame(vpage);
  }

  return kernel_stack;
}

pte_t *InitializeChildKernelStack()
{
  pte_t *kernel_stack = (pte_t *)malloc(KSTACK_PAGES * sizeof(pte_t));
  if (kernel_stack == NULL)
  {
    TracePrintf(0, "Failed to allocate kernel stack\n");
    Halt();
  }

  for (int i = 0; i < KSTACK_PAGES; i++)
  {
    int frame = GetFrame();
    if (frame == -1)
    {
      TracePrintf(0, "Failed to allocate frame\n");
      Halt();
    }
    kernel_stack[i].valid = 1;
    kernel_stack[i].pfn = frame;
    kernel_stack[i].prot = PROT_READ | PROT_WRITE;
    AllocateFrame(frame);
  }

  return kernel_stack;
}

void MapScratch(int frame)
{
  int scratch_vpn = SCRATCH_ADDR >> PAGESHIFT;
  page_table_region0[scratch_vpn].valid = 1;
  page_table_region0[scratch_vpn].pfn = frame;
  page_table_region0[scratch_vpn].prot = PROT_READ | PROT_WRITE;
  WriteRegister(REG_TLB_FLUSH, SCRATCH_ADDR);
}

void UnmapScratch()
{
  int scratch_vpn = SCRATCH_ADDR >> PAGESHIFT;
  page_table_region0[scratch_vpn].valid = 0;
}

void InitializeTrapTable()
{
  for (int i = 0; i < TRAP_VECTOR_SIZE; i++)
  {
    trap_table[i] = TrapNotHandled;
  }
  trap_table[TRAP_KERNEL] = TrapKernelHandler;
  trap_table[TRAP_CLOCK] = TrapClockHandler;
  trap_table[TRAP_MEMORY] = TrapMemoryHandler;
  trap_table[TRAP_ILLEGAL] = TrapIllegalHandler;
  trap_table[TRAP_MATH] = TrapMathHandler;
  trap_table[TRAP_TTY_RECEIVE] = TrapTtyReceiveHandler;
  trap_table[TRAP_TTY_TRANSMIT] = TrapTtyTransmitHandler;
  trap_table[TRAP_DISK] = TrapDiskHandler;
}

void KernelStart(char *cmd_args[], unsigned int pmem_size, UserContext *uctxt)
{
  TracePrintf(0, "KernelStart\n");
  current_kernel_brk_page = _orig_kernel_brk_page;
  int num_frames = NUM_FRAMES(pmem_size);
  bit_vector_size = BIT_VECTOR_SIZE(num_frames);
  frame_bitmap = (unsigned char *)calloc(bit_vector_size, sizeof(unsigned char));

  InitializeProcessQueues();
  InitSyncLists();
  InitTTY();

  if (frame_bitmap == NULL)
  {
    TracePrintf(0, "Failed to allocate frame bitmap\n");
    Halt();
  }

  /*---------------------------------
   * Initialize region 0 page table
   *--------------------------------*/
  page_table_region0 = (pte_t *)calloc(VMEM_0_PAGES, sizeof(pte_t));
  if (page_table_region0 == NULL)
  {
    TracePrintf(0, "Failed to allocate page table\n");
    Halt();
  }

  for (int i = 0; i < VMEM_0_PAGES; i++)
  {
    if (i < _orig_kernel_brk_page)
    {
      page_table_region0[i].valid = 1;
      page_table_region0[i].pfn = i;
      // Mark frame as used in bitmap
      AllocateFrame(i);
      if (i < _first_kernel_data_page) // Kernel text region
      {
        page_table_region0[i].prot = PROT_READ | PROT_EXEC;
      }
      else // Kernel data/heap region
      {
        page_table_region0[i].prot = PROT_READ | PROT_WRITE;
      }
    }
  }

  /*-------------------------*
   * Initialize kernel stack *
   *-------------------------*/
  for (int i = KSTACK_START_PAGE; i < KSTACK_START_PAGE + KSTACK_PAGES; i++)
  {
    page_table_region0[i].valid = 1;
    page_table_region0[i].pfn = i;
    page_table_region0[i].prot = PROT_READ | PROT_WRITE;
  }

  WriteRegister(REG_PTBR0, (unsigned int)page_table_region0);
  WriteRegister(REG_PTLR0, VMEM_0_PAGES);

  InitializeTrapTable();
  WriteRegister(REG_VECTOR_BASE, (unsigned int)trap_table);

  /*--------------------------------*
   * Initialize idle pcb *
   *--------------------------------*/
  idle_pcb = CreatePCB("idle");
  if (idle_pcb == NULL)
  {
    TracePrintf(0, "Failed to allocate idle pcb\n");
    Halt();
  }
  /*--------------------------------*
   * Initialize region 1 page table *
   *--------------------------------*/

  for (int i = 0; i < NUM_PAGES_REGION1; i++)
  {
    idle_pcb->page_table[i].valid = 0;
  }

  // Initialize idle stack page in region 1
  int idle_stack_page_num = VPN_TO_REGION1_INDEX(MAX_VPN);
  int frame = GetFrame();

  idle_pcb->page_table[idle_stack_page_num].valid = 1;
  idle_pcb->page_table[idle_stack_page_num].pfn = frame;
  idle_pcb->page_table[idle_stack_page_num].prot = PROT_READ | PROT_WRITE;

  WriteRegister(REG_PTBR1, (unsigned int)idle_pcb->page_table);
  WriteRegister(REG_PTLR1, NUM_PAGES_REGION1);

  idle_pcb->kernel_stack = InitializeKernelStack();
  memcpy(&idle_pcb->user_context, uctxt, sizeof(UserContext));
  idle_pcb->user_context.pc = DoIdle;
  idle_pcb->user_context.sp = (void *)(VMEM_1_LIMIT - 4);

  // Determine the name of the initial program to load
  char *name = (cmd_args != NULL && cmd_args[0] != NULL) ? cmd_args[0] : "init";
  TracePrintf(0, "Creating init pcb with name %s\n", name);

  pcb_t *init_pcb = CreatePCB(name);
  if (init_pcb == NULL)
  {
    TracePrintf(0, "Failed to allocate init pcb\n");
    Halt();
  }

  init_pcb->kernel_stack = InitializeChildKernelStack();
  memcpy(&init_pcb->user_context, uctxt, sizeof(UserContext));

  WriteRegister(REG_VM_ENABLE, 1);
  is_vm_enabled = 1;

  // Load the initial program into init_pcb
  WriteRegister(REG_PTBR1, (unsigned int)init_pcb->page_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  if (LoadProgram(name, cmd_args, init_pcb) != SUCCESS)
  {
    TracePrintf(0, "LoadProgram failed for init\n");
    Halt();
  }

  WriteRegister(REG_PTBR1, (unsigned int)idle_pcb->page_table);
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
  TracePrintf(0, "About to clone idle into init\n");
  int rc = KernelContextSwitch(KCCopy, (void *)init_pcb, NULL);
  if (rc == -1)
  {
    TracePrintf(0, "KernelContextSwitch failed when copying idle into init\n");
    Halt();
  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  /*
   * If switch_flag is 0, we are switching from idle, and will enqueue init_pcb and start running idle
   * Otherwise, we are switching from idle to init, and set page table to init_pcb's page table
   */
  if (switch_flag == 0)
  {
    pcb_enqueue(ready_processes, init_pcb);
    switch_flag = 1;
    memcpy(uctxt, &idle_pcb->user_context, sizeof(UserContext));
    SetCurrentProcess(idle_pcb);
  }
  else
  {
    WriteRegister(REG_PTBR1, (unsigned int)init_pcb->page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);
    memcpy(uctxt, &init_pcb->user_context, sizeof(UserContext));
    SetCurrentProcess(init_pcb);
  }

  TracePrintf(0, "Exiting KernelStart\n");
}

KernelContext *KCSwitch(KernelContext *kc_in, void *curr_pcb_p, void *next_pcb_p)
{
  pcb_t *curr_pcb = (pcb_t *)curr_pcb_p;
  pcb_t *next_pcb = (pcb_t *)next_pcb_p;

  // 1. Save current kernel context
  memcpy(&curr_pcb->kernel_context, kc_in, sizeof(KernelContext));

  // 2. Map next process's kernel stack
  for (int i = 0; i < KSTACK_PAGES; i++)
  {
    int vpage = KSTACK_START_PAGE + i;
    page_table_region0[vpage] = next_pcb->kernel_stack[i];
  }

  // 3. Set new current process
  SetCurrentProcess(next_pcb);

  // 4. Set the page table register for the next process
  WriteRegister(REG_PTBR1, (unsigned int)next_pcb->page_table);

  // 5. Flush the TLB (both kernel stack and user space)
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  return &next_pcb->kernel_context;
}

KernelContext *KCCopy(KernelContext *kc_in, void *new_pcb_p, void *not_used)
{
  pcb_t *new_pcb = (pcb_t *)new_pcb_p;

  memcpy(&new_pcb->kernel_context, kc_in, sizeof(KernelContext));

  if (new_pcb->kernel_stack == NULL)
  {
    new_pcb->kernel_stack = InitializeChildKernelStack();
  }
  // Copy the kernel stack from the parent to the child
  pte_t *kernel_stack = new_pcb->kernel_stack;
  for (int i = 0; i < KSTACK_PAGES; i++)
  {
    unsigned int parent_addr = (KSTACK_START_PAGE + i) << PAGESHIFT; // Get the address of the current kernel stack page
    int frame = kernel_stack[i].pfn;                                 // Get the frame number of the new process kernel stack page
    MapScratch(frame);                                               // Temporary map the frame to the scratch address
    memcpy((void *)(SCRATCH_ADDR), (void *)parent_addr, PAGESIZE);   // Copy the parent address to the scratch address
    UnmapScratch();                                                  // Unmap the scratch address
    kernel_stack[i].valid = 1;
    kernel_stack[i].prot = PROT_READ | PROT_WRITE;
  }

  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_KSTACK);

  return kc_in;
}

int SetKernelBrk(void *addr)
{
  unsigned int new_brk_page = (unsigned int)addr >> PAGESHIFT;

  // Always check lower bound
  if (new_brk_page < _orig_kernel_brk_page)
  {
    TracePrintf(0, "SetKernelBrk: Can't lower brk below original\n");
    return ERROR;
  }

  if (!is_vm_enabled)
  {
    // Pre-VM: Just track the brk raise
    int brk_raise = new_brk_page - _orig_kernel_brk_page;
    TracePrintf(1, "Pre-VM brk tracking: %d pages\n", brk_raise);
    return 0;
  }
  else
  {
    // Post-VM: Actually map pages in region 0
    if (new_brk_page > KERNEL_HEAP_MAX_PAGE)
    {
      TracePrintf(0, "SetKernelBrk: Would overlap kernel stack\n");
      return ERROR;
    }

    if (new_brk_page <= current_kernel_brk_page)
    {
      TracePrintf(0, "Lowering kernel brk to page %d\n", new_brk_page);
      // Handle brk lowering - free frames
      for (int i = new_brk_page; i < current_kernel_brk_page; i++)
      {
        int frame = page_table_region0[i].pfn;
        FreeFrame(frame);
        page_table_region0[i].valid = 0;
      }
    }
    else
    {
      // Handle brk raising - allocate frames
      for (int i = current_kernel_brk_page; i < new_brk_page; i++)
      {
        if (i > KERNEL_HEAP_MAX_PAGE)
        {
          TracePrintf(0, "SetKernelBrk: Reached stack boundary\n");
          return ERROR;
        }

        int frame = GetFrame();
        if (frame == -1)
        {
          TracePrintf(0, "SetKernelBrk: Out of physical memory\n");
          return ERROR;
        }
        TracePrintf(0, "Raising kernel brk to page %d\n", frame);

        // Map the new frame to the new brk page
        page_table_region0[i].valid = 1;
        page_table_region0[i].pfn = frame;
        page_table_region0[i].prot = PROT_READ | PROT_WRITE;
      }
    }

    current_kernel_brk_page = new_brk_page;
    TracePrintf(0, "SetKernelBrk: New brk page is %d\n", current_kernel_brk_page);
    return 0;
  }
}

int LoadProgram(char *name, char *args[], pcb_t *proc)

{
  int fd;
  int (*entry)();
  struct load_info li;
  int i;
  char *cp;
  char **cpp;
  char *cp2;
  int argcount;
  int size;
  int text_pg1;
  int data_pg1;
  int data_npg;
  int stack_npg;
  long segment_size;
  char *argbuf;

  /*
   * Open the executable file
   */
  if ((fd = open(name, O_RDONLY)) < 0)
  {
    TracePrintf(0, "LoadProgram: can't open file '%s'\n", name);
    return ERROR;
  }

  if (LoadInfo(fd, &li) != LI_NO_ERROR)
  {
    TracePrintf(0, "LoadProgram: '%s' not in Yalnix format\n", name);
    close(fd);
    return (-1);
  }

  if (li.entry < VMEM_1_BASE)
  {
    TracePrintf(0, "LoadProgram: '%s' not linked for Yalnix\n", name);
    close(fd);
    return ERROR;
  }

  /*
   * Figure out in what region 1 page the different program sections
   * start and end
   */
  text_pg1 = (li.t_vaddr - VMEM_1_BASE) >> PAGESHIFT;
  data_pg1 = (li.id_vaddr - VMEM_1_BASE) >> PAGESHIFT;
  data_npg = li.id_npg + li.ud_npg;
  /*
   *  Figure out how many bytes are needed to hold the arguments on
   *  the new stack that we are building.  Also count the number of
   *  arguments, to become the argc that the new "main" gets called with.
   */
  size = 0;
  for (i = 0; args[i] != NULL; i++)
  {
    TracePrintf(3, "counting arg %d = '%s'\n", i, args[i]);
    size += strlen(args[i]) + 1;
  }
  argcount = i;

  TracePrintf(2, "LoadProgram: argsize %d, argcount %d\n", size, argcount);

  /*
   *  The arguments will get copied starting at "cp", and the argv
   *  pointers to the arguments (and the argc value) will get built
   *  starting at "cpp".  The value for "cpp" is computed by subtracting
   *  off space for the number of arguments (plus 3, for the argc value,
   *  a NULL pointer terminating the argv pointers, and a NULL pointer
   *  terminating the envp pointers) times the size of each,
   *  and then rounding the value *down* to a double-word boundary.
   */
  cp = ((char *)VMEM_1_LIMIT) - size;

  cpp = (char **)(((int)cp -
                   ((argcount + 3 + POST_ARGV_NULL_SPACE) * sizeof(void *))) &
                  ~7);

  /*
   * Compute the new stack pointer, leaving INITIAL_STACK_FRAME_SIZE bytes
   * reserved above the stack pointer, before the arguments.
   */
  cp2 = (caddr_t)cpp - INITIAL_STACK_FRAME_SIZE;

  TracePrintf(1, "prog_size %d, text %d data %d bss %d pages\n",
              li.t_npg + data_npg, li.t_npg, li.id_npg, li.ud_npg);

  /*
   * Compute how many pages we need for the stack */
  stack_npg = (VMEM_1_LIMIT - DOWN_TO_PAGE(cp2)) >> PAGESHIFT;

  TracePrintf(1, "LoadProgram: heap_size %d, stack_size %d\n",
              li.t_npg + data_npg, stack_npg);

  /* leave at least one page between heap and stack */
  if (stack_npg + data_pg1 + data_npg >= MAX_PT_LEN)
  {
    close(fd);
    return ERROR;
  }

  /*
   * This completes all the checks before we proceed to actually load
   * the new program.  From this point on, we are committed to either
   * loading succesfully or killing the process.
   */

  /*
   * Set the new stack pointer value in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.sp = cp2;
   */
  proc->user_context.sp = cp2;
  /*
   * Now save the arguments in a separate buffer in region 0, since
   * we are about to blow away all of region 1.
   */
  cp2 = argbuf = (char *)malloc(size);

  /*
   * ==>> You should perhaps check that malloc returned valid space
   */

  if (argbuf == NULL)
  {
    TracePrintf(0, "Failed to allocate idle pcb\n");
    Halt();
  }

  for (i = 0; args[i] != NULL; i++)
  {
    TracePrintf(3, "saving arg %d = '%s'\n", i, args[i]);
    strcpy(cp2, args[i]);
    cp2 += strlen(cp2) + 1;
  }

  /*
   * Set up the page tables for the process so that we can read the
   * program into memory.  Get the right number of physical pages
   * allocated, and set them all to writable.
   */

  /* ==>> Throw away the old region 1 virtual address space by
   * ==>> curent process by walking through the R1 page table and,
   * ==>> for every valid page, free the pfn and mark the page invalid.
   */

  // Walk through R1 page table and free valid pages
  for (int i = 0; i < MAX_PT_LEN; i++)
  {
    if (proc->page_table[i].valid)
    {
      // Free the physical frame
      int pfn = proc->page_table[i].pfn;
      FreeFrame(pfn);

      // Mark page as invalid
      proc->page_table[i].valid = 0;
    }
  }

  /*
   * ==>> Then, build up the new region1.
   * ==>> (See the LoadProgram diagram in the manual.)
   */

  /*
   * ==>> First, text. Allocate "li.t_npg" physical pages and map them starting at
   * ==>> the "text_pg1" page in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */

  // Allocate and map text pages
  for (int i = text_pg1; i < text_pg1 + li.t_npg; i++)
  {
    int frame = GetFrame();
    if (frame < 0)
    {
      for (int i = text_pg1; i < text_pg1 + li.t_npg; i++)
      {
        if (proc->page_table[i].valid)
        {
          // Free the physical frame
          int pfn = proc->page_table[i].pfn;
          FreeFrame(pfn);

          // Mark page as invalid
          proc->page_table[i].valid = 0;
        }
      }
      return ERROR;
    }

    TracePrintf(0, "Mapping text page %d to frame %d\n", i, frame);
    proc->page_table[i].valid = 1;
    proc->page_table[i].pfn = frame;
    proc->page_table[i].prot = PROT_READ | PROT_WRITE;
  }

  /*
   * ==>> Then, data. Allocate "data_npg" physical pages and map them starting at
   * ==>> the  "data_pg1" in region 1 address space.
   * ==>> These pages should be marked valid, with a protection of
   * ==>> (PROT_READ | PROT_WRITE).
   */

  // Allocate and map data pages
  for (int i = data_pg1; i < data_pg1 + data_npg; i++)
  {
    int frame = GetFrame();
    if (frame < 0)
    {
      for (int i = data_pg1; i < data_pg1 + data_npg; i++)
      {
        if (proc->page_table[i].valid)
        {
          // Free the physical frame
          int pfn = proc->page_table[i].pfn;
          FreeFrame(pfn);

          // Mark page as invalid
          proc->page_table[i].valid = 0;
        }
      }
      return ERROR;
    }

    proc->page_table[i].valid = 1;
    proc->page_table[i].pfn = frame;
    proc->page_table[i].prot = PROT_READ | PROT_WRITE;
  }

  /*
   * ==>> Then, stack. Allocate "stack_npg" physical pages and map them to the top
   * ==>> of the region 1 virtual address space.
   * ==>> These pages should be marked valid, with a
   * ==>> protection of (PROT_READ | PROT_WRITE).
   */

  // Allocate and map stack pages
  for (int i = MAX_PT_LEN - stack_npg; i < MAX_PT_LEN; i++)
  {
    int frame = GetFrame();
    if (frame < 0)
    {
      for (int i = MAX_PT_LEN - stack_npg; i < MAX_PT_LEN; i++)
      {
        if (proc->page_table[i].valid)
        {
          // Free the physical frame
          int pfn = proc->page_table[i].pfn;
          FreeFrame(pfn);

          // Mark page as invalid
          proc->page_table[i].valid = 0;
        }
      }
      return ERROR;
    }

    proc->page_table[i].valid = 1;
    proc->page_table[i].pfn = frame;
    proc->page_table[i].prot = PROT_READ | PROT_WRITE;
  }

  /*
   * ==>> (Finally, make sure that there are no stale region1 mappings left in the TLB!)
   */

  // Flush TLB
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_1);

  /*
   * All pages for the new address space are now in the page table.
   */

  /*
   * Read the text from the file into memory.
   */

  lseek(fd, li.t_faddr, SEEK_SET);
  segment_size = li.t_npg << PAGESHIFT;
  if (read(fd, (void *)li.t_vaddr, segment_size) != segment_size)
  {
    close(fd);
    return KILL; // see ykernel.h
  }

  /*
   * Read the data from the file into memory.
   */
  lseek(fd, li.id_faddr, 0);
  segment_size = li.id_npg << PAGESHIFT;

  if (read(fd, (void *)li.id_vaddr, segment_size) != segment_size)
  {
    close(fd);
    return KILL;
  }

  close(fd); /* we've read it all now */

  /*
   * ==>> Above, you mapped the text pages as writable, so this code could write
   * ==>> the new text there.
   *
   * ==>> But now, you need to change the protections so that the machine can execute
   * ==>> the text.
   *
   * ==>> For each text page in region1, change the protection to (PROT_READ | PROT_EXEC).
   * ==>> If any of these page table entries is also in the TLB,
   * ==>> you will need to flush the old mapping.
   */

  // Change text pages to read/execute
  for (int i = 0; i < li.t_npg; i++)
  {
    proc->page_table[text_pg1 + i].prot = PROT_READ | PROT_EXEC;
  }
  // Flush TLB again since we changed protections
  WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

  /*
   * Zero out the uninitialized data area
   */
  bzero((void *)li.id_end, li.ud_end - li.id_end);

  /*
   * Set the entry point in the process's UserContext
   */

  /*
   * ==>> (rewrite the line below to match your actual data structure)
   * ==>> proc->uc.pc = (caddr_t) li.entry;
   */
  proc->user_context.pc = (caddr_t)li.entry;

  /*
   * Now, finally, build the argument list on the new stack.
   */

  memset(cpp, 0x00, VMEM_1_LIMIT - ((int)cpp));

  *cpp++ = (char *)argcount; /* the first value at cpp is argc */
  cp2 = argbuf;
  for (i = 0; i < argcount; i++)
  { /* copy each argument and set argv */
    *cpp++ = cp;
    strcpy(cp, cp2);
    cp += strlen(cp) + 1;
    cp2 += strlen(cp2) + 1;
  }
  free(argbuf);
  *cpp++ = NULL; /* the last argv is a NULL pointer */
  *cpp++ = NULL; /* a NULL pointer for an empty envp */

  return SUCCESS;
}