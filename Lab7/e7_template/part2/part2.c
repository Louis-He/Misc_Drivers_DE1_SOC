#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "ADXL345.h"
#include "accel_wrappers.h"

/**             your part 2 user code here                   **/
/**  hint: you can call functions from ../accel_wrappers.c   **/
void catchSIGINT(int);

static int run = 1;

int main(void) {
    signal(SIGINT, catchSIGINT);
    
    accel_open();
    printf("Opened accel\n");
    accel_init();
    printf("Initialized accel\n");

    int ready, x, y, z, mg_per_lsb;

    while (run)
    {
        accel_read(&ready, &x, &y, &z, &mg_per_lsb);
        printf("ready: %d, x: %d, y: %d, z: %d, mg_per_lsb: %d\n", ready, x, y, z, mg_per_lsb);

        usleep(20000);
    }

    accel_close();
    printf("Closed accel\n");
    
    return 0;
}

void catchSIGINT(int signum) {
    run = 0;
}