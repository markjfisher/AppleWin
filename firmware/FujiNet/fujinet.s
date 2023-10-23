; FUJINET firmware
;
; cl65 fujinet.s -C fujinet.cfg -o fujinet.bin

; This code must be kept relocatable. Do not use jmp/jsr within it, so it can be loaded into any slot address Cn00-Cnff

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

        .segment "fujinet"

; smartport header, with adjusted 3rd byte to stop boot up sequence detecting it (TODO: fix this)
header:
        lda        #$20
        lda        #$00
        lda        #$83            ; this is normally $03, but using $83 until better SP emulation available
        lda        #$00

; CnFF has low byte of this location:
entrypoint_prodos:
        sec
        bcs     entrypoint

; exactly 3 bytes after prodos entry point
entrypoint_smartport:
        clc

entrypoint:
        ; TODO: do we need to do anything specific for the prodos entry?
        ; if we do need to, then C=1 for prodos, C=0 for smartport

        ; The caller invokes entrypoint with:
        ;   jsr dispatcher
        ;   db command
        ;   dw sp_cmdList
        ;
        ; where sp_cmdList holds:
        ;   db command_count    - number of bytes in the cmdList that are relevant
        ;   db dest             - see table below for dest/unit values. 1 == fujinet, etc.
        ;   dw sp_payload       - always address of payload array (1024)
        ;   ... various additional bytes depending on the command

        ; change the return address by moving return address on by 3 bytes
        pla
        sta     tmp1            ; low byte of return-1
        pla
        sta     tmp2            ; high byte of return-1

        ; grab the data after the jsr call to the entrypoint.
        ; copy form (tmp1)+1..3 to tmp5/6/7
        ldy     #$03
:       lda     (tmp1), y
        sta     tmp5-1, y       ; tmp5/6/7
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

        ; tmp6/7 = sp_cmdList
        ; copy dest(1), sp_payload(2), param1(1) into tmp1/2/3/4
        ldy     #$04
:       lda     (tmp6), y
        sta     tmp1-1, y       ; tmp1/2/3/4
        dey
        bne     :-

        ; tmp1   = dest unit, 0 = command, 1-N = fujinet devices
        ;          1 = DISK_0, 2 = NETWORK, 3 = PRINTER, 4 = FN_CLOCK, 5 = MODEM, 6 = CPM - SHOULD THIS BE DYNAMIC?
        ; tmp2/3 = sp_payload
        ; tmp4   = statusCode or controlCode (depending on tmp5). for other commands, it's unused (TODO: is this correct?)
        ; tmp5   = command, e.g. 0 = status, 4 = control, 6 = open, 7 = close, 8 = read, 9 = write
        ; tmp6/7 = cmdList (most of which is copied into above)

        ; work out 0xC0n0 slot address to write to 
        lda     #>header        ; $Cn
        and     #$0f            ; slot number
        ora     #$08            ; 8+slot
        asl     a
        asl     a
        asl     a
        asl     a               ; x 16, now have the low byte of C0n0 value needed to write to emulator, e.g. $b0 = slot 3
        sta     tmp8            ; we have the n0 part of C0n0
        lda     #$C0            ; hi byte
        sta     tmp9            ; (tmp8) is now C0n0, and we can use y index to write to correct location

        ; need to copy the data up to the card, then get any data back for the results
        ; initialise the copy
        lda     #$00
        tay                     ; y = 0 is clear buffer
        sta     (tmp8), y       ; send the clear buffer command
        lda     (tmp6), y       ; get the command count while y = 0
        tax

        iny                     ; y = 1 is store data in buffer
        lda     tmp4
        sta     (tmp8), y       ; send the command (e.g. READ = 8)
        txa
        sta     (tmp8), y       ; send the command count

        ; depending on which command we are, send different data
        ldx     #$00            ; used to indicate we're doing a write command
        lda     tmp5
        beq     status_or_control
        cmp     #$04
        bne     not_s_or_c

status_or_control:
        lda     tmp4
        sta     (tmp8), y       ; send the status/control code
        clc
        bcc     send_payload_address

not_s_or_c:
        cmp     #$06
        beq     close_or_open
        cmp     #$07
        bne     not_c_or_o

close_or_open:
        lda     #$00
        sta     (tmp8), y       ; this byte isn't used, not sure why the spec has 1 byte in the count
        clc
        bcc     send_payload_address

not_c_or_o:
        cmp     #$09            ; write - we will need to send additional data to buffer
        beq     read_or_write
        cmp     #$08
        bne     not_r_or_w
        ldx     #$01            ; read

read_or_write:
        ; fall into sending payload

send_payload_address:
        lda     tmp2
        sta     (tmp8), y       ; send payload low byte
        lda     tmp3
        sta     (tmp8), y       ; send payload high byte

        cpx     #$01
        bne     do_read

        ; send the additional payload data
        ; length is in sp_cmdlist + 4/5 = (tmp6), 4
        ldy     #$04
        lda     (tmp6), y       ; low byte of length
        sta     tmp10
        iny
        lda     (tmp6), y       ; high byte of length
        sta     tmp11

        ; check we didn't get 0 byte request
        ora     tmp10
        beq     not_r_or_w      ; no data to copy! error out

        lda     tmp2            ; copy payload location into tmp12/13, so we can increment it
        sta     tmp12
        lda     tmp3
        sta     tmp13
        ; subtract 1 for the y index we need
        sec
        lda     tmp12
        sbc     #$01
        bcs     :+
        dec     tmp13

        ldy     #$01
loop:
        lda     (tmp12), y      ; read payload byte
        sta     (tmp8), y       ; send payload byte
        inc     tmp12           ; move payload pointer on by 1
        bne     :+
        inc     tmp13

        ; reduce the byte count by 1
:       dec     tmp10
        bne     loop
        dec     tmp11
        lda     tmp11
        cmp     #$ff
        bne     loop
        beq     do_read

not_r_or_w:
        ; didn't recognise the command, return an error
        ldx     #$00
        lda     #$01
        rts

do_read:
        ; now we can receive the data back from the Fujinet

        lda     #$00
        tax
        rts


; $ff offset is dispatch offset
size     = * - header
gap      = $ff - size

        .res    gap
        .byte   <entrypoint_prodos
