#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "shared_memory.h"


#define MAX_PROCESSES 256

struct Process {
    char executableName[256];
    int priority;
    pid_t pid;
    bool isRunning;
    struct timeval startTime;
    struct timeval endTime;
    long waitTime;
};

// Shared memory structure


// Process queue and count
struct Process queue[MAX_PROCESSES];
struct Process completedQueue[MAX_PROCESSES];
int queueCount = 0;
int completedQueueCount = 0;

int ncpu, tslice;
pid_t scheduler_pid;
bool executionStarted = false;  // To track if SIGINT has been received

// Function prototypes
void sigintHandler(int signum);
void schedulerSignalHandler(int signum);
void sigchldHandler(int sig);
void printProcessCompletionDetails();
void init_shared_memory(SharedMemoryData **sharedData);
void print_shared_memory(SharedMemoryData *sharedData);
void enqueue(SharedMemoryData *sharedData, const char* name, int priority);
struct Process dequeue();

// Function to add process to queue
void enqueue(SharedMemoryData *sharedData, const char* name, int priority) {
    if (queueCount < MAX_PROCESSES) {
        for (int i = 0; i < queueCount; i++) {
            if (strcmp(queue[i].executableName, name) == 0) {
                printf("Duplicate entry: %s\n", name);
                return;
            }
        }

        strncpy(queue[queueCount].executableName, name, sizeof(queue[queueCount].executableName) - 1);
        queue[queueCount].priority = priority;
        queue[queueCount].pid = -1;
        queue[queueCount].isRunning = false;
        queue[queueCount].waitTime = 0;
        printf("Process added to queue: %s with priority %d\n", name, priority);
        queueCount++;
        print_shared_memory(sharedData);
    } else {
        printf("Queue is full. Cannot add more entries.\n");
    }
}

// Function to dequeue process with highest priority
struct Process dequeue() {
    if (queueCount > 0) {
        int highestPriorityIndex = 0;
        for (int i = 1; i < queueCount; i++) {
            if (queue[i].priority < queue[highestPriorityIndex].priority) {
                highestPriorityIndex = i;
            }
        }
        struct Process result = queue[highestPriorityIndex];
        for (int i = highestPriorityIndex; i < queueCount - 1; i++) {
            queue[i] = queue[i + 1];
        }
        queueCount--;
        printf("Dequeued process: %s (PID: %d)\n", result.executableName, result.pid);
        return result;
    }
    return (struct Process){ .pid = -1 };
}

// SIGINT handler for starting execution
void sigintHandler(int signum) {
    printf("SIGINT received. Starting execution...\n");
    executionStarted = true;
}

// SIGCHLD handler to reap child processes
void sigchldHandler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
    printf("Child process reaped.\n");
}

// Scheduler signal handler for process scheduling
void schedulerSignalHandler(int signum) {
    if (!executionStarted) return;  // Only start if SIGINT received

    struct timeval currentTime;
    for (int i = 0; i < ncpu; i++) {
        if (queueCount > 0) {
            struct Process process = dequeue();
            if (process.pid == -1) continue;

            gettimeofday(&currentTime, NULL);
            process.waitTime = (currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000) - 
                               (process.startTime.tv_sec * 1000 + process.startTime.tv_usec / 1000);
            process.isRunning = true;
            process.pid = fork();

            if (process.pid == 0) {
                signal(SIGUSR1, sigintHandler);
                gettimeofday(&process.startTime, NULL);
                printf("Child Process (PID: %d) waiting for SIGUSR1...\n", getpid());
                pause();
                execlp(process.executableName, process.executableName, NULL);
                perror("Failed to execute program");
                exit(1);
            } else {
                printf("Scheduler running: %s (PID: %d)\n", process.executableName, process.pid);
                kill(process.pid, SIGUSR1);
                printf("Sent SIGUSR1 to process (PID: %d)\n", process.pid);
                waitpid(process.pid, NULL, 0);

                gettimeofday(&process.endTime, NULL);
                long completionTime = ((process.endTime.tv_sec * 1000 + process.endTime.tv_usec / 1000) -
                                       (process.startTime.tv_sec * 1000 + process.startTime.tv_usec / 1000));
                if (completionTime < tslice) completionTime = tslice;

                completedQueue[completedQueueCount++] = process;
                printf("Process %s (PID: %d) completed. Completion Time: %ld ms, Wait Time: %ld ms\n",
                       process.executableName, process.pid, completionTime, process.waitTime);
            }
        }
    }
}

// Function to print process completion details upon termination
void printProcessCompletionDetails() {
    printf("\n---- Process Completion Details ----\n");
    for (int i = 0; i < completedQueueCount; i++) {
        struct Process process = completedQueue[i];
        long completionTime = ((process.endTime.tv_sec * 1000 + process.endTime.tv_usec / 1000) -
                               (process.startTime.tv_sec * 1000 + process.startTime.tv_usec / 1000));
        if (completionTime < tslice) completionTime = tslice;

        printf("Process: %s (PID: %d)\n", process.executableName, process.pid);
        printf("Completion Time: %ld ms\n", completionTime);
        printf("Wait Time: %ld ms\n", process.waitTime);
        printf("---------------------------------\n");
    }
}

// Function to initialize shared memory
void init_shared_memory(SharedMemoryData **sharedData) {
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(1);
    }
    ftruncate(shm_fd, sizeof(SharedMemoryData) * MAX_PROCESSES);  // Set the size of the shared memory
    *sharedData = mmap(NULL, sizeof(SharedMemoryData) * MAX_PROCESSES, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (*sharedData == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}

void print_shared_memory(SharedMemoryData *sharedData) {
    printf("Current processes in shared memory:\n");
    printf("Executable: %s\n", sharedData->executableName);
}

int main(int argc, char *argv[]) {
    // printf("Starting SimpleScheduler with %d CPU cores and a time slice of %d milliseconds.\n", ncpu, tslice);
    SharedMemoryData *sharedData = NULL;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <NCPU> <TSLICE>\n", argv[0]);
        return 1;
    }

    ncpu = atoi(argv[1]);
    tslice = atoi(argv[2]);
    printf("Starting SimpleScheduler with %d CPU cores and a time slice of %d milliseconds.\n", ncpu, tslice);
    init_shared_memory(&sharedData);

    signal(SIGALRM, schedulerSignalHandler);
    signal(SIGCHLD, sigchldHandler);
    signal(SIGINT, sigintHandler);  // Register SIGINT handler

    struct itimerval timer;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = tslice * 1000;
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = tslice * 1000;
    setitimer(ITIMER_REAL, &timer, NULL);

    char executableName[256];
    int priority;

    while (true) {
        if (scanf("%s", executableName) != 1 || strcmp(executableName, "exit") == 0) break;

        printf("Enter priority for %s: ", executableName);
        scanf("%d", &priority);
        enqueue(sharedData, executableName, priority);
    }

    printProcessCompletionDetails();
    munmap(sharedData, sizeof(SharedMemoryData));
    shm_unlink(SHARED_MEM_NAME);

    return 0;
}
