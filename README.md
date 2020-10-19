# Blinks Software Development Kit (SDK) for Arduino

This SDK includes all the software and configuration files to tell the Arduino IDE how to download programs in a blink along with some examples you can try out and modify.

These instructions will get you set up to write your own games for the [Blinks platform](https://move38.com/pages/blinks-collection) using the Arduino IDE.

Note that you do not need any of this if you just want to *play* blinks games, only if you want to write them.


## Getting Started

### Software setup

1. Download and run the latest version of the[ Arduino IDE](https://www.arduino.cc/en/Main/Software#download)
2. Go into File->Preferences->Settings and add...
`https://boardsmanager.com/package_move38.com-blinks_index.json`
...under Additional Boards Manager URLs. If you already have something there, you can use a comma to separate the multiple entries
3. Go to Tools->Board->Boards Manager and search for "blinks"
4. Press install on "Blinks by Move38"
5. Pick "Tools->Board->Move38->blink" from the menus 
5. Go to "File->Example->Getting Started->Button Press" to load an example program
6. Program the `Button Press` example code into a blink (see below)


### Getting your code programmed into a blink

Each blink can hold a game in its on-board flash memory. Once a blink has a game programmed into it, it can also share to other blinks over the IR links.  

You will need a programmer, a cable, and a connector to get your code from the Arduino IDE into a blink's flash. 
 
The [Blinks Developer Kit](https://move38.com/products/bare-bones-dev-kit) that includes everything you need to start programming your own games, including a a special blink that connects to an included USB programmer to make downloading your code as easy as pushing the "upload" button in the Ardunio IDE. If you have the dev kit, follow the enclosed instructions to select the correct programmer in the IDE and everything should just work.    
 
But you do not need the dev kit to write your own games. Blinks can be programmed with any Arduino-compatible "AVR ISP" programmer and there are hundreds of these available everywhere. You can even use an[ Arduino UNO board plus a few wires](https://www.arduino.cc/en/Tutorial/ArduinoISP#toc2) to program any blink though the 6-pin In System Programming (ISP) connector inside the battery compartment (the arrow marks pin #1). 

## What is Arduino?

The Arduino IDE is an an integrated development environment that makes it easy to write and download code to compatible microcontroller boards - and every blink is an Arduino compatible board.

We write a program in the IDE using Arduino programming language (basically the C/C++) and then we use a programmer to download the compiled program into a blink. 

## The `blinklib` library

When you pick "blink" from the boards menu, you configure the Arduino IDE to automatically include a set of binks-specific functions whenever it compiles your code. You use these functions to set the colors on the blink's LEDs and check for button presses and send messages over the IR links and everything else a blink can do. You can see all the available `blinklib` functions [here](cores/blinklib/blinklib.h).     

## Serial support

Each blink also has a built in serial port so you can add `printf` statements to your code and view the resulting output on a computer running a serial terminal program. Again, each blink dev kit includes a cable and a USB serial board to make this plug and play, but you can also connect the wires yourself to any serial port than can accept 3.3 - 5 volt signals. More info on serial [here](Service Port.MD).       

## Making changes to the open source `blinklib` library

You can make a fork of this repo and then clone it you the machine that you have the Arduino IDE installed on. Then add a symbolic link called `avr` to the top directory of the cloned repo (the directory with this readme in it) to the `Arduino/hardware/move38` directory. So, for example, on my windows machine I made the symbolic link with the commands...

```
d:
cd D:\Documents\Arduino\hardware
mkdir move38
cd move38
mklink /D avr D:\Github\Move38-Arduino-Platform
```

Quit out of the IDE and reload and you should see a new choice under the `boards` menu.

We are always grateful for pull requests with bug fixes. For new features, best to discuss first [in the forums](https://forum.move38.com/c/softwareresources/9) to see what other think and if anyone is already working on something similar. 

###
