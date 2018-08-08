/*  Fracture (Game)
 *  
 *  Setup: For 3-6 players. Each player gets 5-6 tiles of a specific color.
 *         Game starts will all tiles connected, but completely segregated by color.
 *
 *  Goal: Get your tiles to be touching only other players tiles.
 *        Your tiles must be touching at least two other tiles to feel happily diverse.
 *
 *  Turns: Start from a player of choice and then go clockwise taking turns.
 *         A turn consists of fracturing the population into two sub populations,
 *         and then connecting them back together in any way you please.
 *  
 *  When a player has a tile fully diversified, it will blink in unison with other blinking Blinks of its color,
 *  first player to have all of their tiles blink wins. 
 *
 *  Game devopment by: Celia Pearce, Jeanie Choi, Isabella Carlson, Mike Lazer-Walker, Joshua Sloane
 *
 *  by Jonathan Bobrow
 *  9.28.2017
 */

#define HAPPY_FLASH_DURATION 500

Color displayColor;

Color teamColors[] = {RED, BLUE, YELLOW, GREEN};

byte teamIndex = 0;

Timer happyFlashTimer;
bool happyFlashOn;

void setup() {
}

void loop() {
  
  // change team if triple clicked
  if (buttonMultiClicked()) {
    if (buttonClickCount() == 3) {
      teamIndex++;
      if(teamIndex>= COUNT_OF(teamColors)) {
        teamIndex = 0;      
      }
    }
  }

  byte numNeighbors = 0;
  bool noNeighborsOfSameColor = true;

  // look at neighbors
  FOREACH_FACE(f) {
    if(!isValueReceivedOnFaceExpired(f)) {
      
      numNeighbors++;
    
      // if their color is the same as mine... not happy
      if (getLastValueReceivedOnFace(f) == teamIndex) {
        noNeighborsOfSameColor = false;
      }
    }
  }

  bool isHappy = false;
  
  // if I have two neighbors or more and my neighbors are not my color i'm happy
  if (numNeighbors >= 2 && noNeighborsOfSameColor) {
    isHappy = true;
  }

  bool isOn = true;
  
  // if I'm happy
  if (isHappy) {

    if(happyFlashTimer.isExpired()) {
      happyFlashOn = !happyFlashOn;
      
      happyFlashTimer.set(HAPPY_FLASH_DURATION);
    }
    
    if(!happyFlashOn) {
      isOn = false;
    }
  }

  if(isOn) {
    // have the color on the Blink raise and lower to feel more alive
    byte bri = 220 + 35 * sin_d( (millis()/10) % 360); // oscillate between values 185 and 255
    setColor(dim(teamColors[teamIndex], bri));
  }
  else {
    setColor(OFF);
  }

  setValueSentOnAllFaces(teamIndex);
}


// Sin in degrees ( standard sin() takes radians )

float sin_d( uint16_t degrees ) {

    return sin( ( degrees / 360.0F ) * 2.0F * PI   );
}
