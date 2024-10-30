# SimpleScheduler

## Project Description

**SimpleScheduler** is a basic implementation of a process scheduler in C. It simulates a simple round-robin CPU scheduling policy with multiple CPUs, managing the scheduling of processes with limited CPU resources using time slices (quantums). The project also includes priority scheduling as an advanced feature.

### Key Features:
- **Round-Robin Scheduling**: Processes are scheduled using a round-robin policy with a specified time slice.
- **Priority Scheduling**: Allows users to specify a priority for submitted jobs.
- **Non-blocking Process Management**: Only non-blocking processes can be scheduled, ensuring optimal CPU utilization.
- **Job Statistics**: Tracks and displays job completion time, wait time, and priority-based scheduling impact.

---

## Getting Started

### Prerequisites

- **GCC** or any C compiler
- A **UNIX-based OS** (e.g., Linux, macOS)


## How It Works

1. **SimpleShell** initializes with the number of CPUs (`NCPU`) and time slice (`TSLICE`) as command line arguments. It allows users to submit executable jobs.
2. Submitted jobs are managed by the **SimpleScheduler**, which queues the processes in a round-robin manner and schedules them to run for a specified quantum.
3. The **SimpleScheduler** handles stopping and resuming processes using signals, maintaining statistics for each job.

---

## Code Overview

### Key Files

- **SimpleScheduler.c**: Contains the implementation of the scheduler and scheduling functions.
- **SimpleShell.c**: Implements the command-line shell for job submissions.
- **dummy_main.h**: Includes the necessary modifications to user executables.
- **shared_memory.h**: Contains shared memory structures for inter-process communication.

## Advanced Features (Bonus)

### Priority Scheduling

Users can submit jobs with a priority value between 1 and 4. The scheduler uses this priority to influence the scheduling order of the processes.

---

## Statistics and Output

Upon the completion of all submitted jobs, SimpleShell displays detailed information such as:

- **Process Name**
- **PID**
- **Completion Time**
- **Wait Time**
- **Arrival Time**

It also calculates and displays the average wait and completion times for all jobs.

---

## Future Enhancements

- Implementing more advanced scheduling algorithms like Shortest Job First (SJF) or Multi-Level Queue Scheduling.
- Adding support for job preemption based on priorities.

---

## Contributing

- Sahil Amrawat 2023462
- Vikas Meena 2023593


---
