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

; smartport header, with adjusted 3rd byte to stop boot up sequence detecting it (TODO: fix this)
header:
        cpx     #$20
        ldx     #$00
        cpx     #$83            ; this is normally $03, but using $83 until better SP emulation available
        cpx     #$00

        ; if we're called directly, or via prodos, just print an error.
        ; this is a copy of Oliver Schmidt code for Reload Emulator at https://github.com/vsladkov/reload-emulator/pull/1/files#diff-b889f9e1f8b8d8898b043495b2e25bfc19a9b12655456af92ebab9f92c9e20b0
do_err:
        ldx     loc0
        bne     errexit
        ldx     loc1
        cpx     mslot
        bne     errexit
        cpx     #>*
        bne     errexit
        jmp     sloop

errexit:
        jsr     setscr
        jsr     setkbd
        jsr     settxt
        jsr     home
        ldx     #$07            ; "FN ERROR"
:       lda     errtext, x
        jsr     cout
        dex
        bpl     :-
        jmp     basic

        ; PRODOS entry. Need to understand if this should error or "do nothing" whatever that means.
driver:
        clc
        bcc     do_err

sp_driver:
        ; EVERYTHING is done in Emulator.
        lda     #magic
        ; used to locate where the firmware needs to write slot value.
        ; DO NOT PUT ANYTHING BETWEEN LABEL AND "sta $c000"
slot_write:
        sta     $c000           ; 00 is overwritten by emulator when loading firmware to correct n0 value for this slot

        ; emulator does everything: getting data from device, writing to memory, fixing return address on stack, setting A/X/Y

        cmp     #$01            ; set carry if a is not 0
        rts

errtext:
        .byte 'R'|$80, 'O'|$80, 'R'|$80, 'R'|$80, 'E'|$80, ' '|$80, 'N'|$80, 'F'|$80

        ; write data to the end of the block
        .res    $0100-5-<*
        .byte   <(slot_write+1) ; low byte of 0xC0n0 address for initiating the SP command
        .byte   $00, $00        ; total blocks, causes status to be called to get value
        .byte   $f7             ; status bits (http://www.easy68k.com/paulrsm/6502/PDOS8TRM.HTM)
        .byte   <driver         ; driver entry point
