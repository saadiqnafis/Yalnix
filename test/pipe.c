#include "yuser.h"
#include "yalnix.h"

void test_simple_pipe();
void test_blocking_pipe();
void test_multiple_operations();
void test_pipe_reclaim();
void test_pipe_edge_cases();

int main()
{
  TracePrintf(1, "Starting pipe tests\n");

  // test_simple_pipe();
  // test_blocking_pipe();
  // test_multiple_operations();
  test_pipe_reclaim();
  // test_pipe_edge_cases();

  TracePrintf(1, "All pipe tests completed\n");
  return 0;
}

void test_simple_pipe()
{
  TracePrintf(1, "=== Testing simple pipe operations ===\n");

  int pipe_id;
  int rc = PipeInit(&pipe_id);
  if (rc != 0)
  {
    TracePrintf(1, "PipeInit failed\n");
    Exit(1);
  }

  TracePrintf(1, "Pipe created with ID: %d\n", pipe_id);

  // Test small write and read
  char write_buf[20] = "Hello, pipe world!";
  char read_buf[20];

  rc = PipeWrite(pipe_id, write_buf, 18);
  TracePrintf(1, "PipeWrite returned %d\n", rc);

  rc = PipeRead(pipe_id, read_buf, 18);
  TracePrintf(1, "PipeRead returned %d\n", rc);
  read_buf[rc] = '\0';

  TracePrintf(1, "Read from pipe: '%s'\n", read_buf);

  // Test reclaim
  // rc = Reclaim(pipe_id);
  // TracePrintf(1, "Pipe reclaimed: %s\n", rc == 0 ? "success" : "failed");
}

void test_blocking_pipe()
{
  TracePrintf(1, "=== Testing blocking pipe behavior ===\n");

  int pipe_id;
  PipeInit(&pipe_id);

  int pid = Fork();

  if (pid == 0)
  {
    // Child process - reader
    TracePrintf(1, "Reader process started\n");

    // Sleep to ensure writer gets blocked
    Delay(2);

    char buffer[PIPE_BUFFER_LEN + 20];
    TracePrintf(1, "Reader trying to read from pipe\n");
    int bytes = PipeRead(pipe_id, buffer, PIPE_BUFFER_LEN);
    buffer[bytes] = '\0';

    TracePrintf(1, "Reader read %d bytes: '%s'\n", bytes, buffer);

    // Read again to test multiple reads
    Delay(1);
    bytes = PipeRead(pipe_id, buffer, 20);
    buffer[bytes] = '\0';

    TracePrintf(1, "Reader read another %d bytes: '%s'\n", bytes, buffer);

    Exit(0);
  }
  else
  {
    // Parent process - writer
    TracePrintf(1, "Writer process started\n");

    // Try to write more than buffer size to test blocking
    char buffer[PIPE_BUFFER_LEN + 20];
    for (int i = 0; i < PIPE_BUFFER_LEN + 19; i++)
    {
      buffer[i] = 'A' + (i % 26);
    }
    buffer[PIPE_BUFFER_LEN + 19] = '\0';

    TracePrintf(1, "Writer trying to write %d bytes\n", PIPE_BUFFER_LEN + 19);
    int bytes = PipeWrite(pipe_id, buffer, PIPE_BUFFER_LEN + 19);

    TracePrintf(1, "Writer wrote %d bytes\n", bytes);

    // Wait for child to complete
    int status;
    Wait(&status);
  }
}

void test_multiple_operations()
{
  TracePrintf(1, "=== Testing multiple read/write operations ===\n");

  int pipe_id;
  PipeInit(&pipe_id);

  int pid = Fork();

  if (pid == 0)
  {
    // Child - reader
    char buffer[50];
    int bytes;

    // Read smaller chunks to test partial reads
    for (int i = 0; i < 5; i++)
    {
      bytes = PipeRead(pipe_id, buffer, 30);
      buffer[bytes] = '\0';
      TracePrintf(1, "Read %d: %d bytes: '%s'\n", i, bytes, buffer);
      Delay(1);
    }

    Exit(0);
  }
  else
  {
    // Parent - writer
    char buffer[200];

    // Initialize buffer with distinct patterns
    for (int i = 0; i < 200; i++)
    {
      buffer[i] = '0' + (i % 10);
    }

    // Write in chunks to test multiple writes
    for (int i = 0; i < 5; i++)
    {
      int offset = i * 40;
      int bytes = PipeWrite(pipe_id, buffer + offset, 40);
      TracePrintf(1, "Write %d: %d bytes written\n", i, bytes);
      Delay(1);
    }

    // Wait for child to complete
    int status;
    Wait(&status);
  }
}

void test_pipe_reclaim()
{
  TracePrintf(1, "=== Testing pipe reclamation ===\n");

  int pipe_ids[5];
  char buffer[50] = "This is a test message";
  char read_buffer[50];
  int rc;

  // Create multiple pipes
  for (int i = 0; i < 5; i++)
  {
    PipeInit(&pipe_ids[i]);
    TracePrintf(1, "Created pipe %d: ID %d\n", i, pipe_ids[i]);
  }

  // Test writing to non-reclaimed pipe (should succeed)
  rc = PipeWrite(pipe_ids[0], buffer, strlen(buffer) + 1);
  TracePrintf(1, "Writing to active pipe 0: %s (%d bytes)\n",
              rc > 0 ? "succeeded" : "failed", rc);

  // Reclaim odd-numbered pipes
  for (int i = 1; i < 5; i += 2)
  {
    rc = Reclaim(pipe_ids[i]);
    TracePrintf(1, "Reclaiming pipe %d: %s\n", i, rc == 0 ? "success" : "failed");
  }

  // Try writing to a reclaimed pipe (should fail)
  rc = PipeWrite(pipe_ids[1], buffer, strlen(buffer) + 1);
  TracePrintf(1, "Writing to reclaimed pipe 1: %s\n",
              rc == ERROR ? "failed as expected" : "unexpectedly succeeded");

  // Try reading from non-reclaimed pipe (should succeed)
  rc = PipeRead(pipe_ids[0], read_buffer, sizeof(read_buffer));
  if (rc > 0)
  {
    read_buffer[rc] = '\0'; // Ensure null termination
    TracePrintf(1, "Reading from active pipe 0: succeeded (%d bytes): '%s'\n",
                rc, read_buffer);
  }
  else
  {
    TracePrintf(1, "Reading from active pipe 0: failed\n");
  }

  // Try reading from a reclaimed pipe (should fail)
  rc = PipeRead(pipe_ids[1], read_buffer, sizeof(read_buffer));
  TracePrintf(1, "Reading from reclaimed pipe 1: %s\n",
              rc == ERROR ? "failed as expected" : "unexpectedly succeeded");

  // Reclaim remaining pipes
  for (int i = 0; i < 5; i += 2)
  {
    rc = Reclaim(pipe_ids[i]);
    TracePrintf(1, "Reclaiming pipe %d: %s\n", i, rc == 0 ? "success" : "failed");
  }

  // Try operations on all pipes to make sure they're all reclaimed properly
  for (int i = 0; i < 5; i++)
  {
    rc = PipeWrite(pipe_ids[i], buffer, 5);
    TracePrintf(1, "Final write to pipe %d: %s\n", i,
                rc == ERROR ? "failed as expected" : "unexpectedly succeeded");
  }
}

void test_pipe_edge_cases()
{
  TracePrintf(1, "=== Testing pipe edge cases ===\n");

  int pipe_id;
  PipeInit(&pipe_id);

  int pid = Fork();

  if (pid == 0)
  {
    // Child - test reader edge cases

    // 1. Read from empty pipe (should block)
    TracePrintf(1, "Child: Attempting to read from empty pipe (should block)\n");
    char buffer[10];
    int bytes = PipeRead(pipe_id, buffer, 10);
    TracePrintf(1, "Child: Read %d bytes after blocking\n", bytes);

    // 2. Read with zero length (should error)
    bytes = PipeRead(pipe_id, buffer, 0);
    TracePrintf(1, "Child: Read with zero length: %s\n",
                bytes == ERROR ? "failed as expected" : "unexpectedly succeeded");

    // 3. Read with invalid pipe ID
    bytes = PipeRead(-1, buffer, 10);
    TracePrintf(1, "Child: Read with invalid pipe ID: %s\n",
                bytes == ERROR ? "failed as expected" : "unexpectedly succeeded");

    Exit(0);
  }
  else
  {
    // Parent - test writer edge cases

    // Give child time to block on empty pipe
    Delay(2);

    // 1. Test writing exactly PIPE_BUFFER_LEN bytes
    char buffer[PIPE_BUFFER_LEN];
    for (int i = 0; i < PIPE_BUFFER_LEN; i++)
    {
      buffer[i] = 'X';
    }

    int bytes = PipeWrite(pipe_id, buffer, PIPE_BUFFER_LEN);
    TracePrintf(1, "Parent: Wrote exactly PIPE_BUFFER_LEN bytes: %d\n", bytes);

    // 2. Write with zero length (should error)
    bytes = PipeWrite(pipe_id, buffer, 0);
    TracePrintf(1, "Parent: Write with zero length: %s\n",
                bytes == ERROR ? "failed as expected" : "unexpectedly succeeded");

    // 3. Write with invalid pipe ID
    bytes = PipeWrite(-1, buffer, 10);
    TracePrintf(1, "Parent: Write with invalid pipe ID: %s\n",
                bytes == ERROR ? "failed as expected" : "unexpectedly succeeded");

    // Wait for child to complete
    int status;
    Wait(&status);
  }
}