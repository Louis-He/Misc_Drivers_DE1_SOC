#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include "../include/address_map_arm.h"

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);
// used to exit the program cleanly
volatile sig_atomic_t stop = 0;
void catchSIGINT(int);


char  MESSAGE[] = "     Intel SoC FPGA      ";
unsigned char  SEGMENT_MESSAGE[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x04, 0x54, 0x78, 0x79, 0x38, 0x0,
                                        0x6D, 0x5C, 0x39, 0x0,
                                        0x71, 0x73, 0x7D, 0x77, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
unsigned lowest_message_idx = 5;


int main(void)
{
	int fd = -1;
	fd = open_physical(fd);
	void* base_ptr = map_physical(fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

	volatile unsigned int * KEY_ptr = (unsigned int *) (base_ptr + KEY_BASE + 0xC);
	*KEY_ptr = 0xF;

	unsigned int * HEX3_0_ptr = (unsigned int *) (base_ptr + HEX3_HEX0_BASE);
	unsigned int * HEX5_4_ptr = (unsigned int *) (base_ptr + HEX5_HEX4_BASE);

	printf("\e[2J\e[10;10H----------\e[12;10H----------\n");
    fflush(stdout);

	while (1) {
		// HEX
		*(HEX3_0_ptr) = SEGMENT_MESSAGE[lowest_message_idx+5] | SEGMENT_MESSAGE[lowest_message_idx+4] << 8 |
		SEGMENT_MESSAGE[lowest_message_idx+3] << 16  | SEGMENT_MESSAGE[lowest_message_idx+2] << 24;
		*(HEX5_4_ptr) = SEGMENT_MESSAGE[lowest_message_idx+1] | SEGMENT_MESSAGE[lowest_message_idx] << 8;

        printf("\e[11;10H| %c%c%c%c%c%c |",
               MESSAGE[lowest_message_idx], MESSAGE[lowest_message_idx+1], 
               MESSAGE[lowest_message_idx+2], MESSAGE[lowest_message_idx+3], 
               MESSAGE[lowest_message_idx+4], MESSAGE[lowest_message_idx+5]);
        fflush(stdout);

		struct timespec ts;
		ts.tv_sec = 0;										// used to delay
		ts.tv_nsec = 200000000;								// 10^8 ns = 0.1 sec
		nanosleep (&ts, NULL);


		// KEY
		unsigned int edge_reg = *KEY_ptr & 0xF;
		if (edge_reg != 0) {
			stop ^= 0x1; 
			*KEY_ptr = edge_reg;  // Clear the interrupt
		}

		if (!stop) {
			lowest_message_idx = (lowest_message_idx + 1) % 20;
		}

	}
	

	// EXIT
	*(HEX3_0_ptr) = 0;
	*(HEX5_4_ptr) = 0;
	unmap_physical (base_ptr, LW_BRIDGE_SPAN);	// release the physical-memory mapping
	close_physical (fd);	// close /dev/mem
	printf ("\nExiting program\n");
	return 0;
}


/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}