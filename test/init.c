#include <yuser.h>

int main(void)
{
  TracePrintf(0, "Hello, init!\n");

  TracePrintf(0, "Will delay for 3 ticks\n");
  Delay(3);
  TracePrintf(0, "Back from delay\n");

  TracePrintf(0, "Will delay again for 2 ticks\n");
  Delay(2);
  TracePrintf(0, "Back from delay 2\n");

  int rc = Delay(-1); // This should not work and return with an error
  if (rc != -1)
  {
    TracePrintf(0, "Delay returned %d instead of -1\n", rc);
    Exit(1);
  }
  TracePrintf(0, "Delay returned -1 as expected\n");

  Delay(0); // Should return immediately

  int pid = GetPid();
  TracePrintf(0, "PID of init: %d\n", pid);
  Exit(0);
}