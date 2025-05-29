#include <yuser.h>
#include <hardware.h>

int main()
{
  int terminal = 0; // Use terminal 0
  char buffer[100];

  // Simple write test
  TtyPrintf(terminal, "=== Simple Terminal Test ===\n");
  TtyPrintf(terminal, "Process ID: %d\n", GetPid());

  // Test basic write
  char *message = "This is a direct TtyWrite test\n";
  int result = TtyWrite(terminal, message, strlen(message));
  TtyPrintf(terminal, "TtyWrite returned: %d bytes written\n\n", result);

  // Test simple read
  TtyPrintf(terminal, "Please enter your name: ");
  int bytes_read = TtyRead(terminal, buffer, sizeof(buffer) - 1);
  buffer[bytes_read] = '\0'; // Null-terminate for printing

  TtyPrintf(terminal, "Hello, %s! You entered %d bytes.\n\n", buffer, bytes_read);

  TtyPrintf(terminal, "I'll echo 3 lines that you type:\n");

  for (int i = 0; i < 3; i++)
  {
    TtyPrintf(terminal, "Line %d> ", i + 1);
    bytes_read = TtyRead(terminal, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';
    TtyPrintf(terminal, "Echo: %s\n", buffer);
  }

  TtyPrintf(terminal, "\n=== Test completed successfully ===\n");

  Exit(0);
}
