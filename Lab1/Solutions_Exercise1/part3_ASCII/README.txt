Part3.c is a kernel module designed to handle interrupts generated 
by the KEY pushbuttons of the DE10-Nano. The FPGA must be programmed with the 
DE10_Nano_Computer.rbf system (which is programmed by default upon booting). 
When the module is first loaded, the LEDs get set to 0x200, which turns on 
the leftmost LED. Also, the Terminal window shows the decimal value of the 
rightmost four red LEDs. The module registers an interrupt handler function 
that listens for interrupts coming from the KEY pushbuttons (interrupt is 
generated whenever a button is pressed) and increments the value displayed 
on the LEDs and Terminal window by 1. 

To compile the kernel module use the following command:

  make

To run the module (insert the module into the kernel):

  insmod part3.ko

To stop the module (remove from kernel):

  rmmod part3
