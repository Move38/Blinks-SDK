/*
 *
 *  This library lives in userland and acts as a shim to th blinkos layer
 *
 *  This view tailored to be idiomatic Arduino-y. There are probably better views of the interface if you are not an Arduinohead.
 *
 * In this view, each tile has a "state" that is represented by a number between 1 and 127.
 * This state value is continuously broadcast on all of its faces.
 * Each tile also remembers the most recently received state value from he neighbor on each of its faces.
 *
 * You supply setup() and loop().
 *
 * While in loop(), the world is frozen. All changes you make to the pixels and to data on the faces
 * is buffered until loop returns.
 *
 */

#include <limits.h>
#include <stdint.h>

#include <avr/pgmspace.h>   // PROGMEM for parity lookup table
#include <avr/interrupt.h>  // cli() and sei() so we can get snapshots of multibyte variables

#include <avr/sleep.h>      // sleep_cpu() so we can rest between interrupts.

#include <avr/wdt.h>        // Used in randomize() to get some entropy from the skew between the WDT osicilator and the system clock. 

#include <stddef.h>

#include <string.h>

#include "ArduinoTypes.h"

#include "blinklib.h"

// Here are our magic shared memory links to the BlinkBIOS running up in the bootloader area.
// These special sections are defined in a special linker script to make sure that the addresses
// are the same on both the foreground (this blinklib program) and the background (the BlinkBIOS project compiled to a HEX file)

// The actual memory for these blocks is allocated in main.cpp. Remember, it overlaps with the same blocks in BlinkBIOS code running in the bootloader!

#include "shared/blinkbios_shared_button.h"
#include "shared/blinkbios_shared_millis.h"
#include "shared/blinkbios_shared_pixel.h"
#include "shared/blinkbios_shared_irdata.h"

#include "shared/blinkbios_shared_functions.h"     // Gets us ir_send_packet()


#define TX_PROBE_TIME_MS           150     // How often to do a blind send when no RX has happened recently to trigger ping pong
                                           // Nice to have probe time shorter than expire time so you have to miss 2 messages
                                           // before the face will expire

#define RX_EXPIRE_TIME_MS         200      // If we do not see a message in this long, then show that face as expired

#define VIRAL_BUTTON_PRESS_LOCKOUT_MS   2000    // Any viral button presses received from IR within this time period are ignored 
                                                // since insures that a single press can not circulate around indefinitely.                                                

#define WARM_SLEEP_TIMEOUT_MS   (  10 * 60 * 1000UL )   // 10 mins
                                                        // We will warm sleep if we do not see a button press or remote button press
                                                        // in this long

// This is a special byte that signals that this is a long data packet
// Note that this is also a value value, but we can tell that it is a data by looking at the IR packet len. Datagrams are always >2 bytes. 
// It must appear in the first byte of the data, and the final byte is an inverted checksum of all bytes including this header byte

#define DATAGRAM_SPECIAL_VALUE     0b00101010

// This is a special byte that triggers a warm sleep cycle when received
// It must appear in the first & second byte of data
// When we get it, we virally send out more warm sleep packets on all the faces
// and then we go to warm sleep.

#define TRIGGER_WARM_SLEEP_SPECIAL_VALUE   0b00010101

// This is a special byte that does nothing. 
// It must appear in the first & second byte of data.
// We send it when we warm wake to warm wake our neighbors. 

#define NOP_SPECIAL_VALUE   0b00110011


// We use bit 6 in the IR data to indicate that a button has been pressed so we should 
// postpone sleeping. This spreads a button press to all connected tiles so 
// they will stay awake if any tile in the group gets pressed.

// We use bit 7 in the IR data as ODD parity check. We do ODD to make sure at least 1 bit is always
// set (otherwise 0x00 would be 0x00 with parity). 

// Assumes ( d < IR_DATA_VALUE_MAX )

#if IR_DATA_VALUE_MAX > 63
    #warning The following code assumes that the top two bits of the header byte are available
#endif

// Returns true if odd number of bits set
// TODO: make asm

uint8_t oddParity( uint8_t d ) {
    
    uint8_t bits=0;
    
    while (d) {
        
        if (d & 0b00000001 ) {
            bits++;
        }
        
        d >>=1;
        
    }
    
    return bits & 0xb00000001;
}

static uint8_t irValueEncode( uint8_t d , uint8_t postponeSleepFlag ) {
        
    if (postponeSleepFlag) {
        d |= 0b01000000;            // 6th bit button pressed flag
    }
    
    if ( !oddParity( d )) {
        
        d |= 0b10000000;            // Top bit ODD parity (including postpone sleep flag)
        
    }
    
    return d;
    
}


static uint8_t irValueCheckValid( uint8_t d ) {
        
    return oddParity( d );      // Odd parity

}


// The actual data is hidden in the middle

static uint8_t irValueDecodeData( uint8_t d ) {

    return (d & 0b00111111) ;

}


static uint8_t irValueDecodePostponeSleepFlag( uint8_t d ) {

    return d & 0b01000000 ;

}


// TODO: These structs even better if they are padded to a power of 2 like https://stackoverflow.com/questions/1239855/pad-a-c-structure-to-a-power-of-two

#if IR_DATAGRAM_LEN > IR_RX_PACKET_SIZE
    #error IR_DATAGRAM_LEN must not be bigger than IR_RX_PACKET_SIZE
#endif

// All semantics chosen to have sane startup 0 so we can
// keep this in bss section and have it zeroed out at startup. 

struct face_t {

    uint8_t inValue;        // Last received value on this face, or 0 if no neighbor ever seen since startup
    uint8_t outValue;       // Value we send out on this face
    millis_t expireTime;    // When this face will be considered to be expired (no neighbor there)
    millis_t sendTime;      // Next time we will transmit on this face (set to 0 every time we get a good message so we ping-pong across the link)
    
    uint8_t inDatagramLen;  // 0= No datagram waiting to be read
    uint8_t inDatagramData[IR_DATAGRAM_LEN];

    uint8_t outDatagramLen;  // 0= No datagram waiting to be sent
    uint8_t outDatagramData[IR_DATAGRAM_LEN];
};

static face_t faces[FACE_COUNT];

uint8_t viralButtonPressSendOnFaceBitflags;   // A 1 here means send the viral button press bit on the next IR packet on this face. Cleared when it gets sent. 

Timer viralButtonPressLockoutTimer;     // Set each time we send a viral button press to avoid sending getting into a circular loop

// Millis snapshot for this pass though loop
millis_t now;

// Capture time snapshot
// It is 4 bytes long so we cli() so it can not get updated in the middle of us grabbing it

void updateNow() {
    cli();
    now = blinkbios_millis_block.millis;
    sei();
}

unsigned long millis() {
    return now;
}

// Returns the inverted checksum of all bytes

uint8_t computePacketChecksum( volatile const uint8_t *buffer , uint8_t len ) {

    uint8_t computedChecksum = 0;

    for( uint8_t l=0; l < len ; l++ ) {

        computedChecksum += *buffer++;

    }

    return computedChecksum ^ 0xff ;

}


#if  ( ( IR_LONG_PACKET_MAX_LEN + 3  ) > IR_RX_PACKET_SIZE )

    #error There has to be enough room in the blinkos packet buffer to hold the user packet plus 2 header bytes and one checksum byte

#endif

byte getDatagramLengthOnFace( uint8_t face ) {    
    return faces[face].inDatagramLen;
}

boolean isDatagramReadyOnFace( uint8_t face ) {
    return getDatagramLengthOnFace(face) != 0;
}

const byte *getDatagramOnFace( uint8_t face ) {
    return faces[face].inDatagramData;
}

void markDatagramReadOnFace( uint8_t face ) {
    faces[face].inDatagramLen = 0;
}    

// Jump to the send packet function all way up in the bootloader

uint8_t blinkbios_irdata_send_packet(  uint8_t face, const uint8_t *data , uint8_t len ) {

    // Call directly into the function in the bootloader. This symbol is resolved by the linker to a
    // direct call to the target address.
    return BLINKBIOS_IRDATA_SEND_PACKET_VECTOR(face,data,len);

}

#define SBI(x,b) (x|= (1<<b))           // Set bit
#define CBI(x,b) (x&=~(1<<b))           // Clear bit
#define TBI(x,b) (x&(1<<b))             // Test bit

void sendDatagramOnFace( const void *data, byte len , byte face ) {

    if ( len > IR_DATAGRAM_LEN ) {

        // Ignore request to send oversized packet

        return;

    }
    
    face_t *f = &faces[face];
    
    f->outDatagramLen = len;
    memcpy( f->outDatagramData , data , len ); 
    
}


static void clear_packet_buffers() {

    FOREACH_FACE(f) {

        blinkbios_irdata_block.ir_rx_states[f].packetBufferReady = 0;

    }
}

// Set the color and display it immediately
// for internal use where we do not want the loop buffering

static void setColorNow( Color newColor ) {
    
    setColor( newColor );
    BLINKBIOS_DISPLAY_PIXEL_BUFFER_VECTOR();
        
}


Color dim( Color color, byte brightness) {
    return MAKECOLOR_5BIT_RGB(
        (GET_5BIT_R(color)*brightness)/MAX_BRIGHTNESS,
        (GET_5BIT_G(color)*brightness)/MAX_BRIGHTNESS,
        (GET_5BIT_B(color)*brightness)/MAX_BRIGHTNESS
    );
}

Color lighten( Color color, byte brightness) {
    return MAKECOLOR_5BIT_RGB(
        (GET_5BIT_R(color) + (((MAX_BRIGHTNESS_5BIT- (GET_5BIT_R(color)))*brightness)/MAX_BRIGHTNESS )),
        (GET_5BIT_G(color) + (((MAX_BRIGHTNESS_5BIT- (GET_5BIT_G(color)))*brightness)/MAX_BRIGHTNESS )),
        (GET_5BIT_B(color) + (((MAX_BRIGHTNESS_5BIT- (GET_5BIT_B(color)))*brightness)/MAX_BRIGHTNESS ))
    );
}

// When will we warm sleep due to inactivity
// reset by a button press or seeing a button press bit on
// an incoming packet

Timer warm_sleep_time;

void reset_warm_sleep_timer() {

    warm_sleep_time.set( WARM_SLEEP_TIMEOUT_MS );

}

// Remembers if we have woken from either a BIOS sleep or
// a blinklib forced sleep.

uint8_t hasWarmWokenFlag =0;

// We need to save the pixel buffer when we warm sleep so we display our 
// sleep and wake animations and then restore the original game pixels before restarting the game

pixelColor_t savedPixelBuffer[PIXEL_COUNT];

void savePixels() {
   // Save game pixels
   
   FOREACH_FACE(f) {
       
       savedPixelBuffer[f] = blinkbios_pixel_block.pixelBuffer[f];
       
   }    
}

void restorePixels() {
    // Save game pixels
    
    FOREACH_FACE(f) {
        
        blinkbios_pixel_block.pixelBuffer[f] = savedPixelBuffer[f];
        
    }
}


#define SLEEP_ANIMATION_DURATION_MS     300
#define SLEEP_ANIMATION_MAX_BRIGHTNESS  200

// A special warm sleep trigger packet has len 2 and the two bytes are both the special cookie value
// Because it must be 2 long, this means that the cookie can still be a data value since that value would only have a 1 byte packet

static uint8_t force_sleep_packet[2] = { TRIGGER_WARM_SLEEP_SPECIAL_VALUE , TRIGGER_WARM_SLEEP_SPECIAL_VALUE};

// This packet does nothing except wake up our neighbors

static uint8_t nop_wake_packet[2] = { NOP_SPECIAL_VALUE , NOP_SPECIAL_VALUE};


#define SLEEP_PACKET_REPEAT_COUNT 5     // How many times do we send the sleep and wake packets for redunancy?

static void warm_sleep_cycle() {
    
    BLINKBIOS_POSTPONE_SLEEP_VECTOR();      // Postpone cold sleep so we can warm sleep for a while
    // The cold sleep will eventually kick in if we
    // do not wake from warm sleep in time.
        
    // Save the games pixels so we can restore them on waking
    // we need to do this because the sleep and wake animations
    // will overwrite whatever is there. 
            
    savePixels(); 


    // Ok, now we are virally sending FORCE_SLEEP out on all faces to spread the word
    // and the pixels are off so the user is happy and we are saving power.

    // First send the force sleep packet out to all our neighbors
    // We are indiscriminate, just splat it 5 times everywhere.
    // This is a brute force approach to make sure we get though even with collisions
    // and long packets in flight.
    
    // We also show a little animation while transmitting the packets
    
    // Figure out how much brightness to animate on each packet
        
    const int animation_fade_step = SLEEP_ANIMATION_MAX_BRIGHTNESS / ( SLEEP_PACKET_REPEAT_COUNT * FACE_COUNT );
    
    uint8_t fade_brightness;
    
    // For the sleep animation we start bright and dim to 0 by the end

    // This code picks a start near to SLEEP_ANIMATION_MAX_BRIGHTNESS that makes sure we end up at 0    
    fade_brightness = SLEEP_ANIMATION_MAX_BRIGHTNESS;
    
    for( uint8_t n=0; n<SLEEP_PACKET_REPEAT_COUNT; n++ ) {

        FOREACH_FACE(f) {
            
            setColorNow( dim( BLUE, fade_brightness ) );
            
            fade_brightness -= animation_fade_step;

            //while ( blinkbios_is_rx_in_progress( f ) );     // Wait to clear to send (no guarantee, but better than just blink sending)

            blinkbios_irdata_send_packet( f , force_sleep_packet , sizeof( force_sleep_packet ) );      // Note that we can use sizeof() here becuase the arrayt is explicity uint8_t which is always a byte on AVR

        }

    }
    
    // Ensure that we end up completely off 
    setColorNow( OFF );
        
    // We need to save the time now because it will keep ticking while we are in pre-sleep (where were can get
    // woken back up by a packet). If we did not save it and then restore it later, then all the user timers
    // would be expired when we woke.

    // Save the time now so we can go back in time when we wake up
    cli();
    millis_t save_time = blinkbios_millis_block.millis;
    sei();

    // OK we now appear asleep
    // We are not sending IR so some power savings
    // For the next 2 hours will will wait for a wake up signal
    // TODO: Make this even more power efficient by sleeping between checks for incoming IR.

    blinkbios_button_block.bitflags=0;

    // Here is wuld be nice to idle the CPU for a bit of power savings, but there is a potential
    // race where the BIOS could put us into deep sleep mode and then our idle would be deep sleep.
    // you'd think we could turn of ints and set out mode right before entering idle, but
    // we needs ints on to wake form idle on AVR.

    clear_packet_buffers();     // Clear out any left over packets that were there when we started this sleep cycle and might trigger us to wake unapropriately

    uint8_t saw_packet_flag =0;

    // Wait in idle mode until we either see a non-force-sleep packet or a button press or woke.
    // Why woke? Because eventually the BIOS will make us powerdown sleep inside this loop
    // When that happens, it will take a button press to wake us

    blinkbios_button_block.wokeFlag = 1;    // // Set to 0 upon waking from sleep

    while (!saw_packet_flag && !(blinkbios_button_block.bitflags & BUTTON_BITFLAG_PRESSED) && blinkbios_button_block.wokeFlag) {

        // TODO: This sleep mode currently uses about 2mA. We can get that way down by...
        //       1. Adding a supporess_display_flag to pixel_block to skip all of the display code when in this mode
        //       2. Adding a new_pack_recieved_flag to ir_block so we only scan when there is a new packet
        // UPDATE: Tried all that and it only saved like 0.1-0.2mA and added dozens of bytes of code so not worth it.


        ir_rx_state_t *ir_rx_state = blinkbios_irdata_block.ir_rx_states;

        FOREACH_FACE( f ) {

            if (ir_rx_state->packetBufferReady) {

                if (ir_rx_state->packetBuffer[1] != TRIGGER_WARM_SLEEP_SPECIAL_VALUE ) {

                    saw_packet_flag =1;

                }

                ir_rx_state->packetBufferReady=0;

            }

            ir_rx_state++;
        }

    }

    cli();
    blinkbios_millis_block.millis = save_time;
    BLINKBIOS_POSTPONE_SLEEP_VECTOR();              // It is ok top call like this to reset the inactivity timer
    sei();

    hasWarmWokenFlag = 1;           // Remember that we warm slept
    reset_warm_sleep_timer();

    // Forced sleep mode
    // Really need button down detection in bios so we only wake on lift...
    // BLINKBIOS_SLEEP_NOW_VECTOR();

    // Clear out old packets (including any old FORCE_SLEEP packets so we don't go right back to bed)

    clear_packet_buffers();
            
    // Show smooth wake animation
    
    // This loop empirically works out to be about the right delay.
    // I know this hardcode is hackyish, but we need to save flash space

    // For the wake animation we start off and dim to MAX by the end

    // This code picks a start near to SLEEP_ANIMATION_MAX_BRIGHTNESS that makes sure we end up at 0
    fade_brightness = 0;
    
    for( uint8_t n=0; n<SLEEP_PACKET_REPEAT_COUNT; n++ ) {

        FOREACH_FACE(f) {
            
            // INcrement first - they are already seeing OFF when we start
            fade_brightness += animation_fade_step;                        
            setColorNow( dim( WHITE, fade_brightness ) );
            
            blinkbios_irdata_send_packet( f ,  nop_wake_packet , sizeof( nop_wake_packet ) );      // Note that we can use sizeof() here becuase the arrayt is explicity uint8_t which is always a byte on AVR

        }

    }
                       
    // restore game pixels
    
    restorePixels();
    
}

// Called anytime a the button is pressed or anytime we get a viral button press form a neighbor over IR
// Note that we know that this can not become cyclical because of the lockout delay 

void viralPostponeWarmSleep() {
    
    if (viralButtonPressLockoutTimer.isExpired()) {
        
        viralButtonPressLockoutTimer.set( VIRAL_BUTTON_PRESS_LOCKOUT_MS );
        
        viralButtonPressSendOnFaceBitflags = IR_FACE_BITMASK;

        // Prevent warm sleep
        reset_warm_sleep_timer();
                    
    }    
        
}

static void RX_IRFaces() {

    //  Use these pointers to step though the arrays
    face_t *face = faces;
    volatile ir_rx_state_t *ir_rx_state = blinkbios_irdata_block.ir_rx_states;

    for( uint8_t f=0; f < FACE_COUNT ; f++ ) {

            // Check for anything new coming in...

        if ( ir_rx_state->packetBufferReady ) {

            // Got something, so we know there is someone out there
            // TODO: Should we require the received packet to pass error checks?
            face->expireTime = now + RX_EXPIRE_TIME_MS;

            // This is slightly ugly. To save a buffer, we get the full packet with the BlinkBIOS IR packet type byte.                       

            volatile const uint8_t *packetData = (ir_rx_state->packetBuffer);       
            
            if ( *packetData++ == IR_USER_DATA_HEADER_BYTE ) {        // We only process user data and ignore (and consume) anything else. This is ugly. Sorry. 
            
                uint8_t packetDataLen = (ir_rx_state->packetBufferLen)-1;               // deduct the BlinkBIOS packet type  byte 
            
                // blinkBIOS will only pass use packets with len >0 
            
                uint8_t irDataFirstByte = *packetData;                       
                                                                   
                if (irValueCheckValid( irDataFirstByte )) {                                
                
                    // If we get here, then we know this is a valid packet
                
                    // Clear to send on this face immediately to ping-pong messages at max speed without collisions
                    face->sendTime = 0;
                                
                    if (irValueDecodePostponeSleepFlag(irDataFirstByte )) {
                    
                        // The blink on on the other side of this connection is telling us that a button was pressed recently
                        // Send the viral message to all neighbors.
                                                                                
                        viralPostponeWarmSleep();
                   
                        // We also need to extend hardware sleep
                        // since we did not get a physical button press
                        BLINKBIOS_POSTPONE_SLEEP_VECTOR();
                                                           
                    } 
                

                    uint8_t decodedByte = irValueDecodeData( irDataFirstByte );
                
                    if ( packetDataLen == 1 ) {         // normal user face value, One header byte + One data byte

                        // We got a face value! Save it!

                        face->inValue =decodedByte;


                    } else {        // (packetDataLen>1)  
                    
                
                        if ( decodedByte == DATAGRAM_SPECIAL_VALUE) {
                        
                            uint8_t datagramPayloadLen = packetDataLen-2;           // We deduct 2 from he length to account for the header byte and the trailing checksum byte                        
                            const uint8_t *datagramPayloadData =   packetData+1;    // Skip the packet header byte
                        
                            // Long packets are kind of a special case since we do not mark them read immediately
                            if ( computePacketChecksum( datagramPayloadData , datagramPayloadLen )  ==  datagramPayloadData[ datagramPayloadLen ] ) {        // Run checksum on payload bytes after the header, compare that to the checksum at the end

                                // Ok this packet checks out folks!
                            
                                if ( face->inDatagramLen == 0 && !(datagramPayloadLen > IR_DATAGRAM_LEN) ) {        // Check if buffer free and datagram not too long

                                    face->inDatagramLen = datagramPayloadLen;
                                
                                    memcpy( face->inDatagramData  , datagramPayloadData , datagramPayloadLen);       // Skip the header bytes
                                    
                                }
                                                                                    
                            }

                        } else {    // packetLen > 1 &&  decodedByte != LONG_DATA_SPECIAL_VALUE
                            
                            // Here is look for a magic packet that has 2 bytes of data and both are the special sleep trigger cookie
                            
                            if ( packetDataLen == 2 && decodedByte == TRIGGER_WARM_SLEEP_SPECIAL_VALUE && packetData[1] == TRIGGER_WARM_SLEEP_SPECIAL_VALUE ) {
                                
                                warm_sleep_cycle();                                
                                
                            }
                            
                        }  //  ( decodedByte == LONG_DATA_SPECIAL_VALUE)                     
                    
                    }    //  (packetDataLen>1)              

                } else {
                
                    // Invalid packet received. No good way to show or log this. :/
                
                    //#warning
                    //setColorNow( RED );                

                }
                
            }                
            
            // No matter what, mark buffer as read so we can get next packet
            ir_rx_state->packetBufferReady=0;
                        
        }  // if ( ir_data_buffer->ready_flag )

        face++;
        ir_rx_state++;

    } // for( uint8_t f=0; f < FACE_COUNT ; f++ )

}


// Buffer to build each outgoing IR packet
// This is the easy way to do this, but uses RAM unnecessarily.
// TODO: Make a scatter version of this to save RAM & time

static uint8_t ir_send_packet_buffer[ IR_DATAGRAM_LEN + 2 ];    // header byte + Datagram payload  + checksum byte

static void TX_IRFaces() {

    //  Use these pointers to step though the arrays
    face_t *face = faces;

    for( uint8_t f=0; f < FACE_COUNT ; f++ ) {
        
        // Send one out too if it is time....

        if ( face->sendTime <= now ) {        // Time to send on this face?
                                              // Note that we do not use the rx_fresh flag here because we want the timeout
                                              // to do automatic retries to kickstart things when a new neighbor shows up or
                                              // when an IR message gets missed
                   
            uint8_t outgoingPacketLen;              // Total length of the outgoing packet in ir_send_packet_buffer
            uint8_t outgoiungPacketHeaderValue;     // Value to encode into first byte of outgoing IR packet before transmitting
                                                                      
            // Ok, it is time to send something on this face
            // Do we have a pending datagram? If so, datagrams get priority over face values
                                    
            if (face->outDatagramLen) {
                
                outgoiungPacketHeaderValue = DATAGRAM_SPECIAL_VALUE;

                // Build a datagram into the outgoing buffer including checksum
                                
                uint8_t *d = ir_send_packet_buffer+1;           // Data goes after the 1st byte header            
                const uint8_t *s = face->outDatagramData ;      // Just to convert from void to uint8_t

                uint8_t datagramPayloadLen  = face->outDatagramLen;
                                
                memcpy( d, s , datagramPayloadLen );
                                                
                // First header, then payload, when checksum 
                 ir_send_packet_buffer[1+datagramPayloadLen] = computePacketChecksum( s , datagramPayloadLen );

                outgoingPacketLen = 1 + datagramPayloadLen +1;       // include header byte + payload + checksum (header added below)
                                
                // Note that the outgoing datagram buffer will be cleared below if the IR send succeeds
                
            } else {    
                
                // Just send a normal face value                                
                outgoiungPacketHeaderValue = face->outValue;
                outgoingPacketLen=1;
                                
            }       

            // Encode the header byte with the parity and viral button flag            
                                              
            uint8_t encodedIrValue; 
                                              
            if ( TBI(  viralButtonPressSendOnFaceBitflags , f )) {
                
                // We need to send the viral button press on this face right now
                
                encodedIrValue=  irValueEncode( outgoiungPacketHeaderValue , 1 );
                
                CBI( viralButtonPressSendOnFaceBitflags , f );
                
                                
            } else {
                
                encodedIrValue=  irValueEncode( outgoiungPacketHeaderValue , 0 );
                
            }
            
            ir_send_packet_buffer[0] = encodedIrValue;  // store the encoded header into the outgoing buffer

            if (blinkbios_irdata_send_packet( f , ir_send_packet_buffer  , outgoingPacketLen ) ) {
                
                // Here we set a timeout to keep periodically probing on this face, but
                // if there is a neighbor, they will send back to us as soon as they get what we
                // just transmitted, which will make us immediately send again. So the only case
                // when this probe timeout will happen is if there is no neighbor there.

                // If ir_send_userdata() returns 0, then we could not send becuase there was an RX in progress on this face.
                // Because we do not reset the sentTime in that case, we will automatically try again next pass.

				// We add the face index here to try to spread the sends out in time
				// otherwise the degenerate case is that they can all happen repeatedly in the same
				// pass thugh loop() every time when there are no neighbors.
                
				 
                face->sendTime = now + TX_PROBE_TIME_MS + f;	
                
                
                // Mark any pending datagram as sent
                // safe to do this blindly because datagram always gets priority so it would have been 
                // what was just sent if there was one pending
                face->outDatagramLen = 0;
                
            }

        } // if ( face->sendTime <= now )

        face++;

    } // for( uint8_t f=0; f < FACE_COUNT ; f++ )

}


// Returns the last received state on the indicated face
// Remember that getNeighborState() starts at 0 on powerup.
// so returns 0 if no neighbor ever seen on this face since power-up
// so best to only use after checking if face is not expired first.
// Note the a face expiring has no effect on the getNeighborState()

byte getLastValueReceivedOnFace( byte face ) {

    return faces[face].inValue;

}

// Did the neighborState value on this face change since the
// last time we checked?
// Remember that getNeighborState starts at 0 on powerup.
// Note the a face expiring has no effect on the getNeighborState()

byte didValueOnFaceChange( byte face ) {
    static byte prevState[FACE_COUNT];

    byte curState = getLastValueReceivedOnFace(face);

    if ( curState == prevState[face] ) {
        return false;
    }
    prevState[face] = curState;

    return true;

}



byte isValueReceivedOnFaceExpired( byte face ) {

    return faces[face].expireTime < now;

}

// Returns false if their has been a neighbor seen recently on any face, true otherwise.

bool isAlone() {

	FOREACH_FACE(f) {

		if( !isValueReceivedOnFaceExpired(f) ) {
			return false;
		}

	}
	return true;

}


// Set our broadcasted state on all faces to newState.
// This state is repeatedly broadcast to any neighboring tiles.

// By default we power up in state 0.

void setValueSentOnAllFaces( byte value ) {

     if (value > IR_DATA_VALUE_MAX ) {

         value = IR_DATA_VALUE_MAX;

     }

    FOREACH_FACE(f) {

        faces[f].outValue = value;

    }

}

// Set our broadcasted state on indicated face to newState.
// This state is repeatedly broadcast to the partner tile on the indicated face.

// By default we power up in state 0.

void setValueSentOnFace( byte value , byte face ) {

     if (value > IR_DATA_VALUE_MAX ) {

         value = IR_DATA_VALUE_MAX;

     }

    faces[face].outValue = value;

}



// --------------Button code


// Here we keep a local snapshot of the button block stuff

static uint8_t buttonSnapshotDown;               // 1 if button is currently down (debounced)

static uint8_t buttonSnapshotBitflags;

static uint8_t buttonSnapshotClickcount;         // Number of clicks on most recent multiclick


bool buttonDown(void) {
    return buttonSnapshotDown != 0;
}

static bool grabandclearbuttonflag( uint8_t flagbit ) {
    bool r = buttonSnapshotBitflags & flagbit;
    buttonSnapshotBitflags &= ~ flagbit;
    return r;
}

bool buttonPressed(void) {
    return grabandclearbuttonflag( BUTTON_BITFLAG_PRESSED );
}

bool buttonReleased(void) {
    return grabandclearbuttonflag( BUTTON_BITFLAG_RELEASED );
}

bool buttonSingleClicked() {
    return grabandclearbuttonflag( BUTTON_BITFLAG_SINGLECLICKED );
}

bool buttonDoubleClicked() {
    return grabandclearbuttonflag( BUTTON_BITFLAG_DOUBLECLICKED );
}

bool buttonMultiClicked() {
    return grabandclearbuttonflag( BUTTON_BITFLAG_MULITCLICKED );
}


// The number of clicks in the longest consecutive valid click cycle since the last time called.
byte buttonClickCount(void) {
    return buttonSnapshotClickcount;
}

// Remember that a long press fires while the button is still down
bool buttonLongPressed(void) {
    return grabandclearbuttonflag( BUTTON_BITFLAG_LONGPRESSED );
}

// 6 second press. Note that this will trigger seed mode if the blink is alone so
// you will only ever see this if blink has neighbors when the button hits the 6 second mark.
// Remember that a long press fires while the button is still down
bool buttonLongLongPressed(void) {
    return grabandclearbuttonflag( BUTTON_BITFLAG_3SECPRESSED );
}


// --- Utility functions

Color makeColorRGB( byte red, byte green, byte blue ) {

    // Internal color representation is only 5 bits, so we have to divide down from 8 bits
    return Color( red >> 3 , green >> 3 , blue >> 3 );

}

Color makeColorHSB( uint8_t hue, uint8_t saturation, uint8_t brightness ) {

    uint8_t r;
    uint8_t g;
    uint8_t b;

    if (saturation == 0)
    {
        // achromatic (grey)
        r =g = b= brightness;
    }
    else
    {
        unsigned int scaledHue = (hue * 6);
        unsigned int sector = scaledHue >> 8; // sector 0 to 5 around the color wheel
        unsigned int offsetInSector = scaledHue - (sector << 8);  // position within the sector
        unsigned int p = (brightness * ( 255 - saturation )) >> 8;
        unsigned int q = (brightness * ( 255 - ((saturation * offsetInSector) >> 8) )) >> 8;
        unsigned int t = (brightness * ( 255 - ((saturation * ( 255 - offsetInSector )) >> 8) )) >> 8;

        switch( sector ) {
            case 0:
            r = brightness;
            g = t;
            b = p;
            break;
            case 1:
            r = q;
            g = brightness;
            b = p;
            break;
            case 2:
            r = p;
            g = brightness;
            b = t;
            break;
            case 3:
            r = p;
            g = q;
            b = brightness;
            break;
            case 4:
            r = t;
            g = p;
            b = brightness;
            break;
            default:    // case 5:
            r = brightness;
            g = p;
            b = q;
            break;
        }
    }

    return( makeColorRGB( r  , g   , b  ) );
}

// OMG, the Ardiuno rand() function is just a mod! We at least want a uniform distibution.

// We base our generator on a 32-bit Marsaglia XOR shifter
// https://en.wikipedia.org/wiki/Xorshift

/* The state word must be initialized to non-zero */

// Here we use Marsaglia's seed (page 4)
// https://www.jstatsoft.org/article/view/v008i14
static uint32_t rand_state=2463534242UL;

// Generate a new seed using entropy from the watchdog timer
// This takes about 16ms * 32 bits = 0.5s

void randomize() {
    
    WDTCSR =  _BV(WDIE);                // Enable WDT interrupt, leave timeout at 16ms (this is the shortest timeout)
              
    // The WDT timer is now generating an interrupt about every 16ms
    // https://electronics.stackexchange.com/a/322817    
    
    for( uint8_t bit=32; bit; bit-- ) {
                               
        blinkbios_pixel_block.capturedEntropy=0;                                                          // Clear this so we can check to see when it gets set in the background               
        while (blinkbios_pixel_block.capturedEntropy==0 || blinkbios_pixel_block.capturedEntropy==1  );   // Wait for this to get set in the background when the WDT ISR fires
                                                                                                          // We also ignore 1 to stay balanced since 0 is a valid possible TCNT value that we will ignore                               
        rand_state <<=1;
        rand_state |= blinkbios_pixel_block.capturedEntropy & 0x01;            // Grab just the bottom bit each time to try and maximum entropy
                       
    }   
    
    wdt_disable();
        
}

// Note that rand executes the shift feedback register before returning the next result
// so hopefully we will be spreading out the entropy we get from randomize() on the first invokaton. 

static uint32_t nextrand32()
{
	// Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs" 
	uint32_t x = rand_state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	rand_state = x;
	return x;
}


#define GETNEXTRANDUINT_MAX ( (word) -1 )

word randomWord(void) {
	
	// Grab bottom 16 bits
	
	return ( (uint16_t) nextrand32() );

}

// return a random number between 0 and limit inclusive.
// https://stackoverflow.com/a/2999130/3152071

word random( uint16_t limit ) {

    word divisor = GETNEXTRANDUINT_MAX/(limit+1);
    word retval;

    do {
        retval = randomWord() / divisor;
    } while (retval > limit);

    return retval;
}

/*

    The original Arduino map function which is wrong in at least 3 ways.

    We replace it with a map function that has proper types, does not overflow, has even distribution, and clamps the output range.

    Our code is based on this...

    https://github.com/arduino/Arduino/issues/2466

    ...downscaled to `word` width and with up casts added to avoid overflows (yep, even the corrected code
    in the `map() function equation wrong` discoussion would still overflow :/ ).

    In the casts, we try to keep everything at the smallest possible width as long as possible to hold the result, but we have to bump up
    to do the multiply. We then cast back down to (word) once we divide the (uint32_t) by a (word) since we know that will fit.

    We could trade code for performance here by special casing out each possible overflow condition and reordering the operations to avoid the overflow,
    but for now space more important than speed. User programs can alwasy implement thier own map() if they need it since this will not link in if it is
    not called.

    Here is some example code on how you might efficiently handle those multiplys...

    http://ww1.microchip.com/downloads/en/AppNotes/Atmel-1631-Using-the-AVR-Hardware-Multiplier_ApplicationNote_AVR201.pdf

*/



word map(word x, word in_min, word in_max, word out_min, word out_max)
{
    // if input is smaller/bigger than expected return the min/max out ranges value
    if (x < in_min) {

        return out_min;

    } else if (x > in_max) {

        return out_max;

    } else {

        // map the input to the output range.
        if ((in_max - in_min) > (out_max - out_min)) {

            // round up if mapping bigger ranges to smaller ranges
            // the only time we need full width to avoid overflow is after the multiply but before the divide,
            // and the single (uint32_t) of the first operand should promote the entire expression - hopefully optimally.
            return (word) ( ((uint32_t) (x - in_min)) * (out_max - out_min + 1) / (in_max - in_min + 1) ) + out_min;

        } else {

            // round down if mapping smaller ranges to bigger ranges
            // the only time we need full width to avoid overflow is after the multiply but before the divide,
            // and the single (uint32_t) of the first operand should promote the entire expression - hopefully optimally.
            return (word) ( ((uint32_t) (x - in_min)) *  (out_max - out_min)  / (in_max - in_min) ) + out_min;

        }
    }
}

// Returns the device's unique 8-byte serial number
// TODO: This should this be in the core for portability with an extra "AVR" byte at the front.

// 0xF0 points to the 1st of 8 bytes of serial number data
// As per "13.6.8.1. SNOBRx - Serial Number Byte 8 to 0"


const byte * const serialno_addr = ( const byte *)   0xF0;


// Read the unique serial number for this blink tile
// There are 9 bytes in all, so n can be 0-8


byte getSerialNumberByte( byte n ) {

    if (n>8) return(0);

    return serialno_addr[n];

}

// Returns the currently blinkbios version number. 
// Useful to check is a newer feature is available on this blink.

byte getBlinkbiosVersion() {
    return BLINKBIOS_VERSION_VECTOR();    
}

// Returns 1 if we have slept and woken since last time we checked
// Best to check as last test at the end of loop() so you can
// avoid intermediate display upon waking.

uint8_t hasWoken(void) {

    uint8_t ret = 0;

    if (hasWarmWokenFlag) {
        ret =1;
        hasWarmWokenFlag = 0;
    }

    if (blinkbios_button_block.wokeFlag==0) {       // This flag is set to 0 when waking!
        ret=1;
        blinkbios_button_block.wokeFlag=1;
    }

    return ret;

}

// Information on how the current game was loaded

uint8_t startState(void) {
    
    switch ( blinkbios_pixel_block.start_state ) {
        
        case BLINKBIOS_START_STATE_DOWNLOAD_SUCCESS:
            return START_STATE_DOWNLOAD_SUCCESS;
            
        case BLINKBIOS_START_STATE_WE_ARE_ROOT:
            return START_STATE_WE_ARE_ROOT;            
    }
    
    // Safe catch all to be safe in case new ones are ever added
    return START_STATE_POWER_UP;    
}

// --- Pixel functions

// Change the tile to the specified color
// NOTE: all color changes are double buffered
// and the display is updated when loop() returns


// Set the pixel on the specified face (0-5) to the specified color
// NOTE: all color changes are double buffered
// and the display is updated when loop() returns

// A buffer for the colors.
// We use a buffer so we can update all faces at once during a vertical
// retrace to avoid visual tearing from partially applied updates

void setColorOnFace( Color newColor , byte face ) {

    // This is so ugly, but we need to match the volatile in the shared block to the newColor
    // There must be a better way, but I don't know it other than a memcpy which is even uglier!

    // This at least gets the semantics right of coping a snapshot of the actual value.

    blinkbios_pixel_block.pixelBuffer[face].as_uint16 =  newColor.as_uint16;              // Size = 1940 bytes


    // This BTW compiles much worse

    //  *( const_cast<Color *> (&blinkbios_pixel_block.pixelBuffer[face])) =  newColor;       // Size = 1948 bytes

}


void setColor( Color newColor) {

    FOREACH_FACE(f) {
        setColorOnFace( newColor , f );
    }

}


void setFaceColor(  byte face, Color newColor ) {

    setColorOnFace( newColor , face );

}


// This truly lovely code from the FastLED library
// https://github.com/FastLED/FastLED/blob/master/lib8tion/trig8.h
// ...adapted to save RAM by stroing the table in PROGMEM

/// Fast 8-bit approximation of sin(x). This approximation never varies more than
/// 2% from the floating point value you'd get by doing
///
///     float s = (sin(x) * 128.0) + 128;
///
/// @param theta input angle from 0-255
/// @returns sin of theta, value between 0 and 255

PROGMEM const uint8_t b_m16_interleave[] = { 0, 49, 49, 41, 90, 27, 117, 10 };

byte sin8_C( byte theta)
{
    uint8_t offset = theta;
    if( theta & 0x40 ) {
        offset = (uint8_t)255 - offset;
    }
    offset &= 0x3F; // 0..63

    uint8_t secoffset  = offset & 0x0F; // 0..15
    if( theta & 0x40) secoffset++;

    uint8_t section = offset >> 4; // 0..3
    uint8_t s2 = section * 2;
    const uint8_t* p = b_m16_interleave;
    p += s2;
    
    uint8_t b   =  pgm_read_byte(p);
    p++;
    uint8_t m16 =  pgm_read_byte(p);

    uint8_t mx = (m16 * secoffset) >> 4;

    int8_t y = mx + b;
    if( theta & 0x80 ) y = -y;

    y += 128;

    return y;
}

// #define NO_STACK_WATCHER to disable the stack overflow detection.
// saves a few bytes of flash and 2 bytes RAM

#ifdef NO_STACK_WATCHER


    void statckwatcher_init() {
    }

    uint8_t stackwatcher_intact() {
        return 1;
    }


#else 

    // We use this sentinel to see if we blew the stack
    // Note that we can not statically initialize this because it is not
    // in the initialized part of the data section.
    // We check it periodically from the ISR

    uint16_t __attribute__(( section(".stackwatcher") )) stackwatcher;

    #define STACKWATCHER_MAGIC_VALUE 0xBABE

    void statckwatcher_init() {
        stackwatcher = STACKWATCHER_MAGIC_VALUE;
    }

    uint8_t stackwatcher_intact() {
        return stackwatcher == STACKWATCHER_MAGIC_VALUE;
    }
    
    

#endif

// This is the main event loop that calls into the arduino program
// (Compiler is smart enough to jmp here from main rather than call!
//     It even omits the trailing ret!
//     Thanks for the extra 4 bytes of flash gcc!)

void __attribute__((noreturn)) run(void)  {
    
    // TODO: Is this right? Should hasWoke() return true or false on the first check after start up?

    blinkbios_button_block.wokeFlag = 1;        // Clear any old wakes (wokeFlag is cleared to 0 on wake)

    updateNow();                    // Initialize out internal millis so that when we reset the warm sleep counter it is right, and so setup sees the right millis time
    reset_warm_sleep_timer();
    
    statckwatcher_init();   // Set up the sentinel byte at the top of RAM used by variables so we can tell if stack clobbered it

    setup();

    while (1) {
        
        // Did we blow the stack?
        
        if (!stackwatcher_intact()) {
            // If so, show error code to user
            BLINKBIOS_ABEND_VECTOR(4);
        }

        // Here we check to enter seed mode. The button must be held down for 6 seconds and we must not have any neighbors
        // Note that we directly read the shared block rather than our snapshot. This lets the 6 second flag latch and
        // so to the user program if we do not enter seed mode because we have neighbors. See?

        if (( blinkbios_button_block.bitflags & BUTTON_BITFLAG_3SECPRESSED) && isAlone() ) {

            // Button has been down for 6 seconds and we are alone...
            // Signal that we are about to go into seed mode with full blue...
            
            // First save the game pixels because our blue seed spin is going to mess them up
            // and we will need to get them back if the user continues to hold past the seed phase
            // and into the warm sleep phase.

            savePixels();            

            // Now wait until either the button is lifted or is held down past 7 second mark
            // so we know what to do

            uint8_t face = 0;

            while ( blinkbios_button_block.down && ! ( blinkbios_button_block.bitflags & BUTTON_BITFLAG_6SECPRESSED)  ) {

                // Show a very fast blue spin that it would be hard for a user program to make
                // during the 1 second they have to let for to enter seed mode

                setColor(OFF);
                setColorOnFace( BLUE , face++ );
                if (face==FACE_COUNT) face=0;
                BLINKBIOS_DISPLAY_PIXEL_BUFFER_VECTOR();

            }
            
            restorePixels();

            if ( blinkbios_button_block.bitflags & BUTTON_BITFLAG_6SECPRESSED ) {

                // Held down past the 7 second mark, so this is a force sleep request

                warm_sleep_cycle();

                // Clear out the press that put us to sleep so we do not see it again
                // Also clear out everything else so we start with a clean slate on waking
                                
                blinkbios_button_block.bitflags = 0;

            } else {

                // They let go before we got to 7 seconds, so enter SEED mode! (and never return!)

                // Give instant visual feedback that we know they let go of the button
                // Costs a few bytes, but the checksum in the bootloader takes a a sec to complete before we start sending)
                setColorNow( OFF );

                BLINKBIOS_BOOTLOADER_SEED_VECTOR();

                __builtin_unreachable();

            }
        }

        if ( ( blinkbios_button_block.bitflags & BUTTON_BITFLAG_6SECPRESSED)  ) {

            warm_sleep_cycle();

            // Clear out the press that put us to sleep so we do not see it again
            // Also clear out everything else so we start with a clean slate on waking
            blinkbios_button_block.bitflags = 0;

        }

        // Capture time snapshot
        // Used by millis() and Timer thus functions
        // This comes after the possible button holding to enter seed mode
       
        updateNow();
                
        if ( blinkbios_button_block.bitflags & BUTTON_BITFLAG_PRESSED  ) {  // Any button press resets the warm sleep timeout
            viralPostponeWarmSleep();
        }

        cli();
        buttonSnapshotDown       = blinkbios_button_block.down;
        buttonSnapshotBitflags  |= blinkbios_button_block.bitflags;     // Or any new flags into the ones we got
        blinkbios_button_block.bitflags=0;                              // Clear out the flags now that we have them
        buttonSnapshotClickcount = blinkbios_button_block.clickcount;
        sei();


        // Update the IR RX state
        // Receive any pending packets
        RX_IRFaces();

        loop();

        // Update the pixels to match our buffer

        BLINKBIOS_DISPLAY_PIXEL_BUFFER_VECTOR();

        // Transmit any IR packets waiting to go out
        // Note that we do this after loop had a chance to update them.
        TX_IRFaces();

        if (warm_sleep_time.isExpired()) {

            warm_sleep_cycle();

        }
        
    }


}
