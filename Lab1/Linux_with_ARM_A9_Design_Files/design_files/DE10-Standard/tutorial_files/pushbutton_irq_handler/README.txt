The pushbutton_irq_handler is a kernel module designed to handle interrupts generated by the 
pushbuttons of the DE10-Standard board. The FPGA must be programmed with the
DE10_Standard_Computer.rbf system (which is programmed by default upon booting). 
When the module is first loaded, the LEDs get set to 0x200, which turns on the leftmost LED. 
The module registers an interrupt handler function that listens for interrupts coming from 
the pushbutton (interrupt is generated whenever a button is pressed) and increments the value 
displayed on the LEDs by 1. 

To compile the kernel module use the following command:

  make

To run the module (insert the module into the kernel):

  insmod pushbutton_irq_handler.ko

To stop the module (remove from kernel):

  rmmod pushbutton_irq_handler
