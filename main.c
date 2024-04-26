#include <30F4011.h>
#DEVICE ADC=10
#device ICSP=1
#use delay(clock=64000000,crystal=8000000)

#FUSES NOWDT                    //No Watch Dog Timer
#FUSES CKSFSM                   //Clock Switching is enabled, fail Safe clock monitor is enabled
#FUSES BORV42                   //Brownout reset at 4.5V
#FUSES WRT                      //Program Memory Write Protected
#FUSES PROTECT                  //Code protected from reads
#include <math.h>


#define P1TCON 0x01C0
#define P1TMR 0x01C2 
#define P1TPER 0x01C4
#define P1SECMP 0x01C6
#define PWM1CON1 0x01C8 
#define PWM1CON2 0x01CA
#define P1DTCON1 0x01CC
#define P1DTCON2 0x01CE  
#define P1FLTACON 0x01D0
#define P1OVDCON 0x01D4   
#define P1DC1 0x01D6   
#define P1DC2 0x01D8
#define P1DC3 0x01DA
#define IPC14 0x00C0  
#define CLKDIV 0x0744 
#define PLLFBD 0x0746


#define ADC_pin sAN0
#define LED_PIN PIN_B5

#define voltage_offset 1000//1248
#define low_duty_limit 50
#define high_duty_limit 1950
#define SLOPE 5.4
 
#define break_level 5
#define sustain_level 200  
#define break_amplitude 0 
#define pedestal_amplitude 950
#define peak_amplitude 950


#define throttle_PIN PIN_B1
#define PWM_tick_pin PIN_B2
#define TIM_tick_pin PIN_B3

  
  
void initMCPWM(void);
void fill_sine_table(void);  
void timer_reload(void);
void voltage_gain(void); 


unsigned int16 duty[3]={voltage_offset,voltage_offset,voltage_offset},sample=0;
signed int16 peak_voltage =0;//  1184; 
signed int32 reference[3] = {0,0,0};

const unsigned max_samples=30.0;
signed int16 sine_table[max_samples];
unsigned int16 sine_index,phase_angle[3] = {0 , 0 , 0};  
double theeta;

const unsigned max_freq = 250; //Hz
//!const double per_clock_tick = 0.006405; //ms
const double per_clock_tick = 0.007996; //ms
unsigned int16 timer_table[max_freq+1];

unsigned int16 gain_table[256];
unsigned int16 raw_adc =0 ;
signed int16 throttle_level = 0;
unsigned int16 freq = 1;
unsigned int16 temp = 0;

int1 tick = 0;
int8 tick_count = 0;
unsigned long millis_count = 0;


#int_PWM1
void  PWM1_isr(void) 
{

   tick_count++;
   if(tick_count >= 8)
   {   
      output_bit(PWM_tick_pin , 1);
      millis_count++;
      tick = 1;
      tick_count=0;
   
   }
   if (millis_count >= 500) {
      output_toggle(LED_PIN);
      millis_count = 0;
   }

}
#INT_TIMER1
void  timer1_isr(void) 
{
   output_bit(TIM_tick_pin , 1);
//!   delay_us(10);
//!   output_bit(TIM_tick_pin , 0); 
//!   
   
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
  
//!   if(sample < 15)
//!   {
//!      output_bit(Sync_Out,1);
//!   }
//!   else
//!   {
//!      output_bit(Sync_Out,0);
//!   }
      output_bit(TIM_tick_pin , 0);
}

   
void main()
{ 
 
   freq = 1;
   
   initMCPWM();
   fill_sine_table();
   timer_reload();
   voltage_gain(); 
   
   output_drive(LED_PIN);
   output_drive(PWM_tick_pin);
   output_drive(TIM_tick_pin);   
   
   setup_adc(ADC_CLOCK_DIV_32);
   setup_adc_ports(ADC_pin);
   set_adc_channel(0);
   delay_us(10);
   
   setup_timer1(TMR_INTERNAL | TMR_DIV_BY_64, timer_table[freq]);            
   enable_interrupts(INT_TIMER1);  
      
   enable_interrupts(INT_PWM1);  
   enable_interrupts(INTR_GLOBAL);
   
//!   duty[0] = 50;
//!   duty[1] = 50;    
//!   duty[2] = 50;  
//!   
//!   *(P1DC1+1) = duty[0]>>8;   *P1DC1 = duty[0];
   
  
   
    
   while(TRUE)
   {
      if (tick) {
         raw_adc = read_adc();
         if (raw_adc > 1023) 
         {
            raw_adc = 1023;
         }
         raw_adc = raw_adc >> 2;
         throttle_level = raw_adc;  
         if (throttle_level > 255)
         {
            throttle_level = 255;
         }
         if (throttle_level < 0)   
         {
            throttle_level = 0;
         }
         freq = throttle_level - 5 ;   
         peak_voltage = gain_table[throttle_level];
         setup_timer1(TMR_INTERNAL | TMR_DIV_BY_64, timer_table[freq]); 
           
        
         output_bit(PWM_tick_pin , 0);
         tick = 0;      
      }     
    } 
}


void initMCPWM(void) 
{    
   *(P1TCON+1)=0x80;  *P1TCON=0x02;
   *(P1TPER+1)=0x03;  *P1TPER=0xE7;  
   *(P1SECMP+1)=0x00;  *P1SECMP=0x01; //
   *(PWM1CON1+1)=0x00;  *PWM1CON1=0x77;  
   *(PWM1CON2+1)=0x00;  *PWM1CON2=0x02;
   *(P1DTCON1+1)=0x00;  *P1DTCON1=0x10; //0x09
   *(P1DTCON2+1)=0x00;  *P1DTCON2=0x00;
   *(P1FLTACON+1)=0x00;  *P1FLTACON=0x00; //0x0000
   *(P1OVDCON+1)=0x3F;  *P1OVDCON=0x0F;
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


void timer_reload(void) 
{
   double intr_per_sample = 0.0;
   
   for (int sample = 1 ; sample <= max_freq ; sample++) 
   {
      intr_per_sample = ((1.0/sample)*1000)/max_samples;
      timer_table[sample] = intr_per_sample/per_clock_tick;     
   }
   timer_table[0] = timer_table[1];
}

void voltage_gain(void) 
{
   for (int i = 0 ; i <= break_level ; i++) {
            gain_table[i] = 0; 
   }
   
   for (int i = break_level+1 ; i <= sustain_level; i++) 
   {
       temp = SLOPE * i + pedestal_amplitude;
       if(temp > peak_amplitude ) 
       {
         temp = peak_amplitude;
       }
       gain_table[i] = temp;
   }
   for (int i = sustain_level+1 ; i <= 255; i++) {     
      gain_table[i] = peak_amplitude;
   }

}


