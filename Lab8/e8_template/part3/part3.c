#define _GNU_SOURCE
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
#include <stdbool.h>

#define KEY_RELEASED 0
#define KEY_PRESSED 1

/**  your part 3 user code here  **/

void* lw_bridge_virtual;
int fd = -1;

double NOTES_FREQ[] = {MIDC, DFLAT, DNAT, EFLAT, ENAT, FNAT, GFLAT, GNAT, AFLAT, ANAT, BFLAT, BNAT, HIC};
int KEY_CODE[] = {0x10, 0x3, 0x11, 0x4, 0x12, 0x13, 0x6, 0x14, 0x7, 0x15, 0x8, 0x16, 0x17};

static bool key_pressed[13];
static double key_val[13];

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

int note_idx(int key_code) {
    int i;
    for (i = 0; i < 13; i++) {
        if (KEY_CODE[i] == key_code) {
            return i;
        }
    }

    return -1;
}

void audio_thread() {
    // // TODO need to update this function
    int num_ones = 0;
    int i;
    int time_stamp = 0;

    while (true) {
        num_ones = 0;
        for (i = 0; i < 13; i++) {
            key_val[i] = (key_pressed[i] ? 1 : key_val[i] * 0.9995);
            if (key_val[i]) {
                num_ones++;
            }
        }

        volatile unsigned int * audio_ptr = (unsigned int *) (lw_bridge_virtual + AUDIO_BASE);
        double val = 0;
        for (i = 0; i < 13; i++) {
            if (key_val[i] < 0.0001) {
                continue;
            }

            double sample_val = sin(NOTES_FREQ[i] * time_stamp) * key_val[i];
            val += (MAX_VOLUME / 13 * sample_val);
        }

        // printf("val: %f\n", val);

        volatile unsigned int* audio_fifo_space = (unsigned int *)(lw_bridge_virtual + AUDIO_BASE + 0x4);
        while ((*audio_fifo_space & 0xF0000000) < 1) {
            audio_fifo_space = (unsigned int *)(lw_bridge_virtual + AUDIO_BASE + 0x4);
        }

        // double sample_val = sample(NOTES_FREQ[i], time);
        // double val = MAX_VOLUME * sample_val;
        *(audio_ptr + 2) = (int) (val);
        *(audio_ptr + 3) = (int) (val);
        time_stamp ++;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <keyboard path>\n", argv[0]);
        return -1;
    }

    if (map_mem() == -1) {
        printf("Error: could not map memory\n");
        return -1;
    }

    // initialize key_pressed and key_val
    size_t i;
    for (i = 0; i < 13; i++)
    {
        key_pressed[i] = false;
        key_val[i] = 0;
    }
    
    // create a thread for audio
    pthread_t audio_thread_id;
    pthread_create(&audio_thread_id, NULL, audio_thread, NULL);

    // the main thread will handle the keyboard
    if ((fd = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1) {
        printf ("Could not open %s\n", argv[1]);
        return -1;
    }

    struct input_event ev;
    int event_size = sizeof (struct input_event);
    while (1)
    {
        if (read (fd, &ev, event_size) < event_size) {
            continue;
        }

        if (ev.type == EV_KEY && ev.value == KEY_PRESSED){
            printf("Pressed key: 0x%04x\n", (int)ev.code);
            int idx = note_idx(ev.code);
            if (idx != -1) {
                key_pressed[idx] = true;
            }
        } else if (ev.type == EV_KEY && ev.value == KEY_RELEASED){
            printf("Released key: 0x%04x\n", (int)ev.code);
            int idx = note_idx(ev.code);
            if (idx != -1) {
                key_pressed[idx] = false;
            }
        }
    }
    
    unmap_mem();
    return 0;
}