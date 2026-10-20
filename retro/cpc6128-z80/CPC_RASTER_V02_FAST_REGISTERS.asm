; MOTEUR3D CPC6128 / RASTERISATION Z80 / MODE 0 / V0.2.0
; BRANCHE RASTER — 19 OCTOBRE 2026
; Source demonstrative RASM / WinAPE compatible (ASSEMBLAGE NON TESTE).
; 160x200 / 16 encres / deux pixels par octet (80 octets par ligne).
; 12 TRIANGLES VISIBLES par pose, backface culling precalcule, couleurs unies.
; 8 orientations 3D preprojetees hors CPC, 2 images dans chacune des
; banques RAM 4, 5, 6, 7. Le CPC rasterise vraiment les triangles :
; arêtes x.8 en virgule fixe, spans horizontaux, ecriture ecran Mode 0.
; Vitesse REELLE INCONNUE : aucune mesure emulateur ou cycle trace.
;
; LOAD &8000 depuis BASIC, puis CALL &8000. DI / firmware et ROMs OFF.
; Pour quitter : RESET matériel. Pas d'appels OS pendant l'animation.
; La logique de projection 3D se trouve dans la generation des tables :
; les matrices 3D ne sont PAS recalculees sur ce Z80.
;
; MEMOIRE CPC6128:
;   &0000-&3FFF: RAM base, buffers de spans &3000 / &3100, pile &3FF0
;   &4000-&7FFF: banque RAM4-7 commute par GA C4..C7, 2 poses / banque
;   &8000-&BFFF: programme + 8 poses preprojetees + adresses ecran
;   &C000-&FFFF: ecran Mode 0 (RAM de base)
; 128 Ko accessibles par bancs ; la demo n'en remplit pas la totalite.

        ORG &8000
SCENE_BYTES EQU 264             ; 12 triangles x 22 octets
PAIR_BYTES  EQU 528             ; 2 poses par banque
TRI_COUNT   EQU 12
SpanLeft    EQU &3000
SpanRight   EQU &3100

Start:
        DI
        LD SP,&3FF0
        LD BC,&7F00
        LD A,&8C                  ; mode 0, deux ROMs desactivees
        OUT (C),A
        CALL InitPalette
        CALL CopyBankedPoses
        CALL ClearBackdrop         ; whole 16KB cleared just once
        XOR A
        LD (FrameIndex),A

FrameLoop:
        CALL ClearDirty
        LD A,(FrameIndex)
        SRL A
        ADD A,&C4
        CALL BankSelect
        LD IX,&4000
        LD A,(FrameIndex)
        AND 1
        JR Z,SelectFirst
        LD DE,SCENE_BYTES
        ADD IX,DE
SelectFirst:
        LD A,TRI_COUNT
        LD (TrianglesLeft),A

RasterNext:
        CALL RasterTriangle
        LD DE,22
        ADD IX,DE
        LD A,(TrianglesLeft)
        DEC A
        LD (TrianglesLeft),A
        JR NZ,RasterNext

        LD A,(FrameIndex)
        INC A
        AND 7
        LD (FrameIndex),A
        JR FrameLoop

; A=&C4..&C7. Banque etendue active sous &4000-&7FFF.
; BC doit toujours viser le port Gate Array &7Fxx.
BankSelect:
        PUSH BC
        LD BC,&7F00
        OUT (C),A
        POP BC
        RET

InitPalette:
        LD HL,HardwareColours
        LD E,0
PaletteNext:
        LD BC,&7F00
        LD A,E
        OUT (C),A
        LD A,(HL)
        OUT (C),A
        INC HL
        INC E
        LD A,E
        CP 16
        JR NZ,PaletteNext
        RET

; Copier la geometrie pre-projetee dans chaque banque 4..7.
; Toutes les autres routines continuent dans &8000-&BFFF (RAM fixe).
CopyBankedPoses:
        LD HL,Pose0
        LD A,&C4
        LD (InitBank),A
CopyNextBank:
        LD A,(InitBank)
        CALL BankSelect
        LD DE,&4000
        LD BC,PAIR_BYTES
        LDIR
        LD A,(InitBank)
        INC A
        LD (InitBank),A
        CP &C8
        JR NZ,CopyNextBank
        LD A,&C4
        CALL BankSelect
        RET

; Mode 0 solid pattern for identical left & right pixel:
; pen1=&03, pen2=&30, pen3=&33, pen4=&0C, etc.
; Garde le fond bicolore sky/ground. Refaite par image.
ClearBackdrop:
        LD IX,ScreenLines
        LD B,200
ClearNextRow:
        LD L,(IX+0)
        LD H,(IX+1)
        LD A,B
        CP 101
        LD A,&30                   ; ground pen 2
        JR C,UseGround
        LD A,&03                   ; sky pen 1
UseGround:
        LD C,80
FillClearRow:
        LD (HL),A
        INC HL
        DEC C
        JR NZ,FillClearRow
        INC IX
        INC IX
        DJNZ ClearNextRow
        RET

; Fast redraw of just old/new bounding rectangles.
; left bound even, right bound odd => even number of VRAM bytes
; (2 Mode0 pixels per byte), so every pair is one PUSH BC.
ClearDirty:
        LD A,(FrameIndex)
        ADD A,A
        ADD A,A
        LD E,A
        LD D,0
        LD HL,DirtyBounds
        ADD HL,DE
        LD A,(HL)
        LD (DirtyLeft),A
        INC HL
        LD A,(HL)
        LD (DirtyRight),A
        INC HL
        LD A,(HL)
        LD (ClearY),A
        INC HL
        LD A,(HL)
        LD (DirtyBottom),A
        LD A,(DirtyRight)
        INC A
        LD B,A
        LD A,(DirtyLeft)
        LD C,A
        LD A,B
        SUB C
        SRL A
        LD (ClearPairs),A
ClearDirtyRow:
        LD A,(ClearY)
        LD B,A
        LD A,(DirtyBottom)
        CP B
        RET Z
        RET C
        LD L,B
        LD H,0
        ADD HL,HL
        LD DE,ScreenLines
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        EX DE,HL
        LD A,(DirtyRight)
        INC A
        LD E,A
        LD D,0
        ADD HL,DE
        LD A,(ClearY)
        CP 100
        JR C,DirtySky
        LD A,&30
        JR DirtySetColour
DirtySky:
        LD A,&03
DirtySetColour:
        LD B,A
        LD C,A
        LD A,(ClearPairs)
        LD D,A
        LD (SavedSP),SP
        LD SP,HL
DirtyFillLoop:
        PUSH BC                 ; 2 bytes per push; SP moves backwards
        DEC D
        JR NZ,DirtyFillLoop
        LD SP,(SavedSP)         ; always before JR/CALL/RET
        LD A,(ClearY)
        INC A
        LD (ClearY),A
        JR ClearDirtyRow

; Triangle in IX, record structure:
;   +0 ink (packed mode0 pen), +1 ymin, +2 ymax excl., +3 reserve
;   +4 edge 0, +10 edge 1, +16 edge 2 (6 bytes each):
;     ystart,yend,xstart lo,hi,slope lo,hi
; Each edge is stepped in signed 8.8 X using one ADD HL,DE per scanline.
RasterTriangle:
        LD A,(IX+0)
        LD (Ink),A
        LD A,(IX+2)
        LD B,A
        LD A,(IX+1)
        LD (CurrentY),A
        LD C,A
        LD A,B
        SUB C
        RET Z
        LD B,A
        LD L,C
        LD H,&30                  ; SpanLeft[y]
        LD D,&31
        LD E,C                   ; SpanRight[y], DE points same row
ClearSpans:
        LD A,&FF
        LD (HL),A
        XOR A
        LD (DE),A
        INC HL
        INC DE
        DJNZ ClearSpans

        PUSH IX
        POP HL
        LD DE,4
        ADD HL,DE
        CALL StepEdge
        PUSH IX
        POP HL
        LD DE,10
        ADD HL,DE
        CALL StepEdge
        PUSH IX
        POP HL
        LD DE,16
        ADD HL,DE
        CALL StepEdge

DrawSpans:
        LD A,(CurrentY)
        LD B,A
        LD A,(IX+2)
        CP B
        RET Z
        RET C
        LD L,B
        LD H,&30
        LD A,(HL)
        CP &FF
        JR Z,AdvanceY
        LD (LeftByte),A
        LD H,&31
        LD A,(HL)
        LD (RightByte),A
        LD B,A
        LD A,(LeftByte)
        CP B
        JR C,SpanGood
        JR Z,SpanGood
        JR AdvanceY
SpanGood:
        LD A,(CurrentY)
        LD L,A
        LD H,0
        ADD HL,HL
        LD DE,ScreenLines
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        EX DE,HL
        LD A,(LeftByte)
        LD E,A
        LD D,0
        ADD HL,DE
        LD A,(RightByte)
        LD B,A
        LD A,(LeftByte)
        LD C,A
        LD A,B
        SUB C
        INC A
        LD B,A
        LD A,(Ink)
WriteSpan:
        LD (HL),A
        INC HL
        DJNZ WriteSpan

AdvanceY:
        LD A,(CurrentY)
        INC A
        LD (CurrentY),A
        JR DrawSpans

; HL points to 6-byte edge: y0,y1,x0.8.8,dx/dy.8.8.
; For every scanline update min and max byte-column X.
StepEdge:
        LD A,(HL)
        LD (EdgeY),A
        INC HL
        LD A,(HL)
        LD (EdgeEnd),A
        INC HL
        LD E,(HL)
        INC HL
        LD D,(HL)               ; DE = X initial in 8.8
        INC HL
        LD C,(HL)
        INC HL
        LD B,(HL)               ; BC = slope in 8.8
        ; One EXX pair to transfer both hot values per EDGE.
        ; No per-scanline RAM fetch of X or slope thereafter.
        PUSH DE
        PUSH BC
        EXX
        POP DE                  ; DE' = slope
        POP HL                  ; HL' = current X
        EXX
        LD A,(EdgeY)
        LD C,A                  ; current scanline Y
        LD A,(EdgeEnd)
        SUB C
        RET Z
        LD B,A                  ; number of scanlines
EdgeLoopFast:
        EXX
        LD A,H                  ; integer X in HL'
        ADD HL,DE               ; alternate HL' += DE'
        EXX
        LD L,C
        LD H,&30
        CP (HL)
        JR NC,FastNoLeft
        LD (HL),A
FastNoLeft:
        LD H,&31
        CP (HL)
        JR C,FastNoRight
        LD (HL),A
FastNoRight:
        INC C
        DJNZ EdgeLoopFast
        RET

FrameIndex:     DB 0
InitBank:       DB 0
TrianglesLeft:  DB 0
CurrentY:       DB 0
LeftByte:       DB 0
RightByte:      DB 0
Ink:            DB 0
EdgeY:          DB 0
EdgeEnd:        DB 0
EdgeX:          DW 0
StepX:          DW 0
SavedSP:        DW 0
ClearY:         DB 0
ClearPairs:     DB 0
DirtyLeft:      DB 0
DirtyRight:     DB 0
DirtyBottom:    DB 0

; Palette Gate Array: valeurs hardware couleur 0x40+code.
; Codes indicatifs : rendu reel dependra du CPC et du moniteur.
HardwareColours:
        DB &54,&57,&52,&4A,&4C,&4E,&44,&4B
        DB &43,&47,&46,&45,&55,&4D,&4F,&53

; Double cubes stylisés, rotation préprojetée sur huit angles,
; 24 triangles par pose, triés par distance à la caméra pour Painter.
; Vertices projetés en 80 colonnes vidéo, 200 lignes.
; Source générée mathématiquement lors de la production du fichier.
Pose0:
        DB &FC,&57,&5A,&00,&59,&5A,&00,&41,&00,&F0,&57,&59,&00,&37,&00,&05,&57,&5A,&00,&37,&00,&FE ; visible triangle 0
        DB &CF,&59,&7A,&00,&73,&7A,&00,&40,&49,&FF,&59,&73,&00,&41,&F6,&FF,&59,&7A,&00,&41,&D1,&FF ; visible triangle 1
        DB &F0,&5A,&74,&00,&5E,&74,&00,&3C,&80,&FF,&5A,&5E,&00,&31,&C0,&02,&5A,&74,&00,&31,&00,&00 ; visible triangle 2
        DB &FC,&59,&5E,&00,&5A,&5E,&00,&31,&C0,&02,&59,&5E,&00,&41,&00,&FF,&59,&5A,&00,&41,&00,&F0 ; visible triangle 3
        DB &F0,&5E,&7A,&00,&74,&7A,&00,&31,&AB,&01,&5E,&7A,&00,&3C,&F7,&FF,&5E,&74,&00,&3C,&80,&FF ; visible triangle 4
        DB &CF,&59,&7A,&00,&59,&7A,&00,&41,&D1,&FF,&59,&5E,&00,&41,&00,&FF,&5E,&7A,&00,&3C,&F7,&FF ; visible triangle 5
        DB &C0,&50,&56,&00,&52,&56,&00,&29,&40,&F8,&50,&52,&00,&17,&00,&09,&50,&56,&00,&17,&D5,&FD ; visible triangle 6
        DB &3F,&52,&87,&00,&76,&87,&00,&29,&88,&FF,&52,&76,&00,&29,&00,&00,&52,&87,&00,&29,&D9,&FF ; visible triangle 7
        DB &C0,&52,&59,&00,&56,&59,&00,&0A,&55,&07,&52,&59,&00,&29,&B7,&FE,&52,&56,&00,&29,&40,&F8 ; visible triangle 8
        DB &3F,&52,&87,&00,&52,&87,&00,&29,&D9,&FF,&52,&59,&00,&29,&B7,&FE,&59,&87,&00,&20,&06,&00 ; visible triangle 9
        DB &0C,&56,&80,&00,&59,&80,&00,&20,&83,&FF,&56,&59,&00,&0A,&55,&07,&56,&80,&00,&0A,&12,&00 ; visible triangle 10
        DB &0C,&59,&87,&00,&80,&87,&00,&0D,&DB,&02,&59,&87,&00,&20,&06,&00,&59,&80,&00,&20,&83,&FF ; visible triangle 11

Pose1:
        DB &CC,&58,&78,&00,&70,&78,&00,&32,&40,&00,&58,&70,&00,&32,&00,&00,&58,&78,&00,&32,&10,&00 ; visible triangle 0
        DB &FC,&57,&5D,&00,&57,&5D,&00,&3D,&AB,&FE,&57,&58,&00,&3D,&00,&F5,&58,&5D,&00,&32,&9A,&00 ; visible triangle 1
        DB &CC,&58,&78,&00,&58,&78,&00,&32,&10,&00,&58,&5D,&00,&32,&9A,&00,&5D,&78,&00,&35,&F7,&FF ; visible triangle 2
        DB &FC,&57,&5D,&00,&5C,&5D,&00,&41,&00,&F4,&57,&5C,&00,&3D,&CD,&00,&57,&5D,&00,&3D,&AB,&FE ; visible triangle 3
        DB &F0,&5C,&78,&00,&77,&78,&00,&40,&00,&F4,&5C,&77,&00,&41,&F7,&FF,&5C,&78,&00,&41,&89,&FF ; visible triangle 4
        DB &F0,&5C,&78,&00,&5C,&78,&00,&41,&89,&FF,&5C,&5D,&00,&41,&00,&F4,&5D,&78,&00,&35,&F7,&FF ; visible triangle 5
        DB &C0,&4F,&56,&00,&52,&56,&00,&0E,&C0,&06,&4F,&56,&00,&21,&25,&01,&4F,&52,&00,&21,&AB,&F9 ; visible triangle 6
        DB &0C,&52,&77,&00,&5A,&77,&00,&12,&EE,&FF,&52,&5A,&00,&0E,&80,&00,&52,&77,&00,&0E,&0E,&00 ; visible triangle 7
        DB &0C,&5A,&88,&00,&77,&88,&00,&10,&3C,&00,&5A,&88,&00,&12,&0B,&00,&5A,&77,&00,&12,&EE,&FF ; visible triangle 8
        DB &3F,&56,&88,&00,&7F,&88,&00,&29,&AB,&FD,&56,&7F,&00,&29,&00,&00,&56,&88,&00,&29,&94,&FF ; visible triangle 9
        DB &C0,&52,&5A,&00,&52,&5A,&00,&0E,&80,&00,&56,&5A,&00,&29,&40,&FA,&52,&56,&00,&0E,&C0,&06 ; visible triangle 10
        DB &3F,&56,&88,&00,&56,&88,&00,&29,&94,&FF,&56,&5A,&00,&29,&40,&FA,&5A,&88,&00,&12,&0B,&00 ; visible triangle 11

Pose2:
        DB &F0,&59,&7A,&00,&73,&7A,&00,&40,&49,&FF,&59,&73,&00,&41,&F6,&FF,&59,&7A,&00,&41,&D1,&FF ; visible triangle 0
        DB &FC,&57,&5E,&00,&59,&5E,&00,&41,&00,&FF,&57,&59,&00,&37,&00,&05,&57,&5E,&00,&37,&B7,&00 ; visible triangle 1
        DB &FC,&57,&5E,&00,&57,&5E,&00,&37,&B7,&00,&57,&5A,&00,&37,&00,&FE,&5A,&5E,&00,&31,&C0,&02 ; visible triangle 2
        DB &CC,&5A,&7A,&00,&74,&7A,&00,&31,&AB,&01,&5A,&74,&00,&31,&00,&00,&5A,&7A,&00,&31,&50,&00 ; visible triangle 3
        DB &F0,&59,&7A,&00,&59,&7A,&00,&41,&D1,&FF,&59,&5E,&00,&41,&00,&FF,&5E,&7A,&00,&3C,&F7,&FF ; visible triangle 4
        DB &CC,&5A,&7A,&00,&5A,&7A,&00,&31,&50,&00,&5A,&5E,&00,&31,&C0,&02,&5E,&7A,&00,&3C,&F7,&FF ; visible triangle 5
        DB &C0,&50,&59,&00,&50,&59,&00,&17,&E4,&00,&51,&59,&00,&28,&E0,&FE,&50,&51,&00,&17,&00,&11 ; visible triangle 6
        DB &0F,&51,&76,&00,&59,&76,&00,&1F,&4F,&00,&51,&59,&00,&28,&E0,&FE,&51,&76,&00,&28,&00,&00 ; visible triangle 7
        DB &C0,&50,&59,&00,&50,&56,&00,&17,&D5,&FD,&56,&59,&00,&0A,&00,&07,&50,&59,&00,&17,&E4,&00 ; visible triangle 8
        DB &0F,&59,&87,&00,&76,&87,&00,&28,&88,&FF,&59,&87,&00,&1F,&06,&00,&59,&76,&00,&1F,&4F,&00 ; visible triangle 9
        DB &3F,&56,&80,&00,&59,&80,&00,&1F,&8A,&FF,&56,&59,&00,&0A,&00,&07,&56,&80,&00,&0A,&12,&00 ; visible triangle 10
        DB &3F,&59,&87,&00,&80,&87,&00,&0D,&B7,&02,&59,&87,&00,&1F,&06,&00,&59,&80,&00,&1F,&8A,&FF ; visible triangle 11

Pose3:
        DB &F3,&58,&78,&00,&70,&78,&00,&32,&40,&00,&58,&70,&00,&32,&00,&00,&58,&78,&00,&32,&10,&00 ; visible triangle 0
        DB &FC,&57,&5C,&00,&57,&5C,&00,&3D,&CD,&00,&57,&58,&00,&3D,&00,&F5,&58,&5C,&00,&32,&C0,&03 ; visible triangle 1
        DB &F3,&58,&78,&00,&58,&78,&00,&32,&10,&00,&58,&5D,&00,&32,&9A,&00,&5D,&78,&00,&35,&F7,&FF ; visible triangle 2
        DB &FC,&58,&5D,&00,&58,&5C,&00,&32,&C0,&03,&58,&5D,&00,&32,&9A,&00,&5C,&5D,&00,&41,&00,&F4 ; visible triangle 3
        DB &CC,&5D,&78,&00,&77,&78,&00,&40,&00,&F4,&5D,&78,&00,&35,&F7,&FF,&5D,&77,&00,&35,&6C,&00 ; visible triangle 4
        DB &CC,&5C,&77,&00,&5D,&77,&00,&35,&6C,&00,&5C,&5D,&00,&41,&00,&F4,&5C,&77,&00,&41,&F7,&FF ; visible triangle 5
        DB &C0,&50,&59,&00,&50,&52,&00,&21,&80,&F6,&52,&59,&00,&0E,&6E,&00,&50,&59,&00,&21,&39,&FE ; visible triangle 6
        DB &3F,&52,&77,&00,&59,&77,&00,&11,&EF,&FF,&52,&59,&00,&0E,&6E,&00,&52,&77,&00,&0E,&07,&00 ; visible triangle 7
        DB &C0,&50,&59,&00,&50,&59,&00,&21,&39,&FE,&55,&59,&00,&29,&00,&FA,&50,&55,&00,&21,&9A,&01 ; visible triangle 8
        DB &3F,&59,&88,&00,&77,&88,&00,&0F,&3C,&00,&59,&88,&00,&11,&0B,&00,&59,&77,&00,&11,&EF,&FF ; visible triangle 9
        DB &0F,&55,&7E,&00,&59,&7E,&00,&11,&A6,&00,&55,&59,&00,&29,&00,&FA,&55,&7E,&00,&29,&00,&00 ; visible triangle 10
        DB &0F,&59,&88,&00,&7E,&88,&00,&29,&CD,&FD,&59,&88,&00,&11,&0B,&00,&59,&7E,&00,&11,&A6,&00 ; visible triangle 11

Pose4:
        DB &FC,&57,&5A,&00,&57,&59,&00,&37,&00,&05,&57,&5A,&00,&37,&00,&FE,&59,&5A,&00,&41,&00,&F0 ; visible triangle 0
        DB &CC,&59,&73,&00,&5E,&73,&00,&3C,&31,&00,&59,&5E,&00,&41,&00,&FF,&59,&73,&00,&41,&F6,&FF ; visible triangle 1
        DB &F3,&5A,&7A,&00,&74,&7A,&00,&31,&AB,&01,&5A,&74,&00,&31,&00,&00,&5A,&7A,&00,&31,&50,&00 ; visible triangle 2
        DB &CC,&5E,&7A,&00,&73,&7A,&00,&40,&49,&FF,&5E,&7A,&00,&3C,&F7,&FF,&5E,&73,&00,&3C,&31,&00 ; visible triangle 3
        DB &FC,&59,&5E,&00,&59,&5A,&00,&41,&00,&F0,&5A,&5E,&00,&31,&C0,&02,&59,&5E,&00,&41,&00,&FF ; visible triangle 4
        DB &F3,&5A,&7A,&00,&5A,&7A,&00,&31,&50,&00,&5A,&5E,&00,&31,&C0,&02,&5E,&7A,&00,&3C,&F7,&FF ; visible triangle 5
        DB &C0,&50,&56,&00,&50,&52,&00,&17,&00,&09,&50,&56,&00,&17,&D5,&FD,&52,&56,&00,&29,&40,&F8 ; visible triangle 6
        DB &3C,&52,&76,&00,&59,&76,&00,&20,&4F,&00,&52,&59,&00,&29,&B7,&FE,&52,&76,&00,&29,&00,&00 ; visible triangle 7
        DB &3C,&59,&87,&00,&76,&87,&00,&29,&88,&FF,&59,&87,&00,&20,&06,&00,&59,&76,&00,&20,&4F,&00 ; visible triangle 8
        DB &C0,&52,&59,&00,&52,&56,&00,&29,&40,&F8,&56,&59,&00,&0A,&55,&07,&52,&59,&00,&29,&B7,&FE ; visible triangle 9
        DB &0F,&56,&87,&00,&80,&87,&00,&0D,&DB,&02,&56,&80,&00,&0A,&12,&00,&56,&87,&00,&0A,&78,&00 ; visible triangle 10
        DB &0F,&56,&87,&00,&56,&87,&00,&0A,&78,&00,&56,&59,&00,&0A,&55,&07,&59,&87,&00,&20,&06,&00 ; visible triangle 11

Pose5:
        DB &CF,&58,&70,&00,&5D,&70,&00,&35,&D8,&FF,&58,&5D,&00,&32,&9A,&00,&58,&70,&00,&32,&00,&00 ; visible triangle 0
        DB &FC,&57,&5D,&00,&57,&58,&00,&3D,&00,&F5,&58,&5D,&00,&32,&9A,&00,&57,&5D,&00,&3D,&AB,&FE ; visible triangle 1
        DB &CF,&5D,&78,&00,&70,&78,&00,&32,&40,&00,&5D,&78,&00,&35,&F7,&FF,&5D,&70,&00,&35,&D8,&FF ; visible triangle 2
        DB &FC,&57,&5D,&00,&57,&5D,&00,&3D,&AB,&FE,&5C,&5D,&00,&41,&00,&F4,&57,&5C,&00,&3D,&CD,&00 ; visible triangle 3
        DB &F3,&5D,&78,&00,&77,&78,&00,&40,&00,&F4,&5D,&78,&00,&35,&F7,&FF,&5D,&77,&00,&35,&6C,&00 ; visible triangle 4
        DB &F3,&5C,&77,&00,&5D,&77,&00,&35,&6C,&00,&5C,&5D,&00,&41,&00,&F4,&5C,&77,&00,&41,&F7,&FF ; visible triangle 5
        DB &C0,&4F,&56,&00,&4F,&56,&00,&21,&25,&01,&4F,&52,&00,&21,&AB,&F9,&52,&56,&00,&0E,&C0,&06 ; visible triangle 6
        DB &0F,&52,&88,&00,&77,&88,&00,&10,&3C,&00,&52,&77,&00,&0E,&0E,&00,&52,&88,&00,&0E,&1C,&00 ; visible triangle 7
        DB &C0,&52,&5A,&00,&52,&56,&00,&0E,&C0,&06,&52,&5A,&00,&0E,&80,&00,&56,&5A,&00,&29,&40,&FA ; visible triangle 8
        DB &0F,&52,&88,&00,&52,&88,&00,&0E,&1C,&00,&52,&5A,&00,&0E,&80,&00,&5A,&88,&00,&12,&0B,&00 ; visible triangle 9
        DB &3C,&56,&7F,&00,&5A,&7F,&00,&12,&9F,&00,&56,&5A,&00,&29,&40,&FA,&56,&7F,&00,&29,&00,&00 ; visible triangle 10
        DB &3C,&5A,&88,&00,&7F,&88,&00,&29,&AB,&FD,&5A,&88,&00,&12,&0B,&00,&5A,&7F,&00,&12,&9F,&00 ; visible triangle 11

Pose6:
        DB &FC,&57,&5E,&00,&57,&5E,&00,&37,&B7,&00,&59,&5E,&00,&41,&00,&FF,&57,&59,&00,&37,&00,&05 ; visible triangle 0
        DB &FC,&57,&5E,&00,&57,&5A,&00,&37,&00,&FE,&5A,&5E,&00,&31,&C0,&02,&57,&5E,&00,&37,&B7,&00 ; visible triangle 1
        DB &F3,&59,&73,&00,&5E,&73,&00,&3C,&31,&00,&59,&5E,&00,&41,&00,&FF,&59,&73,&00,&41,&F6,&FF ; visible triangle 2
        DB &CF,&5A,&74,&00,&5E,&74,&00,&3C,&80,&FF,&5A,&5E,&00,&31,&C0,&02,&5A,&74,&00,&31,&00,&00 ; visible triangle 3
        DB &F3,&5E,&7A,&00,&73,&7A,&00,&40,&49,&FF,&5E,&7A,&00,&3C,&F7,&FF,&5E,&73,&00,&3C,&31,&00 ; visible triangle 4
        DB &CF,&5E,&7A,&00,&74,&7A,&00,&31,&AB,&01,&5E,&7A,&00,&3C,&F7,&FF,&5E,&74,&00,&3C,&80,&FF ; visible triangle 5
        DB &0C,&51,&87,&00,&76,&87,&00,&28,&88,&FF,&51,&76,&00,&28,&00,&00,&51,&87,&00,&28,&DA,&FF ; visible triangle 6
        DB &C0,&50,&59,&00,&51,&59,&00,&28,&E0,&FE,&50,&51,&00,&17,&00,&11,&50,&59,&00,&17,&E4,&00 ; visible triangle 7
        DB &C0,&50,&59,&00,&50,&59,&00,&17,&E4,&00,&50,&56,&00,&17,&D5,&FD,&56,&59,&00,&0A,&00,&07 ; visible triangle 8
        DB &0C,&51,&87,&00,&51,&87,&00,&28,&DA,&FF,&51,&59,&00,&28,&E0,&FE,&59,&87,&00,&1F,&06,&00 ; visible triangle 9
        DB &3C,&56,&87,&00,&80,&87,&00,&0D,&B7,&02,&56,&80,&00,&0A,&12,&00,&56,&87,&00,&0A,&73,&00 ; visible triangle 10
        DB &3C,&56,&87,&00,&56,&87,&00,&0A,&73,&00,&56,&59,&00,&0A,&00,&07,&59,&87,&00,&1F,&06,&00 ; visible triangle 11

Pose7:
        DB &F0,&58,&70,&00,&5D,&70,&00,&35,&D8,&FF,&58,&5D,&00,&32,&9A,&00,&58,&70,&00,&32,&00,&00 ; visible triangle 0
        DB &FC,&57,&5C,&00,&58,&5C,&00,&32,&C0,&03,&57,&5C,&00,&3D,&CD,&00,&57,&58,&00,&3D,&00,&F5 ; visible triangle 1
        DB &F0,&5D,&78,&00,&70,&78,&00,&32,&40,&00,&5D,&78,&00,&35,&F7,&FF,&5D,&70,&00,&35,&D8,&FF ; visible triangle 2
        DB &FC,&58,&5D,&00,&58,&5D,&00,&32,&9A,&00,&5C,&5D,&00,&41,&00,&F4,&58,&5C,&00,&32,&C0,&03 ; visible triangle 3
        DB &CF,&5C,&78,&00,&77,&78,&00,&40,&00,&F4,&5C,&77,&00,&41,&F7,&FF,&5C,&78,&00,&41,&89,&FF ; visible triangle 4
        DB &CF,&5C,&78,&00,&5C,&78,&00,&41,&89,&FF,&5C,&5D,&00,&41,&00,&F4,&5D,&78,&00,&35,&F7,&FF ; visible triangle 5
        DB &3C,&52,&88,&00,&77,&88,&00,&0F,&3C,&00,&52,&77,&00,&0E,&07,&00,&52,&88,&00,&0E,&18,&00 ; visible triangle 6
        DB &C0,&50,&59,&00,&50,&59,&00,&21,&39,&FE,&50,&52,&00,&21,&80,&F6,&52,&59,&00,&0E,&6E,&00 ; visible triangle 7
        DB &C0,&50,&59,&00,&55,&59,&00,&29,&00,&FA,&50,&55,&00,&21,&9A,&01,&50,&59,&00,&21,&39,&FE ; visible triangle 8
        DB &0C,&55,&88,&00,&7E,&88,&00,&29,&CD,&FD,&55,&7E,&00,&29,&00,&00,&55,&88,&00,&29,&92,&FF ; visible triangle 9
        DB &3C,&52,&88,&00,&52,&88,&00,&0E,&18,&00,&52,&59,&00,&0E,&6E,&00,&59,&88,&00,&11,&0B,&00 ; visible triangle 10
        DB &0C,&55,&88,&00,&55,&88,&00,&29,&92,&FF,&55,&59,&00,&29,&00,&FA,&59,&88,&00,&11,&0B,&00 ; visible triangle 11

DirtyBounds:
        DB 8,67,78,139 ; previous 7 and current 0
        DB 8,67,77,139 ; previous 0 and current 1
        DB 8,67,77,139 ; previous 1 and current 2
        DB 8,67,78,139 ; previous 2 and current 3
        DB 8,67,78,139 ; previous 3 and current 4
        DB 8,67,77,139 ; previous 4 and current 5
        DB 8,67,77,139 ; previous 5 and current 6
        DB 8,67,78,139 ; previous 6 and current 7

; 200 adresses CRTC CPC pour ecran Mode0 160x200, 80 bytes / ligne.
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

; END — boucle infinie, uniquement RESET matériel pour quitter.
