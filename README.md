# Yalnix Operating System Kernel

## Overview

The Yalnix Kernel is an educational operating system implementing core OS functionalities including process management, virtual memory, synchronization primitives, and inter-process communication. Built for the Yalnix hardware architecture, it provides a foundation for understanding fundamental operating system concepts.

## System Architecture

### Memory Management

The kernel implements a two-region virtual memory system:
- **Region 0**: Kernel space (0x0 - VMEM_1_BASE)
- **Region 1**: User space (VMEM_1_BASE - VMEM_1_LIMIT)

Memory is managed through paged virtual memory with TLB support, handling memory protection, stack growth, and frame allocation.

```
Memory Layout:
┌───────────────┐VMEM_1_LIMIT
│   User Text   │
│   User Data   │
│   User Heap   │
│               │
│   User Stack  │
└───────────────┘ 
┌───────────────┐VMEM_1_BASE
│  Kernel Stack │
│  Kernel Heap  │
│  Kernel Data  │
│  Kernel Text  │
└───────────────┘0x00000000

```

### Process Management

Processes are represented by Process Control Blocks (PCBs) that track:
- Process state (running, ready, blocked, defunct, orphan)
- Page tables and memory mappings
- Context information
- Parent-child relationships
- Resource allocations

The scheduler implements basic round-robin scheduling using ready and blocked queues.

### Synchronization

The kernel provides three synchronization primitives:
1. **Locks**: Mutex locks with wait queues
2. **Condition Variables**: For process signaling
3. **Pipes**: For data transfer between processes

All synchronization objects use a unified ID scheme with type flags (LOCK_ID_FLAG, CONDVAR_ID_FLAG, PIPE_ID_FLAG).

### I/O Subsystem

Terminal I/O is implemented through a TTY subsystem supporting:
- Buffered reads and writes
- Process blocking when resources unavailable
- Interrupt-driven I/O

## Key Components

### Process Management (`process.h`)

The process subsystem manages the process lifecycle:
- Process creation and termination
- Basic parent-child relationship tracking
- Resource allocation and deallocation
- Context switches

### Memory Management (`kernel.h`)

Memory management provides:
- Frame allocation via bitmap
- Page table management
- Stack growth handling
- Memory access validation
- TLB management

### Synchronization (`synchronization.h`)

The synchronization primitives include:
- Lock acquisition/release with blocking
- Condition variable wait/signal/broadcast
- Pipe read/write with appropriate blocking

### TTY Subsystem (`tty.h`)

The terminal I/O system handles:
- Read/write operations to terminals
- Process queues for blocked readers/writers
- Buffer management for input/output

### Queue Management (`queue.h`)

A simple but effective queue implementation for:
- Process queues
- Wait queues
- TTY operation queues

### Trap Handling (`trap_handler.h`)

The trap handler processes:
- System calls (via TrapKernelHandler)
- Clock interrupts (TrapClockHandler)
- Memory violations (TrapMemoryHandler)
- TTY I/O interrupts (TrapTtyReceiveHandler, TrapTtyTransmitHandler)

## Security Features

- **Memory Protection**: Strict validation of user pointers
- **Resource ID Validation**: Type and range checking for all IDs
- **Address Space Isolation**: Complete separation of user/kernel memory
- **Buffer Validation**: Boundary checking for all memory operations

## Building and Running

```bash
# Build the kernel
make

# Run with no args (will run init)
./yalnix

# Run with specific program
./yalnix test_file
```

## Roadmap

- [x] Process creation/context switching
- [x] Virtual memory implementation
- [x] System Calls
- [x] Scheduling
- [x] Synchronization
- [x] IPC
- [x] TTY I/O

Future Goals:
- [ ] Disk
- [ ] Interactive shell
- [ ] File system

## Acknowledgments

Based on the Yalnix hardware framework developed by Dartmouth College's Department of Computer Science.