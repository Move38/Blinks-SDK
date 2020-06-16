// Example showing how to force a blink to hardware sleep immedeately
// It is probably better to let blinklib manage this for you. Use with caution. 

// This demo flashes RED->GREEN->BLUE and then hardware sleeps. A button press wakes it 
// and repeasts the cycle. 

#define BLINKBIOS_SLEEP_NOW_VECTOR boot_vector12

// Calling BLINKBIOS_SLEEP_NOW_VECTOR() will immedeately put the blink into hardware sleep
// It can only wake up from a button press.

extern "C" void BLINKBIOS_SLEEP_NOW_VECTOR()  __attribute__((used)) __attribute__((noinline));


void setup() {
  // put your setup code here, to run once:

}


// How many times to flash before sleeping
const byte FLASH_BEFORE_SLEEP = 3;

// How long between flashes in milliseconds
const unsigned int FLASH_PERIOD_MS = 1000;

// How long is each flash on for in milliseconds
const unsigned int FLASH_ON_MS = 500;


static byte flash_count = 0; 
static Timer period_timer;
static Timer on_timer;

void loop() {

  if ( on_timer.isExpired() ) {

    setColor( OFF ) ;
    
  }
    
  if ( period_timer.isExpired() ) { 
    

    if ( flash_count >= FLASH_BEFORE_SLEEP ) {

      BLINKBIOS_SLEEP_NOW_VECTOR();
      
      flash_count  = 0;

    }
    
    period_timer.set( FLASH_PERIOD_MS );


    switch (flash_count) {

      case 0:     setColor( RED ); break;
      case 1:     setColor( GREEN ); break;
      case 2:     setColor( BLUE ); break;

    }


    on_timer.set( FLASH_ON_MS );   

    flash_count++;    
    
  } 
  
   
}
