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
tmp9    := $8a
tmp10   := $8b
tmp11   := $8c
tmp12   := $8d
tmp13   := $8e

slot    := $90          ; $90/$91 are pointer to C0n0 location for this particular slot of firmware

        .segment "fujinet"

; smartport header, with adjusted 3rd byte to stop boot up sequence detecting it (TODO: fix this)
header:
        lda        #$20
        lda        #$00
        lda        #$83         ; this is normally $03, but using $83 until better SP emulation available
        lda        #$00

; CnFF has low byte of this location:
entrypoint_prodos:
        sec
        bcs     entrypoint

; exactly 3 bytes after prodos entry point
entrypoint_smartport:
        clc

entrypoint:
        ; -----------------------------------------------------------------------------------------------
        ; GET SLOT NUMBER
        ; -----------------------------------------------------------------------------------------------
        lda     #$00            ; byte offset 13, written to by AppleWin when loading this firmware with correct $n0 value
        sta     slot            ; create a ZP variable that points to our reading/writing routine
        lda     #$C0
        sta     slot+1

        ; TODO: do we need to do anything specific for the prodos entry?
        ; if we do need to, then C=1 for prodos, C=0 for smartport

        ; change the return address by adding 3 bytes to skip the data
        pla
        sta     tmp1            ; low byte of return-1
        pla
        sta     tmp2            ; high byte of return-1

        ; grab the data after the jsr call to the entrypoint.
        ; copy form (tmp1)+1..3 to tmp3/4/5
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

        ; start transfer to card
        lda     #$00
        ldy     #$0e
        sta     (slot), y       ; clear card buffer (C0nE)
        tay
        lda     tmp3            ; command
        sta     (slot), y       ; send to card (C0n0-C0nD)

        ; tmp4/5 = sp_cmdList
        ; send 6 bytes from cmdlist, which will be all the data any command needs to know about
        ldy     #$00
:       lda     (tmp4), y
        sta     (slot), y       ; send to card
        iny
        cpy     #$06
        bne     :-

        ; get payload into tmp1/2
        ldy     #$02
        lda     (tmp4), y
        sta     tmp1
        iny
        lda     (tmp4), y
        sta     tmp2

        ; are we a write command? if so, send LEN bytes to card for the data being written
        lda     tmp3
        cmp     #$09
        bne     not_write

        ; get len
        iny     ; y = 4
        lda     (tmp4), y
        sta     tmp6
        iny
        lda     (tmp4), y
        sta     tmp7

        ; validate it is > 0
        ora     tmp6
        bne     have_bytes      ; no data to copy! error out

        ; ERROR out, this is a write with no bytes.
        rts

have_bytes:
        lda     tmp1            ; copy payload location into tmp8/9, so we can increment it
        sta     tmp8
        lda     tmp2
        sta     tmp9

        ldy     #$00
loop:
        lda     (tmp8), y       ; read payload byte
        sty     tmp10           ; save y, as we need to to be 0 for write
        ldy     #$00
        sta     (slot), y       ; send payload byte to card
        ldy     tmp10

        inc     tmp8            ; move payload pointer on by 1
        bne     :+
        inc     tmp9

        ; reduce the byte count by 1
:       dec     tmp6
        bne     loop
        dec     tmp7
        lda     tmp7
        cmp     #$ff
        bne     loop
        ; fall through to process command        

not_write:      ; c591
        ldy     #$0f
        sta     (slot), y       ; send process command (C0nE)


        ; HANDLE RETURNED DATA

        ldy     #$00
        lda     (slot), y
        sta     tmp3            ; len(lo)
        iny
        lda     (slot), y
        sta     tmp4            ; len(hi)

        ;; quick test, just use lo, set slot lo to n2 (C0n2)
        ;; move the tmp1 (payload) pointer back by 2 for initial y value
        sec
        lda     tmp1
        sbc     #$02
        sta     tmp1
        bcs     :+
        dec     tmp2

:       ldx     tmp3            ; byte count in x
        iny                     ; y = 2, card: next byte

l1:     lda     (slot), y
        sta     (tmp1), y
        ; move payload on, keep y index same
        inc     tmp1
        bne     :+
        inc     tmp2
:       dex
        bne     l1

        lda     #$00
        tax
        rts


; $ff offset is dispatch offset
size     = * - header
gap      = $ff - size

        .res    gap
        .byte   <entrypoint_prodos
