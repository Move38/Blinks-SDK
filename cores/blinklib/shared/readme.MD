Do not manually change these files!

They come from the `blinkbios` and must travel as a coherent set. 

`bootloader/BlinkBIOS.hex` contains the compiled image of the BlinkBIOS code. It is loaded in upper flash and takes care of lots of common hardware tasks so that individual games do not need duplicated code. 

To be able to successfully talk to the BIOS code, the blink user code must be compiled against the matching header files.

All communications between user code the bios happens though the shared memory blocks defined in these headers. Fields marked as bios-volatile can be updated in background by the bios. 
 
