; QBasic CPC6128 -- QEDIT editor ROM v0.2 (AMSDOS filename RAM fix)
; Compatible Amstrad CPC6128 expansion ROM, standard MODE 2 (80x25).
; Install as upper ROM slot 1; from BASIC: MEMORY &5FFF : |QEDIT
; MEMORY must be set BEFORE launch to protect #6000..#97FF.
; Editor occupies upper ROM window #C000..#FFFF; user text in RAM.
; Does NOT implement QBasic runtime, syntax colouring or overscan yet.
;
; Licensed MIT. Source: retro/cpc6128-qbasic/editor/QEDIT_ROM.asm

KM_WAIT_KEY     EQU #BB18
SCR_SET_MODE    EQU #BC0E
SCR_SET_INK     EQU #BC32
SCR_SET_BORDER  EQU #BC38
TXT_SET_CURSOR  EQU #BB75
TXT_OUTPUT      EQU #BB5A
TXT_WR_CHAR     EQU #BB5D
TXT_SET_PEN     EQU #BB90
TXT_SET_PAPER   EQU #BB96
TXT_CUR_OFF     EQU #BB84
TXT_CUR_ON      EQU #BB81
CAS_IN_OPEN     EQU #BC77
CAS_IN_CLOSE    EQU #BC7A
CAS_IN_CHAR     EQU #BC80
CAS_OUT_OPEN    EQU #BC8C
CAS_OUT_CLOSE   EQU #BC8F
CAS_OUT_CHAR    EQU #BC95
CAS_OUT_ABANDON EQU #BC92

DOC_BASE        EQU #6000
LINE_BYTES      EQU 80
MAX_LINES       EQU 120
MAX_COL         EQU 75       ; columns 0..75 => 76 editable characters
VIEW_LINES      EQU 21       ; screen rows 3..23
CUR_LINE        EQU #8800
CUR_COL         EQU #8801
TOP_LINE        EQU #8802
DOC_LINES       EQU #8803
PENDING         EQU #8804
TMP_ROW         EQU #8805
TMP_COUNT       EQU #8806
TMP_CHAR        EQU #8807
FILE_NAME_RAM   EQU #8810    ; 8 bytes, accessible when AMSDOS selects upper ROM 7
IO_BUFFER       EQU #9000    ; 2048 bytes for AMSDOS/cassette streams

        ORG #C000
        DB 1,0,1,0          ; background ROM, mark, version, revision
        DW RSX_NAMES
        JP ROM_INIT         ; executed at ROM scan
        JP EDITOR_START     ; invoked by |QEDIT
RSX_NAMES:
        DB 'QINI','T'+#80
        DB 'QEDI','T'+#80
        DB 0

ROM_INIT:
        ; Reserve RAM space for extension-chain bookkeeping. Firmware adds |QEDIT
        ; to its command table only when carry is SET (mandatory).
        DEC HL
        DEC HL
        DEC HL
        DEC HL
        DEC HL
        DEC HL
        DEC HL
        DEC HL
        SCF
        RET

EDITOR_START:
        ; BASIC must have reserved DOC_BASE..IO_BUFFER+2047 using MEMORY &5FFF.
        CALL NEW_DOCUMENT
        CALL DEMO_DOCUMENT
        CALL SCREEN_INIT
FULL_REDRAW:
        CALL REDRAW_SCREEN
WAIT_KEY:
        CALL KM_WAIT_KEY
        PUSH AF
        CALL ERASE_CURSOR
        POP AF    ; firmware IRQs must stay ENABLED for keyboard
        CP #F0             ; arrow up
        JP Z,MOVE_UP
        CP #F1             ; arrow down
        JP Z,MOVE_DOWN
        CP #F2             ; arrow left
        JP Z,MOVE_LEFT
        CP #F3             ; arrow right
        JP Z,MOVE_RIGHT
        CP 13
        JP Z,NEWLINE
        CP 127             ; DEL on CPC: backspace
        JP Z,BACKSPACE
        CP 16              ; CLR: delete under cursor
        JP Z,DELETE_CHAR
        CP 19              ; CTRL+S save
        JP Z,SAVE_FILE
        CP 15              ; CTRL+O open
        JP Z,LOAD_FILE
        CP 14              ; CTRL+N new
        JP Z,RESET_FILE
        CP 8               ; CTRL+H help
        JP Z,SHOW_HELP
        CP #FC             ; ESC return to BASIC
        JP Z,EXIT_EDITOR
        CP 17              ; CTRL+Q return to BASIC
        JP Z,EXIT_EDITOR
        CP 18              ; CTRL+R / RUN not implemented in editor ROM
        JP Z,NO_RUNTIME
        CP 32
        JP C,MAIN_LOOP
        CP 127
        JP NC,MAIN_LOOP
        JP INSERT_CHAR


; Fast redraw path after an edit or movement: only affected document row and status.
MAIN_LOOP:
        LD A,(CUR_LINE)
        LD B,A
        LD A,(TOP_LINE)
        LD C,A
        LD A,B
        SUB C
        ADD A,3
        LD L,A
        LD H,2
        CALL TXT_SET_CURSOR
        LD A,(CUR_LINE)
        CALL DRAW_DOCUMENT_ROW
        CALL DRAW_STATUS
        CALL PAINT_CURSOR
        JP WAIT_KEY

; Erase the highlighted character in old location, without changing RAM.
ERASE_CURSOR:
        CALL CURSOR_POSITION
        CALL POSITION_POINTER
        LD A,(HL)
        CALL TXT_WR_CHAR
        RET

; HL = video cursor position for current document character.
CURSOR_POSITION:
        LD A,(CUR_LINE)
        LD B,A
        LD A,(TOP_LINE)
        LD C,A
        LD A,B
        SUB C
        ADD A,3
        LD L,A
        LD A,(CUR_COL)
        ADD A,2
        LD H,A
        CALL TXT_SET_CURSOR
        RET

EXIT_EDITOR:
        CALL TXT_CUR_ON
        RET

; Reset without demo (new document)
RESET_FILE:
        CALL NEW_DOCUMENT
        JP FULL_REDRAW

; Clear 9600 bytes of RAM document, set one logical line.
NEW_DOCUMENT:
        LD HL,DOC_BASE
        LD DE,DOC_BASE+1
        LD BC,MAX_LINES*LINE_BYTES-1
        LD (HL),32
        LDIR
        XOR A
        LD (CUR_LINE),A
        LD (CUR_COL),A
        LD (TOP_LINE),A
        INC A
        LD (DOC_LINES),A
        RET

; Load demonstration lines directly from ROM, one NUL-terminated line each.
DEMO_DOCUMENT:
        LD HL,DEMO_TEXT
        LD DE,DOC_BASE
        LD B,0
DEMO_COPY:
        LD A,(HL)
        INC HL
        CP 255
        JR Z,DEMO_DONE
        OR A
        JR Z,DEMO_END_LINE
        LD (DE),A
        INC DE
        JR DEMO_COPY
DEMO_END_LINE:
        INC B
        PUSH HL
        LD A,B
        CALL LINE_POINTER
        EX DE,HL
        POP HL
        JR DEMO_COPY
DEMO_DONE:
        LD A,B
        INC A
        LD (DOC_LINES),A
        RET

SCREEN_INIT:
        LD A,2
        CALL SCR_SET_MODE
        ; PEN 0 = dark blue, PEN 1 = bright white
        XOR A
        LD BC,#0101
        CALL SCR_SET_INK
        LD A,1
        LD BC,#1A1A
        CALL SCR_SET_INK
        LD BC,#0101
        CALL SCR_SET_BORDER
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        CALL TXT_CUR_OFF
        RET

; Print a zero-terminated ROM string at absolute column H / row L.
PRINT_AT:
        PUSH HL
        CALL TXT_SET_CURSOR
        POP HL
        RET

; HL -> NUL terminated text, at current screen cursor
PRINT_STRING:
        LD A,(HL)
        OR A
        RET Z
        PUSH HL
        CALL TXT_OUTPUT
        POP HL
        INC HL
        JR PRINT_STRING

; HL = row and column packed (H col, L row), DE = text pointer.
SHOW_TEXT:
        PUSH DE
        CALL TXT_SET_CURSOR
        POP HL
        JP PRINT_STRING

; print fixed 80 characters (spaces already stored in RAM)
DRAW_DOCUMENT_ROW:
        ; A=actual document row
        LD B,A
        LD A,(DOC_LINES)
        CP B
        JR Z,DRAW_BLANK_LINE
        JR C,DRAW_BLANK_LINE
        LD A,B
        CALL LINE_POINTER
        LD B,76
DRAW_DOC_LOOP:
        LD A,(HL)
        PUSH HL
        PUSH BC
        CALL TXT_WR_CHAR
        POP BC
        POP HL
        INC HL
        DJNZ DRAW_DOC_LOOP
        RET
DRAW_BLANK_LINE:
        LD B,76
DRAW_BLANK_LOOP:
        LD A,32
        PUSH BC
        CALL TXT_WR_CHAR
        POP BC
        DJNZ DRAW_BLANK_LOOP
        RET

REDRAW_SCREEN:
        CALL TXT_CUR_OFF
        ; Header reversed white background / blue letters
        LD A,1
        CALL TXT_SET_PAPER
        XOR A
        CALL TXT_SET_PEN
        LD HL,#0101
        LD DE,HEADER_TEXT
        CALL SHOW_TEXT
        ; Normal document colours
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        LD HL,#0102
        LD DE,TITLE_TEXT
        CALL SHOW_TEXT
        ; 21 editable lines, status & help strips below
        XOR A
        LD (TMP_ROW),A
DRAW_ROWS:
        LD A,(TMP_ROW)
        ADD A,3
        LD L,A
        LD H,2
        CALL TXT_SET_CURSOR
        LD A,(TMP_ROW)
        LD B,A
        LD A,(TOP_LINE)
        ADD A,B
        CALL DRAW_DOCUMENT_ROW
        LD A,(TMP_ROW)
        INC A
        LD (TMP_ROW),A
        CP VIEW_LINES
        JR C,DRAW_ROWS
        ; row 24: status (position)
        CALL DRAW_STATUS
        ; row 25: shortcuts
        LD A,1
        CALL TXT_SET_PAPER
        XOR A
        CALL TXT_SET_PEN
        LD HL,#0119
        LD DE,FOOTER_TEXT
        CALL SHOW_TEXT
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        JP PAINT_CURSOR

DRAW_STATUS:
        LD HL,#0118
        LD DE,STATUS_TEXT
        CALL SHOW_TEXT
        LD A,(CUR_LINE)
        INC A
        LD HL,#0A18
        CALL PRINT_DECIMAL
        LD A,(CUR_COL)
        INC A
        LD HL,#1718
        CALL PRINT_DECIMAL
        RET

PAINT_CURSOR:
        ; Highlight char at current cursor without altering source RAM.
        CALL CURSOR_POSITION
        LD A,1
        CALL TXT_SET_PAPER
        XOR A
        CALL TXT_SET_PEN
        CALL POSITION_POINTER
        LD A,(HL)
        CALL TXT_WR_CHAR
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        RET

; PRINT_DECIMAL A=0..120, H=col L=row (decimal without leading zero)
PRINT_DECIMAL:
        PUSH AF
        CALL TXT_SET_CURSOR
        POP AF
        LD B,0
        LD C,0
HUNDRED_COUNT:
        CP 100
        JR C,TENS_BEGIN
        SUB 100
        INC B
        JR HUNDRED_COUNT
TENS_BEGIN:
        PUSH AF
        LD A,B
        LD C,B
        OR A
        JR Z,NO_HUNDREDS
        ADD A,48
        CALL TXT_OUTPUT
NO_HUNDREDS:
        POP AF
        LD B,0
TENS_LOOP:
        CP 10
        JR C,ONES
        SUB 10
        INC B
        JR TENS_LOOP
ONES:
        PUSH AF
        LD A,B
        OR A
        JR NZ,PRINT_TENS
        LD A,C
        OR A
        JR Z,SKIP_TENS
        XOR A
PRINT_TENS:
        ADD A,48
        CALL TXT_OUTPUT
SKIP_TENS:
        POP AF
        ADD A,48
        CALL TXT_OUTPUT
        RET

; Input A=line index [0..119] => HL=DOC_BASE+80*A.
LINE_POINTER:
        LD L,A
        LD H,0
        LD E,A
        LD D,0
        ADD HL,HL
        ADD HL,HL
        ADD HL,DE       ; 5*A
        ADD HL,HL
        ADD HL,HL
        ADD HL,HL
        ADD HL,HL       ; 80*A
        LD DE,DOC_BASE
        ADD HL,DE
        RET

; HL points to current character's address in RAM
POSITION_POINTER:
        LD A,(CUR_LINE)
        CALL LINE_POINTER
        LD A,(CUR_COL)
        LD E,A
        LD D,0
        ADD HL,DE
        RET

; Input printable A, insert at cursor; shift right within 76 char viewport.
INSERT_CHAR:
        LD (PENDING),A
        LD A,(CUR_COL)
        CP MAX_COL
        JP Z,INSERT_LAST
        JP NC,MAIN_LOOP
        LD E,A
        LD A,MAX_COL
        SUB E
        LD C,A
        LD B,0
        CALL POSITION_POINTER
        PUSH HL
        LD A,(CUR_LINE)
        CALL LINE_POINTER
        LD DE,MAX_COL-1
        ADD HL,DE
        LD E,L
        LD D,H
        INC DE
        LDDR
        POP HL
        LD A,(PENDING)
        LD (HL),A
        LD A,(CUR_COL)
        INC A
        LD (CUR_COL),A
        JP MAIN_LOOP
INSERT_LAST:
        CALL POSITION_POINTER
        LD A,(PENDING)
        LD (HL),A
        JP MAIN_LOOP

DELETE_CHAR:
        LD A,(CUR_COL)
        CP MAX_COL
        JP Z,DELETE_LAST
        JP NC,MAIN_LOOP
        LD E,A
        LD A,MAX_COL
        SUB E
        LD C,A
        LD B,0
        CALL POSITION_POINTER
        LD D,H
        LD E,L
        INC HL
        LDIR
        LD A,32
        LD (DE),A
        JP MAIN_LOOP
DELETE_LAST:
        CALL POSITION_POINTER
        LD (HL),32
        JP MAIN_LOOP

BACKSPACE:
        LD A,(CUR_COL)
        OR A
        JP Z,MAIN_LOOP
        DEC A
        LD (CUR_COL),A
        JP DELETE_CHAR

MOVE_LEFT:
        LD A,(CUR_COL)
        OR A
        JP Z,MAIN_LOOP
        DEC A
        LD (CUR_COL),A
        JP MAIN_LOOP
MOVE_RIGHT:
        LD A,(CUR_COL)
        CP MAX_COL
        JP NC,MAIN_LOOP
        INC A
        LD (CUR_COL),A
        JP MAIN_LOOP
MOVE_UP:
        LD A,(CUR_LINE)
        OR A
        JP Z,MAIN_LOOP
        DEC A
        LD (CUR_LINE),A
        LD B,A
        LD A,(TOP_LINE)
        CP B
        JP C,MAIN_LOOP
        JP Z,MAIN_LOOP
        LD A,B
        LD (TOP_LINE),A
        JP FULL_REDRAW
MOVE_DOWN:
        LD A,(CUR_LINE)
        INC A
        LD B,A
        LD A,(DOC_LINES)
        CP B
        JP Z,MAIN_LOOP
        JP C,MAIN_LOOP
        LD A,B
        LD (CUR_LINE),A
        LD B,A
        LD A,(TOP_LINE)
        ADD A,VIEW_LINES
        CP B
        JP NZ,MAIN_LOOP
        LD A,(TOP_LINE)
        INC A
        LD (TOP_LINE),A
        JP FULL_REDRAW

; Splits current line. Moves lower text lines downward; supports 120 lines.
; Non-interactive: usable by file loader without unnecessary redraws.
NEWLINE:
        CALL SPLIT_LINE
        JP FULL_REDRAW
SPLIT_LINE:
        LD A,(DOC_LINES)
        CP MAX_LINES
        RET NC
        DEC A
        LD (TMP_ROW),A
SHIFT_ROWS:
        LD A,(TMP_ROW)
        LD B,A
        LD A,(CUR_LINE)
        CP B
        JR NC,SHIFT_DONE
        LD A,B
        CALL LINE_POINTER
        LD DE,LINE_BYTES-1
        ADD HL,DE
        LD D,H
        LD E,L
        LD BC,LINE_BYTES
        PUSH HL
        LD HL,LINE_BYTES
        ADD HL,DE
        EX DE,HL             ; HL = source+79, DE = destination+79
        POP HL
        LDDR
        LD A,(TMP_ROW)
        DEC A
        LD (TMP_ROW),A
        JR SHIFT_ROWS
SHIFT_DONE:
        LD A,(DOC_LINES)
        INC A
        LD (DOC_LINES),A
        ; New inserted line is first cleared entirely.
        LD A,(CUR_LINE)
        INC A
        CALL LINE_POINTER
        LD B,LINE_BYTES
CLEAR_NEW_ROW:
        LD (HL),32
        INC HL
        DJNZ CLEAR_NEW_ROW
        ; Transfer current line's suffix to the next line.
        LD A,(CUR_COL)
        LD E,A
        LD A,MAX_COL+1
        SUB E
        LD (TMP_COUNT),A
        CALL POSITION_POINTER
        PUSH HL
        LD A,(CUR_LINE)
        INC A
        CALL LINE_POINTER
        EX DE,HL
        POP HL
        LD A,(TMP_COUNT)
        LD C,A
        LD B,0
        LDIR
        ; Blank suffix in original line.
        CALL POSITION_POINTER
        LD A,(TMP_COUNT)
        LD B,A
BLANK_SPLIT:
        LD (HL),32
        INC HL
        DJNZ BLANK_SPLIT
        LD A,(CUR_LINE)
        INC A
        LD (CUR_LINE),A
        XOR A
        LD (CUR_COL),A
        ; update top line if needed
        LD A,(CUR_LINE)
        LD B,A
        LD A,(TOP_LINE)
        ADD A,VIEW_LINES
        CP B
        RET NZ
        LD A,(TOP_LINE)
        INC A
        LD (TOP_LINE),A
        RET

; Copy ROM-resident filename to stable RAM before AMSDOS switches upper ROM.
; All bytes supplied to CAS_OUT_OPEN/CAS_IN_OPEN must be readable with ROM 7 active.
PREPARE_DISK_NAME:
        LD HL,FILE_NAME
        LD DE,FILE_NAME_RAM
        LD BC,8
        LDIR
        RET

; SAVE DEMO.BAS as ASCII, each line trimmed, CR LF separators.
SAVE_FILE:
        CALL PREPARE_DISK_NAME
        LD HL,FILE_NAME_RAM
        LD B,8
        LD DE,IO_BUFFER
        CALL CAS_OUT_OPEN
        JP NC,FILE_SAVE_OPEN_ERROR
        XOR A
        LD (TMP_ROW),A
SAVE_NEXT_ROW:
        LD A,(TMP_ROW)
        CALL LINE_POINTER
        PUSH HL
        LD DE,MAX_COL
        ADD HL,DE
        LD B,MAX_COL+1
TRIM_SAVE:
        LD A,(HL)
        CP 32
        JR NZ,SAVE_TRIM_DONE
        DEC HL
        DJNZ TRIM_SAVE
SAVE_TRIM_DONE:
        LD A,B
        LD (TMP_COUNT),A
        POP HL
        LD A,(TMP_COUNT)
        OR A
        JR Z,SAVE_NEWLINE
        LD B,A
SAVE_CHARS:
        LD A,(HL)
        PUSH HL
        PUSH BC
        CALL CAS_OUT_CHAR
        POP BC
        POP HL
        JR NC,SAVE_ABORT
        INC HL
        DJNZ SAVE_CHARS
SAVE_NEWLINE:
        LD A,13
        CALL CAS_OUT_CHAR
        JR NC,SAVE_ABORT
        LD A,10
        CALL CAS_OUT_CHAR
        JR NC,SAVE_ABORT
        LD A,(TMP_ROW)
        INC A
        LD (TMP_ROW),A
        LD B,A
        LD A,(DOC_LINES)
        CP B
        JR NZ,SAVE_NEXT_ROW
        CALL CAS_OUT_CLOSE
        JP NC,FILE_SAVE_CLOSE_ERROR
        LD HL,MSG_SAVED
        JP SHOW_MESSAGE
SAVE_ABORT:
        ; Do not turn a failed write into a successful close of a partial file.
        CALL CAS_OUT_ABANDON
        LD HL,MSG_SAVE_WRITE_ERROR
        JP SHOW_MESSAGE
FILE_SAVE_OPEN_ERROR:
        LD HL,MSG_SAVE_OPEN_ERROR
        JP SHOW_MESSAGE
FILE_SAVE_CLOSE_ERROR:
        LD HL,MSG_SAVE_CLOSE_ERROR
        JP SHOW_MESSAGE

; LOAD DEMO.BAS ASCII; truncates text at 76 chars and 120 lines.
LOAD_FILE:
        CALL PREPARE_DISK_NAME
        LD HL,FILE_NAME_RAM
        LD B,8
        LD DE,IO_BUFFER
        CALL CAS_IN_OPEN
        JP NC,FILE_LOAD_ERROR
        CALL NEW_DOCUMENT
LOAD_BYTE:
        CALL CAS_IN_CHAR
        JR NC,LOAD_DONE
        CP 10
        JR Z,LOAD_BYTE
        CP 13
        JR Z,LOAD_NEWLINE
        CP 9
        JR Z,LOAD_SPACE
        CP 32
        JR C,LOAD_BYTE
        CP 127
        JR NC,LOAD_BYTE
        CALL IMPORT_CHAR
        JR LOAD_BYTE
LOAD_SPACE:
        LD A,32
        CALL IMPORT_CHAR
        JR LOAD_BYTE
LOAD_NEWLINE:
        LD A,(DOC_LINES)
        CP MAX_LINES
        JR NC,LOAD_DONE
        CALL SPLIT_LINE
        JR LOAD_BYTE
LOAD_DONE:
        CALL CAS_IN_CLOSE
        XOR A
        LD (TOP_LINE),A
        LD (CUR_LINE),A
        LD (CUR_COL),A
        LD HL,MSG_LOADED
        JP SHOW_MESSAGE
FILE_LOAD_ERROR:
        LD HL,MSG_LOAD_ERROR
        JP SHOW_MESSAGE

; Import char in A, append to current line during loading (no insertion shift).
IMPORT_CHAR:
        LD (TMP_CHAR),A
        LD A,(CUR_COL)
        CP MAX_COL+1
        RET NC
        CALL POSITION_POINTER
        LD A,(TMP_CHAR)
        LD (HL),A
        LD A,(CUR_COL)
        INC A
        LD (CUR_COL),A
        RET

NO_RUNTIME:
        LD HL,MSG_NO_RUN
        JP SHOW_MESSAGE
SHOW_HELP:
        LD HL,MSG_HELP
SHOW_MESSAGE:
        ; A modal message on row 24. Wait for a key, then redraw.
        PUSH HL
        LD HL,#0118
        CALL TXT_SET_CURSOR
        POP HL
        CALL PRINT_STRING
        CALL KM_WAIT_KEY
        JP FULL_REDRAW

HEADER_TEXT:
        DB ' Fichier  Edition  Recherche  Execution  Debogage  Options  Fenetre  Aide   ',0
TITLE_TEXT:
        DB '                         [ DEMO.BAS ]        QBasic CPC6128 v0.2',0
STATUS_TEXT:
        DB 'Ligne:     Col:      INS  |  MODE 2  |  120 lignes max                   ',0
FOOTER_TEXT:
        DB ' Ctrl+S Enregistrer  Ctrl+O Ouvrir  Ctrl+N Nouveau  Ctrl+H Aide  Esc Quitter ',0
MSG_HELP:
        DB 'Fleches naviguer; Enter inserer ligne; DEL effacer; Ctrl+S/O/N; ESC quitter ',0
MSG_NO_RUN:
        DB 'Execution QBasic: ROM moteur non encore installee. Tapez une touche.       ',0
MSG_SAVED:
        DB 'DEMO.BAS enregistre. Tapez une touche.                                       ',0
MSG_LOADED:
        DB 'DEMO.BAS charge. Tapez une touche.                                           ',0
MSG_SAVE_OPEN_ERROR:
        DB 'Erreur ouverture disque (Ctrl+S). Appuyez sur une touche.                    ',0
MSG_SAVE_WRITE_ERROR:
        DB 'Erreur ecriture disque (Ctrl+S). Appuyez sur une touche.                     ',0
MSG_SAVE_CLOSE_ERROR:
        DB 'Erreur fermeture disque (Ctrl+S). Appuyez sur une touche.                    ',0
MSG_LOAD_ERROR:
        DB 'Erreur ouverture DEMO.BAS.                                                  ',0
FILE_NAME:
        DB 'DEMO.BAS'
DEMO_TEXT:
        DB 'DIM i AS INTEGER',0
        DB 'DIM texte AS STRING',0
        DB 'CLS',0
        DB "' Exemple QBasic CPC6128 sans numeros de ligne",0
        DB 'texte = "Hello CPC !"',0
        DB 'FOR i = 1 TO 10',0
        DB '    PRINT texte; i',0
        DB 'NEXT i',0
        DB 'END',255

ROM_END:
        DEFS #10000-$,#FF
