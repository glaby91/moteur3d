; MOTOR3D CPC6128 / Z80 / ROM OFF / VERSION 0.1.0 — 2026-10-19
; Ultra-light FUN DEMO: 80 rays, one per 4-pixel MODE 1 byte-column.
; 2.5D ray-march in a 16x16 world. NOT a full mathematical V9 port.
;
; Assemble with RASM/WinAPE-compatible syntax, ORG &8000, output binary.
; Load at &8000 and CALL &8000 from BASIC (loader ONLY).
; Thereafter: DI, lower/upper ROM disabled; no firmware calls / interrupts.
;
; RAM topology CPC6128:
;   &0000-&3FFF : RAM 0 / stack &3FF0
;   &4000-&7FFF : switched RAM 4(map),5(depth->height LUT),6(texture),7(cache)
;   &8000-&BFFF : program / source tables
;   &C000-&FFFF : native 16KB video RAM, 320x200 MODE 1 (4 inks)
; The eight physical 16KB banks total 128KB, though demo only fills a
; fraction of each expansion bank. Bank 4 stays active while tracing.
;
; Video CPC line address = &C000 + (Y AND 7)*&800 + (Y/8)*80
; Mode1 byte solid pen 1=&0F, pen 2=&F0, pen 3=&FF.
;
        ORG &8000
Start:
        DI
        LD SP,&3FF0
        LD BC,&7F00
        LD A,&8D          ; Gate Array: MODE 1, lower+upper ROM OFF
        OUT (C),A
        CALL SetPalette
        CALL InitBanks
        CALL FillBackdrop

        XOR A
        LD (Column),A
RayColumn:
        LD A,(Column)
        CP 80
        JR NC,Forever
        CALL CastColumn
        CALL FetchBankData
        CALL DrawColumn
        LD A,(Column)
        INC A
        LD (Column),A
        JR RayColumn

Forever:
        JR Forever        ; no firmware, IRQ disabled, press hardware RESET

; Register A = C4..C7 selects expansion RAM4..RAM7 in 4000-7FFF.
; BC preserved so callers can use LD BC for LDIR and local counters.
MapBank:
        PUSH BC
        LD BC,&7F00
        OUT (C),A
        POP BC
        RET

SetPalette:
        LD BC,&7F00
        XOR A             ; Pen 0 = black
        OUT (C),A
        LD A,&54
        OUT (C),A
        LD A,1            ; Pen 1 = bright green (ground)
        OUT (C),A
        LD A,&52
        OUT (C),A
        LD A,2            ; Pen 2 = sky blue (sky)
        OUT (C),A
        LD A,&57
        OUT (C),A
        LD A,3            ; Pen 3 = bright yellow (walls)
        OUT (C),A
        LD A,&4A
        OUT (C),A
        RET

InitBanks:
        LD A,&C4          ; Extended RAM4: world map, 256 bytes
        CALL MapBank
        LD HL,MapData
        LD DE,&4000
        LD BC,256
        LDIR

        LD A,&C5          ; Extended RAM5: distance / height, 64 bytes
        CALL MapBank
        LD HL,HeightTable
        LD DE,&4000
        LD BC,64
        LDIR

        LD A,&C6          ; Extended RAM6: material byte patterns, 64 bytes
        CALL MapBank
        LD HL,TextureTable
        LD DE,&4000
        LD BC,64
        LDIR

        LD A,&C7          ; Extended RAM7: per-column ray depth cache
        CALL MapBank
        XOR A
        LD HL,&4000
        LD DE,&4001
        LD BC,79
        LD (HL),A
        LDIR
        LD A,&C4
        CALL MapBank       ; Leave map RAM4 active for ray casting
        RET

FillBackdrop:
        LD IX,ScreenLines  ; y 0..199, 80 bytes per line (interleaved)
        LD B,200
FillLineLoop:
        LD L,(IX+0)
        LD H,(IX+1)
        LD A,B
        CP 101
        LD A,&0F          ; ground = pen 1
        JR C,FillGround
        LD A,&F0          ; sky = pen 2
FillGround:
        LD E,80
Fill80Bytes:
        LD (HL),A
        INC HL
        DEC E
        JR NZ,Fill80Bytes
        INC IX
        INC IX
        DJNZ FillLineLoop
        RET

; Fixed view: camera at X=2.5, Y=8.5, points EAST (positive X).
; Ray step X=1/8 of a map cell, Y step varies with screen column.
; X/Y use unsigned 8.8 fixed point in RAM. 1 tested map cell per step.
CastColumn:
        LD HL,&0280
        LD (RayX),HL
        LD HL,&0880
        LD (RayY),HL
        LD A,(Column)
        SUB 40
        SRA A             ; signed lateral component -20..+19
        LD E,A
        LD D,0
        BIT 7,E
        JR Z,PositiveDY
        DEC D             ; signed extension to 16 bits
PositiveDY:
        LD (RayDY),DE
        XOR A
        LD (Steps),A
RayStep:
        LD HL,(RayX)
        LD DE,&0020       ; advance 1/8 map cell along X
        ADD HL,DE
        LD (RayX),HL
        LD HL,(RayY)
        LD DE,(RayDY)
        ADD HL,DE
        LD (RayY),HL
        LD A,(Steps)
        INC A
        LD (Steps),A
        CP 63
        JR NC,RayDone
        LD A,(RayX+1)
        CP 16
        JR NC,RayDone
        LD B,A            ; X tile = high byte
        LD A,(RayY+1)
        CP 16
        JR NC,RayDone
        ADD A,A           ; index = Y*16 + X
        ADD A,A
        ADD A,A
        ADD A,A
        OR B
        LD L,A
        LD H,&40          ; RAM4 (map bank) currently visible
        LD A,(HL)
        OR A
        JR Z,RayStep
RayDone:
        RET

; All four expanded banks USED at runtime: map4, height5,
; texture6, cache7. Restore RAM4 after sampling other banks.
FetchBankData:
        LD A,&C5
        CALL MapBank
        LD A,(Steps)
        LD L,A
        LD H,&40
        LD A,(HL)
        LD (WallHeight),A

        LD A,&C6
        CALL MapBank
        LD A,(Steps)
        LD L,A
        LD H,&40
        LD A,(HL)
        LD (WallInk),A

        LD A,&C7
        CALL MapBank
        LD A,(Column)
        LD L,A
        LD H,&40
        LD A,(Steps)
        LD (HL),A         ; store depth for possible later frame

        LD A,&C4
        CALL MapBank
        RET

; Full wall-height bar centred at Y=100, one MODE1 byte (4px) wide.
; This avoids per-pixel pen encoding and any need for the CPC OS.
DrawColumn:
        LD A,(WallHeight)
        SRL A             ; half height
        LD B,A
        LD A,100
        SUB B
        LD (DrawY),A
        LD A,100
        ADD A,B
        LD (DrawBottom),A
DrawVertical:
        LD A,(DrawY)
        CP 200
        RET NC
        LD L,A
        LD H,0
        ADD HL,HL         ; y * 2, word address in ScreenLines
        LD DE,ScreenLines
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)        ; DE = actual video address of y
        LD A,(Column)
        LD L,A
        LD H,0
        ADD HL,DE        ; column is directly one Mode1 byte
        LD A,(WallInk)
        LD (HL),A
        LD A,(DrawY)
        INC A
        LD (DrawY),A
        LD B,A
        LD A,(DrawBottom)
        CP B
        JR NC,DrawVertical
        RET

Column:       DB 0
Steps:        DB 0
WallHeight:   DB 0
WallInk:      DB 0
DrawY:        DB 0
DrawBottom:   DB 0
RayX:         DW 0
RayY:         DW 0
RayDY:        DW 0

; An 8x8-like ray-map illustration enclosed by 16x16 walls.
; Every 16-byte row is in the map bank at &4000.
MapData:
  DB 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 ; y=0
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=1
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=2
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=3
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=4
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=5
  DB 1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1 ; y=6
  DB 1,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1 ; y=7
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=8
  DB 1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1 ; y=9
  DB 1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1 ; y=10
  DB 1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1 ; y=11
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=12
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=13
  DB 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 ; y=14
  DB 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 ; y=15

; Height LUT is sampled from bank5 for steps 0..63:
HeightTable:
  DB 160,155,103,78,62,52,44,39
  DB 34,31,28,26,24,22,21,19
  DB 18,17,16,16,15,14,13,13
  DB 12,12,11,11,11,10,10,10
  DB 9,9,9,9,8,8,8,8
  DB 8,7,7,7,7,7,7,6
  DB 6,6,6,6,6,6,6,6
  DB 5,5,5,5,5,5,5,5

; Pattern bytes are consumed from bank6. This is NOT a bitmap UV texture:
; just retro-style shading stripes depending on ray depth.
TextureTable:
  DB &F,&FF,&FF,&FF,&FF,&F,&FF,&F0
  DB &FF,&FF,&F,&FF,&FF,&FF,&F0,&F
  DB &FF,&FF,&FF,&FF,&F,&F0,&FF,&FF
  DB &FF,&F,&FF,&FF,&F0,&FF,&F,&FF
  DB &FF,&FF,&FF,&F,&FF,&FF,&FF,&FF
  DB &F,&FF,&F0,&FF,&FF,&F,&FF,&FF
  DB &FF,&F0,&F,&FF,&FF,&FF,&FF,&F
  DB &F0,&FF,&FF,&FF,&F,&FF,&FF,&F0

; CPC standard CRTC: 200 interleaved display scanlines from C000
ScreenLines:
  DW &C000,&C800,&D000,&D800,&E000,&E800,&F000,&F800
  DW &C050,&C850,&D050,&D850,&E050,&E850,&F050,&F850
  DW &C0A0,&C8A0,&D0A0,&D8A0,&E0A0,&E8A0,&F0A0,&F8A0
  DW &C0F0,&C8F0,&D0F0,&D8F0,&E0F0,&E8F0,&F0F0,&F8F0
  DW &C140,&C940,&D140,&D940,&E140,&E940,&F140,&F940
  DW &C190,&C990,&D190,&D990,&E190,&E990,&F190,&F990
  DW &C1E0,&C9E0,&D1E0,&D9E0,&E1E0,&E9E0,&F1E0,&F9E0
  DW &C230,&CA30,&D230,&DA30,&E230,&EA30,&F230,&FA30
  DW &C280,&CA80,&D280,&DA80,&E280,&EA80,&F280,&FA80
  DW &C2D0,&CAD0,&D2D0,&DAD0,&E2D0,&EAD0,&F2D0,&FAD0
  DW &C320,&CB20,&D320,&DB20,&E320,&EB20,&F320,&FB20
  DW &C370,&CB70,&D370,&DB70,&E370,&EB70,&F370,&FB70
  DW &C3C0,&CBC0,&D3C0,&DBC0,&E3C0,&EBC0,&F3C0,&FBC0
  DW &C410,&CC10,&D410,&DC10,&E410,&EC10,&F410,&FC10
  DW &C460,&CC60,&D460,&DC60,&E460,&EC60,&F460,&FC60
  DW &C4B0,&CCB0,&D4B0,&DCB0,&E4B0,&ECB0,&F4B0,&FCB0
  DW &C500,&CD00,&D500,&DD00,&E500,&ED00,&F500,&FD00
  DW &C550,&CD50,&D550,&DD50,&E550,&ED50,&F550,&FD50
  DW &C5A0,&CDA0,&D5A0,&DDA0,&E5A0,&EDA0,&F5A0,&FDA0
  DW &C5F0,&CDF0,&D5F0,&DDF0,&E5F0,&EDF0,&F5F0,&FDF0
  DW &C640,&CE40,&D640,&DE40,&E640,&EE40,&F640,&FE40
  DW &C690,&CE90,&D690,&DE90,&E690,&EE90,&F690,&FE90
  DW &C6E0,&CEE0,&D6E0,&DEE0,&E6E0,&EEE0,&F6E0,&FEE0
  DW &C730,&CF30,&D730,&DF30,&E730,&EF30,&F730,&FF30
  DW &C780,&CF80,&D780,&DF80,&E780,&EF80,&F780,&FF80

; End of binary. Boot through CALL &8000. No RETURN to firmware.
