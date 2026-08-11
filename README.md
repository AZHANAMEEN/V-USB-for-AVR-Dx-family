# V-USB for AVR Dx family

This is a port of the classic v usb library for avr chips to bitbang usb, to the newer avr dx lineup (only hardware tested on da chips specifically the AVR64DA28 and the AVR128DB26 By   vslinuxdotnet (it compiles successfully using the arduino ide for dd and du chips too)

# Project Structure 
/--------examples--------/

Ths directory contains some examples based on the ported code 

A generic hid example.

A single button keyboard example that types "A".

All are in .ino format so you can install the library and then open any one of them to test it out. 

/--------vusb licenses and vid pid info--------/

Licences and usb vid/pid usage info carried on from the original repo, original developer- "OBJECTIVE DEVELOPMENT"

/--------usbdrv--------/

This is the main directory  with the library files and driver code.
Here is a concise summary of what each file is.

usbdrv.h ............... Driver interface definitions and technical docs.

usbdrv.c ............... High level language part of the driver. 

usbdrvasm16.inc ......... Assembler routines for 16 MHz clock .

usbdrvasm24.inc ......... Assembler routines for 24 MHz clock . <del>(under work)<del>

asmcommon.inc .......... Common assembler routines. Included by usbdrvasm*.inc, don't link it directly!
                           
usbdrvasm.S ............ Assembler part of the driver. This module is mostly a stub and includes one of the usbdrvasm*.S files depending on processor clock.
                           
usbdrvasm.asm .......... Compatibility stub for IAR-C-compiler. Use this module instead of usbdrvasm.S when you assembler with IAR's tools.             
                           
usbportability.h ....... Header with compiler-dependent stuff.   

usbconfig-prototype.h .. Prototype for your own usbdrv.h file.  

oddebug.c .............. Debug functions. Only used when DEBUG_LEVEL is defined to a value greater than 0.          
                           
oddebug.h .............. Interface definitions of the debug module.  

  *Some of these definitions have been taken from the vusb page so that it does not become confusing*

# Usage

  You can start by making a minimal circuit for testing on breadboard like this-- **Make sure you power it with 3.3v else you will also need to add 3.6v zener diodes to safely operate over 3.3v like 5v**

  
  <img width="500" height="500" alt="breadboard circuit" src="https://github.com/user-attachments/assets/90d3e2ec-bbcb-4d8d-a5ef-39dfc2d359b9" />

  
  -- I.e. connect power with a 1uf electrolytic and a 0.1 uf ceramic capacitor on pins 20(+) and 21(-) 


  -- Make sure to define the D+ (PIN_PD6) And D- (PIN_PD7) pins according to your needs and interrupt availability 

  
  -- Default is pin 6 and 7 of port D which compiles fine on AVR-DA-DD Chips(even DU chips but those already have usb) but not tested in-hardware for DD or DU chips.

  
  -- A resistor of 1.5k ohm according to usb ls specs from 3.3v to D- (PIN_PD7) pin. if you do not have a 1.5k use whatever closest value you have try to stay under 3k ohm. 

  
  -- if you run the microcontroller at > 3.3v you will also need to add zener diodes from pin to ground to clamp voltage (as usb runs on strictly 3.3v data lines) after the 68 ohm series resistors 

  
  --I have skipped the termination resistors for now and im running at 3.3v so no need of zeners, a simple config to test software. 

  
  -- next step would be to connect the updi programmer to pin 19 (on the avr64da28), you can make a simple updi programmer with a serial adapter using its rx and tx pin with a 4.7k resistor in seies with *only* the tx pin and connect both to the updi pin ((i have used a ftdi usb to uart adapter for this)).


  -- You can head over to the github page [DXCore GitHub Repository](https://github.com/SpenceKonde/DxCore) for arduno ide installation. which you can use the library with in arduino ide. 

  -- Now dial in your chip settings in dxcore options and choose youe chip and the 16Mhz setting or 24Mhz which works now. 

  -- Just add the usbdrv folder to your libraries folder and you are good to go.

  -- head over to the examples in *Examples* folder in the repo and look at some of them.

  -- please note that you **cannot** mention usbdrv.h using the #include<> syntax rather use this 
  
    
    extern "C" {
      #include "usbdrv.h"
    }
    
    
  -- For making custom configs use the "usbconfig-prototype.h" or use the already provided for generic hid keyboard. 

  -- THANK YOU VERY MUCH!

# progress

--16Mhz working for hid devices--

<del>--24Mhz still has issues--</del>

--24 mhz finally working thanks to Vasco santos (vslinuxdotnet)--
