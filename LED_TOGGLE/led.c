#include <led.h>



void main()
{
   



   while(TRUE)
   {
      output_bit(PIN_B5 , 1);
      delay_ms(1000);
      output_bit(PIN_B5 , 0);
      //TODO: User Code
   }

}
