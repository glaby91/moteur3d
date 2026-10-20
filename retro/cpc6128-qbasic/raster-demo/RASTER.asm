; QEDIT raster MODE 2 prototype CPC6128 (4 MHz) - RASM
; Build: rasm RASTER.asm -ob RASTER.BIN
; Launch: MEMORY &7FFF : LOAD "RASTER.BIN",&8000 : CALL &8000
        ORG #8000
KM_READ_CHAR EQU #BB09
SCR_SET_INK EQU #BC32
SCR_SET_BORDER EQU #BC38
SCR_VSYNC EQU #BD19
START:
        LD A,0
        LD BC,#0101
        CALL SCR_SET_INK
        LD A,1
        LD BC,#1A1A
        CALL SCR_SET_INK
        LD A,0
        LD BC,#0101
        CALL SCR_SET_BORDER
FRAME:
        CALL SCR_VSYNC
        DI
        LD BC,#7F00
        LD A,1
        OUT (C),A
        LD A,#4B          ; white foreground
        OUT (C),A
        LD A,55           ; adjustable top margin
        CALL WAIT_LINES
        LD A,#4B
        OUT (C),A
        LD A,69
        CALL WAIT_LINES
        LD A,#4C          ; bright RED error zone
        OUT (C),A
        LD A,16
        CALL WAIT_LINES
        LD A,#4B          ; white valid zone
        OUT (C),A
        LD A,64
        CALL WAIT_LINES
        LD A,#4A          ; bright YELLOW status
        OUT (C),A
        LD A,24
        CALL WAIT_LINES
        LD A,#4B          ; restore before firmware IRQ
        OUT (C),A
        EI
        CALL KM_READ_CHAR
        JR NC,FRAME
        CP 27
        JR Z,FINISH
        CP #FC
        JR Z,FINISH
        CP 'Q'
        JR Z,FINISH
        CP 'q'
        JR NZ,FRAME
FINISH:
        LD A,1
        LD BC,#1A1A
        CALL SCR_SET_INK
        RET
; Approximate 259 T-states per loop, 256 T-states per CPC scanline.
; PUSH/POP preserve BC=Gate Array port throughout colour changes.
WAIT_LINES:
        PUSH BC
WL_OUTER:
        LD B,18
WL_INNER:
        DJNZ WL_INNER
        DEC A
        JR NZ,WL_OUTER
        POP BC
        RET
