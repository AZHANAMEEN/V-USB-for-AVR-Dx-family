extern "C" {
    #include "usbdrv.h"
}
#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#define REPORT_SIZE 8

const PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    (char)0xa1, 0x01,              // COLLECTION (Application)
    
    // Modifier Keys (Left/Right Ctrl, Shift, Alt, GUI - 8 bits total)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, (char)0xe0,              //   USAGE_MINIMUM (Keyboard Left Control)
    0x29, (char)0xe7,              //   USAGE_MAXIMUM (Keyboard Right GUI)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,                    //   REPORT_SIZE (1 bit)
    0x95, 0x08,                    //   REPORT_COUNT (8 items = 1 byte)
    (char)0x81, 0x02,              //   INPUT (Data,Variable,Absolute) -> Modifier Byte
    
    // Reserved Byte (Required by standard Boot Keyboard protocol)
    0x95, 0x01,                    //   REPORT_COUNT (1 item)
    0x75, 0x08,                    //   REPORT_SIZE (8 bits = 1 byte)
    (char)0x81, 0x03,              //   INPUT (Constant,Variable,Absolute) -> Padding Byte
    
    // LED Outputs (Num Lock, Caps Lock, etc. - 5 bits used, 3 bits padding)
    0x95, 0x05,                    //   REPORT_COUNT (5 items)
    0x75, 0x01,                    //   REPORT_SIZE (1 bit)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    (char)0x91, 0x02,              //   OUTPUT (Data,Variable,Absolute)
    0x95, 0x01,                    //   REPORT_COUNT (1 item)
    0x75, 0x03,                    //   REPORT_SIZE (3 bits)
    (char)0x91, 0x03,              //   OUTPUT (Constant,Variable,Absolute) -> LED Padding
    
    // Key Arrays (6 concurrent standard keypresses capability)
    0x95, 0x06,                    //   REPORT_COUNT (6 items = 6 bytes)
    0x75, 0x08,                    //   REPORT_SIZE (8 bits)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)
    (char)0x81, 0x00,              //   INPUT (Data,Array,Absolute) -> 6 Key Bytes
    
    (char)0xc0                     // END_COLLECTION
};

static uchar reportBuffer[REPORT_SIZE];
static uchar bytesRemaining;
static uchar idleRate;

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (usbRequest_t *)data;

    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch(rq->bRequest) {
        case USBRQ_HID_GET_REPORT:
            usbMsgPtr = (usbMsgPtr_t)reportBuffer;
            return sizeof(reportBuffer);

        case USBRQ_HID_SET_REPORT:
            bytesRemaining = rq->wLength.bytes[0];
            if(bytesRemaining > sizeof(reportBuffer)) {
                bytesRemaining = sizeof(reportBuffer);
            }
            return USB_NO_MSG;

        case USBRQ_HID_GET_IDLE:
            usbMsgPtr = (usbMsgPtr_t)&idleRate;
            return 1;

        case USBRQ_HID_SET_IDLE:
            idleRate = rq->wValue.bytes[1];
            return 0;
        }
    }

    return 0;
}

uchar usbFunctionWrite(uchar *data, uchar len)
{
    uchar offset;
    uchar i;

    if(len > bytesRemaining) {
        len = bytesRemaining;
    }

    offset = sizeof(reportBuffer) - bytesRemaining;
    for(i = 0; i < len; i++) {
        reportBuffer[offset + i] = data[i];
    }

    bytesRemaining -= len;
    return bytesRemaining == 0;
}

void setup()
{
    // Configure pin PA6 as our input button with internal pull-up
  pinMode(PIN_PA6, INPUT_PULLUP);
    
    // Clear out the buffer safely to start clean
  memset(reportBuffer, 0, sizeof(reportBuffer));

  usbInit();

  sei();
}

static bool lastState = HIGH;

void loop()
{

  usbPoll();
  usbPoll();

  bool currentState = digitalRead(PIN_PA6);
  usbPoll();

  if(currentState != lastState && usbInterruptIsReady()) {
    memset(reportBuffer, 0, sizeof(reportBuffer));
    if(currentState == LOW) {
      reportBuffer[2] = 0x04;    // 'A'
    }
    else {
      reportBuffer[2] = 0x00;
    }
    usbSetInterrupt(reportBuffer, sizeof(reportBuffer));

    lastState = currentState;
    }
  usbPoll();
  usbPoll();
}