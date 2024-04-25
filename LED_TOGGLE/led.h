#include <24FJ128GA006.h>
#device ICSP=1
#use delay(internal=8000000)

#FUSES NOWDT                 	//No Watch Dog Timer
#FUSES CKSFSM                	//Clock Switching is enabled, fail Safe clock monitor is enabled


#use FIXED_IO( B_outputs=PIN_B5 )


