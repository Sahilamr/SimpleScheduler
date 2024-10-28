#ifndef DUMMY_MAIN_H
#define DUMMY_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// Dummy main definition (to replace the main of any executable)
int dummy_main(int argc, char **argv);

// Signal handler for SIGINT
void handle_sigint(int signum) {
    // Placeholder logic for handling SIGINT
    printf("Process %d received SIGINT: Handling interrupt...\n", getpid());
}

// Setup signal handling for the process
void setup_signal_handling() {
    // Register signal handler for SIGINT
    signal(SIGINT, handle_sigint);
    printf("Process %d is waiting for SIGINT...\n", getpid());
}

// Wrapper for the original program logic
int main(int argc, char **argv) {
    // Setup signal handling
    setup_signal_handling();

    // Call the original logic inside dummy_main
    int ret = dummy_main(argc, argv);
    
    return ret;
}

// Ensure that the user-provided main becomes dummy_main
#define main dummy_main

#endif /* DUMMY_MAIN_H */
