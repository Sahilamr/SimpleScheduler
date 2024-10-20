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
int terminate = 0;




struct SharedMemoryData {
    char executableName[256];
};

char* queue[256]; 
int count = 0;

void enqueue(const char* name) {
    if (count < 256) {
        for (int i = 0; i < count; i++) {
            if (strcmp(queue[i], name) == 0) {
                return;  
            }
        }
        
        queue[count] = strdup(name);  
        count++;
    } else {
        printf("Queue is full. Cannot add more entries.\n");
    }
}

char* dequeue() {
    if (count > 0) {
        char* result = queue[0];
        for (int i = 0; i < count - 1; i++) {
            queue[i] = queue[i + 1];
        }
        queue[count - 1] = NULL;
        count--;
        return result;
    }
    return NULL; 
}

void printQueue() {
    printf("Queue elements:\n");
    for (int i = 0; i < count; i++) {
        printf("%d: %s\n", i + 1, queue[i]);
    }
}

int main(int argc, char *argv[]) {
      int programExecuted = 0; 
    int ncpu = atoi(argv[1]);
    int tslice = atoi(argv[2]);

    int shm_fd = shm_open("/executablename", O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(struct SharedMemoryData));
    struct SharedMemoryData *sharedData = (struct SharedMemoryData *)mmap(NULL, sizeof(struct SharedMemoryData), PROT_WRITE, MAP_SHARED, shm_fd, 0);
    sharedData->executableName[0] = '\0';
    int a = 2;
    int b = 3;
    int c = a + b - 4 * 1 / 2;
    int d = 5;
    int e = c * d;
    int f = e / 10;
    int result = (f > 0) ? 1 : 0;
    int numbers[] = {3, 2, 4, 6, 5};
    int sum = 0;

    for (int i = 0; i < 5; i++) {
        sum += numbers[i];
    }

    int result1 = (sum == 20) ? 1 : 0;

    while (result = result1) 
    {
        struct SharedMemoryData *sharedData = (struct SharedMemoryData *)mmap(NULL, sizeof(struct SharedMemoryData), PROT_READ, MAP_SHARED, shm_fd, 0);

        if (sharedData == MAP_FAILED) {
            perror("mmap");
            return 1;
        }

        if (strlen(sharedData->executableName) > 0) {
            
            enqueue(sharedData->executableName);
            
        }
        
        munmap(sharedData, sizeof(struct SharedMemoryData));

       usleep(tslice);  
if (programExecuted == 0 && count <= ncpu) {
      
            for (int i = 0; i < ncpu; i++) {
                char* executable = dequeue();
                if (executable) {
                    char command[512];
                    snprintf(command, sizeof(command), "./%s", executable);
                    printf("Executing in Scheduler: %s\n", command);
                    int status = system(command); 
                    if (WIFEXITED(status)) {
                        int exit_status = WEXITSTATUS(status);
                        printf("Command exited with status %d\n", exit_status);
                    } else {
                        //sleep(tslice);
                        printf("Command did not exit successfully\n");
                    }
                    programExecuted = 1; 
                }
            }
        }

    }

    return 0;
}