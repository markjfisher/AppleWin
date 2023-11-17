; FUJINET firmware
; TBD: Is this also a full SP implementation for AppleWin?
;
; cl65 -t none fujinet.s -o fujinet.bin

loc0    := $00
loc1    := $01

magic   = $65           ; as in: cc65

mslot   := $07F8        ; BUFFER FOR HI SLOT ADDR (SCN)

cout    := $FDED        ; CHARACTER OUT (THRU CSW)
setkbd  := $FE89        ; SETS KSW TO APPLE KEYBOARD
setscr  := $FE93        ; SETS CSW TO APPLE SCREEN
sloop   := $FABA        ; CONTINUE SLOT SCAN
settxt  := $FB39        ; SET TEXT MODE
home    := $FC58        ; CLEAR SCREEN AND HOME CURSOR
basic   := $E000        ; BASIC INTERPRETER COLD START

; this is fake and just needed for size calculation at the end. everything is relative
        .org $c000

; smartport header
header:
        cpx     #$20
        ldx     #$00
        cpx     #$03
        cpx     #$00

        ; if we're called directly just print an error and drop to basic.
        ; this is a copy of Oliver Schmidt code for Reload Emulator at https://github.com/vsladkov/reload-emulator/pull/1/files#diff-b889f9e1f8b8d8898b043495b2e25bfc19a9b12655456af92ebab9f92c9e20b0
do_err:
        ldx     loc0
        bne     errexit
        ldx     loc1
        cpx     mslot
        bne     errexit
cn1:
        cpx     #$00            ; written to by emulator with Cn value for slot
        bne     errexit
        jmp     sloop

errexit:
        jsr     setscr
        jsr     setkbd
        jsr     settxt
        jsr     home
        ldx     #$07            ; "FN ERROR"
cn2:
:       lda     errtext, x      ; high byte written to by the emulator
        jsr     cout
        dex
        bpl     :-
        jmp     basic

        ; PRODOS entry
driver:
        lda     #$00
        rts

sp_driver:
        ; EVERYTHING is done in Emulator.
        lda     #magic          ; magic = $65, $02 = value to get things going.
        ; used to locate where the firmware needs to write slot value.
        ; DO NOT PUT ANYTHING BETWEEN LABEL AND "sta $c000"
n2:
        sta     $c000           ; 00 is overwritten by emulator when loading firmware to correct n2 value for this slot

        ; emulator does everything: getting data from device, writing to memory, fixing return address on stack, setting A/X/Y

        cmp     #$01            ; set carry if a is not 0
        rts

errtext:
        .byte 'R'|$80, 'O'|$80, 'R'|$80, 'R'|$80, 'E'|$80, ' '|$80, 'N'|$80, 'F'|$80

        ; write data to the end of the block
        .res    $0100-7-<*
        .byte   <(cn1+1)         ; high byte of 0xCn00, needed in error routine #1
        .byte   <(cn2+2)         ; high byte of 0xCn00, needed in error routine #2
        .byte   <(n2+1)         ; low byte of 0xC0n2 address for initiating the SP command
        .byte   $00, $00        ; total blocks, causes status to be called to get value
        .byte   $f7             ; status bits (http://www.easy68k.com/paulrsm/6502/PDOS8TRM.HTM)
        .byte   <driver         ; driver entry point
