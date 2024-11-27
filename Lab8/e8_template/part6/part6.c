#define _GNU_SOURCE
#include <sched.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <linux/input.h>
#include <pthread.h>
#include "address_map_arm.h"
#include "defines.h"
#include "stopwatch.h"
#include "video.h"
#include "KEY.h"
#include "HEX.h"
#include "LEDR.h"
#include "audio.h"


/**  your part 6 user code here  **/

#define KEY_RELEASED 0
#define KEY_PRESSED 1

#define video_BYTES 8
#define audio_BYTES 64
#define NODE_MAX 512

#define BALCK 0x0000
#define RED 0xF800
#define GREEN 0x07E0
#define YELLOW 0xFFE0
#define BLUE 0x001F
#define MAGENTA 0xF81F
#define CYAN 0x07FF
#define WHITE 0xFFFF

#define MAX_STOPWATCH_TIME 59 * 60 * 100 + 59 * 100 + 99 // 59 mins 59 seconds 99 milliseconds
/**  your part 3 user code here  **/

void catchSIGINT(int);

void* lw_bridge_virtual;
int keyboard_fd = -1;
int timer_fd    = -1;
int video_FD    = -1;
int ledr_fd     = -1;
int audio_fd    = -1;
int screen_x, screen_y;
static int run = 1;

bool is_recording = false;
bool is_playing = false;

double NOTES_FREQ[] = {MIDC, DFLAT, DNAT, EFLAT, ENAT, FNAT, GFLAT, GNAT, AFLAT, ANAT, BFLAT, BNAT, HIC};
int KEY_CODE[] = {0x10, 0x3, 0x11, 0x4, 0x12, 0x13, 0x6, 0x14, 0x7, 0x15, 0x8, 0x16, 0x17};

struct audio_note_event {
    unsigned time_stamp;
    unsigned key_pressed; // from index 0 -> 12, 13 notes
};

static bool prev_key_pressed[13];
static bool key_pressed[13];
static double key_val[13];
static struct audio_note_event recorded_notes[NODE_MAX];
static int recorded_num = 0;
static int playing_idx = 0;

void plot_pixel(int, int, short);
unsigned convert_key_array_to_int(bool key_pressed[13]);
unsigned convert_stopwatch_to_timestamp(int minute, int second, int millisecond);

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

        // volatile unsigned int * audio_ptr = (unsigned int *) (lw_bridge_virtual + AUDIO_BASE);
        double val = 0;
        for (i = 0; i < 13; i++) {
            if (key_val[i] < 0.0001) {
                continue;
            }

            double sample_val = sin(NOTES_FREQ[i] * time_stamp) * key_val[i];
            val += (MAX_VOLUME / 13 * sample_val);
        }

        // printf("val: %f\n", val);
        // int val_int = (int)val;
        write(audio_fd, "waitw", 5); 

        // // double sample_val = sample(NOTES_FREQ[i], time);
        // // double val = MAX_VOLUME * sample_val;
        char audio_buffer[audio_BYTES];
        sprintf(audio_buffer, "right 0x%d", (int)val);
        write(audio_fd, audio_buffer, strlen(audio_buffer));

        sprintf(audio_buffer, "left %d", (int)val);
        write(audio_fd, audio_buffer, strlen(audio_buffer));

        // // write(audio_fd, "waitw", 5); 

        

        // printf("%s, len: %d\n", audio_buffer, strlen(audio_buffer));
        
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
        // update the LEDR
        if (is_recording) {
            write(ledr_fd, "0x1", 4);
        } else if (is_playing) {
            write(ledr_fd, "0x2", 4);
        } else {
            write(ledr_fd, "0x0", 4);
        }

        // update the screen
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

    if ((ledr_fd = open("/dev/LEDR", O_RDWR)) == -1) {
        printf("Error opening /dev/LEDR");
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

    // Open audio device
    if ((audio_fd = open("/dev/IntelFPGAUP/audio", O_RDWR)) == -1) {
        printf("Error opening /dev/IntelFPGAUP/audio");
        return -1;
    }
    write(audio_fd, "init", 4);
    write(audio_fd, "rate 8000", 9);

    // initialize key_pressed and key_val
    size_t i;
    for (i = 0; i < 13; i++)
    {
        prev_key_pressed[i] = false;
        key_pressed[i] = false;
        key_val[i] = 0;
    }
    
    // create a thread for audio
    pthread_t audio_thread_id;
    pthread_create(&audio_thread_id, NULL, audio_thread, NULL);

    pthread_t render_thread_id;
    pthread_create(&render_thread_id, NULL, render_thread, NULL);

    // the main thread will handle the keyboard
    // if ((keyboard_fd = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1) {
    //     printf ("Could not open %s\n", argv[1]);
    //     return -1;
    // }

    set_processor_affinity(1);

    // set stopwatch
    if ((timer_fd = open("/dev/stopwatch", O_RDWR)) == -1) {
        printf("Error opening /dev/stopwatch");
        return -1;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, "59:59:99", 9);
    write(timer_fd, "disp", 5);
    close(timer_fd);

    struct input_event ev;
    int event_size = sizeof (struct input_event);
    int key_fd; 
    while (run)
    {
        // printf("Waiting for key press\n");
        char key_value[2];
        int key;
        key_fd = open("/dev/KEY", (O_RDWR | O_SYNC));
        if (key_fd == -1) {
            printf("Error opening /dev/KEY\n");
            return -1;
        }
        read(key_fd, key_value, 2);
        close(key_fd);

        if (key_value[0] >= '0' && key_value[0] <= '9') {
            key = key_value[0] - '0';
        } else if (key_value[0] >= 'A' && key_value[0] <= 'F') {
            key = key_value[0] - 'A' + 10;
        } else if (key_value[0] >= 'a' && key_value[0] <= 'f') {
            key = key_value[0] - 'a' + 10;
        } else {
            printf("Invalid key value: %c\n", key_value[0]);
            continue;
        }

        timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
        if (timer_fd == -1) {
            printf("Error opening /dev/stopwatch\n");
            return;
        }
        char timer_counter[9];
        read(timer_fd, timer_counter, 9);

        int minute;
        int second;
        int millisecond;
        sscanf(timer_counter, "%d:%d:%d", &minute, &second, &millisecond);
        int timer_val_in_millis = convert_stopwatch_to_timestamp(minute, second, millisecond);

        if ((key & 0x1) != 0) {
            // flip recording
            is_recording = !is_recording;
            is_playing = false;
            printf("%s to record\n", is_recording ? "Start" : "Stop");
            if ((keyboard_fd = open (argv[1], O_RDONLY | O_NONBLOCK)) == -1) {
                printf ("Could not open %s\n", argv[1]);
                return -1;
            }
            if (is_recording) {
                write(timer_fd, "59:59:99", 9);
                write(timer_fd, "run", 4);
                recorded_num = 0;
            } else {
                write(timer_fd, "stop", 5);
            }
        } else if ((key & 0x2) != 0) {
            // flip playback
            is_playing = !is_playing;
            is_recording = false;
            printf("%s to playback\n", is_playing ? "start" : "stop");
            close(keyboard_fd);
            
            if (is_playing) {
                write(timer_fd, "59:59:99", 9);
                write(timer_fd, "run", 4);
                minute = 59;
                second = 59;
                millisecond = 99;
                timer_val_in_millis = convert_stopwatch_to_timestamp(minute, second, millisecond);
                playing_idx = 0;
            } else {
                write(timer_fd, "stop", 5);
            }
        }
        close(timer_fd);

        if (is_recording) {
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

            // record the notes if any key is different from the previous key
            bool is_diff = false;
            for (i = 0; i < 13; i++) {
                if (key_pressed[i] != prev_key_pressed[i]) {
                    is_diff = true;
                    break;
                }
            }
            memcpy(prev_key_pressed, key_pressed, sizeof(bool) * 13);
            
            if (is_diff) {
                printf("Recording key change: %d\n", recorded_num);
                recorded_notes[recorded_num] = (struct audio_note_event){timer_val_in_millis, convert_key_array_to_int(key_pressed)};
                recorded_num++;
            }

            if (recorded_num >= NODE_MAX - 1) {
                for (i = 0; i < 13; i++) {
                    key_pressed[i] = false;
                }
                recorded_notes[recorded_num] = (struct audio_note_event){timer_val_in_millis, 0};
                recorded_num++;

                is_recording = false;
                timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
                if (timer_fd == -1) {
                    printf("Error opening /dev/stopwatch\n");
                    return;
                }
                write(timer_fd, "stop", 5);
                close(timer_fd);
            }
        } else if (is_playing) {
            // play the recorded notes
            struct audio_note_event note_event = recorded_notes[playing_idx];
            // printf("Playing note at %d, %u %u\n", playing_idx, note_event.time_stamp, timer_val_in_millis);
            if (note_event.time_stamp >= timer_val_in_millis) {
                int j;
                for (j = 0; j < 13; j++) {
                    key_pressed[j] = (note_event.key_pressed >> j) & 0x1;
                }
                playing_idx++;
            }

            if (playing_idx >= recorded_num) {
                is_playing = false;
                timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
                if (timer_fd == -1) {
                    printf("Error opening /dev/stopwatch\n");
                    return;
                }
                write(timer_fd, "stop", 5);
                close(timer_fd);
            }
        }
    }
    
    printf("Joining threads\n"); 
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
    printf("Closed video\n");

    // set stopwatch
    if ((timer_fd = open("/dev/stopwatch", O_RDWR)) == -1) {
        printf("Error opening /dev/stopwatch");
        return -1;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, "59:59:99", 9);
    write(timer_fd, "nodisp", 7);
    close(timer_fd);

    write(ledr_fd, "0x0", 4); 
    close(ledr_fd);
    close(keyboard_fd);
    return 0;
}

void plot_pixel(int x, int y, short color) {
    char command[64];
    sprintf (command, "pixel %d,%d 0x%x\n", x, y, color); 
    write (video_FD, command, strlen(command));
}

unsigned convert_key_array_to_int(bool key_pressed[13]) {
    unsigned key_val = 0;
    int i;
    for (i = 0; i < 13; i++) {
        key_val |= (key_pressed[i] << i);
    }

    return key_val;
}

unsigned convert_stopwatch_to_timestamp(int minute, int second, int millisecond) {
    return (minute * 60 * 100 + second * 100 + millisecond);
}

void catchSIGINT(int signum) {
    run = 0;
}