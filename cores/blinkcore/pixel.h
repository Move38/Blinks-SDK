/*
 * pixel.h
 *
 * All the functions for showing colors on the 6 RGB LEDs on the tile face.
 * Also timekeeping functions since we use the same timer as for the LEDs.
 *
 */ 

#ifndef RGB_PIXELS_H_
#define RGB_PIXELS_H_

#include "blinkcore.h"

#include <avr/io.h>

// True today, but could imagine a blinks with 1 pixel or one with with 18 pixels...

#define PIXEL_COUNT FACE_COUNT

// Setup pins, interrupts. Call once at power up.

void pixel_init(void);

// Enable pixels after a call to pixel_init or pixel_disable
// Pixels will return to the color they had before being disabled.
// Call pixel_setPixels() first to set initial values of pixels before 1st call to pixel_enable()
void pixel_enable(void);

// Turn of all pixels and the timer that drives them.
// You'd want to do this before going to sleep.

void pixel_disable(void);
        
/** Display interface ***/

// Each pixel has 32 brightness levels for each of the three colors (red,green,blue)
// These brightness levels are normalized to be visually linear with 0=off and 31=max brightness

typedef struct {
    uint8_t r:5;
    uint8_t g:5;
    uint8_t b:5;
} pixelColor_t;

// Update the pixel buffer. 

void pixel_bufferedSetPixel( uint8_t pixel, pixelColor_t newColor );

// Display the buffered pixels. Blocks until next frame starts. 

void pixel_displayBufferedPixels(void);

/** Callback interface **/

// This is the number of cycles between calls of the pixel_callback
// It is determined by the programming of the timer that drives
// the RGB LEDs and the way the callback is called form within the
// interrupts generated by that timer combined with the number of
// phases in the pixel ISR handler

#define PIXEL_CYCLES_PER_FRAME (8 * 256 * 5)

// Called at the end of each display frame, which is currently
// about 66Hz

void pixel_callback_onFrame(void) __attribute__((weak));

// Update the pixel buffer with raw PWM register values.
// Larger pwm values map to shorter PWM cycles (255=off) so for red and green
// there is an inverse but very non linear relation between raw value and brightness.
// For blue is is more complicated because of the charge pump. The peak brightness is somewhere
// in the middle.

// Values set here will be shown on the next hardware reload in the ISR - no buffering
// except for the hardware buffering.

// This is mostly useful for utilities to find the pwm -> brightness mapping to be used
// in the gamma lookup table below.

void pixel_rawSetPixel( uint8_t pixel, uint8_t r_pwm , uint8_t g_pwm , uint8_t b_pwm );

#endif /* RGB_PIXELS_H_ */