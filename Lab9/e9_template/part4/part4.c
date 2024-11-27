#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include "address_map_arm.h"
#include "physical.h"
#include "defines.h"
#include "video.h"
#include "SW.h"

#define X_MAX 320
#define Y_MAX 240
#define SIG_MAX 4096
#define EDGE_THRESHOLD 4096/2
#define WHITE 0xFFFF
/** timer data structures **/
struct itimerspec interval_timer_start = {
    .it_interval = {.tv_sec=0,.tv_nsec=SAMPLING_PERIOD_NS},
    .it_value = {.tv_sec=0,.tv_nsec=SAMPLING_PERIOD_NS}};

struct itimerspec interval_timer_stop  = {
    .it_interval = {.tv_sec=0,.tv_nsec=0},
    .it_value = {.tv_sec=0,.tv_nsec=0}};

int run = 1;
void* lw_bridge_virtual = NULL;
volatile unsigned int * adc_ptr = NULL;
volatile unsigned int * KEY_ptr = NULL;
volatile int * SW_ptr = NULL;
int adc_fd   = -1;
int video_FD = -1;

int zoom_out_level = 1;
timer_t interval_timer_id;
unsigned record_idx = 0;
unsigned short records[X_MAX];
unsigned short prev_record = 0;

void catchSIGINT(int);
void draw_waveform(void);
void draw_line(int x0, int x1, int y0, int y1, short int color);
void clean_screen(void);

/** timeout handler **/
void timeout_handler(int signo) {
    bool is_posedge = false;
    bool is_negedge = false;

    int SW_value = *SW_ptr;
    SW_value = SW_value & 0x1;
    bool detect_posedge = (SW_value == 1);

    unsigned short curr_record = (*(adc_ptr)) & 0xFFF;
    if (curr_record > prev_record + EDGE_THRESHOLD) {
        is_posedge = true;
    } else if (curr_record < prev_record - EDGE_THRESHOLD) {
        is_negedge = true;
    }
    prev_record = curr_record;

    unsigned y_cood = Y_MAX - curr_record * Y_MAX / SIG_MAX;

    int key_value = *(KEY_ptr + 3);
    // clear edge register
    *(KEY_ptr + 3) = key_value;

    if ((key_value & 0x1) != 0) {
        zoom_out_level = (zoom_out_level == 8) ? 8 : zoom_out_level + 1;
        printf("Zoom out, level %d\n", zoom_out_level);
    }
    if ((key_value & 0x2) != 0) {
        zoom_out_level = (zoom_out_level == 1) ? 1 : zoom_out_level - 1;
        printf("Zoom in, level %d\n", zoom_out_level);
    }
    if ((key_value & 0x3) != 0) {
        // turn off timer
        timer_settime(interval_timer_id, 0, &interval_timer_stop, NULL);

        interval_timer_start.it_interval.tv_nsec=SAMPLING_PERIOD_NS * zoom_out_level;
        interval_timer_start.it_value.tv_nsec=SAMPLING_PERIOD_NS * zoom_out_level;

        // turn timer back on
        timer_settime(interval_timer_id, 0, &interval_timer_start, NULL);
    }

    if (record_idx == 0) {
        // check if we need to start record a new waveform
        if ((detect_posedge && is_posedge) || (!detect_posedge && is_negedge)) {
            records[record_idx] = y_cood;
            record_idx++;
        }
    } else {
        if (record_idx == X_MAX) {
            // stop recording
            record_idx = 0;

            // turn off timer
            timer_settime(interval_timer_id, 0, &interval_timer_stop, NULL);

            // plot the waveform
            draw_waveform();

            interval_timer_start.it_interval.tv_nsec=SAMPLING_PERIOD_NS * zoom_out_level;
            interval_timer_start.it_value.tv_nsec=SAMPLING_PERIOD_NS * zoom_out_level;

            // turn timer back on
            timer_settime(interval_timer_id, 0, &interval_timer_start, NULL);
            return;
        }

        // continue to record the waveform
        records[record_idx] = y_cood;
        record_idx++;
    }

    // turn off timer
}

void draw_waveform(void) {
    clean_screen();
    unsigned i;
    for (i = 0; i < X_MAX - 1; i++) {
        draw_line(i, i + 1, records[i], records[i + 1], WHITE);
    }

    char command[64];
    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
}

void draw_line(int x0, int x1, int y0, int y1, short int color) {
    char command[64];
    sprintf (command, "line %d,%d %d,%d 0x%x\n", x0, y0, x1, y1, color); 
    write (video_FD, command, strlen(command));
}

void clean_screen() {
    char command[64];
    sprintf (command, "clear\n"); 
    write (video_FD, command, strlen(command));
}

int main(int argc, char* argv[])
{
    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    // setup ADC and LW bridge
    if ((adc_fd = open_physical (adc_fd)) == -1)
        return (-1);
    else if ((lw_bridge_virtual = map_physical (adc_fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == 0)
        return (-1);

    adc_ptr = (unsigned int *)(lw_bridge_virtual + ADC_BASE);
    // enable auto update
    *(adc_ptr + 1) = 0x1;

    // set the switch
    SW_ptr = lw_bridge_virtual + SW_BASE;

    // set the key
    KEY_ptr = lw_bridge_virtual + KEY_BASE;
    *(KEY_ptr + 3) = 0xF; 
    /**  please complete the main function **/

    // Set up the signal handling (version provided in lab instructions)
    struct sigaction act;
    sigset_t set;
    sigemptyset (&set);
    sigaddset (&set, SIGALRM);
    act.sa_flags = 0;
    act.sa_mask = set;
    act.sa_handler = &timeout_handler;
    sigaction (SIGALRM, &act, NULL);

    // set up signal handling (shorter version shown in lecture)
    signal(SIGALRM, timeout_handler);

    // Create a monotonically increasing timer
    timer_create (CLOCK_MONOTONIC, NULL, &interval_timer_id);

    // Start the timer
    timer_settime(interval_timer_id, 0, &interval_timer_start, NULL);

    signal(SIGINT, catchSIGINT);
    // // Stopping the timer
    // timer_settime(interval_timer_id, 0, &interval_timer_stop, NULL);
    while (run) {

    }

    return 0;
}

void catchSIGINT(int signo) {
    printf("Caught %d\n", signo);
    run = 0;
    timer_settime(interval_timer_id, 0, &interval_timer_stop, NULL);
    unmap_physical ((void*) lw_bridge_virtual, LW_BRIDGE_SPAN);
    close_physical(adc_fd);
    clean_screen();
    char command[64];
    sprintf (command, "sync\n");
    write (video_FD, command, strlen(command));
    close(video_FD);
    exit(0);
}