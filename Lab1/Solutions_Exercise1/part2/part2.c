#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include "../address_map_arm.h"

#define	LEFT	0
#define	RIGHT	1

/* Prototypes for functions used to access physical memory addresses */
int open_physical (int);
void * map_physical (int, unsigned int, unsigned int);
void close_physical (int);
int unmap_physical (void *, unsigned int);

void shift_pattern(unsigned int *);

unsigned LED_pattern;								// initial LED pattern
int shift_dir;											// initial LED shift direction

// used to exit the program cleanly
volatile sig_atomic_t stop;
void catchSIGINT(int);

/* 
 * This program displays a sweeping red light on LEDR, which moves left and right
*/

int main(void)
{
	volatile unsigned int * LEDR_ptr;	// virtual pointer to LEDs
	int fd = -1;				// used to open /dev/mem for access to physical addresses
	void *LW_virtual;			// used to map physical addresses for the light-weight bridge
	struct timespec ts;
	time_t start_time, elapsed_time;

	// catch SIGINT from ctrl+c, instead of having it abruptly close this program
   signal(SIGINT, catchSIGINT);
	start_time = time (NULL);

	// Create access to the FPGA light-weight bridge
	if ((fd = open_physical (fd)) == -1)
		return (-1);
	else if ((LW_virtual = map_physical (fd, LW_BRIDGE_BASE, LW_BRIDGE_SPAN)) == NULL)
		return (-1);

	// Set virtual address pointers to the I/O ports
	LEDR_ptr = (unsigned int *) (LW_virtual + LEDR_BASE);

	LED_pattern = 1;									// initial pattern
	shift_dir = LEFT;
	ts.tv_sec = 0;										// used to delay
	ts.tv_nsec = 100000000;							// 10^8 ns = 0.1 sec

	while (!stop)
	{
		*LEDR_ptr = LED_pattern;					// light up the LEDs
		shift_pattern (&LED_pattern);

		/* wait for timer */
		nanosleep (&ts, NULL);
		elapsed_time = time (NULL) - start_time;
		if (elapsed_time > 12 || elapsed_time < 0) stop = 1;
	}
	*LEDR_ptr = 0;			// turn off the LEDs
	unmap_physical (LW_virtual, LW_BRIDGE_SPAN);	// release the physical-memory mapping
	close_physical (fd);	// close /dev/mem
	printf ("\nExiting sample solution program\n");
	return 0;
}

/* Function to allow clean exit of the program */
void catchSIGINT(int signum)
{
	stop = 1;
}

/* shifts the pattern displayed on the LEDs */
void shift_pattern(unsigned *LED_pattern)
{
	if (shift_dir == RIGHT)
		// shift right
		if (*LED_pattern & 0b0000000001)
			shift_dir = LEFT;						// reverse shift direction
		else
			*LED_pattern = *LED_pattern >> 1;
	else
		// left shift
		if (*LED_pattern & 0b1000000000)
			shift_dir = RIGHT;						// reverse shift direction
		else
			*LED_pattern = *LED_pattern << 1;
}

// Open /dev/mem, if not already done, to give access to physical addresses
int open_physical (int fd)
{
	if (fd == -1)
		if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1)
		{
			printf ("ERROR: could not open \"/dev/mem\"...\n");
			return (-1);
		}
	return fd;
}

// Close /dev/mem to give access to physical addresses
void close_physical (int fd)
{
	close (fd);
}

/*
 * Establish a virtual address mapping for the physical addresses starting at base, and
 * extending by span bytes.
 */
void* map_physical(int fd, unsigned int base, unsigned int span)
{
	void *virtual_base;

	// Get a mapping from physical addresses to virtual addresses
	virtual_base = mmap (NULL, span, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, base);
	if (virtual_base == MAP_FAILED)
	{
		printf ("ERROR: mmap() failed...\n");
		close (fd);
		return (NULL);
	}
	return virtual_base;
}

/*
 * Close the previously-opened virtual address mapping
 */
int unmap_physical(void * virtual_base, unsigned int span)
{
	if (munmap (virtual_base, span) != 0)
	{
		printf ("ERROR: munmap() failed...\n");
		return (-1);
	}
	return 0;
}
