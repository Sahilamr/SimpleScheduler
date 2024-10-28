#include <stdio.h>
#include "dummy_main.h"

int main(int argc, char **argv) {
    int sum=0;

    // Simulate work
    for (int i = 0; i < 1000; i++) {
        sum+=i;
      
    }
    printf("%d is the final sum",sum);

    return 0;
}
