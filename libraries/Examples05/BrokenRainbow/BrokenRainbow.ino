// Demo showing an idiomatic state-based method for deteriming the 
// distance from a user-chosen center blink. It correctly handles intervening 
// gaps in groups with a radius of up to 5.

// Hold the button on any blink to make it the center.
// Center lights up white, then rings of blue, green, red, yellow, and cyan 
// for tiles that are 1,2,3,4, & 5 hops away from the center. 

// Editor's note: I am pleased with this strategy's ability to do all this using a meager 
// 55 symbols, I have a hunch that it is possible to do ever better by expolting the symetry
// in rings besides 0, 1, and 2. Sadly, I can no figure out how to do this. Please do let me know
// if you ever figure it out!

#include <avr/pgmspace.h>
#include "Serial.h"


struct loc {
  byte ring;    // How far away from the center? Center is 0. First ring (6 tiles) is 1.
  byte step;    // How far from the radial? In line with the center face is 1, counts clockwise until next radial. (I know- I should have started at 0)
  byte face;    // Which compass heading? Heading 0 is on radial away from center, counts clockwise.
};

struct boarder {
  loc source;
  loc target;  
};


// lotmap is used two ways
// first, a sending tile will look for the location of its sending face to find out what code to send.
// second, a recieving tile will look up the code it gets to find the location of the face the code was recieved on.
// The code is the index in this array just becuase that is convievent. 

// Note that the firest ring in numbered 0 and the first face is numbered 0, but the first step in each ring is 1. 
// It should be numbered 0 for consistancy, but when I first draw the map I started at 1 and too late to redo now. :/

const PROGMEM boarder boarder_map[] = {

  /*

  // Ring 0 only has one tile - the center tile. It is special since all 6 of its
  // faces touch effectively the same target face becuase of the 6-fold symetry around it.   
  // Note this is the only place where a destination loc is used more than once

  // As a knod to practicallity, we will special case out the sends on the center tile to 
  // save 5 symbols.

  // *** Ring 0
  
  {{ 0 , 1 , 0 } , { 1 , 1 , 3 } },
  {{ 0 , 1 , 1 } , { 1 , 1 , 3 } },
  {{ 0 , 1 , 2 } , { 1 , 1 , 3 } },
  {{ 0 , 1 , 3 } , { 1 , 1 , 3 } },
  {{ 0 , 1 , 4 } , { 1 , 1 , 3 } },
  {{ 0 , 1 , 5 } , { 1 , 1 , 3 } },

  */
  
  {{ 0 , 0 , 0 } , { 1 , 1 , 3 } },     // Use special value {0,0,0} for all ring 0 faces. 
                                        // Only used by special case code when we are the center

  // Ring 1 is also special because any tile in this ring is guaranteed to be touching
  // the center, so it will always know that it is at ring 1 without any other inputs,
  // so all other tiles that touch ring 1 tiles (except the center) do not send anything to them,
  // and of course they do not need to send anything to the center since it knows it is the center 
  // by definition. 

  // **** Ring 1 
  // ** Step 1 (Radial - all tiles in this ring are on a radial)

  { { 1 , 1, 0} , { 2 , 1 , 3} },
  { { 1 , 1, 1} , { 2 , 2 , 4} },
  // Face 2 touches 1.1 in ring 1, so no need to send anything
  // Face 3 touches the center, so no need to send anything
  // Face 4 touches 1.1 in ring 1 (on the symetric adjacent radial), so no need to send anything  
  { { 1 , 1, 5} , { 2 , 2, 3} },

  // **** Ring 2
  // ** Step 1 (Radial)

  { { 2 , 1, 0} , { 3 , 1, 3} },
  { { 2 , 1, 1} , { 3 , 2, 4} },
  { { 2 , 1, 2} , { 2 , 2, 5} },
  // This face touches a ring 1 tile, which always touches the center, so no need to send anything
  { { 2 , 1, 4} , { 2 , 2, 2} },    // Wrap to next radial
  { { 2 , 1, 5} , { 3 , 3, 3} },    // Wrap to next radial
  
  // ** Step 2

  { { 2 , 2, 0} , { 3 , 2, 3} },
  { { 2 , 2, 1} , { 3 , 3, 4} },
  { { 2 , 2, 2} , { 2 , 1, 4} },    // Wrap to next radial
  // This face touches a ring 1 tile, which always touchs the center, so no need to send anything
  // This face touches a ring 1 tile, which always touchs the center, so no need to send anything
  { { 2 , 2, 5} , { 2 , 1, 2} },    


  // **** Ring 3
  // ** Step 1 (Radial)

  { { 3 , 1, 0} , { 4 , 1, 3} },
  { { 3 , 1, 1} , { 4 , 2, 4} },
  { { 3 , 1, 2} , { 3 , 2, 5} },
  { { 3 , 1, 3} , { 2 , 1, 0} },  
  { { 3 , 1, 4} , { 3 , 3, 2} },    // Wrap to next radial
  { { 3 , 1, 5} , { 4 , 4, 3} },    // Wrap to next radial
  
  // ** Step 2 

  { { 3 , 2, 0} , { 4 , 2, 3} },
  { { 3 , 2, 1} , { 4 , 3, 4} },
  { { 3 , 2, 2} , { 3 , 3, 5} },    
  { { 3 , 2, 3} , { 2 , 2, 3} },
  { { 3 , 2, 4} , { 2 , 1, 1} },    
  { { 3 , 2, 5} , { 3 , 1, 2} },    

  // Step 3

  { { 3 , 3, 0} , { 4 , 3, 3} },
  { { 3 , 3, 1} , { 4 , 4, 4} },
  { { 3 , 3, 2} , { 3 , 1, 4} },    // Wrap to next radial
  { { 3 , 3, 3} , { 2 , 1, 5} },    // Wrap to next radial
  { { 3 , 3, 4} , { 2 , 2, 1} },    
  { { 3 , 3, 5} , { 3 , 2, 2} },    


  // **** Ring 4
  // ** Step 1 (Radial)

  { { 4 , 1, 0} , { 5 , 1, 3} },
  { { 4 , 1, 1} , { 5 , 2, 4} },
  { { 4 , 1, 2} , { 4 , 2, 5} },
  { { 4 , 1, 3} , { 3 , 1, 0} },  
  { { 4 , 1, 4} , { 4 , 4, 2} },    // Wrap to next radial
  { { 4 , 1, 5} , { 5 , 5, 3} },    // Wrap to next radial
  
  // ** Step 2

  { { 4 , 2, 0} , { 5 , 2, 3} },
  { { 4 , 2, 1} , { 5 , 3, 4} },
  { { 4 , 2, 2} , { 4 , 3, 5} },
  { { 4 , 2, 3} , { 3 , 2, 0} },  
  { { 4 , 2, 4} , { 3 , 1, 1} },    
  { { 4 , 2, 5} , { 4 , 1, 2} },    
  
  // ** Step 3

  { { 4 , 3, 0} , { 5 , 3, 3} },
  { { 4 , 3, 1} , { 5 , 4, 4} },
  { { 4 , 3, 2} , { 4 , 4, 5} },    
  { { 4 , 3, 3} , { 3 , 3, 0} },    
  { { 4 , 3, 4} , { 3 , 2, 1} },    
  { { 4 , 3, 5} , { 4 , 2, 2} },    

  // ** Step 4

  { { 4 , 4, 0} , { 5 , 4, 3} },
  { { 4 , 4, 1} , { 5 , 5, 4} },
  { { 4 , 4, 2} , { 4 , 1, 4} },    // Wrap
  { { 4 , 4, 3} , { 3 , 1, 5} },    // Wrap
  { { 4 , 4, 4} , { 3 , 3, 1} },    
  { { 4 , 4, 5} , { 4 , 3, 2} },    
  
};


const byte IR_IDLE_VALUE = IR_DATA_VALUE_MAX;   // A value we send when we do not have a code to send either 
                                                // becuase there is no center or becuase we have reached the egde

// Should be PROGMEM

const Color ringcolors[] = { WHITE ,  BLUE , GREEN , RED , YELLOW, CYAN };


// Find what code to send on a given face if we know our location
// Linear search for simplicty. Remebr that the index in the boarder_map[] array 
// corresponds to the code for that enrty. 

const byte lookupCodeToSend( byte ring , byte step , byte face ) {

  for( byte i =0; i < COUNT_OF( boarder_map ) ; i++ ) {

    loc l;

    // Read from progmem
    memcpy_P( &l , &boarder_map[i].source, sizeof(loc)) ;
    
    if ( l.ring == ring && l.step == step && l.face == face ) {
      return i;   
    }

    
  }

  // Not found in map

  return IR_IDLE_VALUE;

}

// Return the `loc` struct for a ggiven code

loc readTargetFromBoarderMap( byte index ) {
  loc r;
  memcpy_P( &r , &boarder_map[index].target , sizeof( loc ) );
  return r;
}


void setup() {
  // Function intentionally left blank
}

const byte NO_PARENT_FACE = FACE_COUNT ;   // Signals that we do not currently have a parent

byte parent_face=NO_PARENT_FACE;

Timer lockout_timer;    // This prevents us from forming packet loops by giving changes time to die out. 
                        // Remember that timers init to expired state


// where are we? Only valid when `parent_face != FACE_COUNT`
// Remember this is always relative to parent_face

loc us;

const int LOCKOUT_TIMER_MS = 250;               // This should be long enough for very large loops, but short enough to be unnoticable


void loop() {

  if (buttonDown()) {

    // I am the center blink!

    FOREACH_FACE(f) {

      // This is the hack where we explicity send a special "I am center" code rather than using the locmap (see notes in locmap)
      // We picked 0,0,0 in the boarder_map above for this special code.
      
      setValueSentOnFace( lookupCodeToSend(0,0,0) , f );
      
    }

    // This stuff is to reset everything clean for when the button is released....
  
    parent_face = NO_PARENT_FACE;         
    
    lockout_timer.set(LOCKOUT_TIMER_MS);  

    setColor( ringcolors[0] );    // We are ring 0
  
    // That's all for the center blink!
    
  } else {

    // I am not the center blink!

    if (parent_face == NO_PARENT_FACE ) {   // If we have no parent, then look for one

      if (lockout_timer.isExpired()) {      // ...but only if we are not on lockout
    
        FOREACH_FACE(f) {
  
          if (!isValueReceivedOnFaceExpired(f) ) {

            byte ir_data = getLastValueReceivedOnFace(f);

            if (ir_data != IR_IDLE_VALUE) {

              // Found a parent!
  
              parent_face = f;

              // Look up our location based on what we got

              if ( ir_data < COUNT_OF( boarder_map ) ) {

                // We got a location record telling us where we are in the world
                // Remeber that the face is relative to parent_face

                us =  readTargetFromBoarderMap( ir_data );

              }
            }            
          }        
        }
      }
      
    } else {

      // Make sure our parent is still there and good

      if (isValueReceivedOnFaceExpired(parent_face) || ( getLastValueReceivedOnFace(parent_face ) == IR_IDLE_VALUE) ) {

        // We had a parent, but our parent is now gone 

        parent_face = NO_PARENT_FACE;

        setValueSentOnAllFaces( IR_IDLE_VALUE );  // Propigate the no-parentness to everyone resets viraly 

        lockout_timer.set(LOCKOUT_TIMER_MS);    // Wait this long before accepting a new parent to prevent a loop
                
      }
      
    } 


    if (parent_face == NO_PARENT_FACE ) {

      setColor( OFF ); 
      
    } else {

      setColor( ringcolors[ us.ring ] );
      
    }
    
    // Set our output values relative to our north

    FOREACH_FACE(f) {

      if (parent_face==NO_PARENT_FACE ||  f == parent_face  ) {

        // If we have no parent or this face is our parent face, then send idle
        // We avoid sending back anything real on parent face to help avoid loops

        setValueSentOnFace( IR_IDLE_VALUE , f );

      } else {

        // All transmitted faces are relative to the outward center face as face 0 so
        // we need to use our parent_face to translate that to our internal face number. 
        // So if we got a target face of 0 on our face 0, then everything matches, but if we
        // got a target face 1 on our face 0 then when we send, we must make sure we send 
        // the boarder_map entry for face 1 on our face 0. 

        // Normalized face is the face in the global boarder_map to use for this local face f
        // remember that parent_face is the local face where we recieved the message

        // So `us.face` is the global face number for the local face `parent_face`

        // This actually does not change except when we get a new parent, but let's compute it here for clarity

        byte global_face_number_of_local_face_0 = (( FACE_COUNT + us.face) - parent_face ) % FACE_COUNT ;

        // This is now easy

        byte global_face_number_of_local_face_f = (global_face_number_of_local_face_0 + f) % FACE_COUNT;

        
        byte code = lookupCodeToSend( us.ring , us.step , global_face_number_of_local_face_f );
        
        setValueSentOnFace( code , f  );
        
      }
        
    }

  }

}
