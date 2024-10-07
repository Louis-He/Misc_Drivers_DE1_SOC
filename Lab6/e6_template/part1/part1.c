#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/**  your part 1 user code here  **/
#define video_BYTES 8 // number of characters to read from /dev/video

int screen_x, screen_y;

int main(int argc, char *argv[])
{
    int video_FD; // file descriptor
    char buffer[video_BYTES]; // buffer for data read from /dev/video
    char command[64]; // buffer for commands written to /dev/video
    int x, y;
    // Open the character device driver
    if ((video_FD = open("/dev/video", O_RDWR)) == -1) {
        printf("Error opening /dev/video: %s\n", strerror(errno));
        return -1;
    }
    // Set screen_x and screen_y by reading from the driver
    read(video_FD, buffer, video_BYTES);
    sscanf(buffer, "%d %d", &screen_x, &screen_y);

    // printf("Screen resolution: %d x %d\n", screen_x, screen_y);
    sprintf(command, "clear");
    write(video_FD, command, strlen(command));
    
    // Use pixel commands to color some pixels on the screen
    for (x = 0; x < screen_x; x++) {
        for (y = 0; y < screen_y; y++) {
            // int color = 0xFF0F;
            if (x + y == screen_y) {
                int color = 0xFFF0;
                sprintf(command, "pixel %d,%d 0x%x\0", x, y, color);
                write(video_FD, command, strlen(command));
            }
            
        }
    }

    close (video_FD);
    return 0;
}