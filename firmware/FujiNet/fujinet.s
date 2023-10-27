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
tmp14   := $8f

slot    := $90          ; $90/$91 are pointer to C0n0 location for this particular slot of firmware

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
        ldx     #$00            ; save 16 bytes of ZP to card from $82 to $91
:       lda     tmp1, x
sta_loc1:
        sta     $c000           ; 00 is overwritten by firmware to $nE which is our BACKUP DATA address
        inx
        cpx     #$10
        bne     :-

        ; -----------------------------------------------------------------------------------------------
        ; SET SLOT NUMBER
        ; -----------------------------------------------------------------------------------------------
sta_loc2:
        lda     #$00            ; 00 is overwritten by firmware to $n0
        sta     slot            ; create a ZP variable that points to our reading/writing routine
        lda     #$C0
        sta     slot+1

        ; Get the return address off the stack, need to read next 3 bytes, and adjust return value
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
        ldy     #$0f
        sta     (slot), y       ; clear card buffer (C0nF = 00)
        tay                     ; y = 0 for card write buffer
        lda     tmp3            ; command
        sta     (slot), y       ; send to card (C0n0-C0nD)

        ; tmp4/5 = sp_cmdList
        ; send 6 bytes from cmdlist, which will be all the data any command needs to know about
:       lda     (tmp4), y
        sta     (slot), y       ; send to card
        iny                     ; c0n0-c0n5 = card write to buffer
        cpy     #$06
        bne     :-

        ; get payload into tmp1/2
        ldy     #$02
        lda     (tmp4), y
        sta     tmp1
        iny
        lda     (tmp4), y
        sta     tmp2

        ; write 512 bytes of payload to buffer.
        ; TODO: what commands definitely DO NOT need anything sent? Filter those
        ; This is somewhat wasteful for some commands, but saves a lot of code
        ldx     #$02
        ldy     #$00
        sty     tmp6            ; byte count index
loop:
        lda     (tmp1), y       ; read payload byte
        sta     (slot), y       ; send payload byte to card

        inc     tmp1            ; move payload pointer on by 1
        bne     :+
        inc     tmp2
:       dec     tmp6
        bne     loop
        dex                     ; next page of bytes
        bne     loop
        
        ; restore tmp2 (hi byte of payload) which has been incremented by 2 for the 2 pages of the copy
        dec     tmp2
        dec     tmp2

        ; PROCESS
        ldy     #$0f
        tya
        sta     (slot), y       ; send process command to card (C0nF = anything but 0)

        ; HANDLE RETURNED DATA
        ldy     #$00
        lda     (slot), y
        sta     tmp3            ; len(lo)
        iny
        lda     (slot), y
        sta     tmp4            ; len(hi)

        ; TODO: LOOP FOR ALL LEN NOT JUST LO BYTE
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

; restore ZP
restore_data:
        iny                     ; y = 3, card: restore data
        ldx     #$00
:       lda     (slot), y
        sta     tmp1, x
        inx
        cpx     #$0d            ; 14 bytes, last 2 we deal with separately as they are slot zp location, which we're using in the loop
        bne     :-

        ; restore last 2 bytes into slot/slot+1 ZP location
        lda     (slot), y       ; slot
        pha
        lda     (slot), y       ; slot+1
        sta     slot+1
        pla
        sta     slot
        ; all ZP locations are restored

        lda     #$00
        tax
        rts


; $ff offset is dispatch offset
size     = * - header
gap      = $fd - size

        .res    gap
        .byte   <(sta_loc1+1)              ; these 2 allow firmware to easily find the location it needs to write the n0/nE value to
        .byte   <(sta_loc2+1)
        .byte   <entrypoint_prodos
