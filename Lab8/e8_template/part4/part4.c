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

#define video_BYTES 8

#define BALCK 0x0000
#define RED 0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define BLUE 0x001F
#define MAGENTA 0xF81F
#define CYAN 0x07FF
#define WHITE 0xFFFF
/**  your part 3 user code here  **/

void catchSIGINT(int);

void* lw_bridge_virtual;
int fd = -1;
int keyboard_fd = -1;
int video_FD = -1;
int screen_x, screen_y;
static int run = 1;

double NOTES_FREQ[] = {MIDC, DFLAT, DNAT, EFLAT, ENAT, FNAT, GFLAT, GNAT, AFLAT, ANAT, BFLAT, BNAT, HIC};
int KEY_CODE[] = {0x10, 0x3, 0x11, 0x4, 0x12, 0x13, 0x6, 0x14, 0x7, 0x15, 0x8, 0x16, 0x17};

static bool key_pressed[13];
static double key_val[13];

void plot_pixel(int, int, short);
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

int set_processor_affinity(unsigned int core){
    cpu_set_t cpuset;
    pthread_t current_thread = pthread_self();
    if (core >= sysconf(_SC_NPROCESSORS_ONLN)){
        printf("CPU Core %d does not exist!\n", core);
        return -1;
    }

    // Zero out the cpuset mask
    CPU_ZERO(&cpuset);
    // Set the mask bit for specified core
    CPU_SET(core, &cpuset);
    return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
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
    set_processor_affinity(0);
    // // TODO need to update this function
    int num_ones = 0;
    int i;
    int time_stamp = 0;

    while (run) {
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

void render_thread() {
    set_processor_affinity(1);
    int i, j;
    int x, y, y_mid;
    y_mid = screen_y - j + screen_y / 2;
    char clearcommand[64]; // buffer for command data
    char synccommand[64];
    sprintf(clearcommand, "clear");
    sprintf(synccommand, "sync");
    while (run) {
        write (video_FD, clearcommand, strlen(clearcommand));
        for (i = 0; i < screen_x; i++) {
            x = i;
            double val = 0;
            for (j = 0; j < 13; j++) {
                if (key_val[j] < 0.001) {
                    continue;
                }

                double sample_val = sin(NOTES_FREQ[j] * x) * key_val[j];
                val += sample_val / 12;
            }

            y = y_mid + val * screen_y / 2.0;

            // plot pixel
            // printf("Plotting pixel at %d, %d\n", x, y);
            plot_pixel(x, y, GREEN);
        }
        write (video_FD, synccommand, strlen(synccommand));
    }
}

int main(int argc, char *argv[]) {
    signal(SIGINT, catchSIGINT);

    if (argc != 2) {
        printf("Usage: %s <keyboard path>\n", argv[0]);
        return -1;
    }

    if (map_mem() == -1) {
        printf("Error: could not map memory\n");
        return -1;
    }

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video");
        return -1;
    }
    char video_buffer[video_BYTES];
    read(video_FD, video_buffer, video_BYTES);
    sscanf(video_buffer, "%d %d", &screen_x, &screen_y);

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

    pthread_t render_thread_id;
    pthread_create(&render_thread_id, NULL, render_thread, NULL);

    // the main thread will handle the keyboard
    if ((keyboard_fd = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1) {
        printf ("Could not open %s\n", argv[1]);
        return -1;
    }

    set_processor_affinity(1);

    struct input_event ev;
    int event_size = sizeof (struct input_event);
    while (run)
    {
        if (read (keyboard_fd, &ev, event_size) < event_size) {
            continue;
        }

        if (ev.type == EV_KEY && ev.value == KEY_PRESSED){
            // printf("Pressed key: 0x%04x\n", (int)ev.code);
            int idx = note_idx(ev.code);
            if (idx != -1) {
                key_pressed[idx] = true;
            }
        } else if (ev.type == EV_KEY && ev.value == KEY_RELEASED){
            // printf("Released key: 0x%04x\n", (int)ev.code);
            int idx = note_idx(ev.code);
            if (idx != -1) {
                key_pressed[idx] = false;
            }
        }
    }
    
    //join threads
    pthread_join(audio_thread_id, NULL);
    pthread_join(render_thread_id, NULL);
    
    char command[64]; // buffer for command data
    sprintf (command, "clear\n"); 
    write (video_FD, command, strlen(command));

    sprintf (command, "erase\n"); 
    write (video_FD, command, strlen(command));

    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));

    close(video_FD);
    close(keyboard_fd);

    unmap_mem();
    return 0;
}

void plot_pixel(int x, int y, short color) {
    char command[64];
    sprintf (command, "pixel %d,%d 0x%x\n", x, y, color); 
    write (video_FD, command, strlen(command));
}

void catchSIGINT(int signum) {
    run = 0;
}