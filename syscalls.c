#include "kernel.h"
#include "hardware.h"
#include "yuser.h"
#include "ykernel.h"
#include "yalnix.h"
#include "syscalls.h"
#include "process.h"
#include "synchronization.h"

int SysFork(UserContext *uctxt)
{
  pcb_t *current_pcb = GetCurrentProcess();
  pcb_t *new_pcb = CreatePCB("fork_child");
  new_pcb->parent = current_pcb;

  // Copy the user context passed from the trap handler into the new child PCB
  memcpy(&new_pcb->user_context, uctxt, sizeof(UserContext));

  // Copy the page table content from the parent to the child, allocating new frames for the child
  CopyPageTable(current_pcb, new_pcb);

  // Context switch to the child
  int rc = KernelContextSwitch(KCCopy, new_pcb, NULL);
  if (rc == -1)
  {
    TracePrintf(0, "KernelContextSwitch failed when forking\n");
    Halt();
  }

  // After context switch, check if we're in the child context
  if (GetCurrentProcess()->pid == new_pcb->pid)
  {
    // We're in the child
    WriteRegister(REG_PTBR1, (unsigned int)new_pcb->page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);
    return 0;
  }
  else
  {
    // We're in the parent
    pcb_enqueue(ready_processes, new_pcb);
    pcb_enqueue(current_pcb->children, new_pcb);

    // Make sure the parent's page table is correctly set
    WriteRegister(REG_PTBR1, (unsigned int)current_pcb->page_table);
    WriteRegister(REG_TLB_FLUSH, TLB_FLUSH_ALL);

    return new_pcb->pid;
  }
}

int SysExec(char *filename, char *argvec[])
{
  if (LoadProgram(filename, argvec, GetCurrentProcess()) != SUCCESS)
  {
    TracePrintf(0, "LoadProgram failed for exec\n");
    return ERROR;
  }

  return SUCCESS;
}

void SysExit(int status)
{
  pcb_t *pcb = GetCurrentProcess();

  if (pcb->pid == 1)
  {
    DestroyPCB(pcb);
    Halt();
  }

  pcb_enqueue(defunct_processes, pcb);
  pcb->exit_status = status;

  // First wake up the waiting parent
  pcb_t *parent = pcb->parent;
  if (parent && pcb_in_queue(waiting_parent_processes, parent))
  {
    pcb_remove(waiting_parent_processes, parent);
    parent->state = PCB_STATE_READY;
    pcb_enqueue(ready_processes, parent);
  }

  // Then do context switch
  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
  int rc = KernelContextSwitch(KCSwitch, pcb, next);

  if (rc == -1)
  {
    TracePrintf(0, "KernelContextSwitch failed when exiting\n");
    Halt();
  }
}

int SysWait(int *status_ptr)
{
  pcb_t *current_pcb = GetCurrentProcess();
  if (current_pcb->children->head == NULL)
  {
    TracePrintf(0, "No children to wait for\n");
    return ERROR;
  }
  for (pcb_t *pcb = defunct_processes->head; pcb != NULL; pcb = pcb->next)
  {
    if (pcb->parent->pid == current_pcb->pid)
    {
      *status_ptr = pcb->exit_status;
      pcb_remove(defunct_processes, pcb);
      pcb_remove(current_pcb->children, pcb);
      int pid = pcb->pid;
      DestroyPCB(pcb);
      return pid;
    }
  }
  pcb_enqueue(waiting_parent_processes, current_pcb);
  current_pcb->state = PCB_STATE_BLOCKED;
  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;
  int rc = KernelContextSwitch(KCSwitch, current_pcb, next);
  if (rc == -1)
  {
    TracePrintf(0, "KernelContextSwitch failed when waiting\n");
    Halt();
  }

  for (pcb_t *pcb = defunct_processes->head; pcb != NULL; pcb = pcb->next)
  {
    TracePrintf(0, "parent pid: %d, child pid: %d\n", current_pcb->pid, pcb->pid);
    if (pcb->parent->pid == current_pcb->pid)
    {
      pcb_remove(defunct_processes, pcb);
      pcb_enqueue(ready_processes, pcb);
      pcb->state = PCB_STATE_READY;
      *status_ptr = pcb->exit_status;
      int pid = pcb->pid;
      DestroyPCB(pcb);
      return pid;
    }
  }
  return ERROR;
}

int SysGetPid(void)
{
  pcb_t *pcb = GetCurrentProcess();
  return pcb->pid;
}

int SysBrk(void *addr)
{
  unsigned int new_addr = (unsigned int)addr;
  if (addr == NULL || new_addr < VMEM_1_BASE || new_addr > VMEM_1_LIMIT)
  {
    return ERROR;
  }

  pcb_t *pcb = GetCurrentProcess();

  int new_brk_page = (new_addr >> PAGESHIFT) - (NUM_PAGES_REGION1);
  if (pcb->brk == NULL)
  {
    int brk_start_page = 0;
    for (int i = 0; i < NUM_PAGES_REGION1; i++)
    {
      if (pcb->page_table[i].valid == 0)
      {
        brk_start_page = i;
        break;
      }
    }

    for (int i = brk_start_page; i < new_brk_page; i++)
    {
      int frame = GetFrame();
      if (frame == -1)
      {
        return ERROR;
      }
      pcb->page_table[i].valid = 1;
      TracePrintf(0, "Allocating frame %d for page %d\n", frame, i);
      pcb->page_table[i].pfn = frame;
      pcb->page_table[i].prot = PROT_READ | PROT_WRITE;
    }
    pcb->brk = (void *)new_addr;
    TracePrintf(0, "pcb->brk: %p\n", pcb->brk);
    return 0;
  }

  int brk_page = ((unsigned int)pcb->brk >> PAGESHIFT) - NUM_PAGES_REGION1;
  if (new_brk_page == brk_page)
  {
    TracePrintf(0, "Brk is already at the new address\n");
    return ERROR;
  }

  if (new_brk_page > brk_page)
  {
    for (int i = brk_page; i < new_brk_page; i++)
    {
      pcb->page_table[i].valid = 1;
      int frame = GetFrame();
      if (frame == -1)
      {
        return ERROR;
      }
      TracePrintf(0, "Allocating frame %d for page %d\n", frame, i);
      pcb->page_table[i].pfn = frame;
      pcb->page_table[i].prot = PROT_READ | PROT_WRITE;
    }
  }
  else
  {
    TracePrintf(0, "Deallocating pages from %d to %d\n", brk_page - 1, new_brk_page);
    for (int i = brk_page - 1; i >= new_brk_page; i--)
    {
      if (pcb->page_table[i].valid == 0)
      {
        break;
      }

      int frame = pcb->page_table[i].pfn;
      pcb->page_table[i].valid = 0;
      FreeFrame(frame);
      WriteRegister(REG_TLB_FLUSH, (i << PAGESHIFT) + VMEM_0_SIZE);
    }
  }

  pcb->brk = (void *)new_addr;
  TracePrintf(0, "pcb->brk: %p\n", pcb->brk);

  return 0;
}

int SysDelay(int clock_ticks)
{
  pcb_t *pcb = GetCurrentProcess();
  if (pcb == NULL)
  {
    return ERROR;
  }

  if (clock_ticks < 0)
  {
    return ERROR;
  }
  if (clock_ticks == 0)
  {
    return 0;
  }

  // Set the number of ticks the process needs to wait
  pcb->delay_ticks = clock_ticks;

  // Set process state to BLOCKED
  pcb->state = PCB_STATE_BLOCKED;

  // Add process to delay queue
  pcb_enqueue(blocked_processes, pcb);

  // call the next process (if there is one else idle) as current process is blocked
  pcb_t *next = (ready_processes->head != NULL) ? pcb_dequeue(ready_processes) : idle_pcb;

  int rc = KernelContextSwitch(KCSwitch, pcb, next);

  return 0;
}