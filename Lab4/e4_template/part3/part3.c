#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

/**  your part 3 user code here  **/
volatile sig_atomic_t stop = 0;
int run = 1;
void catchSIGINT(int);

int main() {
    signal(SIGINT, catchSIGINT);

    int key_fd = -1;
    int sw_fd = -1;

    int timer_fd = -1;
    int ledr_fd = -1;

    ledr_fd = open("/dev/LEDR", (O_RDWR | O_SYNC));
    if (ledr_fd == -1) {
        printf("Error opening /dev/LEDR: %s\n", strerror(errno));
        return -1;
    }

    timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return -1;
    }
    write(timer_fd, "disp", 5);
    write(timer_fd, "run", 4);
    close(timer_fd);

    while (!stop) {
        char key_value[2];
        int key;
        char sw_value[4];
        int sw_int = 0;

        sw_fd = open("/dev/SW", (O_RDWR | O_SYNC));
        if (sw_fd == -1) {
            printf("Error opening /dev/SW: %s\n", strerror(errno));
            return -1;
        }
        read(sw_fd, sw_value, 4);
        close(sw_fd);

        write(ledr_fd, sw_value, 4);

        int i = 0;
        for (i = 0; i < 3; i++) {
            if (sw_value[i] >= '0' && sw_value[i] <= '9') {
                sw_int = (sw_int << 4) | (sw_value[i] - '0');
            } else if (sw_value[i] >= 'A' && sw_value[i] <= 'F') {
                sw_int = (sw_int << 4) | (sw_value[i] - 'A' + 10);
            } else if (sw_value[i] >= 'a' && sw_value[i] <= 'f') {
                sw_int = (sw_int << 4) | (sw_value[i] - 'a' + 10);
            }
        }


        key_fd = open("/dev/KEY", (O_RDWR | O_SYNC));
        if (key_fd == -1) {
            printf("Error opening /dev/KEY: %s\n", strerror(errno));
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

        if (key == 0) continue;

        if ((key & 0x1) != 0) {
            run = !run;
            timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
            if (timer_fd == -1) {
                printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
                return -1;
            }
            write(timer_fd, run ? "run" : "stop", run ? 4 : 5);
            close(timer_fd);
        }

        if ((key & 0xE) != 0) {
            timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
            if (timer_fd == -1) {
                printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
                return -1;
            }
            char timer_counter[10];
            read(timer_fd, timer_counter, 10);

            // printf("Timer counter: %s\n", timer_counter);
            int minute;
            int second;
            int millisecond;
            sscanf(timer_counter, "%d:%d:%d", &minute, &second, &millisecond);

            if ((key & 0x8) != 0) {
                minute = sw_int;
            }
            if ((key & 0x4) != 0) {
                second = sw_int;
            }
            if ((key & 0x2) != 0) {
                millisecond = sw_int;
            }

            char new_timer_counter[10];
            // printf("New timer counter: %02d:%02d:%02d\n", minute, second, millisecond);
            sprintf(new_timer_counter, "%02d:%02d:%02d", minute, second, millisecond);
            write(timer_fd, new_timer_counter, 10);
            close(timer_fd);
        }
    }

    timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return -1;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, "nodisp", 7);
    close(timer_fd);

    write(ledr_fd, "0000", 4);
    close(ledr_fd);

    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
    run = 0;
    stop = 1;
}
