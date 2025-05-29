#include <yuser.h>

int main(void)
{
  TracePrintf(0, "Hello, fork!\n");

  TracePrintf(0, "Will fork now\n");
  int rc = Fork();
  TracePrintf(0, "Back from fork\n");
  if (rc == 0)
  {
    TracePrintf(0, "I am the child, about to exec brk\n");
    // Exec("brk", NULL); // Will exit with status 0
    Exec("fail", NULL);
    TracePrintf(0, "Should not be here\n");
    Exit(1);
  }
  TracePrintf(0, "I am the parent, fork again\n");
  int rc2 = Fork();
  TracePrintf(0, "rc2: %d\n", rc2);
  if (rc2 == 0)
  {
    TracePrintf(0, "I am the child 2\n");
    Exit(0);
  }
  int status;
  TracePrintf(0, "I am the parent, waiting for child 1\n");
  Wait(&status);
  TracePrintf(0, "I am back, child 1 exited with status %d\n", status);
  TracePrintf(0, "I am the parent, waiting for child 2\n");
  Wait(&status);
  TracePrintf(0, "I am back, child 2 exited with status %d\n", status);
  Delay(5);
  Exit(0);
}