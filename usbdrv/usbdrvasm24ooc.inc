/* Name: usbdrvasm24.inc
 * Project: V-USB, virtual USB port for Atmel's(r) AVR(r) microcontrollers
 * Author: Christian Starkjohann, ported to 24 MHz for AVRxt by Antigravity
 * Creation Date: 2026-06-23
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 *
 * AVRxt (AVR64DA28 / DxCore) timing patch — 2026
 * Instruction timing changes from AVRe/AVRe+ to AVRxt that affect this file:
 *   PUSH : 2 -> 1 cycle   (saves 1 cycle per PUSH; NOPs added to compensate)
 *   ST/STD : 2 -> 1 cycle (saves 1 cycle per store; NOPs added where needed)
 *   SBI/CBI : 2 -> 1 cycle (saves 1 cycle; NOP added after sbi in send path)
 *   LDS : 2 -> 3 cycles   (costs 1 extra cycle; compensated by removing a NOP where LDS occurs)
 *   POP : still 2 cycles  (unchanged; POP_STANDARD/POP_RETI macros unaffected)
 *   LD/LDD/IN/OUT/branches/ALU : all unchanged
 *
 * AVRxt GPR memory-map fix:
 * On classic AVR, GPRs r0-r31 were memory-mapped at data addresses 0x00-0x1F.
 * sendX3AndReti exploited this by loading Y=20 so that `ld shift, y+` read r20 (x3/cnt)
 * directly, avoiding a real buffer. On AVRxt (incl. AVR64DA28) that mapping is gone;
 * address 20 is a peripheral register, so every ACK/NAK would send garbage.
 * FIX: a 1-byte SRAM variable `usbPidBuf` (declared extern, defined in usbdrv.c) is used
 * as the scratch byte. x3 is written there with STS before Y is loaded, and the
 * `subi/sbci` sentinel check is updated to compare against usbPidBuf+2 instead of 22.
 * STS is 2 cycles on BOTH AVRe and AVRxt so no timing compensation is needed for it.
 * The net effect is that the single-byte send path (NAK/ACK) executes 2 cycles later
 * on the bus than on classic AVR; this is well within the USB inter-packet gap tolerance.
 */

/* Do not link this file! Link usbdrvasm.S instead, which includes the
 * appropriate implementation!
 */

/*
General Description:
This file is the 24 MHz version of the assembler part of the USB driver. It
requires a 24 MHz clock (crystal or calibrated oscillator).

Since almost all of this code is timing critical, don't change unless you
really know what you are doing! Many parts require not only a maximum number
of CPU cycles, but even an exact number of cycles!
*/

;max stack usage: [ret(2), YL, SREG, YH, bitcnt, shift, x1, x2, x3, x4, cnt] = 12 bytes
;nominal frequency: 24 MHz -> 16 cycles per bit, 128 cycles per byte
; Numbers in brackets are clocks counted from center of last sync bit
; when instruction starts

.extern usbPidBuf

USB_INTR_VECTOR:
;order of registers pushed: YL, SREG YH, [sofError], bitcnt, shift, x1, x2, x3, x4, cnt

; AVRxt prologue: PUSH is 1 cycle (was 2 on classic AVR)
; We add a nop after each PUSH to maintain original timing
    push    YL                  ;[-28] push only what is necessary to sync with edge ASAP
    nop                         ;[-27] AVRxt: PUSH is 1 cycle (was 2); pad to maintain timing
    in      YL, SREG            ;[-26]
    push    YL                  ;[-25]
    nop                         ;[-24] AVRxt: PUSH is 1 cycle (was 2); pad to maintain timing
    push    YH                  ;[-23]
    nop                         ;[-22] AVRxt: PUSH is 1 cycle (was 2); pad to maintain timing

;----------------------------------------------------------------------------
; Synchronize with sync pattern:
;----------------------------------------------------------------------------
;sync byte (D-) pattern LSb to MSb: 01010100 [1 = idle = J, 0 = K]
;sync up with J to K edge during sync pattern -- use fastest possible loops
;The first part waits at most 1 bit long since we must be in sync pattern.
;YL is guarenteed to be < 0x80 because I flag is clear. When we jump to
;waitForJ, ensure that this prerequisite is met.
waitForJ:
    inc     YL
    sbis    USBIN, USBMINUS
    brne    waitForJ        ; just make sure we have ANY timeout
waitForK:
;The following code results in a sampling window of < 1/4 bit which meets the spec.
    sbis    USBIN, USBMINUS     ;[-21]
    rjmp    foundK              ;[-20]
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
    sbis    USBIN, USBMINUS
    rjmp    foundK
#if USB_COUNT_SOF
    lds     YL, usbSofCount
    inc     YL
    sts     usbSofCount, YL
#endif  /* USB_COUNT_SOF */
#ifdef USB_SOF_HOOK
    USB_SOF_HOOK
#endif
    rjmp    sofError
foundK:                         ;[-18]
;{3, 5} after falling D- edge, average delay: 4 cycles
;bit0 should be at 42 for center sampling (24 MHz: 16 cycles/bit). Currently at ~4 so ~38 cycles till bit 0 sample
;use 1 bit time for setup purposes, then sample again. Numbers in brackets
;are cycles from center of first sync (double K) bit after the instruction
    push    bitcnt              ;[-18] AVRxt: 1 cycle
    nop                         ;[-17] AVRxt: PUSH is 1 cycle; pad to maintain timing
    lds     YL, usbInputBufOffset;[-16] AVRxt: 3 cycles (was 2); occupies [-16],[-15],[-14]
;   [---]                       ;[-15]
;   [---]                       ;[-14]
    clr     YH                  ;[-13]
    subi    YL, lo8(-(usbRxBuf));[-12] [rx loop init]
    sbci    YH, hi8(-(usbRxBuf));[-11] [rx loop init]
    push    shift               ;[-10] AVRxt: 1 cycle
    nop                         ;[-9]  AVRxt: pad for PUSH shift being 1 cycle instead of 2
    ldi     bitcnt, 0x55        ;[-8]  [rx loop init]
    sbis    USBIN, USBMINUS     ;[-7]  we want two bits K (sample 2-3 cycles too early)
    rjmp    haveTwoBitsK        ;[-6]
    pop     shift               ;[-5]  undo the push from before
    pop     bitcnt              ;[-3]  undo the push from before
    rjmp    waitForK            ;[-1]  this was not the end of sync, retry
; The entire loop from waitForK until rjmp waitForK above must not exceed two
; bit times (= 32 cycles at 24 MHz).

;----------------------------------------------------------------------------
; push more registers and initialize values while we sample the first bits:
;----------------------------------------------------------------------------
haveTwoBitsK:
; AVRxt: PUSH is 1 cycle. 5 pushes save 5 cycles total.
; We need to add 5 nop cycles distributed among the 5 pushes.

    push    x1              ;[-5]  AVRxt: 1 cycle
    nop                     ;[-4]  AVRxt: pad PUSH x1
    push    x2              ;[-3]  AVRxt: 1 cycle
    nop                     ;[-2]  AVRxt: pad PUSH x2
    push    x3              ;[-1]  AVRxt: 1 cycle
    nop                     ;[0]   AVRxt: pad PUSH x3
    ldi     shift, 0        ;[1]
    ldi     x3, 1<<4        ;[2]   [rx loop init] first sample is inverse bit, compensate that
    push    x4              ;[3]   == leap   AVRxt: 1 cycle
    nop                     ;[4]   AVRxt: pad PUSH x4

    in      x1, USBIN       ;[5]   <-- sample bit 0
    andi    x1, USBMASK     ;[6]
    bst     x1, USBMINUS    ;[7]
    bld     shift, 7        ;[8]
    push    cnt             ;[9]   AVRxt: 1 cycle
    nop                     ;[10]  pad PUSH cnt
    ldi     cnt, USB_BUFSIZE;[11]  [rx loop init]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    nop                     ;[16]
    nop                     ;[17]
    nop                     ;[18]
    nop                     ;[19]
    nop                     ;[20]
;   rjmp    rxbit1          ;   fallthrough to rxbit1 at [21]/[0]

;----------------------------------------------------------------------------
; Receiver loop (numbers in brackets are cycles within byte after instr)
;----------------------------------------------------------------------------

rxByteLoop:
    brcc    rxByteLoop_taken ;[1],[2] (normally taken to rxByteLoop_taken)
    rjmp    overflow        ;[3]

unstuff6_jump:
    rjmp    unstuff6

rxByteLoop_taken:
    andi    x1, USBMASK     ;[3]
    eor     x2, x1          ;[4]
    subi    x2, 1           ;[5]
    ror     shift           ;[6]
didUnstuff6:
    cpi     shift, 0xfc     ;[7]
    brcc    unstuff6_jump   ;[8] (normally not taken)
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    in      x2, USBIN       ;[16]/[0] <-- sample bit 7
    brcc    unstuff6_jump   ;[1] (wait, logic for bit 7 stuffing is checked here)
    andi    x2, USBMASK     ;[2]
    eor     x1, x2          ;[3]
    subi    x1, 1           ;[4]
    ror     shift           ;[5]
didUnstuff7:
    cpi     shift, 0xfc     ;[6]
    brcc    unstuff7_jump   ;[7] (normally not taken)
    eor     x3, shift       ;[8] reconstruct: x3 is 1 at bit locations we changed, 0 at others
    st      y+, x3          ;[9] store data -- AVRxt: ST is 1 cycle (was 2)
    nop                     ;[10] AVRxt: pad for ST
    ldi     bitcnt, 3       ;[11] reset bitcnt for the next byte
    nop                     ;[12]
    nop                     ;[13]
    rjmp    rxBitLoop       ;[14],[15]
    ; lands at rxBitLoop at cycle 16/0

unstuff7_jump:
    rjmp    unstuff7

rxBitLoop:
    in      x1, USBIN       ;[0] <-- sample bit 0/2/4
    andi    x1, USBMASK     ;[1]
    eor     x2, x1          ;[2]
    andi    x3, 0x3f        ;[3] topmost two bits reserved for 6 and 7
    subi    x2, 1           ;[4]
    ror     shift           ;[5]
    cpi     shift, 0xfc     ;[6]
    brcc    unstuffEven     ;[7]
didUnstuffE:
    lsr     x3          ;[8]
    lsr     x3          ;[9]
    nop                 ;[10]
    nop                 ;[11]
    nop                 ;[12]
    nop                 ;[13]
    nop                 ;[14]
    nop                 ;[15]
rxbit1:
    in      x2, USBIN       ;[0] <-- sample bit 1/3/5
    andi    x2, USBMASK     ;[1]
    breq    se0_jump        ;[2]
    eor     x1, x2          ;[3]
    subi    x1, 1           ;[4]
    ror     shift           ;[5]
    cpi     shift, 0xfc     ;[6]
    brcc    unstuffOdd      ;[7]
didUnstuffO:
    dec     bitcnt          ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    brne    rxBitLoop_jump  ;[14],[15] (normally taken back to rxBitLoop)

    ; If not taken, we fall through to process bit 6
    subi    cnt, 1          ;[15]
    in      x1, USBIN       ;[16]/[0] <-- sample bit 6
    brcc    rxByteLoop_jump ;[1],[2] (taken if not overflowed)
    rjmp    overflow        ;[3]

rxBitLoop_jump:
    rjmp    rxBitLoop

rxByteLoop_jump:
    rjmp    rxByteLoop

se0_jump:
    rjmp    se0

unstuffEven:
    ori     x3, 1<<6        ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    in      x1, USBIN       ;[16]/[0] <-- sample the actual data bit
    andi    shift, ~0x80    ;[1]
    andi    x1, USBMASK     ;[2]
    breq    se0_jump        ;[3]
    rjmp    unstuffEven_back;[4],[5]

unstuffEven_back:
    lsr     x3              ;[6]
    lsr     x3              ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    rjmp    rxbit1          ;[14],[15] (jump lands at rxbit1 at cycle 16)

unstuffOdd:
    ori     x3, 1<<5        ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    in      x2, USBIN       ;[16]/[0] <-- sample the actual data bit
    andi    shift, ~0x80    ;[1]
    andi    x2, USBMASK     ;[2]
    breq    se0_jump        ;[3]
    rjmp    unstuffOdd_back ;[4],[5]

unstuffOdd_back:
    dec     bitcnt          ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    brne    rxBitLoop_jump  ;[14],[15] (normally taken back to rxBitLoop)
    ; if not taken:
    subi    cnt, 1          ;[15]
    in      x1, USBIN       ;[16]/[0] <-- sample bit 6
    brcc    rxByteLoop_jump ;[1],[2]
    rjmp    overflow        ;[3]

unstuff6:
    andi    x2, USBMASK     ;[3]
    ori     x3, 1<<6        ;[4]
    andi    shift, ~0x80    ;[5]
    mov     x1, x2          ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    rjmp    didUnstuff6_back;[14],[15]

unstuff6_jump2:
    rjmp    unstuff6

didUnstuff6_back:
    in      x2, USBIN       ;[16]/[0] <-- sample the actual bit 7
    brcc    unstuff6_jump2  ;[1]
    andi    x2, USBMASK     ;[2]
    eor     x1, x2          ;[3]
    subi    x1, 1           ;[4]
    ror     shift           ;[5]
    rjmp    didUnstuff7     ;[6],[7]

unstuff7:
    ori     x3, 1<<7        ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    in      x2, USBIN       ;[16]/[0] <-- re-sample bit 7
    andi    x2, USBMASK     ;[1]
    andi    shift, ~0x80    ;[2]
    nop                     ;[3]
    nop                     ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    rjmp    didUnstuff7_back;[14],[15]

didUnstuff7_back:
    eor     x3, shift       ;[16]/[0]
    st      y+, x3          ;[1]
    nop                     ;[2] AVRxt ST pad
    ldi     bitcnt, 3       ;[3]
    nop                     ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    rjmp    rxBitLoop_jump  ;[14],[15]

; ===== Transmit path and local trampolines =====

;usbSend:
;pointer to data in 'Y'
;number of bytes in 'cnt' -- including sync byte [range 2 ... 12]
;uses: x1...x4, btcnt, shift, cnt, Y
;Numbers in brackets are time since first bit of sync pattern is sent
usbSendAndReti:             ; 12 cycles until SOP
    in      x2, USBDDR      ;[-12]
    ori     x2, USBMASK     ;[-11]
    sbi     USBOUT, USBMINUS;[-10] prepare idle state; SBI is 1 cycle on AVRxt (was 2)
    nop                     ;[-9]  AVRxt: pad for SBI being 1 cycle instead of 2
    in      x1, USBOUT      ;[-8] port mirror for tx loop
    out     USBDDR, x2      ;[-7] <- acquire bus
    ldi     x4, USBMASK     ;[-6] exor mask
    ldi     shift, 0x80     ;[-5] sync byte is first byte sent
txByteLoop:
    ldi     bitcnt, 0x35    ;[-4] binary 0011 0101
txBitLoop:
    sbrs    shift, 0        ;[-3]
    eor     x1, x4          ;[-2]
    nop                     ;[-1]
    nop                     ;[0]
    nop                     ;[1]
    nop                     ;[2]
    nop                     ;[3]
    nop                     ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    brcc    txBitLoop_back  ;[14],[15] (taken back)

bitstuffN_jump:
    rjmp    bitstuffN

txBitLoop_back:
    out     USBOUT, x1      ;[16]/[0] <-- out N
    ror     shift           ;[1]
    ror     x2              ;[2]
didStuffN:
    cpi     x2, 0xfc        ;[3]
    brcc    bitstuffN_jump  ;[4]
    lsr     bitcnt          ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    brcc    txBitLoop       ;[14],[15] (normally taken back to txBitLoop)
    brne    txBitLoop       ;[16]/[0] (fallback loop control)

    sbrs    shift, 0        ;[1]
    eor     x1, x4          ;[2]
    nop                     ;[3]
    nop                     ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    out     USBOUT, x1      ;[15] <-- out 6

didStuff6:
    ror     shift           ;[0]
    ror     x2              ;[1]
    cpi     x2, 0xfc        ;[2]
    brcs    noStuff6        ;[3],[4]
    nop                     ;[4]
    eor     x1, x4          ;[5]
    ldi     x2, 0           ;[6]
    rol     shift           ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    rjmp    didStuff6       ;[13],[14] out stuff bit at [15]
noStuff6:
    ror     shift           ;[5]
didStuff7:
    ror     x2              ;[6]
    sbrs    x2, 7           ;[7]
    eor     x1, x4          ;[8]
    cpi     x2, 0xfc        ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    out     USBOUT, x1      ;[15] <-- out 7

    brcs    noStuff7        ;[0],[1]
    nop                     ;[1]
    ldi     x2, 0           ;[2]
    nop                     ;[3]
    rjmp    didStuff7       ;[4],[5] out stuff bit at [15]
noStuff7:
    ld      shift, y+       ;[2],[3] (LD is 2 cycles)
    dec     cnt             ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    breq    txDone          ;[13],[14]
    rjmp    txByteLoop      ;[14],[15]
txDone:

;make SE0:
    cbr     x1, USBMASK     ;[15] prepare SE0 [spec says EOP may be 32 to 38 cycles at 24 MHz]
    lds     x2, usbNewDeviceAddr;[16]/[0] AVRxt: LDS is 3 cycles; occupies [0],[1],[2]
    lsl     x2              ;[3] we compare with left shifted address
    subi    YL, lo8(usbPidBuf + 2) ;[4] Only assign address on data packets
    sbci    YH, hi8(usbPidBuf + 2) ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    out     USBOUT, x1      ;[15] <-- out SE0
;set address only after data packet was sent, not after handshake
    breq    skipAddrAssign  ;[0]
    sts     usbDeviceAddr, x2; if not skipped
skipAddrAssign:
    ldi     x2, 1<<USB_INTR_PENDING_BIT;[2] int0 occurred during TX -- clear pending flag
    sts     USB_INTR_PENDING, x2;[3],[4] (sts is 2 cycles on AVRxt)
    ori     x1, USBIDLE     ;[5]
    in      x2, USBDDR      ;[6]
    cbr     x2, USBMASK     ;[7] set both pins to input
    mov     x3, x1          ;[8]
    cbr     x3, USBMASK     ;[9] configure no pullup on both pins
    ldi     x4, 6           ;[10]
se0Delay:
    dec     x4              ;[11]
    brne    se0Delay        ;[12]
    nop                     ;[28]
    nop                     ;[29]
    nop                     ;[30]
    out     USBOUT, x1      ;[31] <-- out J (idle) -- end of SE0 (EOP signal)
    out     USBDDR, x2      ;[32] <-- release bus now
    out     USBOUT, x3      ;[33] <-- ensure no pull-up resistors are active
    rjmp    doReturn        ;[34],[35]

macro POP_STANDARD ; 14 cycles
; POP is still 2 cycles on AVRxt -- no changes needed here
    pop     cnt
    pop     x4
    pop     x3
    pop     x2
    pop     x1
    pop     shift
    pop     bitcnt
    endm
macro POP_RETI     ; 7 cycles
; POP is still 2 cycles on AVRxt -- no changes needed here
    pop     YH
    pop     YL
    out     SREG, YL
    pop     YL
    endm

#include "asmcommon.inc"

; ===== Single byte send path & helpers placed AFTER asmcommon.inc to solve truncation =====

sendNakAndReti:
    ldi     x3, USBPID_NAK  ;[-18]
    rjmp    sendX3AndReti   ;[-17]
sendAckAndReti:
    ldi     cnt, USBPID_ACK ;[-17]
sendCntAndReti:
    mov     x3, cnt         ;[-16]
sendX3AndReti:
    sts     usbPidBuf, x3      ;[-15] write PID byte to SRAM scratch; STS = 2 cycles on AVRe and AVRxt
;   [---]                      ;[-14] second cycle of STS
    ldi     YL, lo8(usbPidBuf) ;[-13] point Y at scratch byte instead of classic r20 alias
    ldi     YH, hi8(usbPidBuf) ;[-12]
    ldi     cnt, 2          ;[-11] (send path is 2 cycles later on bus vs classic; within spec)
    rjmp    usbSendAndReti

; --- TX bitstuff helpers ---
bitstuffN:
    eor     x1, x4          ;[6]
    ldi     x2, 0           ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    out     USBOUT, x1      ;[16]/[0] <-- out the stuff bit!
    rjmp    didStuffN       ;[1],[2]

bitstuff6:
    eor     x1, x4          ;[5]
    ldi     x2, 0           ;[6]
    rol     shift           ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    out     USBOUT, x1      ;[16]/[0] <-- out stuff bit
    rjmp    didStuff6       ;[1],[2]

bitstuff7:
    ldi     x2, 0           ;[2]
    nop                     ;[3]
    nop                     ;[4]
    nop                     ;[5]
    nop                     ;[6]
    nop                     ;[7]
    nop                     ;[8]
    nop                     ;[9]
    nop                     ;[10]
    nop                     ;[11]
    nop                     ;[12]
    nop                     ;[13]
    nop                     ;[14]
    nop                     ;[15]
    out     USBOUT, x1      ;[16]/[0] <-- out stuff bit
    rjmp    didStuff7       ;[1],[2]
