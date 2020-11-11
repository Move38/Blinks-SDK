/*
 * blkinklib.h
 *
 * Used for programming blinks by Move38 with the Arduino IDE. Move info at move38.com.
 *
 * This file is automatically included by the Arduino IDE when the blinks board type is selected. 
 *
 */

#ifndef BLINKLIB_H_
#define BLINKLIB_H_

#include <limits.h>         // UINTLONG_MAX for NEVER
#include "ArduinoTypes.h"

// Number of faces on a blink. Looks nicer than hardcoding '6' everywhere.

#define FACE_COUNT 6

/*

    IR communications functions

*/

// The value of the data sent and received on faces via IR can be between 0 and IR_DATA_VALUE_MAX

#define IR_DATA_VALUE_MAX 63

// Returns the last received value on the indicated face
// Between 0 and IR_DATA_VALUE_MAX inclusive
// returns 0 if no neighbor ever seen on this face since power-up
// so best to only use after checking if face is not expired first.

byte getLastValueReceivedOnFace( byte face );

// Did the neighborState value on this face change since the
// last time we checked?

// Note the a face expiring has no effect on the last value

byte didValueOnFaceChange( byte face );

// false if messages have been recently received on the indicated face
// (currently configured to 100ms timeout in `expireDurration_ms` )

byte isValueReceivedOnFaceExpired( byte face );

// Returns false if there has been a neighbor seen recently on any face, returns true otherwise.
bool isAlone();

// Set value that will be continuously broadcast on specified face.
// Value should be between 0 and IR_DATA_VALUE_MAX inclusive.
// If a value greater than IR_DATA_VALUE_MAX is specified, IR_DATA_VALUE_MAX will be sent.
// By default we power up with all faces sending the value 0.

void setValueSentOnFace( byte value , byte face );

// Same as setValueSentOnFace(), but sets all faces in one call.

void setValueSentOnAllFaces( byte value );

/* --- Datagram processing */

// A datagram is a set of 1-IR_DATAGRAM_MAX_LEN bytes that are atomically sent over the IR link
// The datagram is sent immediately on a best efforts basis. If it is not received by the other side then
// it is lost forever. Each datagram sent is received at most 1 time. Once you have processed a received datagram
// then you must mark it as read before you can receive the next one on that face. 

// Must be smaller than IR_RX_PACKET_SIZE

#define IR_DATAGRAM_LEN 16

// Returns the number of bytes waiting in the data buffer, or 0 if no packet ready.
byte getDatagramLengthOnFace( uint8_t face );

// Returns true if a packet is available in the buffer
boolean isDatagramReadyOnFace( uint8_t face );

 // Returns a pointer to the actual received datagram data
 // This should really be a (void *) so it can be assigned to any pointer type,
 // but in C++ you can not cast a (void *) into something else so it doesn't really work there
 // and I think too ugly to have these functions that are inverses of each other to take/return different types.
 // Thanks, Stroustrup.
const byte *getDatagramOnFace( uint8_t face );

// Frees up the buffer holding the datagram data. Do this as soon as possible after you have
// processed the datagram to free up the slot for the next incoming datagram on this face.
// If a new datagram is recieved on a face before markDatagramReadOnFace() is called then
// the new datagram is silently discarded. 

void markDatagramReadOnFace( uint8_t face );

// Send a datagram.  
// Datagram is sent as soon as possible and takes priority over sending a value on face.
// If you call sendDatagramOnFace() and there is already a pending datagram, the older pending
// one will be replaced with the new one. 

// Note that if the len>IR_DATAGRAM_LEN then packet will never be sent or recieved

void sendDatagramOnFace(  const void *data, byte len , byte face );


/*

	This set of functions let you test for changes in the environment.

*/

/*

    Button functions

*/

// Debounced view of button state. True if the button currently pressed.

bool buttonDown(void);

// Was the button pressed or lifted since the last time we checked?
// Note that these register the change the instant the button state changes
// without any delay, so good for latency-sensitive cases.
// It is debounced, so the button must have been in the previous state a minimum
// debounce time before a new detection will occur.

bool buttonPressed(void);
bool buttonReleased(void);

// Was the button single, double , or multi clicked since we last checked?
// Note that there is a delay after the button is first pressed
// before a click is registered because we have to wait to
// see if another button press is coming.
// A multiclick is 3 or more clicks

// Remember that these click events fire a short time after the button is lifted on the final click
// If the button is held down too long on the last click, then click interaction is aborted.

bool buttonSingleClicked();

bool buttonDoubleClicked();

bool buttonMultiClicked();

// The number of clicks in the longest consecutive valid click cycle since the last time called.
byte buttonClickCount(void);

// Remember that a long press fires while the button is still down
bool buttonLongPressed(void);

// Remember that a long long press fires while the button is still down
bool buttonLongLongPressed(void);

/*

	This set of functions lets you control the colors on the face RGB LEDs

*/


// Ok, it kills me to include this #include here because it pulls all of these symbols into
// the Arduino namespace... but, dude, Arduino pulls in everything anyway!!!!
// To do it right we'd break out the only thing we actually want (pixelColor_t) into
// its own header file and only include that. Maybe someday.
// TODO: Break pixelColor_t into blinkbios_shared_pixelcolor_t.h just for neatness counts

#include "shared/blinkbios_shared_pixel.h"

typedef pixelColor_t Color;

// Number of visible brightness levels in each channel of a color
#define BRIGHTNESS_LEVELS_5BIT 32
#define MAX_BRIGHTNESS_5BIT    (BRIGHTNESS_LEVELS_5BIT-1)

// R,G,B are all in the domain 0-31
// Here we expose the internal color representation, but it is worth it
// to get the performance and size benefits of static compilation

#define MAKECOLOR_5BIT_RGB(r,g,b) (pixelColor_t(r,g,b,1))

#define RED         MAKECOLOR_5BIT_RGB(MAX_BRIGHTNESS_5BIT, 0                    ,0)
#define ORANGE      MAKECOLOR_5BIT_RGB(MAX_BRIGHTNESS_5BIT,MAX_BRIGHTNESS_5BIT/2 ,0)
#define YELLOW      MAKECOLOR_5BIT_RGB(MAX_BRIGHTNESS_5BIT,MAX_BRIGHTNESS_5BIT   ,0)
#define GREEN       MAKECOLOR_5BIT_RGB( 0                 ,MAX_BRIGHTNESS_5BIT   ,0)
#define CYAN        MAKECOLOR_5BIT_RGB( 0                 ,MAX_BRIGHTNESS_5BIT   ,MAX_BRIGHTNESS_5BIT)
#define BLUE        MAKECOLOR_5BIT_RGB( 0                 , 0                    ,MAX_BRIGHTNESS_5BIT)
#define MAGENTA     MAKECOLOR_5BIT_RGB(MAX_BRIGHTNESS_5BIT, 0                    ,MAX_BRIGHTNESS_5BIT)

#define WHITE       MAKECOLOR_5BIT_RGB(MAX_BRIGHTNESS_5BIT,MAX_BRIGHTNESS_5BIT   ,MAX_BRIGHTNESS_5BIT)

#define OFF         MAKECOLOR_5BIT_RGB( 0                 , 0                    , 0)

#define GET_5BIT_R(color) (color.r)
#define GET_5BIT_G(color) (color.g)
#define GET_5BIT_B(color) (color.b)

// Dim the specified color. Brightness is 0-255 (0=off, 255=don't dim at all-keep original color)
// Inlined to allow static simplification at compile time

#define MAX_BRIGHTNESS (255)

Color dim( Color color, byte brightness);

// This maps 0-255 values to 0-31 values with the special case that 0 (in 0-255) is the only value that maps to 0 (in 0-31)
// This leads to some slight non-linearity since there are not a uniform integral number of 1-255 values
// to map to each of the 1-31 values.

// Make a new color from RGB values. Each value can be 0-255.

Color makeColorRGB( byte red, byte green, byte blue );

// Make a new color in the HSB colorspace. All values are 0-255.

Color makeColorHSB( byte hue, byte saturation, byte brightness );

// Change the tile to the specified color
// NOTE: all color changes are double buffered
// and the display is updated when loop() returns

void setColor( Color newColor);

// Set the pixel on the specified face (0-5) to the specified color
// NOTE: all color changes are double buffered
// and the display is updated when loop() returns

void setColorOnFace( Color newColor , byte face );

// DEPREICATED: Use setColorOnFace()
//void setFaceColor( byte face , Color newColor ) __attribute__ ((deprecated));
void setFaceColor(  byte face, Color newColor );

/*

    Timing functions

*/

// Number of running milliseconds since power up.
//
// Important notes:
// 1) does not increment while sleeping
// 2) is only updated between loop() interations
// 3) is monotonic upwards
// 4) is not continuous, so always use greater than
//    and less than rather than equals for comparisons
// 5) overflows after about 50 days
// 6) is only accurate to about +/-10%

unsigned long millis(void);

class Timer {

	private:

		uint32_t m_expireTime;		    // When this timer will expire

	public:

		Timer() {};		                // Timers come into this world pre-expired.

		bool isExpired();

        uint32_t getRemaining();

        void set( uint32_t ms );        // This time will expire ms milliseconds from now

		void add( uint16_t ms );

        void never(void);               // Make this timer never expire (unless set() again)

};


/*

    Utility functions

*/


// Return a random number between 0 and limit inclusive.
// By default uses a hardcoded seed. If you need different blinks
// to generate different streams of random numbers, then call
// randomize() once (probably in setup()) to generate a truely random seed. 

word random( word limit );

// Generate a random 16 bit word. Slightly faster than random(),
// but be careful because you will not get a uniform distribution
// unless your desired range is a power of 2. 

word randomWord(void);

// Generate a new random seed using entropy from the watchdog timer
// This takes about 16ms * 32 bits = 0.5s

void randomize();

// Read the unique serial number for this blink tile
// There are 9 bytes in all, so n can be 0-8

#define SERIAL_NUMBER_LEN 9

byte getSerialNumberByte( byte n );

// Returns the current blinkbios version number.
// Useful to check is a newer feature is available on this blink.

byte getBlinkbiosVersion();

// Map one set to another
// Note that this explodes to big code, so do the explicit calculations
// by hand if you are running out of flash space. 

word map(word x, word in_min, word in_max, word out_min, word out_max);

// Maps theta 0-255 to values 0-255 in a sine wave
// Based on fabulous FastLED library code here...
// https://github.com/FastLED/FastLED/blob/master/lib8tion/trig8.h#L159

byte sin8_C( byte theta);

/* Power functions */

// The blink will automatically sleep if the button has not been pressed in
// more than 10 minutes. The sleep is preemptive - the blink stops in the middle of whatever it
// happens to be doing.

// The blink wakes from sleep when the button is pressed. Upon waking, it picks up from exactly
// where it left off. It is up to the programmer to check to see if the blink has slept and then
// woken and react accordingly if desired.

// Returns 1 if we have woken from sleep since last time we checked

uint8_t hasWoken(void);

// Information on how the current game was loaded

#define START_STATE_POWER_UP            0   // Loaded the built-in game (for example, after battery insertion or failed download) 
#define START_STATE_WE_ARE_ROOT         1   // Completed sending a download seed (running built-in game)
#define START_STATE_DOWNLOAD_SUCCESS    2   // Completed receiving a downloaded game (running the downloaded game)

uint8_t startState(void);

/*

    These hook functions are filled in by the sketch

*/


// Called when this sketch is first loaded and then
// every time the tile wakes from sleep

void setup(void);

// Called repeatedly just after the display pixels
// on the tile face are updated

void loop();


/*

	Some syntactic sugar to make our programs look not so ugly.

*/



// 'Cause C ain't got no iterators and all those FOR loops are too ugly.
// TODO: Yuck, gcc expands this loop index to a word, which costs a load, a compare, and a multiply. :/
#define FOREACH_FACE(x) for(uint8_t x = 0; x < FACE_COUNT ; ++ x)       // Pretend this is a real language with iterators

// Get the number of elements in an array.
#define COUNT_OF(x) ((sizeof(x)/sizeof(x[0])))


#endif /* BLINKLIB_H_ */
