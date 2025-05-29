#include "yuser.h"

int x;
int y;

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
  Brk(0);               // Our design is so that if Brk is called with address below VMEM_1_BASE and above VMEM_1_LIMIT, it will fail gracefully.
  Brk((void *)1081344); // will free all that was allocated till page 4
  Brk((void *)1073152); // this will free page 3 which is not part of the heap, it is for data segment
  x = 10;               // Touching variable on page 3, so this will crash segfault
  Exit(0);              // This should not be reached
}