#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>

char time_limit[10] = "00:05:00";
double secondsTotal = 5;

void catchSIGINT(int);

void set_stopwatch() {
    signal(SIGINT, catchSIGINT);

    int timer_fd = -1;

    timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, time_limit, 9);
    write(timer_fd, "disp", 5);
    close(timer_fd);

    while (1) {
        char key_value[2];
        int key;
        int key_fd = open("/dev/KEY", (O_RDWR | O_SYNC));
        if (key_fd == -1) {
            printf("Error opening /dev/KEY: %s\n", strerror(errno));
            return;
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

        if ((key & 0x1) != 0) {
            // Game Start!
            break;
        }

        if ((key & 0xE) != 0) {
            timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
            if (timer_fd == -1) {
                printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
                return;
            }
            char timer_counter[9];
            read(timer_fd, timer_counter, 9);

            int minute;
            int second;
            int millisecond;
            sscanf(timer_counter, "%d:%d:%d", &minute, &second, &millisecond);

            int sw_fd = open("/dev/SW", (O_RDWR | O_SYNC));
            char sw_value[4];
            int sw_int = 0;
            if (sw_fd == -1) {
                printf("Error opening /dev/SW: %s\n", strerror(errno));
                return;
            }
            read(sw_fd, sw_value, 4);
            close(sw_fd);
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

            if ((key & 0x8) != 0) {
                minute = sw_int;
            }
            if ((key & 0x4) != 0) {
                second = sw_int;
            }
            if ((key & 0x2) != 0) {
                millisecond = sw_int;
            }

            sprintf(time_limit, "%02d:%02d:%02d", minute, second, millisecond);
            secondsTotal = (minute * 60 + second) + millisecond / 1000.0;
            write(timer_fd, time_limit, 10);
            close(timer_fd);
        }
    }
}

void start_time() {
    int timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return;
    }
    write(timer_fd, "run", 4);
    close(timer_fd);
}

void reset_time() {
    int timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return;
    }
    write(timer_fd, time_limit, 9);
    close(timer_fd);
}

double check_remaining_time() {
    int timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return -1;
    }
    char timer_counter[10];
    read(timer_fd, timer_counter, 10);
    close(timer_fd);

    int minute;
    int second;
    int millisecond;
    sscanf(timer_counter, "%d:%d:%d", &minute, &second, &millisecond);

    return minute * 60 + second + millisecond / 1000.0;
}

int generate_problem(int max) {
    int a = rand() % max;
    int b = rand() % max;
    int c = a + b;

    printf("%d + %d = ?\n", a, b);
    return c;
}

/**  your part 4 user code here  **/
int main() {
    int timer_fd = -1;

    srand(time(NULL));

    printf("Set stopwatch if desired. Press KEY0 to start\n");
    set_stopwatch();
    start_time();

    int is_time_up = 0;
    int correct_count = 0;
    double secondsUsedTotal = 0;
    int maxVal = 10;
    int step = 10;
    while (!is_time_up) {
        int ans = generate_problem(maxVal);
        int user_ans = -1;            

        reset_time();
        while (ans != user_ans) {
            scanf("%d", &user_ans);

            int remaining_time = check_remaining_time();
            if (remaining_time <= 0) {
                is_time_up = 1;
                break;
            }

            if (ans != user_ans) {
                printf("Try again: ");
                fflush(stdout);
            } else {
                secondsUsedTotal += secondsTotal - remaining_time;
                correct_count++;

                maxVal += step;
                if (correct_count > 10) {
                    step = 20;
                } else if (maxVal > 10000) {
                    maxVal = 10000;
                }
            }
        }
    }

    printf("Time expired! You answered %d questions, in an average of %.2lf second\n", correct_count, secondsUsedTotal / correct_count);

    timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return -1;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, "nodisp", 7);
    close(timer_fd);

    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
    int timer_fd = open("/dev/stopwatch", (O_RDWR | O_SYNC));
    if (timer_fd == -1) {
        printf("Error opening /dev/stopwatch: %s\n", strerror(errno));
        return;
    }
    write(timer_fd, "stop", 5);
    write(timer_fd, "nodisp", 7);
    close(timer_fd);

    exit(0);
}
