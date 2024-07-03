#include <30F4011.h>
#include <Inc/lookup_tables.h>
#DEVICE ADC=10
#device ICSP=1
#use delay(clock=64000000,crystal=8000000)
#use rs232(UART1, baud=57600, RECEIVE_BUFFER=100 , stream=UART_PORT1)

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES CKSFSM                   //Clock Switching is enabled, fail Safe clock monitor is enabled
#FUSES BORV42                   //Brownout reset at 4.5V
#FUSES WRT                      //Program Memory Write Protected
#FUSES PROTECT                  //Code protected from reads
#include <math.h>
#include <stdlib.h>
#define clock_freq 64000000

#define P1TCON    0x01C0
#define P1TMR     0x01C2 
#define P1TPER    0x01C4
#define P1SECMP   0x01C6
#define PWM1CON1  0x01C8 
#define PWM1CON2  0x01CA
#define P1DTCON1  0x01CC
#define P1DTCON2  0x01CE  
#define P1FLTACON 0x01D0
#define P1OVDCON  0x01D4     
#define P1DC1     0x01D6   
#define P1DC2     0x01D8
#define P1DC3     0x01DA
#define IPC14     0x00C0  
#define CLKDIV    0x0744 
#define PLLFBD    0x0746


/*------------QUADRATURE ENCODER REGISTERS-------------------*/
#define QEICON  0x0122       // Control Status Register.
#define DFLTCON 0x0124      // Digital Filter Control Register.
#define POSCNT  0x0126     // Position Count Register.
#define MAXCNT  0x0128    // Maximum Count Register.
#define ADPCFG  0x02A8   // Encoder Multiplexed pins.
#define IEC2    0x0090  // for enabling the QE interrupt.
#define IPC10   0x00A8 // QEI interrupt priority Register.

#define UPDN 3       // Direction testing flag.
/*------------QUADRATURE ENCODER REGISTERS-------------------*/

#define TIM_OVERFLOW_VAL 65535
#define U1TXREG   0x0210
#define U1BRG     0x0214

#define ADC_pin sAN1
#define LED_PIN PIN_C13

#define voltage_offset 1000//1248
#define low_duty_limit 50
#define high_duty_limit 1950
#define SLOPE 0

#define break_level 5
#define sustain_level 200  
#define break_amplitude 0 

#define pedestal_amplitude 500
#define peak_amplitude 950


#define throttle_PIN PIN_B1
#define PWM_tick_pin PIN_B2
#define TIM_tick_pin PIN_B3
#define ENC_TICK_PIN PIN_B0

#define init_freq 30

 
  
void initMCPWM(void);
void fill_sine_table(void);  
int1 QEI_get_direction(void); 

unsigned int16 duty[3]={voltage_offset,voltage_offset,voltage_offset},sample=0;
signed int16 peak_voltage =0;//  1184; 
signed int32 reference[3] = {0,0,0};

const unsigned max_samples=30.0;
signed int16 sine_table[max_samples];
unsigned int16 sine_index,phase_angle[3] = {0 , 0 , 0};  
double theeta;

const unsigned max_freq = 250; //Hz

unsigned int16 raw_adc =0 ;
signed int16 throttle_level = 0;
unsigned int16 freq = 1;

int32 position_count = 0;
int32 prev_count = 0;
int32 enc_count = 0;
int32 position_count_new = 0;
int1 direction_flag = 0;
unsigned int16 position_capture = 0;

int1 tick = 0;
int1 enc_tick = 0;
int1 uart_tick = 0;
int1 drive_inc = 0;
int8 tick_count = 0;
int16 sec_tick = 0;
unsigned long millis_count = 0;
unsigned long uart_millis = 0;

char Serial_OutputBuffer[100];

unsigned int16 ascending_speed , descending_speed , attained_speed ,attained_throttle, loaded_speed , prev_speed;  

int1 descend_flag = 0;
int1 ascend_flag = 0;

static int1 enc_mutx = 0;
#int_PWM1
void  PWM1_isr(void) 
{

   tick_count++;
   if(tick_count >= 8)
   {     
      millis_count++;
      uart_millis++;
//!      sec_tick++;
      tick = 1;
      
      tick_count=0;
   
   }
   if (uart_millis >= 10) 
   {
//!      uart_tick = 1;
      uart_millis = 0;
      
   
   }
   if (millis_count >= 100) {
      enc_tick = 1;
      
      millis_count = 0;
      
   } 


}

#INT_TIMER3
void  timer3_isr(void) 
{
 
   sample = (sample+1)%max_samples;
   phase_angle[0] = sample;
   phase_angle[1] = (sample+10)%max_samples;
   phase_angle[2] = (sample+20)%max_samples; 

   for (int i = 0 ; i < 3 ; i++) {
      
      reference[i] = sine_table[phase_angle[i]];
      reference[i] = reference[i] * peak_voltage; 
      if( reference[i] > 0)
      {
         reference[i] = reference[i] >> 8; 
      }
      else if( reference[i] < 0)
      {
         reference[i] = 0 - reference[i];
         reference[i] = reference[i] >> 8;
         reference[i] = 0 - reference[i];
      }
      
      reference[i] = reference[i] + voltage_offset;
      if(reference[i] > high_duty_limit )
      { 
         reference[i] = high_duty_limit;
      }
      if(reference[i] < low_duty_limit)
      { 
         reference[i] = low_duty_limit;
      }
   }
      
   *P1DC1 = reference[0];  *(P1DC1+1) = reference[0]>>8;
   *P1DC2 = reference[1];  *(P1DC2+1) = reference[1]>>8;
   *P1DC3 = reference[2];  *(P1DC3+1) = reference[2]>>8;

   setup_timer2(TMR_INTERNAL | TMR_DIV_BY_1 | TMR_32_BIT , timer_table[freq]); //need to implement on TIM2 register configuration.
   output_bit(TIM_tick_pin , 0);
   
}

   
void main()
{ 
   

   *U1BRG = 8;    // setting uart baudrate to 115200.
   
   sprintf(Serial_OutputBuffer, "\nMotor Control Unit v0.1\r\n");
   printf(Serial_OutputBuffer);
   
   
   attained_throttle = 20;
   position_capture = 0;
   freq = 1;
   
   initMCPWM();
   fill_sine_table();

   output_drive(LED_PIN);

   setup_adc(ADC_CLOCK_DIV_32);
   setup_adc_ports(ADC_pin);
   set_adc_channel(1);
   delay_us(10);
   
   setup_qei( QEI_MODE_X2 , QEI_FILTER_DIV_1 ,0);
   setup_timer2(TMR_INTERNAL | TMR_DIV_BY_1 | TMR_32_BIT , timer_table[freq]);
   enable_interrupts(INT_TIMER3);   // enable interrupt in timer3 register (in case of 32bit mode) 

   enable_interrupts(INT_PWM1);
   enable_interrupts(INTR_GLOBAL);
   

   while(TRUE)         
   {
      
      
      if (tick) {
         
         raw_adc = read_adc();
         
//!         output_bit(LED_PIN , 1);
         if (raw_adc > 1023) 
         {
            raw_adc = 1023;
         }

         raw_adc = raw_adc -200;
         throttle_level = raw_adc;
        
         
         if (throttle_level > 255)
         {
            throttle_level = 255;
         }
         if (throttle_level < 0)   
         {  
            throttle_level = 0; 
         }
 
          ascending_speed  = ascend_speed_table[attained_throttle];
          descending_speed = descend_speed_table[attained_throttle];  
          loaded_speed = ascend_speed_table[attained_throttle -1];
          
          if (!enc_mutx) 
          {
            attained_speed = position_count_new;      // READING ENCODER's SHARED VARIABLE. 
 
          }
          
          
          if(throttle_level > attained_throttle && attained_speed >= ascending_speed)
          {
            attained_throttle++;

          }
          else if (throttle_level > attained_throttle && attained_speed < loaded_speed)   
          {
            
            attained_throttle--;
            if (attained_throttle < 20) 
            {
               attained_throttle = 20;
               
            }
          
          }
          else if(throttle_level < attained_throttle && attained_speed <= descending_speed)
          {
            
            attained_throttle--; 
            if (attained_throttle < 20)
            {
               attained_throttle = 20;
            }
             
          }
          else if (throttle_level == attained_throttle)
          {
          //
          }
        
         freq = attained_throttle;
         peak_voltage = gain_table[attained_throttle];
         
         prev_speed = attained_speed;
         
         tick = 0;  
//!         output_bit(LED_PIN , 0);
        
      } 
      
      if (uart_tick) 
      {
         
         uart_tick = 0;
      
      }
      if (enc_tick) 
      {
         enc_mutx = 1;
         position_count = *(POSCNT +1);
         position_count = position_count << 8;
         position_count = position_count | *POSCNT;   
         

         direction_flag = QEI_get_direction();

         enc_count = position_count - prev_count;
     
         if (direction_flag == 1 && enc_count < 0) 
         {
            enc_count = enc_count + TIM_OVERFLOW_VAL ;

         }

         if (direction_flag == 0 && enc_count > 0)   
         {
            enc_count = enc_count - TIM_OVERFLOW_VAL ;
         }
   
         enc_count = abs(enc_count);
         enc_count = enc_count >> 1;
         prev_count = position_count;           
         position_count_new = enc_count;
         
         sprintf(Serial_OutputBuffer, "\r\n %d,%d,%d,%d,%d,%d" , throttle_level , attained_throttle,  attained_speed , ascending_speed, descending_speed , loaded_speed);
         printf(Serial_OutputBuffer);
         
         enc_mutx = 0;
         enc_tick = 0;

      }
    } 
}    


void initMCPWM(void) 
{    
   *(P1TCON+1)  =  0x80;  *P1TCON =  0x02;
   *(P1TPER+1)  =  0x03;  *P1TPER =  0xE7;  
   *(P1SECMP+1) =  0x00;  *P1SECMP=  0x01; //
   *(PWM1CON1+1)=  0x00;  *PWM1CON1= 0x77;  
   *(PWM1CON2+1)=  0x00;  *PWM1CON2= 0x02;
   *(P1DTCON1+1)=  0x00;  *P1DTCON1= 0x10; //0x09
   *(P1DTCON2+1)=  0x00;  *P1DTCON2= 0x00;
   *(P1FLTACON+1)= 0x00;  *P1FLTACON=0x00; //0x0000
   *(P1OVDCON+1)=  0x3F;  *P1OVDCON= 0x0F;
   
   *(P1DC1+1) = duty[0]>>8;   *P1DC1 = duty[0]; 
   *(P1DC2+1) = duty[1]>>8;   *P1DC2 = duty[1];
   *(P1DC3+1) = duty[2]>>8;   *P1DC3 = duty[2];  
   *(IPC14+1) =0x00;*(IPC14) =0x70;
}


   
void fill_sine_table(void)
{
   for(sine_index=0;sine_index < max_samples;sine_index++)  
   {
      theeta=sine_index*2.0*PI/max_samples;
      sine_table[sine_index]=255*sin(theeta);
   }
}

int1 QEI_get_direction(void) 
{
   if (*(QEICON +1) & (1 << UPDN)) return 1;
   
   return 0;

}






  
   



//-----------------IMPLEMENATION REQUIRED--------------//

//==========RESPONSE ON BREAK INPUT===========
// DRIVE PWM TO 0 ON BREAK INPUT...


//=========INPUT FREQUENCY VS MOTOR SPEED DIFFERENCE CALCULATION====
/*
   speed  = (freq_table[throttle_level] * 120) / poles
   enc_freq = (speed * 64) / 60
   
   slip = motor_enc_freq - enc_freq;
   
*/
/*
==========FOLLOW UP MOTOR SPEED ALGORITHM===========
- sending throttle value from serial.
- is speed_table[throttle_level] == motor_enc
- if yes go to next speed value.

if (current_speed != set_speed && current_speed == next_speed)
         {
            next_speed++;
            throttle_level++;
            prev_speed = current_speed;
         }
         
         if (current_speed <= prev_speed)
         {
            // speed is decreasing.
            prev_speed = current_speed;
            
         }
         
         
         //!             if (attained_speed < loaded_speed) 
//!             {
//!               position_capture = attained_speed;
//!               descend_flag = 1;
//!                attained_throttle--; 
//!                if (attained_throttle < 20)
//!                {
//!                   attained_throttle = 20;
//!                }
//!             }
//!             else {descend_flag = 0;}

//-----------drive test code--------//
      if (drive_inc) 
      {

         
         attained_throttle++;
         if (attained_throttle > 255)
         {
            attained_throttle = 255;
         }
         if (attained_throttle < 0)   
         {  
            attained_throttle = 0; 
         }
         freq = attained_throttle;
         peak_voltage = gain_table[attained_throttle];

         drive_inc = 0;
      }
      
      //!   if (sec_tick >= 1000) 
//!   {
//!      drive_inc = 1;
//!      sec_tick = 0;
//!   
//!   }

   // printing tables//
   for (int i = 0 ;i <= 255; i++) 
   {  
      sprintf(Serial_OutputBuffer , "[%d] : %d\n" , i , ascend_speed_table[i]);
      printf(Serial_OutputBuffer);
   }
  

*/






  
