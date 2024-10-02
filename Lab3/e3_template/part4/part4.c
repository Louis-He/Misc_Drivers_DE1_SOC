#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

volatile sig_atomic_t stop = 0;
void catchSIGINT(int);


/**  your part 4 user code here  **/
int main() {
    int key_fd = -1;
    int sw_fd = -1;

    int ledr_fd = -1;
    int hex_fd = -1;

    int accu_sum = 0;

    ledr_fd = open("/dev/LEDR", (O_RDWR | O_SYNC));
    if (ledr_fd == -1) {
        printf("Error opening /dev/LEDR: %s\n", strerror(errno));
        return -1;
    }

    hex_fd = open("/dev/HEX", (O_RDWR | O_SYNC));
    if (hex_fd == -1) {
        printf("Error opening /dev/HEX: %s\n", strerror(errno));
        return -1;
    }

    char hex_value[6];
    sprintf(hex_value, "%06d", accu_sum);
    write(hex_fd, hex_value, 6);

    while (!stop) {
        char key_value[2];
        key_fd = open("/dev/KEY", (O_RDWR | O_SYNC));
        if (key_fd == -1) {
            printf("Error opening /dev/KEY: %s\n", strerror(errno));
            return -1;
        }
        read(key_fd, key_value, 2);
        close(key_fd);

        // printf("Key pressed: %c\n", key_value[0]);

        if (key_value[0] == '0') { continue; }

        printf("Key pressed: %c\n", key_value[0]);

        // if any key pressed
        char sw_value[4];

        sw_fd = open("/dev/SW", (O_RDWR | O_SYNC));
        if (sw_fd == -1) {
            printf("Error opening /dev/SW: %s\n", strerror(errno));
            return -1;
        }
        read(sw_fd, sw_value, 4);
        close(sw_fd);

        write(ledr_fd, sw_value, 4);

        int sw_int = 0;
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

        accu_sum += sw_int;
        char hex_value[6];
        sprintf(hex_value, "%06d", accu_sum);
        write(hex_fd, hex_value, 6);
    }

    return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}