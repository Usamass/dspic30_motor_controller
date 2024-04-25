#include <30F4011.h>
#device ICSP=1
//!#use delay(clock=40000000,crystal=8000000)
#use delay(clock=40000000, Aux: crystal=8000000)

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES CKSFSM                   //Clock Switching is enabled, fail Safe clock monitor is enabled
#FUSES LPOL_LOW                 //Low-Side Transistors Polarity is Active-Low (PWM 0,2,4 and 6)
#FUSES PWMPIN                   //PWM outputs disabled upon Reset





