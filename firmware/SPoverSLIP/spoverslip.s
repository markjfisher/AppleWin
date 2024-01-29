; FUJINET firmware
;
; cl65 -t none fujinet.s -o fujinet.bin

loc0    := $00
loc1    := $01

spmagic := $65          ; magic byte to trigger SmartPort code in Emulator
pdmagic := $66          ; magic byte to trigger ProDos code in Emulator

cmd     := $42          ; disk driver addresses, used during direct boot
unit    := $43
bufl    := $44
bufh    := $45
blockl  := $46
blockh  := $47

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

        ; This is a copy of Oliver Schmidt code for Reload Emulator at 
        ; https://github.com/vsladkov/reload-emulator/pull/1/files#diff-b889f9e1f8b8d8898b043495b2e25bfc19a9b12655456af92ebab9f92c9e20b0
        ; It attempts to load block 0000 into $0800, and run the routine at $0801.
        ; If prodos tells us there is no disk, it detects if we are in an autoboot scan and continues that,
        ; otherwise, it reports an error, as someone else is trying to run the code at our slot address Cn00 (e.g PR#n)
  
        stx        blockl
        stx        blockh
        stx        bufl
        inx                     ; cmd = 1 => read
        stx        cmd
        ldx        #>$0800      ; high byte of read buffer location
        stx        bufh 
n16_1:
        ldx        #$00         ; replaced by Card with slot * 16 == Drive 1 indicator
        stx        unit
cn3:
        jsr        driver       ; need to adjust the location as it compiles to c0, but need cN
        bcs        do_err
        ldx        $0800        ; disk II block count
        dex
        bne        do_err       ; must be 1
        ldx        $0801        ; first opcode from block read
        beq        do_err       ; must not be BRK (prodos seems to be $38 for SEC)
n16_2:
        ldx        #$00         ; (slot * 16) == Drive 1 indicator for this slot (drive 2 would have high bit set)
        jmp        $0801        ; run from 1st byte of loaded block

        ; Check if this is an autoboot scan (Cn00 will be in locations 00, 01), and MSLOT is also the current card being read, i.e. Cn.
        ; If it is, continue the autoscan, otherwise someone jumped to Cn00 (us) but we did not have a disk to use.
do_err:
        ldx     loc0
        bne     errexit
        ldx     loc1
        cpx     mslot
        bne     errexit         ; 0000 does not contain a Cn00 address, so we are not autoboot scanning
cn1:
        cpx     #$00            ; written to by emulator with Cn value for slot, check if mslot (current scan) is our slot
        bne     errexit
        jmp     sloop           ; we did see our card in 0000, but we are not bootable, and mslot etc hinted we are scanning, so continue scanning

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

        ; PRODOS entry, this has to be 3 bytes before the SP entry
driver:
        sec
        bcs     do_prodos

sp_driver:
        ; EVERYTHING is done in Emulator.
        lda     #spmagic
        bne     n2

do_prodos:
        lda     #pdmagic

        ; used to locate where the firmware needs to write slot value.
n2:
        sta     $c000           ; 00 is overwritten by emulator when loading firmware to correct n2 value for this slot

        ; emulator does everything: getting data from device, writing to memory, fixing return address on stack (if SmartPort call), setting A/X/Y

        cmp     #$01            ; set carry if a is not 0
        rts

errtext:
        .byte 'R'|$80, 'O'|$80, 'R'|$80, 'R'|$80, 'E'|$80, ' '|$80, 'N'|$80, 'F'|$80

        ; write data to the end of the block
        .res    $0100-10-<*

        ; all these bytes are read by AppleWin card code to determine where to write into firmware the current slot values needed by firmware
        .byte   <(n16_1+1)      ; slot * 16 for direct boot #1
        .byte   <(n16_2+1)      ; slot * 16 for direct boot #2
        .byte   <(cn1+1)        ; high byte of 0xCn00, needed in error routine #1 (SINGLE BYTE)
        .byte   <(cn2+2)        ; high byte of 0xCn00, needed in error routine #2 (HIGH BYTE of pair)
        .byte   <(cn3+2)        ; high byte of 0xCn00, needed in jsr driver (HIGH BYTE of pair)
        .byte   <(n2+1)         ; low byte of 0xC0n2 address for initiating the SP command

        ; final bytes for Cn00 for A2 to understand about this card
        .byte   $00, $00        ; total blocks, causes status to be called to get value
        .byte   $f7             ; status bits (http://www.easy68k.com/paulrsm/6502/PDOS8TRM.HTM)
        .byte   <driver         ; driver entry point
