#include "yuser.h"

int main(void)
{
  TracePrintf(0, "Hello, brk!\n");
  void *mem = malloc(4096);
  if (mem == NULL)
  {
    TracePrintf(0, "Failed to allocate memory\n");
    Exit(1);
  }
  TracePrintf(0, "Memory allocated at %p\n", mem);
  TracePrintf(0, "Memory freed\n");
  void *mem2 = malloc(100000);
  if (mem2 == NULL)
  {
    TracePrintf(0, "Failed to allocate memory\n");
    Exit(1);
  }
  Brk(0); // Our design is so that if Brk is called with address below VMEM_1_BASE and above VMEM_1_LIMIT, it will fail gracefully.
  Exit(0);
}