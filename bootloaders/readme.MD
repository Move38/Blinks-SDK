This folder contains the BlinkBIOS bootloader HEX file. The BlinkBIOS is a hardware abstraction layer that is loaded into the bootloader area of flash.

The blinkBIOS is an abstraction layer that provides access to the blinks hardware and is defined by the `blinkbios_*.h` files in the `cores/blinklib/shared` directory. 

Ideally we would like to program the BIOS once into each blink when it is manufactured and then only update the application section of the flash thereafter, but unfortunately on this chip the entire flash is always erased on each hardware programming cycle, so we must reprogram the bootloader each time. That is why this HEX file must be included here.
  
This file is used by...

1. The `program` recipe in `platform.txt` that downloads a new sketch to a blink. 
2. The `Export Binary` function which creates a single HEX file with both the current sketch and the BIOS. Note that this folder must be named `bootloader` as this is hardcoded into the Arduino IDE. 


 