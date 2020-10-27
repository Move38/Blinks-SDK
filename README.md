# Move38 Blinks for the Arduino IDE
An Arduino core for the Blinks gaming tile. More info at...
http://move38.com

## Roadmap

This core requires at least Arduino IDE v1.6.2, where v1.6.5+ is recommended. <br/>
* [Programmers](#programmers)
* **[How to install](#how-to-install)**

Covers installing this repo so that you can write and download code into a Blinks tile using the Arduino IDE. 

Ends with loading a `HelloBlink` program it a tile.

### Writing games

The best way to start writing games is to work your way though the examples in the "File->Examples->Examples for Blink Tile" menu in the Arduino IDE (after you have installed this repo as described in the Getting Started above).  
  

### [Service Port](Service Port.MD)

Describes the service port connector on each blink. Lets you add `print` statements to your programs, which can be very helpful during development.  

## How to install

Click on the "Download ZIP" button in the upper right corner of this repo. Extract the ZIP file, and move the extracted files to the location "**~/Documents/Arduino/hardware/Move38-manual/avr**". Create the folder if it doesn't exist. This readme file should be located at "**~/Documents/Arduino/hardware/Move38-manual/avr/README.md**" when you are done.

Open Arduino IDE, and a new category in the boards menu called "Move38-manual" will show up.

In the future, we'll offer a simplified Arduino Boards Manager install path.

### Notes 

* We called the "vendor/maintainer" folder `Move38-manual` so that you can also use the boards manager and you will be able to tell the two apart in the boards menu.

* You must manually create the `avr` folder and you must also manually move the files out from this repo into this folder. We could not automatically have the folds inside the repo match the Arduino required folder layout because in in the boards manager, the architecture is in the JSON file rather than the folder structure. Arg. 

* The "**~/Documents/Arduino/hardware/Move38-manual/avr**" folder is a Git repo and is also set up for easy editing in Atmel Studio with a solution inside the `\AS7` sub-folder. 

### API Layers

The blinks hardware can do incredible things, and you can have unfettered access to it at any level you want. This documents describes those layers from bare metal up.  



#### Hardware Abstraction Layer

Most programmers will want to use the high level `blinks` API, but if you want to get closer to the hardware you can directly call into the `HAL` (Hardware Abstraction Layer) that the `blinks` API is built on top of. Documentation for this layer is in the [README.md](cores/blinkcore/README.md) in the `cores/blinkscore` folder.
