; FUJINET firmware
;
; cl65 fujinet.s -C fujinet.cfg -o fujinet.bin

; This code must be kept relocatable. Do not use jmp/jsr within it, so it can be loaded into any slot address Cn00-Cnff

; The caller invokes entrypoint with:
;   jsr dispatch_address
;   db command
;   dw sp_cmdList
;
; where sp_cmdList holds:
;   db command_count    - number of bytes in the cmdList that are relevant
;   db dest             - see table below for dest/unit values. 1 == fujinet, etc.
;   dw sp_payload       - always address of payload array (1024)
;   ... various additional bytes depending on the command, e.g.
;    - ctrl/stat code (1) (control / status commands)
;    - num bytes (2) (read / write commands)


        .feature pc_assignment

; defind ZP vars we can use
tmp1    := $82
tmp2    := $83
tmp3    := $84
tmp4    := $85
tmp5    := $86
tmp6    := $87
tmp7    := $88
tmp8    := $89

slot    := $8A          ; 2 byte pointer to C0n0 location for this particular slot of firmware

        .segment "fujinet"

; smartport header, with adjusted 3rd byte to stop boot up sequence detecting it (TODO: fix this)
header:
        lda        #$20
        lda        #$00
        lda        #$83         ; this is normally $03, but using $83 until better SP emulation available
        lda        #$00

; CnFF has low byte of this location:
; If we ever need to handle ProDos, we'll have to change firmware quite a bit.
entrypoint_prodos:
        sec
        bcs     entrypoint

; exactly 3 bytes after prodos entry point
entrypoint_smartport:
        clc

entrypoint:
        ; If ProDOS is a thing, we need to put some changes in here. C=0 is SP, C=1 is ProDOS

        ; -----------------------------------------------------------------------------------------------
        ; BACKUP ZP DATA TO CARD
        ; -----------------------------------------------------------------------------------------------
        ; save all our ZP locations to card, so we can reset them later
        ldx     #$00            ; save ZP temp variable locations to card from
:       lda     tmp1, x
sta_loc1:
        sta     $c000           ; 00 is overwritten by firmware to $nE which is our BACKUP DATA address
        inx
        cpx     #$0A
        bne     :-

        ; -----------------------------------------------------------------------------------------------
        ; SET SLOT NUMBER
        ; -----------------------------------------------------------------------------------------------
sta_loc2:
        lda     #$00            ; 00 is overwritten by firmware to $n0
        sta     slot            ; create a ZP variable that points to our reading/writing routine
        lda     #$C0
        sta     slot+1

        ; -----------------------------------------------------------------------------------------------
        ; GET CMDLIST DATA AND FIX RETURN ADDRESS
        ; -----------------------------------------------------------------------------------------------
        pla
        sta     tmp1            ; low byte of return-1
        pla
        sta     tmp2            ; high byte of return-1

        ; grab the data after the jsr call to the entrypoint.
        ; copy to tmp3/4/5
        ldy     #$03
:       lda     (tmp1), y
        sta     tmp3-1, y       ; tmp3/4/5
        dey
        bne     :-

        ; add 3 to the return address and save back to stack
        clc
        lda     tmp1
        adc     #$03
        sta     tmp1
        bcc     :+
        inc     tmp2

:       lda     tmp2
        pha
        lda     tmp1
        pha

        ; -----------------------------------------------------------------------------------------------
        ; CARD: START SENDING COMMAND DATA
        ; -----------------------------------------------------------------------------------------------
        lda     #$00
        ldy     #$0f
        sta     (slot), y       ; clear card buffer (C0nF = 00)
        tay                     ; y = 0 for card write buffer
        lda     tmp3            ; command
        sta     (slot), y       ; send command to card (C0n0-C0nD)

        ; tmp4/5 = sp_cmdList
        ; send 6 bytes from cmdlist, which will be all the data any command needs to know about
:       lda     (tmp4), y
        sta     (slot), y       ; send to card
        iny                     ; c0n0-c0n5 = card write to buffer
        cpy     #$06
        bne     :-

        ; reuse tmp1/2 for payload location
        ldy     #$02
        lda     (tmp4), y
        sta     tmp1
        iny
        lda     (tmp4), y
        sta     tmp2

        ; -----------------------------------------------------------------------------------------------
        ; WRITE PAYLOAD TO CARD
        ; -----------------------------------------------------------------------------------------------

        ; either skip sending bytes for commands not requiring it
        ; or 512 to be sure all sent, and keep code simple.

        lda     tmp3
        ; SP_CMD_STATUS           = 0 [0]
        ; SP_CMD_CONTROL          = 4 [512]
        ; SP_CMD_OPEN             = 6 [0]
        ; SP_CMD_CLOSE            = 7 [0]
        ; SP_CMD_READ             = 8 [0]
        ; SP_CMD_WRITE            = 9 [512]
        cmp     #$04
        beq     do_512
        cmp     #$09
        beq     do_512
        ; everything else, skip copy
        bne     start_process

do_512:
        ldx     #$02
        ldy     #$00            ; card: write to buffer
        sty     tmp6            ; byte count index
send_payload_loop:
        lda     (tmp1), y       ; read payload byte
        sta     (slot), y       ; send payload byte to card

        inc     tmp1            ; move payload pointer on by 1
        bne     :+
        inc     tmp2
:       dec     tmp6
        bne     send_payload_loop
        dex                     ; next page of bytes
        bne     send_payload_loop

        ; restore tmp2 (hi byte of payload) which has been incremented by 2 for the 2 pages of the copy
        dec     tmp2
        dec     tmp2

        ; -----------------------------------------------------------------------------------------------
        ; CARD: PROCESS - this causes FujiNet call
        ; -----------------------------------------------------------------------------------------------
start_process:
        ldy     #$0f
        lda     #$01
        sta     (slot), y       ; send process command to card (C0nF = 1)

        ; -----------------------------------------------------------------------------------------------
        ; CARD: READ RESPONSE
        ; -----------------------------------------------------------------------------------------------
        ldy     #$00
        lda     (slot), y       ; card: get lo byte count
        sta     tmp3
        iny
        lda     (slot), y       ; card: get hi byte count
        sta     tmp4            ; tmp3/4 = count

        ; loop over given bytes, do lo count first, then any pages from hi count
        ldx     tmp3            ; tmp3 is lo byte count
l1:     ldy     #$02            ; card: read next byte
        lda     (slot), y
        ldy     #$00            ; 0 offset for payload
        sta     (tmp1), y
        ; move payload (tmp1/2) on by 1 byte, can't change y
        inc     tmp1
        bne     :+
        inc     tmp2
:       dex
        bne     l1
        dec     tmp4
        bmi     end_copy        ; would fail if there are > #$8100 bytes to copy. Assumption is at most #$200, so should be safe
        bpl     l1              ; x is already 0, which will trigger 256 more bytes per 

end_copy:

        ; -----------------------------------------------------------------------------------------------
        ; RESTORE ZP DATA
        ; -----------------------------------------------------------------------------------------------
        ldy     #$03            ; card: restore data
        ldx     #$00
:       lda     (slot), y
        sta     tmp1, x
        inx
        cpx     #$08            ; do 8 bytes for tmp1-8
        bne     :-

        ; restore last 2 bytes manually into slot/slot+1 ZP location (used by loop above, so must do directly)
        lda     (slot), y       ; slot
        pha
        lda     (slot), y       ; slot+1
        sta     slot+1
        pla
        sta     slot
        ; all ZP locations are restored, return 0 for no error

        lda     #$00
        tax
        rts

; $ff offset is dispatch offset
size     = * - header
gap      = $fd - size                   ; allow for 3 bytes at the end

        .res    gap
        .byte   <(sta_loc1+1)           ; these 2 allow firmware to easily find the location it needs to write the n0/nE value to
        .byte   <(sta_loc2+1)
        .byte   <entrypoint_prodos      ; entry point
