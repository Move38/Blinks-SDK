/*
  Mortals for Blinks

   Setup: 2 player game. Tiles die slowly over time, all at the same rate.
   Moving a single tile to a new place allows it to suck life from
   surrounding tiles, friend or foe.

   Blinks start with 60 seconds of life

   When a tile is moved alone, it sucks 5 seconds of health from each one of
   its newly found neighbors. A non-moved neighbor with a new neighbor looses 5 seconds
   and animates in the direction it lost the life (i.e. where the neighbor showed up)

    Tiles resets health to full when triple press occurs

     States for game piece.
     Alive/Dead
     Team
     Attack, Injured

   Technical Details:
   A long press on the tile changes the color of the tile for prototyping (switch state 1 or 2)


*/

#define ATTACK_VALUE 5                // Amount of health you loose when attacked.
#define ATTACK_DURRATION_MS   100     // Time between when we see first new neighbor and when we stop attacking.
#define HEALTH_STEP_TIME_MS  1000     // Health decremented by 1 unit this often

#define INJURED_DURRATION_MS     750     // How long we stay injured after we are attacked. Prevents multiple hits on the same attack cycle.
#define INJURY_DECAY_VALUE        10     // How much the injury decays each interval
#define INJURY_DECAY_INTERVAL_MS  30     // How often we decay the the injury

#define INITIAL_HEALTH         60
#define MAX_HEALTH             90

#define MAX_TEAMS           4

byte team = 0;

int health;

Timer healthTimer;  // Count down to next time we loose a unit of health
Timer injuryDecayTimer; // Timing to fade away the injury

byte injuryBrightness = 0;
byte injuredFace;

byte deathBrightness = 0;

enum State {
  DEAD,
  ALIVE,
  ENGUARDE,   // I am ready to attack!
  ATTACKING,  // Short window when I have already come across my first victim and started attacking
  INJURED
};

byte mode = DEAD;

Timer modeTimeout;     // Started when we enter ATTACKING, when it expires we switch back to normal ALIVE.
// Started when we are injured to make sure we don't get injured multiple times on the same attack

void setup() {
}


void loop() {

  // Update our mode first

  if (buttonDoubleClicked()) {
    // reset game piece
    mode = ALIVE;
    health = INITIAL_HEALTH;
    healthTimer.set(HEALTH_STEP_TIME_MS);
  }

  if (buttonLongPressed()) {
    // change team
    team = (team + 1) % MAX_TEAMS;
  }

  if (healthTimer.isExpired()) {

    if (health > 0) {

      health--;
      healthTimer.set(HEALTH_STEP_TIME_MS);

    } else {

      mode = DEAD;

    }

  }

  if ( mode != DEAD ) {

    if (isAlone()) {

      mode = ENGUARDE;      // Being lonesome makes us ready to attack!

    } else {  // !isAlone()

      if (mode == ENGUARDE) {   // We were ornery, but saw someone so we begin our attack in earnest!

        mode = ATTACKING;
        modeTimeout.set( ATTACK_DURRATION_MS );
      }

    }


    if (mode == ATTACKING || mode == INJURED ) {

      if (modeTimeout.isExpired()) {
        mode = ALIVE;
      }
    }
  } // !DEAD

  FOREACH_FACE(f) {


    if (!isValueReceivedOnFaceExpired(f)) {

      byte neighborMode = getLastValueReceivedOnFace(f);

      if ( mode == ATTACKING ) {

        // We take our flesh when we see that someone we attacked is actually injured

        if ( neighborMode == INJURED ) {

          // TODO: We should really keep a per-face attack timer to lock down the case where we attack the same tile twice in a since interaction.

          health = min( health + ATTACK_VALUE , MAX_HEALTH );

        }

      } else if ( mode == ALIVE ) {

        if ( neighborMode == ATTACKING ) {

          health = max( health - ATTACK_VALUE , 0 ) ;

          mode = INJURED;

          injuredFace = f;  // set the face we are injured on

          injuryBrightness = 255; // Start very injured

          modeTimeout.set( INJURED_DURRATION_MS );

        }

      } else if (mode == INJURED) {

        if (modeTimeout.isExpired()) {

          mode = ALIVE;

        }
        else {

          if ( injuryDecayTimer.isExpired() ) {

            injuryDecayTimer.set( INJURY_DECAY_INTERVAL_MS );

            injuryBrightness -= INJURY_DECAY_VALUE;
          }
        }
      }
    }
  }


  // Update our display based on new state

  switch (mode) {

    case DEAD:
      displayGhost();
      break;

    case ALIVE:
      displayAlive();
      break;

    case ENGUARDE:
      displayEnguarde();
      break;

    case ATTACKING:
      displayAttack();
      break;

    case INJURED:
      displayInjured( injuredFace );
      break;

  }

  setValueSentOnAllFaces( mode );       // Tell everyone around how we are feeling

}

/*
   This map() functuion is now in Arduino.h in /dev
   It is replicated here so this skect can compile on older API commits
*/

long map_m(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

/*
   Sin in degrees ( standard sin() takes radians )
*/

float sin_d( uint16_t degrees ) {

  return sin( ( degrees / 360.0F ) * 2.0F * PI   );
}

/*
    Function to return a brightness while breathing based on the period of the breath
*/

byte breathe(word period_ms, byte minBrightness, byte maxBrightness) {

  byte brightness;


  brightness = map_m( 50 * (1 + sin_d( (360 * ((millis() % period_ms)) / (float)period_ms ))), 0, 100, minBrightness, maxBrightness);

  // TODO: if breaths are very short, be sure to focus on the extremes (i.e. light/dark)

  return brightness;
}

/*
  get the team color for our team
*/
Color teamColor( byte t ) {

  return makeColorHSB(60 + t * 50, 255, 255);

}

/*
   Display state for living Mortals
*/
void displayAlive() {
  if ( health > 50 ) {
    deathBrightness = 255;
    setColor( dim( teamColor( team ), breathe(6400, 128, 255) ) );
  } else if ( health <= 50 && health > 40 ) {
    setColor( dim( teamColor( team ), breathe(3200, 96, 255) ) );
  } else if ( health <= 40 && health > 30 ) {
    setColor( dim( teamColor( team ), breathe(1600, 64, 255) ) );
  } else if ( health <= 30 && health > 20 ) {
    setColor( dim( teamColor( team ), breathe(800, 64, 255) ) );
  } else if ( health <= 20 && health > 10 ) {
    setColor( dim( teamColor( team ), breathe(400, 32, 255) ) );
  } else if ( health <= 10 && health > 0 ) {
    setColor( dim( teamColor( team ), breathe(200, 32, 255) ) );
  } else {
    // glow bright white and fade out when we die
    setColor( dim(WHITE, deathBrightness) );
    if (deathBrightness > 7) {
      deathBrightness -= 8;
    }
  }
}

/*
   Display state for injured Mortal
   takes the face we were injured on

*/
void displayInjured(byte face) {

  // first we display our health
  displayAlive();

  // then we update the side that was injured

  // brighten the sides with neighbors
  if ( injuryBrightness > 32 ) {
    setFaceColor( face, dim( RED, injuryBrightness) );
  }

}

/*

*/
void displayGhost() {

  setColor(OFF);

  FOREACH_FACE(f) {

    setFaceColor(f, dim( RED, 32 + 32 * sin_d( (60 * f + millis() / 8) % 360)));  // slow dim rotation, just take my word for it :)

  }
}

/*

*/
void displayEnguarde() {

  setColor( OFF );

  setFaceColor( (millis() / 100) % FACE_COUNT, teamColor( team ) );

}

/*

*/
void displayAttack() {

  setColor( OFF );

  setFaceColor( rand(FACE_COUNT), teamColor( team ) );

}
