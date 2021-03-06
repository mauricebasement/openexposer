#include <avr/io.h>
#include <avr/interrupt.h>
#include "LaserDriver.h"
#include "configuration.h"
#include <util/delay.h>

uint16_t offset = 0ul;

uint16_t data_table[512];
uint16_t begin_delay = 7500+offset;
uint16_t end_delay = 47500ul;
uint16_t sync_timeout_delay = 60000ul;

static uint8_t laser_state;
volatile uint8_t write_line_enable = 1;
static uint16_t last_line_start;

int exposing_cycle_count = LASER_EXPOSING_CYCLES;
int exposing_cycles = 0;
uint8_t exposing_done = 0;



#define LS_SYNC 0
#define LS_LINE 1



// Opto Capture ISR
ISR(TIMER1_CAPT_vect){
  	last_line_start = ICR1;

	TCCR1A = (1<<COM1A1) ;                //turn off output on compare match
        TCCR1A = (1<<COM1A1) | (1<<FOC1A);    //Force compare match (Laser off)
       
        // Input Capture ausschalten
        TIMSK1 &= ~(1<<ICIE1); //turn ourselves off

	uint16_t start_time = ICR1 + begin_delay; //calculate at which timer value to begin lasering 
        OCR1A = start_time;                  //set OCR1A to that value (this is used from laser write line)
        OCR1B = start_time - 500;            //set up OCR1B to occur long enough before that to have the interrupt

                                         //routine ready and waiting for the OCR1A match
        laser_state = LS_LINE;
}


void laser_write_line(){

	uint16_t * data = data_table;

	//wait for OCR0A compare match (begin of line, laser off)
	while((TIFR1 & (1<<OCF1A)) == 0);
	   TIFR1 = (1<<OCF1A);
       
	TCCR1A = (1<<COM1A0); //Toggle  laser on compare match from now
        
        while((TIFR1 & (1<<OCF1A)) == 0);  //and wait for it...
            TIFR1 = (1<<OCF1A);
       
	while(1){

		uint16_t tmp  = *data++;

		if(tmp == 0){               //end mark?
			break;                    //yes: end line
		};

		OCR1A +=tmp;                   //no: delay that amount

		while((TIFR1 & (1<<OCF1A)) == 0);  //and wait for it...
		  TIFR1 = (1<<OCF1A);

	}
       
}


ISR(TIMER1_COMPB_vect){
        
  

	if(laser_state == LS_LINE){
                
                
                TCCR1A = (1<<COM1A1) | (1<<FOC1A);
		if(write_line_enable){
			laser_write_line();
                        exposing_cycles++;

		} else {
                     while((TIFR1 & (1<<OCF1A)) == 0);
	                 TIFR1 = (1<<OCF1A);
                      while((TIFR1 & (1<<OCF1A)) == 0);
	                 TIFR1 = (1<<OCF1A);
                    
                }
            
                if(exposing_cycles == exposing_cycle_count){
            
                    exposing_cycles = 0;
                    exposing_done = 1;
                    
                }   
                
		TCCR1A = (1<<COM1A1) ;
		TCCR1A = (1<<COM1A1) | (1<<FOC1A); //Assure that laser is off		
                 
        
		TIFR1  =  (1<<ICF1); //clear flag
		TIMSK1 |= (1<<ICIE1);  //turn on opto capture int

		TCCR1A = (1<<COM1A1) | (1<<COM1A0); //turn laser on on next compare
                                            //for the sync pulse that hits the opto
	
		OCR1A = last_line_start + end_delay;           //set up the time for the sync pulse
		laser_state = LS_SYNC;              //tell ourselves that we are in "searching for sync" state
		OCR1B = last_line_start + sync_timeout_delay;  //set up a timeout for the sync pulse to occur
		                                    //(which brings us back in to the interrupt handler to the
		                                    //case below, because the ICR interrupt has not occured
		                                    //and set the laser_state to LS_LINE, and reprogrammed the comnpares)


	}else{
             
      
  
		//LS_SYNC (we have missed the sync pulse)
		TCCR1A = (1<<COM1A1) ;
		TCCR1A = (1<<COM1A1) | (1<<FOC1A); //Assure that laser is off
                
		TIFR1  =  (1<<ICF1); //clear flag
		TIMSK1 |= (1<<ICIE1);  //turn on opto capture int
		TCCR1A = (1<<COM1A1) | (1<<COM1A0); //turn laser on on next compare (sync)
            
		OCR1A = OCR1B + end_delay;
		OCR1B = OCR1B + sync_timeout_delay;
        
	}

      


}

// fill data table with test pattern
void fill_data_table(){

	uint16_t x;
     
       
        for(x=0;x < 80; x++){
   
	    data_table[x] = 400-x;
            
        }
        
       data_table[81] = 0;
       
       
       
       data_table[0] = 5000;
       data_table[1] = 40;
       data_table[2] = 0;
       
       
       //data_table[0] = 0;
}


void set_exposing_cycles(){
      
}



// initialize laser 
void laser_init(){
  
        
	fill_data_table();
        
        SET_DDR(LASER);
        
        SET_DDR(LASER_PWM);
	OUTPUT_ON(LASER_PWM);
     
        TCCR1B = 1; 

}


// Turn Laser Timer on and activate Opto Capture
void laser_on(){

	TCNT1 = 1;

	OCR1B = 0;
	OCR1A = 0;
      
          
  	TIFR1 = (1<<OCF1B) | (1<<ICF1);
	TIMSK1 |= (1<<OCIE1B) | (1<<ICIE1); //capture b and icp int on (they do all the work)	
}

// Turn Laser Timer off and deactivate laser opto
void laser_off(){

	TIMSK1 &= ~ ((1<<OCIE1B) | (1<<ICIE1)); //ints off
	TCCR1A = 0; //disconnect laser from timer
}





