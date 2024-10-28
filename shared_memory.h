// shared_memory.h
#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <sys/types.h>
#include <stdbool.h>
#include <sys/time.h>

#define MAX_PROCESSES 256

typedef struct {
    char executableName[256];  // Name of the executable
    int priority;              // Priority of the process
    pid_t pid;                 // PID of the process
    bool isRunning;            // Status of the process (running or not)
    struct timeval startTime;  // Start time of the process
    struct timeval endTime;    // End time of the process
    long waitTime;             // Total wait time of the process
} SharedMemoryData;

#define SHARED_MEM_NAME "/executablename"

#endif // SHARED_MEMORY_H
