This program increments the value displayed on the red LEDs of the DE10-Standard board by 1. 
For this program to function, the FPGA must be programmed with the DE10_Standard_Computer.rbf
programming file, and the lightweight HPS2FPGA bridge must be enabled.

To compile the program use the following command:

  gcc increment_leds.c -o increment_leds

Execute using:

  ./increment_leds
