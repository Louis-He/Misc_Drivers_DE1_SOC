#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>

#define video_BYTES 8 

/**  your part 2 user code here  **/
int screen_x, screen_y;
int main(int argc, char *argv[])
{
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
    for (i = 0 ; i < 20; i++) {
        sprintf (command, "line %d,%d %d,%d 0x%x\n", 0, i, screen_x - 1,
        i, 0x07E0); 
        write (video_FD, command, strlen(command));
    }
    
    for (i = 20 ; i < 40; i++) {
        sprintf (command, "line %d,%d %d,%d 0x%x\n", 0, i, screen_x - 1,
        i, 0xFFE0); 
        write (video_FD, command, strlen(command));
    }

    for (i = 40 ; i < 60; i++) {
        sprintf (command, "line %d,%d %d,%d 0x%x\n", 0, i, screen_x - 1,
        i+20, 0x07FF); 
        write (video_FD, command, strlen(command));
    }
    
    close (video_FD);
    return 0;
}