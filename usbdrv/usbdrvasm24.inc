/* Name: usbdrvasm24_from12_wip.inc
 * Project: V-USB, virtual USB port for Atmel's(r) AVR(r) microcontrollers
 * Author: Azhan Ameen
 * Creation Date: 2026-06-24
 * Tabsize: 4
 * Copyright: (c) 2007 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary (CommercialLicense.txt)
 *
 * WORK IN PROGRESS:
 * This file intentionally lives outside usbdrv/ and is not selected by usbdrvasm.S.
 * It is a fresh 24 MHz experiment based on the classic 12 MHz usbdrvasm12.inc
 * structure supplied for analysis, not on the repository's existing 24 MHz port.
 *
 * Design notes for this draft:
 * - 12 MHz low-speed USB has 8 CPU cycles per bit; 24 MHz has 16 CPU cycles per bit.
 * - The receive and transmit engines below keep the 12 MHz control-flow shape and add
 *   explicit padding to stretch bit cells toward 16 cycles.
 * - AVRxt timing is accounted for where the current Dx port requires it:
 *     PUSH       1 cycle  (classic AVR was 2)
 *     ST/STD     1 cycle  (classic AVR was 2)
 *     SBI/CBI    1 cycle  (classic AVR was 2)
 *     LDS        3 cycles (classic AVR was 2)
 *     POP        2 cycles (unchanged)
 * - The classic Y=20/r20 memory alias is invalid on AVRxt, so ACK/NAK single-byte
 *   sends use the SRAM byte usbPidBuf supplied by usbdrv.c.
 *
 * This is timing-study material. It should be cycle-audited on hardware or with a
 * trace before being moved into usbdrv/ or selected by the build dispatcher.
 */

/* Do not link this file directly. It is a WIP clock-specific implementation. */

;Software-receiver engine. Strict timing! Do not use without cycle validation.
;interrupt response time: 4 cycles + insn running = 7 max if interrupts always enabled
;24 MHz target: 16 cycles per USB bit, 128 cycles per byte.
;The original 12 MHz engine had 8 cycles per bit; this draft doubles the bit-cell
;spacing mostly with nop/nop2 padding while preserving the 12 MHz labels.
;
;max stack usage follows the classic 12 MHz shape plus x4 for TX save:
;[ret(2), YL, SREG, YH, shift, x1, x2, x3, cnt, x4] = 12 bytes


.extern usbPidBuf

USB_INTR_VECTOR:
    push    YL                  ; AVRxt: 1 cycle
    nop                         ; pad
    in      YL, SREG
    push    YL                  ; AVRxt: 1 cycle
    nop                         ; pad
    push    YH                  ; AVRxt: 1 cycle
    nop                         ; pad

waitForJ:
    inc     YL
    sbis    USBIN, USBMINUS
    brne    waitForJ
waitForK:
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
#endif
#ifdef USB_SOF_HOOK
    USB_SOF_HOOK
#endif
    rjmp    sofError
foundK:
    push    bitcnt              ; AVRxt: 1 cycle
    lds     YL, usbInputBufOffset ; AVRxt: 3 cycles (1+3=4 exactly equals classic 2+2)
    clr     YH
    subi    YL, lo8(-(usbRxBuf))
    sbci    YH, hi8(-(usbRxBuf))
    push    shift               ; AVRxt: 1 cycle
    nop                         ; AVRxt: pad
    ldi     shift,0x40
    nop2
    nop2                        ; <--- ADICIONADO: Atraso extra de 2 ciclos para alinhar a fase a 24MHz
    nop                         ; one more to get stable
    ldi     bitcnt, 5
    sbis    USBIN, USBMINUS
    rjmp    haveTwoBitsK
    pop     shift
    pop     bitcnt
    rjmp    waitForK

haveTwoBitsK:
    push    x1                  ; AVRxt: 1 cycle
    nop                         ; pad
    push    x2                  ; AVRxt: 1 cycle
    nop                         ; pad
    push    x3                  ; AVRxt: 1 cycle (keep to respect ABI)
    nop                         ; pad
    nop                         ; was ldi leap2 (removed for 24MHz)
    push    x4                  ; AVRxt: 1 cycle
    nop                         ; pad
    nop                         ; was ldi leap (removed for 24MHz)
    push    cnt                 ; AVRxt: 1 cycle
    nop                         ; pad
    ldi     cnt, USB_BUFSIZE
    ldi     x2, 1<<USBPLUS
    
;----------------------------------------------------------------------------
; 24 MHz Receiver Loop (Exactly 16 cycles per bit)
;----------------------------------------------------------------------------
bitloop:
    in      x1, USBIN           ; [0] sample line state
    andi    x1, USBMASK         ; [1]
    breq    rx_se0_tramp        ; [2] both lines low -> trampolim curto
    cpse    x1, x2              ; [3] 
    rjmp    handle0             ; [4] line state differs
    sec                         ; [5] line state matches (bit is 1)
    ror     shift               ; [6] shift "1" into data
    brcs    b6checkUnstuff      ; [7] 
    nop2                        ; [9] pad to 16 cycles
    nop2                        ; [11]
    nop                         ; [12]
    dec     bitcnt              ; [13]
    brne    bitloop             ; [14] (2 cycles taken -> [16] = [0])
    ldi     x1, 0               ; [15] unstuff b0-b5 flag
    in      bitcnt, USBIN       ; [16] -> [0]
    rjmp    unstuff             ; [1]

rx_se0_tramp:                   ; Colocado após rjmp para evitar salto longo falhado
    rjmp    se0

handle0:
    mov     x2, x1              ; [6] Set x2 to current line state
    ldi     bitcnt, 6           ; [7] reset unstuff counter
    lsr     shift               ; [8] shift "0" into data
    brcs    bit7                ; [9] (2 cycles taken -> arrives at [11])
    nop2                        ; [10] pad to 16 cycles
    nop2                        ; [12]
    rjmp    bitloop             ; [14] (2 cycles taken -> [16] = [0])

b6checkUnstuff:
    dec     bitcnt              ; [9]
    breq    unstuff6            ; [10] (2 cycles taken -> arrives at [12])

bit7:
    subi    cnt, 1              ; [11]
    brcs    rx_overflow_tramp   ; [12] -> trampolim curto
    nop2                        ; [13] pad to 16 cycles
    nop                         ; [15]
    in      x1, USBIN           ; [16] -> [0]
    andi    x1, USBMASK         ; [1]
    cpse    x1, x2              ; [2]
    rjmp    b7handle0           ; [3] (2 cycles taken -> arrives at [5])
    sec                         ; [4] (cpse skips -> 2 cycles -> [4])
    ror     shift               ; [5]
    st      y+, shift           ; [6] AVRxt ST = 1 cycle
    nop                         ; [7] pad for ST
    ldi     shift, 0x40         ; [8]
    nop2                        ; [10] pad to 16 cycles
    nop2                        ; [12]
    nop                         ; [13]
    dec     bitcnt              ; [14]
    brne    bitloop             ; [15] (2 taken -> [0])
    ldi     x1, 1               ; [16] unstuff flag
    in      bitcnt, USBIN       ; [0]
    rjmp    unstuff             ; [1]

rx_overflow_tramp:              ; Colocado após rjmp para evitar salto longo falhado
    rjmp    overflow

b7handle0:
    mov     x2, x1              ; [5] 
    ldi     bitcnt, 6           ; [6]
    lsr     shift               ; [7]
    st      y+, shift           ; [8] AVRxt: ST is 1 cycle
    nop                         ; [9] pad
    ldi     shift, 0x40         ; [10]
    nop2                        ; [11] pad to 16
    nop                         ; [13]
    rjmp    bitloop             ; [14] (2 cycles taken -> [16] = [0])

unstuff6:
    ldi     x1, 0xFF            ; [12] 
    nop2                        ; [13]
    nop                         ; [15]
    in      bitcnt, USBIN       ; [16] -> [0]
    rjmp    .+0                 ; [1] (2 cycles -> arrives unstuff at [3])

unstuff:                        ; all paths arrive at [3]
    mov     x2, bitcnt          ; [3]
    ldi     bitcnt, 6           ; [4]
    andi    x2, USBMASK         ; [5]
    cpi     x1, 0               ; [6]
    nop2                        ; [7]
    brmi    bit7                ; [9] (2 cycles taken -> arrives bit7 at [11])
    nop                         ; [10]
    breq    bitloop_tramp       ; [11] (2 cycles taken -> arrives tramp at [13])
    nop2                        ; [12] b7 unstuff path
    rjmp    bitloop             ; [14] (2 cycles taken -> arrives bitloop at [0])

bitloop_tramp:
    nop                         ; [13]
    rjmp    bitloop             ; [14] (2 cycles taken -> arrives bitloop at [0])

;----------------------------------------------------------------------------
; End of receive loop. Handle EOP
;----------------------------------------------------------------------------
macro POP_STANDARD ; 14 cycles
    pop     cnt
    pop     x4
    pop     x3
    pop     x2
    pop     x1
    pop     shift
    pop     bitcnt
    endm
macro POP_RETI     ; 7 cycles
    pop     YH
    pop     YL
    out     SREG, YL
    pop     YL
    endm

#include "asmcommon.inc"

bitstuffN:
    eor     x1, x4          
    ldi     x2, 0           
    nop2                    
    nop                     
    out     USBOUT, x1      ; OUT stuff bit exactly at +16 cycles
    rjmp    didStuffN

bitstuff7:
    eor     x1, x4          
    ldi     x2, 0           
    rol     shift           
    nop2                    
    nop2                    
    out     USBOUT, x1      ; OUT stuff bit exactly at +16 cycles
    rjmp    didStuff7_after_out

sendNakAndReti:
    ldi     x3, USBPID_NAK  
    rjmp    sendX3AndReti   
sendAckAndReti:
    ldi     cnt, USBPID_ACK 
sendCntAndReti:
    mov     x3, cnt         
sendX3AndReti:
    sts     usbPidBuf, x3      
    ldi     YL, lo8(usbPidBuf) 
    ldi     YH, hi8(usbPidBuf) 
    ldi     cnt, 2          

;----------------------------------------------------------------------------
; 24 MHz Transmit Loop (Exactly 16 cycles per bit)
;----------------------------------------------------------------------------
usbSendAndReti:             
    in      x2, USBDDR      
    ori     x2, USBMASK     
    sbi     USBOUT, USBMINUS; AVRxt: SBI is 1 cycle
    nop                     ; pad
    in      x1, USBOUT      
    out     USBDDR, x2      ; acquire bus
    ldi     x4, USBMASK     
    ldi     shift, 0x80     
txByteLoop:
    ldi     bitcnt, 0x49    
txBitLoop:
    sbrs    shift, 0        
    eor     x1, x4          
    out     USBOUT, x1      ; <-- OUT N
    ror     shift           
    ror     x2              
didStuffN:
    nop2                    ; pad 
    nop2                    
    nop                     
    cpi     x2, 0xfc        
    brcc    bitstuffN       
    lsr     bitcnt          
    brcc    txBitLoop       
    brne    txBitLoop       

    nop                     ; pad for bit 7 out logic
    sbrs    shift, 0        
    eor     x1, x4          
didStuff7:
    out     USBOUT, x1      ; <-- OUT 7
didStuff7_after_out:
    ror     shift           
    ror     x2              
    nop2                    
    nop                     
    cpi     x2, 0xfc        
    brcc    bitstuff7       
    ld      shift, y+       ; AVRxt: LD unchanged (2 cycles)
    dec     cnt             
    brne    txByteLoop      

;make SE0:
    cbr     x1, USBMASK     
    lds     x2, usbNewDeviceAddr ; AVRxt: LDS is 3 cycles
    lsl     x2              
    out     USBOUT, x1      ; <-- out SE0 START
    subi    YL, lo8(usbPidBuf + 2) 
    sbci    YH, hi8(usbPidBuf + 2) 
    breq    skipAddrAssign  
    sts     usbDeviceAddr, x2 
skipAddrAssign:
    ldi     x2, 1<<USB_INTR_PENDING_BIT
    USB_STORE_PENDING(x2)   
    ori     x1, USBIDLE     
    in      x2, USBDDR      
    cbr     x2, USBMASK     
    mov     x3, x1          
    cbr     x3, USBMASK     
    ldi     x4, 7           ; 7 loops * 3 cycles = 21 cycles + 11 overhead = 32 cycles (~2 bit times)
se0Delay:
    dec     x4              
    brne    se0Delay        
    out     USBOUT, x1      ; <-- out J (idle) / EOP END
    out     USBDDR, x2      
    out     USBOUT, x3      
    rjmp    doReturn
