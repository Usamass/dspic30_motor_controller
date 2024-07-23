typedef struct 
{
   int32 kalman_big_1;
   int32 kalman_big_2;
   int32 kalman_diff;
   
   int32 KALMAN_UP;
   int32 KALMAN_DOWN;   
   
   
}  KALMAN;


void KALMAN_begin(KALMAN* kalman_filter , unsigned int16* input , unsigned int16* filtered_out);
void KALMAN_init(KALMAN* kalman_filter , int32 kalman_up , int32 kalman_down);

//--------------------FUNCTIONS IMPLEMENTATION--------------------------//

void KALMAN_init(KALMAN* kalman_filter , int32 kalman_up , int32 kalman_down)
{
   kalman_filter->kalman_big_1 = 0;
   kalman_filter->kalman_big_2 = 0;
   
   kalman_filter->KALMAN_UP = kalman_up;
   kalman_filter->KALMAN_DOWN = kalman_down;

}
void KALMAN_begin(KALMAN* kalman_filter , unsigned int16* input , unsigned int16* filtered_out) 
{
   kalman_filter->kalman_big_1 = *input << kalman_filter->KALMAN_UP;
   kalman_filter->kalman_diff = kalman_filter->kalman_big_1 - kalman_filter->kalman_big_2;

   if (kalman_filter->kalman_diff > 0) {

        kalman_filter->kalman_diff = kalman_filter->kalman_diff >> kalman_filter->KALMAN_DOWN;
        kalman_filter->kalman_big_2 = kalman_filter->kalman_big_2 + kalman_filter->kalman_diff;

   }
   else if (kalman_filter->kalman_diff < 0) {

        kalman_filter->kalman_diff = 0 - kalman_filter->kalman_diff;
        kalman_filter->kalman_diff = kalman_filter->kalman_diff >> kalman_filter->KALMAN_DOWN;
        kalman_filter->kalman_big_2 = kalman_filter->kalman_big_2 - kalman_filter->kalman_diff;

    }

    *filtered_out = kalman_filter->kalman_big_2 >> kalman_filter->KALMAN_UP;
   
}
