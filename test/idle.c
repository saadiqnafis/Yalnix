#include <yuser.h>

int x = 0;

int main(void)
{
  TracePrintf(0, "Hello from idle\n");

  // Print the address of x
  TracePrintf(0, "The address of x is: %p\n", &x);

  while (1)
  {
    // We try to read x, but eventually the PTE will be modified to be invalid, and we will segfault.
    x++;
    TracePrintf(0, "x = %d\n", x);
    Delay(3);
  }

  return 0;
}