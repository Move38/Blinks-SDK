// Simple synchronizing fire fly demo

// Put a bunch of blinks together and they will eventually all sync to flash at the same time.
// Press the button to force a blink to immediately flash.
// Read more here: http://josh.com/ly/Firefly_sync.pdf


// Consts

// When urge gets to to this value, then we flash and set back to 0

unsigned int URGE_MAX_MS = 2000;


// Variables

// Urge starts at 0 and increases by 1 every every millisecond that passes
// Also increases when we see a neighbor flash

static unsigned urge_ms=0;

// Rememeber what time it was last time we ran loop()
// so we can calculate how many milliseconds passed since then

static unsigned long last_time_ms=0;


// This just increments each time we send a flash. We send this over the IRs so
// neighbors can tell when we have flashed.
// It is not the count that counts, just that it changes each time we flash.

byte flashCountValue = 0;

// Embelish the flash with a soothing fade to black

Timer flash_fade_timer; 


void setup() {
    // put your setup code here, to run once:
}

void loop() {    

    // UI

    if (buttonPressed() ) {

        // Flash immedeately when button pressed

        urge_ms= URGE_MAX_MS;
        
    }

    // Timekeeping

    unsigned diff_ms = millis() - last_time_ms;

    last_time_ms = millis();
    

    // Increment urge
    
    urge_ms += diff_ms;


    // Check for changes (flashes) from neighbors
    
    FOREACH_FACE(f) {

        if (didValueOnFaceChange(f)) {

            // If we see a flash, increment our urge to flash proportional to
            // out current urge. Higher divisors = longer sync times.

            // THIS is where the magic happens.

            urge_ms = urge_ms + (urge_ms/50);
            
        }
        
    }


    // Check if we can not hold it any more
    
    if (urge_ms>=URGE_MAX_MS) {

        // FLASH!!!


        // For simplicity, we will fade out over the next 255ms
        // which happens to look nice and also mean no math is needed
        // to map to the milliseconds left to the brightness
        
        flash_fade_timer.set( 255 );

        urge_ms=0;

        // Signal flash to neighbors

        flashCountValue++;

        if (flashCountValue >= IR_DATA_VALUE_MAX) {
            flashCountValue = 0 ;
        }
        
        setValueSentOnAllFaces( flashCountValue );

    }

    setColor( dim( GREEN , flash_fade_timer.getRemaining() ) );

}
