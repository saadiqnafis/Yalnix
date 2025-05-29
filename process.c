#include "process.h"
#include "ykernel.h"
#include "queue.h"
#include "kernel.h"

pcb_queue_t *ready_processes = NULL;
pcb_queue_t *blocked_processes = NULL;
pcb_queue_t *defunct_processes = NULL;
pcb_queue_t *waiting_parent_processes = NULL;
pcb_t *idle_pcb = NULL;

void InitializeProcessQueues()
{
  ready_processes = pcb_queue_create();
  if (ready_processes == NULL)
  {
    TracePrintf(0, "InitializeProcessQueues: Failed to create ready queue\n");
    Halt();
  }

  blocked_processes = pcb_queue_create();
  if (blocked_processes == NULL)
  {
    TracePrintf(0, "InitializeProcessQueues: Failed to create blocked queue\n");
    Halt();
  }

  defunct_processes = pcb_queue_create();
  if (defunct_processes == NULL)
  {
    TracePrintf(0, "InitializeProcessQueues: Failed to create defunct queue\n");
    Halt();
  }

  waiting_parent_processes = pcb_queue_create();
  if (waiting_parent_processes == NULL)
  {
    TracePrintf(0, "InitializeProcessQueues: Failed to create waiting parent queue\n");
    Halt();
  }
}

pcb_t *GetCurrentProcess()
{
  return current_process;
}

void SetCurrentProcess(pcb_t *process)
{
  current_process = process;
  process->state = PCB_STATE_RUNNING;
}

pcb_t *CreatePCB(char *name)
{
  pcb_t *pcb = (pcb_t *)malloc(sizeof(pcb_t));
  if (pcb == NULL)
  {
    TracePrintf(0, "CreatePCB: Failed to allocate memory for pcb\n");
    return NULL;
  }

  pcb->state = PCB_STATE_READY;
  pcb->page_table = (pte_t *)calloc(NUM_PAGES_REGION1, sizeof(pte_t));
  if (pcb->page_table == NULL)
  {
    TracePrintf(0, "CreatePCB: Failed to allocate memory for page table\n");
    free(pcb);
    return NULL;
  }

  pcb->kernel_stack = NULL;
  pcb->brk = NULL;
  pcb->next = NULL;
  pcb->prev = NULL;
  pcb->parent = NULL;
  pcb->delay_ticks = -1;
  pcb->exit_status = 0;
  pcb->children = pcb_queue_create();
  if (pcb->children == NULL)
  {
    TracePrintf(0, "CreatePCB: Failed to allocate memory for children queue\n");
    free(pcb);
    return NULL;
  }

  pcb->name = name;
  pcb->pid = helper_new_pid(pcb->page_table);

  // Initialize TTY-related fields
  pcb->tty_read_buf = NULL;
  pcb->tty_read_len = 0;
  pcb->tty_write_buf = NULL;
  pcb->tty_write_len = 0;
  pcb->kernel_read_buffer = NULL;
  pcb->kernel_read_size = 0;

  return pcb;
}

void DestroyPCB(pcb_t *pcb)
{
  if (pcb->children != NULL)
  {
    for (pcb_t *child = pcb->children->head; child != NULL; child = child->next)
    {
      child->parent = NULL;
      child->state = PCB_STATE_ORPHAN;
    }
    free(pcb->children);
  }

  if (pcb->next != NULL)
  {
    pcb->next->prev = pcb->prev;
  }
  if (pcb->prev != NULL)
  {
    pcb->prev->next = pcb->next;
  }

  for (int i = 0; i < NUM_PAGES_REGION1; i++)
  {
    if (pcb->page_table[i].valid == 1)
    {
      FreeFrame(pcb->page_table[i].pfn);
      TracePrintf(0, "DestroyPCB: Freed frame %d for page %d\n", pcb->page_table[i].pfn, i);
    }
  }

  free(pcb->page_table);
  free(pcb->kernel_stack);
  free(pcb);
}

void UpdateDelayedPCB()
{
  TracePrintf(0, "Calling UpdateDelay\n");
  if (!pcb_queue_is_empty(blocked_processes))
  {
    for (pcb_t *pcb = blocked_processes->head; pcb != NULL; pcb = pcb->next)
    {
      if (pcb->delay_ticks == -1)
      {
        continue;
      }
      pcb->delay_ticks--;
      TracePrintf(0, "The delay is now %d\n", pcb->delay_ticks);
      if (pcb->delay_ticks == 0)
      {
        pcb_remove(blocked_processes, pcb);
        pcb_enqueue(ready_processes, pcb);
      }
    }
  }
}

void PrintPageTable(pcb_t *pcb)
{
  for (int i = 0; i < NUM_PAGES_REGION1; i++)
  {
    TracePrintf(0, "Page table[%d]: %d, pfn: %d, prot: %d\n", i, pcb->page_table[i].valid, pcb->page_table[i].pfn, pcb->page_table[i].prot);
  }
}

/*
in kernel stack copy
- parent kernel stack VPN 126 -> PFN 126
- child kernel stack VPN 126 -> PFN 23
- map 125 -> 23
- copy 126 to 125 (will copy memory contents of pfn 126 to pfn)

in page table r1 copy
- pte_parent[1].valid = 1, .pfn = 25
- pte_child[1].valid = 1, .pfn = 31
*/
void CopyPageTable(pcb_t *parent, pcb_t *child)
{
  pte_t *parent_pt = parent->page_table;
  pte_t *child_pt = child->page_table;

  for (int i = 0; i < NUM_PAGES_REGION1; i++)
  {
    if (parent_pt[i].valid == 1)
    {
      int child_frame = GetFrame();
      child_pt[i].pfn = child_frame;

      unsigned int parent_addr = (i + NUM_PAGES_REGION1) << PAGESHIFT;
      MapScratch(child_frame); // Temporary map the frame to the scratch address
      memcpy((void *)SCRATCH_ADDR, (void *)parent_addr, PAGESIZE);
      UnmapScratch();

      child_pt[i].prot = parent_pt[i].prot;
      child_pt[i].valid = 1;
    }
  }
}
