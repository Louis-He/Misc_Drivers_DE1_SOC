#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define video_BYTES 8

int run = 1; // used to exit the program cleanly
void catchSIGINT(int);

/**  your part 3 user code here  **/
int screen_x, screen_y;
int main() {
    signal(SIGINT, catchSIGINT);

    int video_FD; // file descriptor
    char video_buffer[video_BYTES]; // buffer for video char data
    char command[64]; // buffer for command data

    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }

    // Read VGA screen size from the video driver
    // Set screen_x and screen_y by reading from the driver
    read(video_FD, video_buffer, video_BYTES);
    sscanf(video_buffer, "%d %d", &screen_x, &screen_y);

    /* Draw a few lines */
    int i = 0;
    int dir = 1;
    while (run) {
        write(video_FD, command, strlen(command));

        sprintf (command, "line %d,%d %d,%d 0x%x\n", 0, i, screen_x - 1,
            i, 0x0); 
        write (video_FD, command, strlen(command));

        i += dir;

        sprintf (command, "line %d,%d %d,%d 0x%x\n", 0, i, screen_x - 1,
            i, 0x07E0); 
        write (video_FD, command, strlen(command));
        sprintf(command, "sync\n");

        if (i == 0 || i == screen_y) {
            dir = -dir;
        }
    }

    printf("Exiting...\n");
    sprintf(command, "clear");
    write (video_FD, command, strlen(command));

    sprintf(command, "sync");
    write (video_FD, command, strlen(command));

    close (video_FD);
    return 0;
}

void catchSIGINT(int signum) {
    run = 0;
}