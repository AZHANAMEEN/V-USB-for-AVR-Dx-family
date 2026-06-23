#include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <usbdrv.h>

#define REPORT_SIZE 8

PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x06, 0x00, 0xff,       /* Usage Page (Vendor Defined) */
    0x09, 0x01,             /* Usage (Vendor Usage 1) */
    0xa1, 0x01,             /* Collection (Application) */
    0x15, 0x00,             /* Logical Minimum (0) */
    0x26, 0xff, 0x00,       /* Logical Maximum (255) */
    0x75, 0x08,             /* Report Size (8 bits) */
    0x95, REPORT_SIZE,      /* Report Count */
    0x09, 0x00,             /* Usage */
    0x82, 0x02, 0x01,       /* Input (Data, Variable, Absolute, Buffered) */
    0x09, 0x00,             /* Usage */
    0x92, 0x02, 0x01,       /* Output (Data, Variable, Absolute, Buffered) */
    0x09, 0x00,             /* Usage */
    0xb2, 0x02, 0x01,       /* Feature (Data, Variable, Absolute, Buffered) */
    0xc0                    /* End Collection */
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
    uchar i;

    for(i = 0; i < sizeof(reportBuffer); i++) {
        reportBuffer[i] = 0;
    }

    usbInit();
    usbDeviceDisconnect();
    delay(250);
    usbDeviceConnect();
    sei();
}

void loop()
{
    static unsigned long lastReportMs;
    unsigned long now;

    usbPoll();

    now = millis();
    if(usbInterruptIsReady() && (now - lastReportMs >= 100)) {
        lastReportMs = now;
        reportBuffer[0]++;
        reportBuffer[1] = (uchar)(now & 0xff);
        reportBuffer[2] = (uchar)((now >> 8) & 0xff);
        usbSetInterrupt(reportBuffer, sizeof(reportBuffer));
    }
}
