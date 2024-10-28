#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "shared_memory.h"

/* Constants */
#define ARG_MAX_COUNT 1024
#define MAX_BACKGROUND_PROCESSES 100
#define HISTORY_MAXITEMS 100

/* Struct for background processes */
typedef struct {
    pid_t pid;
    char *cmd;
} BackgroundProcess;

/* Global variables */
BackgroundProcess background_processes[MAX_BACKGROUND_PROCESSES];
int bg_process_count = 0;
char **history;
int history_len = 0;
pid_t *pids;
time_t *start_times;
double *durations;
volatile sig_atomic_t exit_shell = 0; // Flag for SIGINT

/* Function declarations */
void init_history();
void add_to_history(char *cmd, pid_t pid, double duration);
void print_history();
void check_background_processes();
void launch_command(char *cmd);
void execute_single_command(char *cmd);
void execute_piped_commands(char *cmd_parts[], int num_parts);
int is_blank(char *input);
int handle_builtin(char *input);
void enqueue_for_scheduler(const char *cmd, int priority);
void handle_scheduler_signal(int signo);
void execute_shared_memory_command(); 
void handle_sigint(int signo); // SIGINT handler

/* Initialize history array */
void init_history() {
    history = calloc(HISTORY_MAXITEMS, sizeof(char *));
    pids = calloc(HISTORY_MAXITEMS, sizeof(pid_t));
    start_times = calloc(HISTORY_MAXITEMS, sizeof(time_t));
    durations = calloc(HISTORY_MAXITEMS, sizeof(double));
    if (!history || !pids || !start_times || !durations) {
        fprintf(stderr, "error: memory allocation failed\n");
        exit(EXIT_FAILURE);
    }
}

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

/* Add a command to the history */
void add_to_history(char *cmd, pid_t pid, double duration) {
    char *line = strdup(cmd);
    if (line == NULL) return;

    if (history_len == HISTORY_MAXITEMS) {
        free(history[0]);
        memmove(history, history + 1, sizeof(char *) * (HISTORY_MAXITEMS - 1));
        memmove(pids, pids + 1, sizeof(pid_t) * (HISTORY_MAXITEMS - 1));
        memmove(start_times, start_times + 1, sizeof(time_t) * (HISTORY_MAXITEMS - 1));
        memmove(durations, durations + 1, sizeof(double) * (HISTORY_MAXITEMS - 1));
        history_len--;
    }

    history[history_len] = line;
    pids[history_len] = pid;
    start_times[history_len] = time(NULL);
    durations[history_len] = duration;
    history_len++;
}

/* Print the command history */
void print_history() {
    for (int i = 0; i < history_len; i++) {
        printf("%d %s (pid: %d, duration: %.2f seconds)\n", i, history[i], pids[i], durations[i]);
    }
}

/* Check for completed background processes */
void check_background_processes() {
    for (int i = 0; i < bg_process_count; i++) {
        int status;
        pid_t result = waitpid(background_processes[i].pid, &status, WNOHANG);
        
        if (result == background_processes[i].pid) {
            printf("[Background] PID: %d finished command: %s\n", background_processes[i].pid, background_processes[i].cmd);
            free(background_processes[i].cmd);
            for (int j = i; j < bg_process_count - 1; j++) {
                background_processes[j] = background_processes[j + 1];
            }
            bg_process_count--;
            i--;
        }
    }
}

/* Function to handle signal from scheduler */
void handle_scheduler_signal(int signo) {
    if (signo == SIGUSR1) {
        printf("Scheduler allowed execution.\n");
    } else if (signo == SIGUSR2) {
        printf("Scheduler paused execution.\n");
    }
}

/* Execute a command using exec, supporting pipes and background processes */
void launch_command(char *cmd) {
    char *args[ARG_MAX_COUNT];
    int tokenCount = 0;

    char *cmd_part = strtok(cmd, "|");
    char *cmd_parts[ARG_MAX_COUNT];
    int num_parts = 0;

    while (cmd_part != NULL) {
        cmd_parts[num_parts++] = cmd_part;
        cmd_part = strtok(NULL, "|");
    }

    if (num_parts == 1) {
        execute_single_command(cmd_parts[0]);
    } 
}

void print_shared_memory() {
    // Open shared memory for reading
    int shm_fd = shm_open(SHARED_MEM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return; // Return if failed to open shared memory
    }

    SharedMemoryData *sharedData = mmap(NULL, sizeof(SharedMemoryData), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (sharedData == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return; // Return if mmap fails
    }

    // Print the contents of the shared memory
    printf("Current processes in shared memory:\n");
    printf("Executable: %s, Priority: %d\n", sharedData->executableName, sharedData->priority);

    // Clean up
    munmap(sharedData, sizeof(SharedMemoryData));
    close(shm_fd);
}

/* Submit command to scheduler */
void enqueue_for_scheduler(const char *cmd, int priority) {
    int shm_fd = shm_open(SHARED_MEM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        exit(EXIT_FAILURE);
    }

    ftruncate(shm_fd, sizeof(SharedMemoryData)); // Set size of shared memory
    SharedMemoryData *sharedData = mmap(NULL, sizeof(SharedMemoryData), PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (sharedData == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    strncpy(sharedData->executableName, cmd, sizeof(sharedData->executableName) - 1);
    sharedData->executableName[sizeof(sharedData->executableName) - 1] = '\0'; // Null-terminate the string
    sharedData->priority = priority;
    printf("Command submitted to scheduler: %s with priority %d\n", cmd, priority);

    munmap(sharedData, sizeof(SharedMemoryData));
    close(shm_fd);
}

void execute_single_command(char *cmd) {
    char *args[ARG_MAX_COUNT];
    int tokenCount = 0;
    int background = 0;

    // Handle "submit" commands
    if (strncmp(cmd, "submit ", 7) == 0) {
        char *program = cmd + 7;  // Skip the "submit " part
        
        // Split the command into arguments
        char *token = strtok(program, " ");
        while (token != NULL && tokenCount < ARG_MAX_COUNT) {
            args[tokenCount++] = token;
            token = strtok(NULL, " ");
        }
        args[tokenCount] = NULL; // Null-terminate the argument list

        // The first argument after "submit" should be the program/executable name
        if (tokenCount > 0) {
            char *executable = args[0];
            int priority = (tokenCount > 1) ? atoi(args[1]) : 1; // Optional priority value, defaults to 1

            // Enqueue the command for the scheduler
            enqueue_for_scheduler(executable, priority);

            printf("Submitted executable: %s with priority: %d\n", executable, priority);
            return; // Exit the function to avoid further processing
        }
    }

    // Regular command execution (not a submit command)
    size_t cmd_len = strlen(cmd);
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    } else if (pid == 0) { // Child process
        // Execute the command
        char *exe_name = strtok(cmd, " ");
        execvp(exe_name, args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
        printf("Command executed: %s\n", cmd);
        add_to_history(cmd, pid, difftime(time(NULL), start_times[history_len - 1]));
    }
}

/* Signal handler for SIGINT */
void handle_sigint(int signo) {
    // Set the exit flag
    exit_shell = 1;
}
/* Check if a command is blank */
int is_blank(char *input) {
    int n = strlen(input);
    for (int i = 0; i < n; i++) {
        if (!isspace(input[i]))
            return 0;
    }
    return 1;
}

/* Handle built-in commands */
int handle_builtin(char *input) {
    if (strcmp(input, "exit") == 0) {
        return -1; // Indicates exit
    }
    if (strcmp(input, "history") == 0) {
        print_history();
        return 0; // Handled
    }
    if (strncmp(input, "cd", 2) == 0) {
        char *dir = strtok(input + 3, " ");
        if (chdir(dir) != 0) {
            perror("cd");
        }
        return 0; // Handled
    }
    return 1; // Not a built-in command
}
void execute_shared_memory_command() {
    SharedMemoryData *sharedData = NULL;
    int shm_fd = shm_open(SHARED_MEM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return; 
    }

    sharedData = mmap(NULL, sizeof(SharedMemoryData), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (sharedData == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return; 
    }

 
    if (strlen(sharedData->executableName) > 0) {
        pid_t pid = fork();
        if (pid == 0) {
         
            char *args[ARG_MAX_COUNT];
            args[0] = sharedData->executableName;
            args[1] = NULL; 

            execvp(args[0], args);
            perror("exec");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            waitpid(pid, NULL, 0);
        }
    }

  
    munmap(sharedData, sizeof(SharedMemoryData));
    close(shm_fd);
}

/* Main function */

int main(int argc, char *argv[]) {


    SharedMemoryData *sharedData = NULL;
    init_shared_memory(&sharedData);


    if (argc != 3) {
        fprintf(stderr, "Usage: %s NCPU TSLICE\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int ncpu = atoi(argv[1]);
    int tslice = atoi(argv[2]);


    int scheduler_pid = fork();
    if (scheduler_pid == 0) {


        execl("./scheduler", "./scheduler", argv[1], argv[2], NULL);
        perror("Scheduler exec failed");
        exit(EXIT_FAILURE);
    }

    printf("Starting SimpleShell with %d CPU cores and a time slice of %d milliseconds.\n", ncpu, tslice);


    init_history();


    char input[ARG_MAX_COUNT];
    while (1) {
        printf("myshell> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (feof(stdin)) break;
            perror("fgets");
            continue;
        }

        input[strcspn(input, "\n")] = 0; 
        if (is_blank(input)) continue;
        int result = handle_builtin(input);
        if (result == -1) break;

   
        execute_single_command(input);
  
        print_shared_memory();
    }


    for (int i = 0; i < history_len; i++) free(history[i]);
    free(history);
    free(pids);
    free(start_times);
    free(durations);

    return 0;
}
