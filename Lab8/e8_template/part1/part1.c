#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <physical.h>
#include <linux/input.h>
#include <pthread.h>
#include "address_map_arm.h"
#include "defines.h"

void* lw_bridge_virtual;
int fd = -1;

double NOTES_FREQ[] = {MIDC, DFLAT, DNAT, EFLAT, ENAT, FNAT, GFLAT, GNAT, AFLAT, ANAT, BFLAT, BNAT, HIC};

/**  your part 1 user code here  **/
int map_mem() {

    // open /dev/mem
    if ((fd = open_physical (fd)) == -1)
        return (-1);
    else if ((lw_bridge_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == 0)
        return (-1);

    return 0;

}

void unmap_mem() {
    unmap_physical ((void*) lw_bridge_virtual, LW_BRIDGE_SPAN);
    close_physical(fd);
}



int main() {
    if (map_mem() == -1) {
        printf("Error: could not map memory\n");
        return -1;
    }

    double time_step = 1.0 / SAMPLING_RATE;
    double time_end = 0.3; // last 0.3 seconds
    int i;
    int j;
    for (i = 0; i < 13; i++) {
        for (j = 0; j < SAMPLING_RATE * time_end; j++) {
            volatile unsigned int * audio_ptr = (unsigned int *) (lw_bridge_virtual + AUDIO_BASE);
            double sample_val = sin(NOTES_FREQ[i] * j);
            double val = MAX_VOLUME * sample_val / 3;

            volatile unsigned int* audio_fifo_space = (unsigned int *)(lw_bridge_virtual + AUDIO_BASE + 0x4);
            while ((*audio_fifo_space & 0xF0000000) < 1) {
                audio_fifo_space = (unsigned int *)(lw_bridge_virtual + AUDIO_BASE + 0x4);
            }

            *(audio_ptr + 2) = (int) (val);
            *(audio_ptr + 3) = (int) (val);
        }
    }

    unmap_mem();
    return 0;
}