; QBasic CPC6128 -- QEDIT editor ROM v0.4.2 build 005 (navigation cache correction)
; Compatible Amstrad CPC6128 expansion ROM, standard MODE 2 (80x25).
; Install as upper ROM slot 1; from BASIC: MEMORY &5FFF : |QEDIT
; MEMORY must be set BEFORE launch to protect #6000..#A993.
; Editor occupies upper ROM window #C000..#FFFF; user text in RAM.
; Does NOT implement QBasic runtime, syntax colouring or overscan yet.
;
; Licensed MIT. Source: retro/cpc6128-qbasic/editor/QEDIT_ROM.asm

KM_WAIT_KEY     EQU #BB18
KM_GET_CONTROL  EQU #BB36
KM_SET_CONTROL  EQU #BB33
SCR_SET_MODE    EQU #BC0E
SCR_GET_MODE    EQU #BC11
SCR_GET_INK     EQU #BC35
SCR_GET_BORDER  EQU #BC3B
SCR_SET_INK     EQU #BC32
SCR_SET_BORDER  EQU #BC38
TXT_SET_CURSOR  EQU #BB75
SCR_SW_ROLL    EQU #BC50  ; firmware software scroll of an arbitrary screen rectangle
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

DOC_BASE        EQU #6000    ; only 21 visible plain-text lines live here
LINE_BYTES      EQU 80
MAX_LINES       EQU 1500
MAX_COL         EQU 75       ; columns 0..75 => 76 editable characters
VIEW_LINES      EQU 21       ; screen rows 3..23
CUR_LINE        EQU #8800
CUR_COL         EQU #8801
TOP_LINE        EQU #8840    ; 16-bit absolute first visible line
DOC_LINES       EQU #8842    ; 16-bit logical number of lines
PENDING         EQU #8804
TMP_ROW         EQU #8805
TMP_COUNT       EQU #8806
TMP_CHAR        EQU #8807
SAVED_MODE      EQU #8808
MENU_SEL        EQU #8809
MENU_ITEM       EQU #880A
MENU_DRAW_INDEX EQU #880B
MENU_OLD_ITEM   EQU #880E
MENU_PTR        EQU #880C  ; word
SAVED_INK0_B    EQU #8818
SAVED_INK0_C    EQU #8819
SAVED_INK1_B    EQU #881A
SAVED_INK1_C    EQU #881B
SAVED_BORDER_B  EQU #881C
SAVED_BORDER_C  EQU #881D
FILE_NAME_RAM   EQU #8810    ; 8 bytes, accessible when AMSDOS selects upper ROM 7
IO_BUFFER       EQU #9000    ; 2048 bytes for AMSDOS/cassette streams
INDEX_BASE      EQU #9800    ; 1500 entries x3 bytes (bank/address)
ENC_BUF         EQU #8A00    ; max 76 bytes of tokenized line
TEMP_LINE       EQU #8B00    ; 80-byte scratch / disk line
DIRTY_FLAG      EQU #8844
STORE_BANK      EQU #8845    ; bank 0..3 => GA C4..C7
STORE_ADDR      EQU #8846    ; next append address [#4000,#8000]
REQUEST_INDEX   EQU #8848    ; two-byte absolute logical index
REQUEST_SOURCE  EQU #884A
REQUEST_DEST    EQU #884C
ENC_LEN         EQU #884E
ENC_LEFT        EQU #884F
ENC_QUOTED      EQU #8850
ENC_PREV_WORD   EQU #8851
ENC_PTR         EQU #8852
ENC_SOURCE      EQU #8854
TOK_IDX         EQU #8856
TOK_WORD_PTR    EQU #8857
TOK_MATCH_LEN   EQU #8859
TOK_TMP         EQU #885A
DECODE_LEN      EQU #885B
DECODE_PTR      EQU #885C
DECODE_DEST     EQU #885E
STORE_ERROR     EQU #8860
IO_COL          EQU #8861
SAVE_INDEX      EQU #8862
DEMO_SOURCE     EQU #8864
STRESS_INDEX    EQU #8866
; Go-to-line input state. #8870..#887A lies outside the paged RAM window
; and outside all token/storage state, so bank switches cannot corrupt it.
GOTO_BUF        EQU #8870    ; four ASCII digits
GOTO_COUNT      EQU #8874
GOTO_REMAIN     EQU #8875
GOTO_TARGET     EQU #8876    ; 16-bit 1-based input; then 0-based index
GOTO_CHAR       EQU #8878
GOTO_CAND_TOP   EQU #8879    ; 16-bit
GOTO_MAX_TOP    EQU #887B    ; 16-bit
KEY_OLD_CTRL    EQU #8880    ; 5 prior CTRL translations


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
        CALL SAVE_DISPLAY_STATE
        CALL NEW_DOCUMENT
        CALL DEMO_DOCUMENT
        CALL CACHE_BUILD
        CALL SCREEN_INIT
        CALL INSTALL_NAV_KEYS
FULL_REDRAW:
        CALL COMMIT_DIRTY
        JR NC,FULL_REDRAW_UNCOMMITTED
        CALL CACHE_BUILD
FULL_REDRAW_UNCOMMITTED:
        CALL REDRAW_SCREEN
        CALL WAIT_HIDE
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
        CP 12              ; Ctrl+L loads built-in 50-line scrolling test
        JP Z,LOAD_LONG_DEMO
        CP 7               ; Ctrl+G: jump to logical source line
        JP Z,GOTO_LINE
        CP 2               ; Ctrl+B: beginning of document
        JP Z,NAV_BEGIN
        CP 5               ; Ctrl+E: end of document
        JP Z,NAV_END
        CP 21              ; Ctrl+U: page up 21 rows
        JP Z,NAV_PAGE_UP
        CP 4               ; Ctrl+D: page down 21 rows
        JP Z,NAV_PAGE_DOWN
        CP 20              ; Ctrl+T toggles interactive menus (WinAPE reserves PC F10)
        JP Z,MENU_START
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


 ; Modified text: redraw only the affected editor row and the two status numbers.
MAIN_LOOP:
        LD A,(CUR_LINE)
        ADD A,3
        LD L,A
        LD H,2
        CALL TXT_SET_CURSOR
        LD A,(CUR_LINE)
        CALL DRAW_DOCUMENT_ROW
        JP NAV_FAST

; Navigation never changes the document bitmap; the old cursor character
; was restored in WAIT_KEY. Avoid re-rendering 76 glyphs for a cursor move.
NAV_FAST:
        CALL UPDATE_POSITION_FAST
        CALL PAINT_CURSOR
        JP WAIT_KEY

; Status layout on 80-column MODE 2 screen: line at columns 10-13
; (four digits); "Col:" starts at 14; column at 20-22 (three digits).
; Do not clear into the static "Col:" / "INS" captions while navigating.
UPDATE_POSITION_FAST:
        LD HL,#0A18
        CALL CLEAR_DIGITS
        CALL CURRENT_ABS_INDEX
        INC HL
        LD DE,#0A18
        CALL PRINT_DECIMAL_16
        LD HL,#1418
        CALL CLEAR_COL_DIGITS
        LD A,(CUR_COL)
        INC A
        LD HL,#1418
        JP PRINT_DECIMAL
CLEAR_DIGITS:
        LD B,4
        JR CLEAR_STATUS_FIELD
CLEAR_COL_DIGITS:
        LD B,3
CLEAR_STATUS_FIELD:
        CALL TXT_SET_CURSOR
CLEAR_DIGITS_LOOP:
        PUSH BC
        LD A,32
        CALL TXT_WR_CHAR
        POP BC
        DJNZ CLEAR_DIGITS_LOOP
        RET

; Fast viewport scroll: use CPC firmware's rectangle scroll, not the
; slow complete 21x76 character redraw. SCR_SW_ROLL accepts A=paper,
; B=1 scroll up, B=0 scroll down, HL=top left, DE=bottom right,
; all coordinates are zero based (see CPC6128 OS sources).
; Editor rows are screen 3..23 and columns 2..77, 1 based.

; Fast viewport scroll: shift 20 cached rows (1600 RAM bytes), fetch only
; the entering token line from a paged 16K RAM bank, scroll the VRAM rectangle.
SCROLL_ONE_DOWN:
        LD HL,DOC_BASE+LINE_BYTES
        LD DE,DOC_BASE
        LD BC,(VIEW_LINES-1)*LINE_BYTES
        LDIR
        LD HL,(TOP_LINE)
        LD DE,VIEW_LINES-1
        ADD HL,DE
        LD DE,DOC_BASE+(VIEW_LINES-1)*LINE_BYTES
        CALL LOAD_LOGICAL_LINE
        LD HL,TOK_ERRORS+1
        LD DE,TOK_ERRORS
        LD BC,VIEW_LINES-1
        LDIR
        LD A,(TOK_INVALID_FLAG)
        LD (TOK_ERRORS+VIEW_LINES-1),A
        XOR A
        LD B,1
        LD HL,#0102
        LD DE,#4C16
        CALL SCR_SW_ROLL
        LD HL,#0217
        CALL TXT_SET_CURSOR
        LD A,VIEW_LINES-1
        CALL DRAW_DOCUMENT_ROW
        JP NAV_FAST
SCROLL_ONE_UP:
        LD HL,DOC_BASE+(VIEW_LINES-1)*LINE_BYTES-1
        LD DE,DOC_BASE+VIEW_LINES*LINE_BYTES-1
        LD BC,(VIEW_LINES-1)*LINE_BYTES
        LDDR
        LD HL,(TOP_LINE)
        LD DE,DOC_BASE
        CALL LOAD_LOGICAL_LINE
        LD HL,TOK_ERRORS+VIEW_LINES-2
        LD DE,TOK_ERRORS+VIEW_LINES-1
        LD BC,VIEW_LINES-1
        LDDR
        LD A,(TOK_INVALID_FLAG)
        LD (TOK_ERRORS),A
        XOR A
        LD B,0
        LD HL,#0102
        LD DE,#4C16
        CALL SCR_SW_ROLL
        LD HL,#0203
        CALL TXT_SET_CURSOR
        XOR A
        CALL DRAW_DOCUMENT_ROW
        JP NAV_FAST

; The built-in demo is written into paged compact storage, not displayed from
; ROM directly.  Existing Ctrl+L behavior (50 rows) remains available.
LOAD_LONG_DEMO:
        CALL NEW_DOCUMENT
        LD HL,LONG_DEMO_TEXT
        CALL BUILD_ROM_DEMO
        JP FULL_REDRAW

; Exercises all 1500 logical line indices and the 4-bank token store.
; Repeat a short QBasic line to keep stress data within 64 KiB.
LOAD_STRESS_1500:
        CALL WAIT_SHOW
        CALL NEW_DOCUMENT
        LD HL,STRESS_TEXT
        LD (DEMO_SOURCE),HL
        LD HL,0
        LD (STRESS_INDEX),HL
STRESS_LOOP:
        LD HL,(DEMO_SOURCE)
        CALL COPY_ROM_LINE_TO_TEMP
        LD HL,(STRESS_INDEX)
        LD DE,TEMP_LINE
        CALL WRITE_LOGICAL_LINE
        JP NC,STRESS_DONE
        LD HL,(STRESS_INDEX)
        INC HL
        LD (STRESS_INDEX),HL
        LD DE,MAX_LINES
        OR A
        SBC HL,DE
        JR NZ,STRESS_LOOP
STRESS_DONE:
        LD HL,(STRESS_INDEX)
        LD (DOC_LINES),HL
        JP FULL_REDRAW

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
        ADD A,3
        LD L,A
        LD A,(CUR_COL)
        ADD A,2
        LD H,A
        CALL TXT_SET_CURSOR
        RET

; Mode, palette and border must be saved *before* enabling MODE 2.
; SCR_SET_MODE clears the editor display on exit, restoring BASIC's mode.
SAVE_DISPLAY_STATE:
        CALL SCR_GET_MODE
        LD (SAVED_MODE),A
        XOR A
        CALL SCR_GET_INK
        LD A,B
        LD (SAVED_INK0_B),A
        LD A,C
        LD (SAVED_INK0_C),A
        LD A,1
        CALL SCR_GET_INK
        LD A,B
        LD (SAVED_INK1_B),A
        LD A,C
        LD (SAVED_INK1_C),A
        CALL SCR_GET_BORDER
        LD A,B
        LD (SAVED_BORDER_B),A
        LD A,C
        LD (SAVED_BORDER_C),A
        RET

EXIT_EDITOR:
        CALL RESTORE_NAV_KEYS
        CALL TXT_CUR_OFF
        LD A,(SAVED_MODE)
        CALL SCR_SET_MODE       ; firmware resets text streams and clears screen
        LD A,(SAVED_INK0_B)
        LD B,A
        LD A,(SAVED_INK0_C)
        LD C,A
        XOR A
        CALL SCR_SET_INK
        LD A,(SAVED_INK1_B)
        LD B,A
        LD A,(SAVED_INK1_C)
        LD C,A
        LD A,1
        CALL SCR_SET_INK
        LD A,(SAVED_BORDER_B)
        LD B,A
        LD A,(SAVED_BORDER_C)
        LD C,A
        CALL SCR_SET_BORDER
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        CALL TXT_CUR_ON
        RET

; Reset without demo (new document)
RESET_FILE:
        CALL NEW_DOCUMENT
        JP FULL_REDRAW

; Clear 9600 bytes of RAM document, set one logical line.

; New document: only an index of 1500 x3 bytes is cleared; the 64K data
; banks do not need to be wiped, because the index is authoritative.
NEW_DOCUMENT:
        LD HL,INDEX_BASE
        LD DE,INDEX_BASE+1
        LD BC,MAX_LINES*3-1
        LD (HL),#FF
        LDIR
        LD HL,#4000
        LD (STORE_ADDR),HL
        XOR A
        LD (STORE_BANK),A
        LD (DIRTY_FLAG),A
        LD (STORE_ERROR),A
        LD (CUR_LINE),A
        LD (CUR_COL),A
        LD HL,0
        LD (TOP_LINE),HL
        INC HL
        LD (DOC_LINES),HL
        RET

DEMO_DOCUMENT:
        LD HL,DEMO_TEXT
        JP BUILD_ROM_DEMO

; HL -> ROM demo with 0-separated lines and FF at end.
BUILD_ROM_DEMO:
        LD (DEMO_SOURCE),HL
        LD HL,0
        LD (STRESS_INDEX),HL
DEMO_BUILD_NEXT:
        LD HL,(DEMO_SOURCE)
        CALL COPY_ROM_LINE_TO_TEMP
        LD (DEMO_SOURCE),HL
        LD HL,(STRESS_INDEX)
        LD DE,TEMP_LINE
        CALL WRITE_LOGICAL_LINE
        RET NC
        LD HL,(STRESS_INDEX)
        INC HL
        LD (STRESS_INDEX),HL
        LD (DOC_LINES),HL
        LD HL,(DEMO_SOURCE)
        LD A,(HL)
        CP #FF
        RET Z
        JR DEMO_BUILD_NEXT

; Entry HL = ROM line start, copy <=76 chars to fixed scratch; returns HL
; at next line or FF terminator.  Built-in demos always use NUL lines.
COPY_ROM_LINE_TO_TEMP:
        LD DE,TEMP_LINE
        LD B,76
COPY_ROM_NEXT:
        LD A,(HL)
        OR A
        JR Z,COPY_ROM_END
        CP #FF
        JR Z,COPY_ROM_END
        LD (DE),A
        INC HL
        INC DE
        DJNZ COPY_ROM_NEXT
COPY_ROM_END:
        LD A,(HL)
        CP #FF
        JR Z,COPY_ROM_PAD
        OR A
        JR Z,COPY_ROM_ADVANCE
COPY_ROM_SKIP:
        INC HL
        LD A,(HL)
        OR A
        JR NZ,COPY_ROM_SKIP
COPY_ROM_ADVANCE:
        INC HL
COPY_ROM_PAD:
        LD A,B
        OR A
        RET Z
COPY_ROM_SPACES:
        LD A,32
        LD (DE),A
        INC DE
        DJNZ COPY_ROM_SPACES
        RET

; WAIT overlay below Aide, screen row 2, columns 73-76.
; Only used around slower operations; not on each keystroke.
WAIT_SHOW:
        LD HL,#4902
        LD DE,WAIT_TEXT
        JP SHOW_TEXT
WAIT_HIDE:
        LD HL,#4902
        LD DE,WAIT_BLANK
        JP SHOW_TEXT
WAIT_TEXT: DB 'WAIT',0
WAIT_BLANK: DB '    ',0

; Amstrad CPC key matrix (row*8 + column), firmware CTRL translation.
; G=52 B=54 E=58 U=42 D=61; make shortcuts deterministic in WinAPE.
; Store previous mappings, restore when leaving QEDIT.
NAV_KEY_SCAN: DB 52,54,58,42,61
NAV_KEY_CODE: DB 7,2,5,21,4
INSTALL_NAV_KEYS:
        LD DE,NAV_KEY_SCAN
        LD HL,KEY_OLD_CTRL
        LD C,5
INK_LOOP:
        LD A,(DE)
        PUSH BC
        PUSH DE
        PUSH HL
        CALL KM_GET_CONTROL
        POP HL
        LD (HL),A
        POP DE
        POP BC
        INC HL
        INC DE
        DEC C
        JR NZ,INK_LOOP
        LD DE,NAV_KEY_SCAN
        LD HL,NAV_KEY_CODE
        LD C,5
INK_SET:
        LD A,(DE)
        LD B,(HL)
        PUSH BC
        PUSH DE
        PUSH HL
        CALL KM_SET_CONTROL
        POP HL
        POP DE
        POP BC
        INC HL
        INC DE
        DEC C
        JR NZ,INK_SET
        RET
RESTORE_NAV_KEYS:
        LD DE,NAV_KEY_SCAN
        LD HL,KEY_OLD_CTRL
        LD C,5
RNK_LOOP:
        LD A,(DE)
        LD B,(HL)
        PUSH BC
        PUSH DE
        PUSH HL
        CALL KM_SET_CONTROL
        POP HL
        POP DE
        POP BC
        INC HL
        INC DE
        DEC C
        JR NZ,RNK_LOOP
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

; The viewport has only 21 plain-text cache lines.  Banked RAM is consulted
; on page turns, not for ordinary cursor movements or text rendering.
; Minimal line diagnostics; A=0 valid/unchecked, 1 unclosed quote,
; 2 unbalanced parentheses. Ignore quote contents and REM/' comments.
; Scan 76 visible characters of one cached source line (HL).
TOK_INVALID_FLAG EQU #8888
TOK_ERRORS EQU #8890    ; 21 per-row invalid-token flags
LINE_DIAG EQU #8885
DIAG_QUOTES EQU #8886
DIAG_DEPTH EQU #8887
CHECK_LINE_MINIMAL:
        XOR A
        LD (DIAG_QUOTES),A
        LD (DIAG_DEPTH),A
        LD B,76
CLM_SCAN:
        LD A,(HL)
        CP 39
        JR Z,CLM_APOSTROPHE
        CP 34
        JR NZ,CLM_OUTSIDE
        LD A,(DIAG_QUOTES)
        XOR 1
        LD (DIAG_QUOTES),A
        JR CLM_NEXT
CLM_APOSTROPHE:
        LD A,(DIAG_QUOTES)
        OR A
        JR Z,CLM_DONE
        JR CLM_NEXT
CLM_OUTSIDE:
        LD A,(DIAG_QUOTES)
        OR A
        JR NZ,CLM_NEXT
        LD A,(HL)
        CP '('
        JR NZ,CLM_CLOSE
        LD A,(DIAG_DEPTH)
        INC A
        LD (DIAG_DEPTH),A
        JR CLM_NEXT
CLM_CLOSE:
        CP ')'
        JR NZ,CLM_NEXT
        LD A,(DIAG_DEPTH)
        OR A
        JR Z,CLM_BAD_PAREN
        DEC A
        LD (DIAG_DEPTH),A
CLM_NEXT:
        INC HL
        DJNZ CLM_SCAN
CLM_DONE:
        LD A,(DIAG_QUOTES)
        OR A
        JR NZ,CLM_BAD_QUOTE
        LD A,(DIAG_DEPTH)
        OR A
        JR NZ,CLM_BAD_PAREN
        XOR A
        RET
CLM_BAD_QUOTE:
        LD A,1
        RET
CLM_BAD_PAREN:
        LD A,2
        RET

DRAW_DOCUMENT_ROW:
        PUSH AF
        CALL LINE_POINTER
        PUSH HL
        CALL CHECK_LINE_MINIMAL
        LD (LINE_DIAG),A
        POP HL
        POP AF
        PUSH HL
        LD E,A
        LD D,0
        LD HL,TOK_ERRORS
        ADD HL,DE
        LD A,(HL)
        OR A
        JR Z,ROW_TOKEN_OK
        LD A,(LINE_DIAG)
        OR A
        JR NZ,ROW_TOKEN_OK
        LD A,3
        LD (LINE_DIAG),A
ROW_TOKEN_OK:
        POP HL
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
        ; 76 chars start at col 2: after them cursor is col 78.
        ; Put diagnostic at col 79, keeping source text untouched.
        LD A,32
        CALL TXT_WR_CHAR
        LD A,(LINE_DIAG)
        OR A
        JR Z,LINE_DIAG_BLANK
        CP 3
        JR Z,LINE_DIAG_TOKEN
        LD A,'!'
        CALL TXT_WR_CHAR
        RET
LINE_DIAG_TOKEN:
        LD A,'T'
        CALL TXT_WR_CHAR
        RET
LINE_DIAG_BLANK:
        LD A,32
        CALL TXT_WR_CHAR
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
        ; The viewport cache contains 21 rows starting at TOP_LINE.
        ; Draw using a relative cache row, not an absolute source line.
        LD A,(TMP_ROW)
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
        LD HL,#0A18
        CALL CLEAR_DIGITS
        CALL CURRENT_ABS_INDEX
        INC HL
        LD DE,#0A18
        CALL PRINT_DECIMAL_16
        LD HL,#1418
        CALL CLEAR_COL_DIGITS
        LD A,(CUR_COL)
        INC A
        LD HL,#1418
        CALL PRINT_DECIMAL
        RET

; Print a 16-bit positive line number 1..1500 in a FIXED 4-column
; right-aligned field. Leading blanks are printed so adjacent labels survive.
; Input: HL=number, DE=screen cursor (D=column, E=row).
; Subtraction must use SBC HL,DE with carry test; the previous v0.4.0
; version inverted the ADD/borrow test and produced wrong/missing numbers.
PRINT_DECIMAL_16:
        PUSH HL
        EX DE,HL
        CALL TXT_SET_CURSOR
        POP HL
        LD C,0                ; have any nonzero digits been printed?
        LD DE,1000
        LD B,0
PDEC_THOUSANDS:
        OR A
        SBC HL,DE
        JR C,PDEC_THOUSANDS_END
        INC B
        JR PDEC_THOUSANDS
PDEC_THOUSANDS_END:
        ADD HL,DE             ; restore remainder after oversubtraction
        CALL PDEC_LEADING
        LD DE,100
        LD B,0
PDEC_HUNDREDS:
        OR A
        SBC HL,DE
        JR C,PDEC_HUNDREDS_END
        INC B
        JR PDEC_HUNDREDS
PDEC_HUNDREDS_END:
        ADD HL,DE
        CALL PDEC_LEADING
        LD DE,10
        LD B,0
PDEC_TENS:
        OR A
        SBC HL,DE
        JR C,PDEC_TENS_END
        INC B
        JR PDEC_TENS
PDEC_TENS_END:
        ADD HL,DE
        CALL PDEC_LEADING
        LD A,L                ; remainder 0..9
        ADD A,'0'
        JP TXT_OUTPUT
PDEC_LEADING:
        LD A,B
        OR A
        JR NZ,PDEC_HAVE_DIGIT
        LD A,C
        OR A
        JR NZ,PDEC_PRINT_ZERO
        LD A,32               ; blank until first nonzero digit
        JP TXT_OUTPUT
PDEC_HAVE_DIGIT:
        LD C,1
PDEC_PRINT_ZERO:
        LD A,B
        ADD A,'0'
        JP TXT_OUTPUT

CURRENT_ABS_INDEX:
        LD HL,(TOP_LINE)
        LD A,(CUR_LINE)
        LD E,A
        LD D,0
        ADD HL,DE
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

; Input A=viewport row [0..20] => HL=DOC_BASE+80*A.
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
        JP SET_DIRTY_AND_REDRAW
INSERT_LAST:
        CALL POSITION_POINTER
        LD A,(PENDING)
        LD (HL),A
        JP SET_DIRTY_AND_REDRAW

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
        JP SET_DIRTY_AND_REDRAW
DELETE_LAST:
        CALL POSITION_POINTER
        LD (HL),32
        JP SET_DIRTY_AND_REDRAW

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
        JP Z,NAV_FAST
        DEC A
        LD (CUR_COL),A
        JP NAV_FAST
MOVE_RIGHT:
        LD A,(CUR_COL)
        CP MAX_COL
        JP NC,NAV_FAST
        INC A
        LD (CUR_COL),A
        JP NAV_FAST

SET_DIRTY_AND_REDRAW:
        LD A,1
        LD (DIRTY_FLAG),A
        JP MAIN_LOOP

MOVE_UP:
        CALL CURRENT_ABS_INDEX
        LD A,H
        OR L
        JP Z,NAV_FAST
        CALL COMMIT_DIRTY
        JP NC,STORE_ERROR_MESSAGE
        LD A,(CUR_LINE)
        OR A
        JR Z,MOVE_UP_SCROLL
        DEC A
        LD (CUR_LINE),A
        JP NAV_FAST
MOVE_UP_SCROLL:
        LD HL,(TOP_LINE)
        DEC HL
        LD (TOP_LINE),HL
        JP SCROLL_ONE_UP

MOVE_DOWN:
        CALL CURRENT_ABS_INDEX
        INC HL
        LD DE,(DOC_LINES)
        OR A
        SBC HL,DE
        JP NC,NAV_FAST
        CALL COMMIT_DIRTY
        JP NC,STORE_ERROR_MESSAGE
        LD A,(CUR_LINE)
        CP VIEW_LINES-1
        JR Z,MOVE_DOWN_SCROLL
        INC A
        LD (CUR_LINE),A
        JP NAV_FAST
MOVE_DOWN_SCROLL:
        LD HL,(TOP_LINE)
        INC HL
        LD (TOP_LINE),HL
        JP SCROLL_ONE_DOWN

; Split logical line: save its suffix, commit the shortened source,
; move the three-byte index records, then create a new tokenized record.
NEWLINE:
        LD HL,(DOC_LINES)
        LD DE,MAX_LINES
        OR A
        SBC HL,DE
        JP NC,NEWLINE_FULL
NEWLINE_CAPACITY_OK:
        ; TEMP_LINE receives the suffix; source suffix becomes spaces.
        CALL POSITION_POINTER
        LD DE,TEMP_LINE
        LD A,(CUR_COL)
        LD B,A
        LD A,76
        SUB B
        LD B,A
NL_COPY_SUFFIX:
        LD A,(HL)
        LD (DE),A
        LD (HL),32
        INC HL
        INC DE
        DJNZ NL_COPY_SUFFIX
        ; Pad the unfilled tail of TEMP_LINE with spaces.
        LD A,(CUR_COL)
        LD B,A
        LD A,B
        OR A
        JR Z,NL_NO_PAD
NL_PAD:
        LD A,32
        LD (DE),A
        INC DE
        DJNZ NL_PAD
NL_NO_PAD:
        LD A,1
        LD (DIRTY_FLAG),A
        CALL COMMIT_DIRTY
        JP NC,STORE_ERROR_MESSAGE
        CALL CURRENT_ABS_INDEX
        INC HL
        LD (REQUEST_INDEX),HL
        CALL INDEX_INSERT_EMPTY
        LD HL,(REQUEST_INDEX)
        LD DE,TEMP_LINE
        CALL WRITE_LOGICAL_LINE
        JP NC,STORE_ERROR_MESSAGE
        ; The cursor moves to inserted line; rebuild viewport.
        LD A,(CUR_LINE)
        CP VIEW_LINES-1
        JR NZ,NL_MOVE_VISIBLE
        LD HL,(TOP_LINE)
        INC HL
        LD (TOP_LINE),HL
        JR NL_FINISH
NL_MOVE_VISIBLE:
        INC A
        LD (CUR_LINE),A
NL_FINISH:
        XOR A
        LD (CUR_COL),A
        JP FULL_REDRAW
NEWLINE_FULL:
        LD HL,MSG_LIMIT
        JP SHOW_MESSAGE

; Insert an empty line at absolute REQUEST_INDEX and shift following
; compact *indices* (not banked data) by 3 bytes each.
INDEX_INSERT_EMPTY:
        LD HL,(DOC_LINES)
        CALL INDEX_ADDRESS
        LD DE,2
        ADD HL,DE                 ; last byte in destination slot [count]
        PUSH HL
        LD DE,-3
        ADD HL,DE                 ; last byte in source slot [count-1]
        POP DE                    ; DE = destination end, HL = source end
        PUSH HL
        LD HL,(DOC_LINES)
        LD DE,(REQUEST_INDEX)
        OR A
        SBC HL,DE                 ; number of indices to shift
        PUSH HL
        ADD HL,HL
        POP DE
        ADD HL,DE                 ; 3 * number of entries
        LD B,H
        LD C,L
        POP HL                    ; source end
        LD A,B
        OR C
        JR Z,INSERT_IDX_DONE
        ; Recalculate destination pointer (avoid DE from count calculation).
        PUSH HL
        LD DE,3
        ADD HL,DE
        EX DE,HL
        POP HL
        LDDR
INSERT_IDX_DONE:
        LD HL,(REQUEST_INDEX)
        CALL INDEX_ADDRESS
        LD (HL),#FF
        LD HL,(DOC_LINES)
        INC HL
        LD (DOC_LINES),HL
        RET

STORE_ERROR_MESSAGE:
        LD HL,MSG_STORE_FULL
        JP SHOW_MESSAGE

; Copy ROM-resident filename to stable RAM before AMSDOS switches upper ROM.
; All bytes supplied to CAS_OUT_OPEN/CAS_IN_OPEN must be readable with ROM 7 active.
PREPARE_DISK_NAME:
        LD HL,FILE_NAME
        LD DE,FILE_NAME_RAM
        LD BC,8
        LDIR
        RET


; Save compressed program as readable DOS-compatible ASCII .BAS.
SAVE_FILE:
        CALL COMMIT_DIRTY
        JP NC,STORE_ERROR_MESSAGE
        CALL PREPARE_DISK_NAME
        LD HL,FILE_NAME_RAM
        LD B,8
        LD DE,IO_BUFFER
        CALL CAS_OUT_OPEN
        JP NC,FILE_SAVE_OPEN_ERROR
        LD HL,0
        LD (SAVE_INDEX),HL
SAVE_NEXT_ROW:
        LD HL,(SAVE_INDEX)
        LD DE,TEMP_LINE
        CALL LOAD_LOGICAL_LINE
        LD HL,TEMP_LINE+75
        LD B,76
TRIM_SAVE:
        LD A,(HL)
        CP 32
        JR NZ,SAVE_TRIM_DONE
        DEC HL
        DJNZ TRIM_SAVE
SAVE_TRIM_DONE:
        LD A,B
        LD (TMP_COUNT),A
        LD HL,TEMP_LINE
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
        LD HL,(SAVE_INDEX)
        INC HL
        LD (SAVE_INDEX),HL
        LD DE,(DOC_LINES)
        OR A
        SBC HL,DE
        JR NZ,SAVE_NEXT_ROW
        CALL CAS_OUT_CLOSE
        JP NC,FILE_SAVE_CLOSE_ERROR
        LD HL,MSG_SAVED
        JP SHOW_MESSAGE
SAVE_ABORT:
        CALL CAS_OUT_ABANDON
        LD HL,MSG_SAVE_WRITE_ERROR
        JP SHOW_MESSAGE
FILE_SAVE_OPEN_ERROR:
        LD HL,MSG_SAVE_OPEN_ERROR
        JP SHOW_MESSAGE
FILE_SAVE_CLOSE_ERROR:
        LD HL,MSG_SAVE_CLOSE_ERROR
        JP SHOW_MESSAGE

; Parse ASCII .BAS into independent variable-length tokenized lines,
; up to 1500 lines / 64 KiB bank store. No line numbers required.
LOAD_FILE:
        CALL PREPARE_DISK_NAME
        LD HL,FILE_NAME_RAM
        LD B,8
        LD DE,IO_BUFFER
        CALL CAS_IN_OPEN
        JP NC,FILE_LOAD_ERROR
        CALL NEW_DOCUMENT
        LD HL,0
        LD (SAVE_INDEX),HL
        CALL INIT_IMPORT_LINE
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
        CALL STORE_IMPORT_LINE
        JR NC,LOAD_LIMIT
        LD HL,(SAVE_INDEX)
        INC HL
        LD (SAVE_INDEX),HL
        LD DE,MAX_LINES
        OR A
        SBC HL,DE
        JR NC,LOAD_LIMIT
        CALL INIT_IMPORT_LINE
        JR LOAD_BYTE
LOAD_DONE:
        CALL STORE_IMPORT_LINE
        CALL CAS_IN_CLOSE
        CALL CACHE_BUILD
        LD HL,MSG_LOADED
        JP SHOW_MESSAGE
LOAD_LIMIT:
        CALL CAS_IN_CLOSE
        CALL CACHE_BUILD
        LD HL,MSG_LIMIT
        JP SHOW_MESSAGE
FILE_LOAD_ERROR:
        LD HL,MSG_LOAD_ERROR
        JP SHOW_MESSAGE

INIT_IMPORT_LINE:
        LD HL,TEMP_LINE
        LD DE,TEMP_LINE+1
        LD BC,75
        LD (HL),32
        LDIR
        XOR A
        LD (IO_COL),A
        RET
IMPORT_CHAR:
        LD (TMP_CHAR),A
        LD A,(IO_COL)
        CP 76
        RET NC
        LD E,A
        LD D,0
        LD HL,TEMP_LINE
        ADD HL,DE
        LD A,(TMP_CHAR)
        LD (HL),A
        LD A,(IO_COL)
        INC A
        LD (IO_COL),A
        RET
STORE_IMPORT_LINE:
        LD HL,(SAVE_INDEX)
        LD DE,TEMP_LINE
        CALL WRITE_LOGICAL_LINE
        RET NC
        LD HL,(SAVE_INDEX)
        INC HL
        LD (DOC_LINES),HL
        SCF
        RET

NO_RUNTIME:
        LD HL,MSG_NO_RUN
        JP SHOW_MESSAGE
;
; ------------------------------------------------------------
; V0.4.2: direct navigation to any logical line (1..DOC_LINES).
; Ctrl+G opens a 4-digit input on status row 24. Enter confirms,
; Esc cancels without exiting; Backspace edits the input.
; The viewport is rebuilt ONLY on confirmation, not for each digit.
; ------------------------------------------------------------
; Fast document navigation through existing GOTO target logic.
; Ctrl+B = Begin, Ctrl+E = End, Ctrl+U = Page Up, Ctrl+D = Page Down.
NAV_BEGIN:
        LD HL,1
        JR NAV_TO_TARGET
NAV_END:
        LD HL,(DOC_LINES)
        JR NAV_TO_TARGET
NAV_PAGE_UP:
        CALL CURRENT_ABS_INDEX
        LD DE,VIEW_LINES
        OR A
        SBC HL,DE
        JR NC,NAV_PAGE_UP_OK
        LD HL,0
NAV_PAGE_UP_OK:
        INC HL
        JR NAV_TO_TARGET
NAV_PAGE_DOWN:
        CALL CURRENT_ABS_INDEX
        LD DE,VIEW_LINES
        ADD HL,DE
        LD DE,(DOC_LINES)
        OR A
        SBC HL,DE
        JR C,NAV_PAGE_DOWN_OK
        JR Z,NAV_PAGE_DOWN_OK
        LD HL,(DOC_LINES)
        JR NAV_TO_TARGET
NAV_PAGE_DOWN_OK:
        LD HL,(TOP_LINE)
        LD DE,(CUR_LINE)
        ADD HL,DE
        LD DE,VIEW_LINES+1
        ADD HL,DE
NAV_TO_TARGET:
        LD (GOTO_TARGET),HL
        JP GOTO_ACCEPT

GOTO_LINE:
        LD HL,#0118
        LD DE,MSG_GOTO_PROMPT
        CALL SHOW_TEXT
        XOR A
        LD (GOTO_COUNT),A
GOTO_READ:
        CALL KM_WAIT_KEY
        CP #FC                 ; ESC: cancel dialog, keep editor running
        JP Z,FULL_REDRAW
        CP 13
        JP Z,GOTO_ENTER
        CP 127
        JP Z,GOTO_BACKSPACE
        CP '0'
        JR C,GOTO_READ
        CP '9'+1
        JR NC,GOTO_READ
        LD (GOTO_CHAR),A
        LD A,(GOTO_COUNT)
        CP 4
        JR NC,GOTO_READ
        LD E,A
        LD D,0
        LD HL,GOTO_BUF
        ADD HL,DE
        LD A,(GOTO_CHAR)
        LD (HL),A
        LD A,(GOTO_COUNT)
        ADD A,30              ; field starts at column 30
        LD H,A
        LD L,24
        CALL TXT_SET_CURSOR
        LD A,(GOTO_CHAR)
        CALL TXT_WR_CHAR
        LD A,(GOTO_COUNT)
        INC A
        LD (GOTO_COUNT),A
        JP GOTO_READ
GOTO_BACKSPACE:
        LD A,(GOTO_COUNT)
        OR A
        JP Z,GOTO_READ
        DEC A
        LD (GOTO_COUNT),A
        ADD A,30
        LD H,A
        LD L,24
        CALL TXT_SET_CURSOR
        LD A,32
        CALL TXT_WR_CHAR
        JP GOTO_READ

GOTO_ENTER:
        LD A,(GOTO_COUNT)
        OR A
        JP Z,FULL_REDRAW
        LD (GOTO_REMAIN),A
        LD HL,0
        LD (GOTO_TARGET),HL
        LD DE,GOTO_BUF
GOTO_PARSE:
        PUSH DE
        LD HL,(GOTO_TARGET)
        LD D,H
        LD E,L
        ADD HL,HL             ; times 2
        ADD HL,HL             ; times 4
        ADD HL,DE             ; times 5
        ADD HL,HL             ; times 10
        POP DE
        LD A,(DE)
        SUB '0'
        LD C,A
        LD B,0
        ADD HL,BC
        LD (GOTO_TARGET),HL
        INC DE
        LD A,(GOTO_REMAIN)
        DEC A
        LD (GOTO_REMAIN),A
        JR NZ,GOTO_PARSE

        ; Accept a one-based number in [1, DOC_LINES], never 0.
        LD HL,(GOTO_TARGET)
        LD A,H
        OR L
        JP Z,GOTO_RANGE_ERROR
        LD DE,(DOC_LINES)
        OR A
        SBC HL,DE
        JR C,GOTO_ACCEPT
        JR Z,GOTO_ACCEPT
GOTO_RANGE_ERROR:
        LD HL,MSG_GOTO_RANGE
        JP SHOW_MESSAGE

GOTO_ACCEPT:
        CALL WAIT_SHOW
        ; Save pending changes before discarding the current 21-line cache.
        CALL COMMIT_DIRTY
        JP NC,STORE_ERROR_MESSAGE
        LD HL,(GOTO_TARGET)
        DEC HL
        LD (GOTO_TARGET),HL       ; logical target 0..1499
        LD DE,10
        OR A
        SBC HL,DE                 ; desired top = max(target-10, 0)
        JR NC,GOTO_CAND_READY
        LD HL,0
GOTO_CAND_READY:
        LD (GOTO_CAND_TOP),HL
        LD HL,(DOC_LINES)
        LD DE,VIEW_LINES
        OR A
        SBC HL,DE                 ; max top = max(DOC_LINES-21,0)
        JR NC,GOTO_MAX_READY
        LD HL,0
GOTO_MAX_READY:
        LD (GOTO_MAX_TOP),HL
        LD DE,(GOTO_CAND_TOP)
        OR A
        SBC HL,DE                 ; compare max-top vs desired-top
        JR NC,GOTO_SET_CAND
        LD HL,(GOTO_MAX_TOP)
        JR GOTO_SET_TOP
GOTO_SET_CAND:
        LD HL,(GOTO_CAND_TOP)
GOTO_SET_TOP:
        LD (TOP_LINE),HL
        EX DE,HL                 ; DE = top
        LD HL,(GOTO_TARGET)
        OR A
        SBC HL,DE                ; 0..20 screen row
        LD A,L
        LD (CUR_LINE),A
        XOR A
        LD (CUR_COL),A
        JP FULL_REDRAW

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


; TOKENIZED STORE ENGINE inserted below.
INCLUDE "QEDIT_V04_CORE.asm"

; ------------------------------------------------------------
; QEDIT v0.3: firmware text-mode popup menus, 8 headings.
; ROM contains labels and commands; no menu data allocated inside document RAM.
; The CPC6128 has no native PC F10 key; WinAPE intercepts F10 to fullscreen.
; Ctrl+T is the portable menu accelerator.
; ------------------------------------------------------------
MENU_START:
        XOR A
        LD (MENU_SEL),A
        LD (MENU_ITEM),A
        JP MENU_DRAW

; Fast menu painter: DO NOT redraw the 21-line editor. Only the
; menu bar and the menu items are touched. Left/right first restores
; the small overlay underneath the previous menu.
MENU_DRAW:
        LD A,1
        CALL TXT_SET_PAPER
        XOR A
        CALL TXT_SET_PEN
        LD HL,#0101
        LD DE,HEADER_TEXT
        CALL SHOW_TEXT
        ; Print the highlighted heading in editor colors.
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_HEAD_X
        ADD HL,DE
        LD A,(HL)
        LD H,A
        LD L,1
        PUSH HL
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_HEAD_NAMES
        ADD HL,HL
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        POP HL
        CALL SHOW_TEXT
        XOR A
        LD (MENU_DRAW_INDEX),A
MENU_PAINT_NEXT:
        LD A,(MENU_DRAW_INDEX)
        LD B,A
        CALL MENU_COUNT
        CP B
        JP Z,MENU_WAIT
        LD A,(MENU_DRAW_INDEX)
        CALL MENU_PAINT_ONE
        LD A,(MENU_DRAW_INDEX)
        INC A
        LD (MENU_DRAW_INDEX),A
        JR MENU_PAINT_NEXT

; Paint just one 20-character popup item.  In UP/DOWN navigation
; only two items (old and new selection) must be rewritten.
MENU_PAINT_ONE:
        LD (MENU_DRAW_INDEX),A
        LD B,A
        LD A,(MENU_ITEM)
        CP B
        JR Z,MENU_PAINT_SELECTED
        LD A,1
        CALL TXT_SET_PAPER
        XOR A
        CALL TXT_SET_PEN
        JR MENU_PAINT_ITEM
MENU_PAINT_SELECTED:
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
MENU_PAINT_ITEM:
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        ADD HL,HL
        LD DE,MENU_LISTS
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        EX DE,HL
        LD A,(MENU_DRAW_INDEX)
        ADD A,A
        LD E,A
        LD D,0
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        PUSH DE
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_POP_X
        ADD HL,DE
        LD A,(HL)
        LD H,A
        LD A,(MENU_DRAW_INDEX)
        ADD A,3
        LD L,A
        POP DE
        CALL SHOW_TEXT
        RET

; Restore only the 4 x 20 character document rectangle covered by
; the previous popup (not the whole text window). This is called
; before changing MENU_SEL, so it knows the OLD popup x coordinate.
MENU_ERASE_POPUP:
        XOR A
        CALL TXT_SET_PAPER
        LD A,1
        CALL TXT_SET_PEN
        XOR A
        LD (MENU_DRAW_INDEX),A
MENU_ERASE_ROW:
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_POP_X
        ADD HL,DE
        LD A,(HL)
        LD H,A
        LD A,(MENU_DRAW_INDEX)
        ADD A,3
        LD L,A
        CALL TXT_SET_CURSOR
        LD A,(MENU_DRAW_INDEX)
        CALL LINE_POINTER
        PUSH HL
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_POP_X
        ADD HL,DE
        LD A,(HL)
        SUB 2                   ; document first char is screen x=2
        LD E,A
        LD D,0
        POP HL
        ADD HL,DE
        LD B,20
MENU_ERASE_TEXT:
        LD A,(HL)
        PUSH HL
        PUSH BC
        CALL TXT_WR_CHAR
        POP BC
        POP HL
        INC HL
        DJNZ MENU_ERASE_TEXT
        JR MENU_ERASE_NEXT
MENU_ERASE_BLANK:
        LD B,20
MENU_ERASE_SPACES:
        LD A,32
        PUSH BC
        CALL TXT_WR_CHAR
        POP BC
        DJNZ MENU_ERASE_SPACES
MENU_ERASE_NEXT:
        LD A,(MENU_DRAW_INDEX)
        INC A
        LD (MENU_DRAW_INDEX),A
        CP 4
        JR C,MENU_ERASE_ROW
        RET

MENU_REDRAW_SELECTION:
        LD A,(MENU_OLD_ITEM)
        CALL MENU_PAINT_ONE
        LD A,(MENU_ITEM)
        CALL MENU_PAINT_ONE
        JP MENU_WAIT

MENU_WAIT:
        CALL KM_WAIT_KEY
        CP #F2     ; left
        JP Z,MENU_LEFT
        CP #F3     ; right
        JP Z,MENU_RIGHT
        CP #F0     ; up
        JP Z,MENU_UP
        CP #F1     ; down
        JP Z,MENU_DOWN
        CP 13      ; ENTER
        JP Z,MENU_EXECUTE
        CP #FC     ; ESC = close menus (only if menu open)
        JP Z,FULL_REDRAW
        CP 20      ; Ctrl+T = toggle menus
        JP Z,FULL_REDRAW
        JP MENU_WAIT

; A = selected heading, return HL = index (0..7).
MENU_INDEX_HL:
        LD L,A
        LD H,0
        RET

; Return A = number of commands for selected heading.
MENU_COUNT:
        LD A,(MENU_SEL)
        CALL MENU_INDEX_HL
        LD DE,MENU_COUNTS
        ADD HL,DE
        LD A,(HL)
        RET

MENU_LEFT:
        CALL MENU_ERASE_POPUP
        LD A,(MENU_SEL)
        OR A
        JR NZ,MENU_LEFT_DEC
        LD A,8
MENU_LEFT_DEC:
        DEC A
        LD (MENU_SEL),A
        XOR A
        LD (MENU_ITEM),A
        JP MENU_DRAW
MENU_RIGHT:
        CALL MENU_ERASE_POPUP
        LD A,(MENU_SEL)
        INC A
        CP 8
        JR C,MENU_RIGHT_OK
        XOR A
MENU_RIGHT_OK:
        LD (MENU_SEL),A
        XOR A
        LD (MENU_ITEM),A
        JP MENU_DRAW
MENU_UP:
        LD A,(MENU_ITEM)
        LD (MENU_OLD_ITEM),A
        OR A
        JR NZ,MENU_UP_DEC
        CALL MENU_COUNT
MENU_UP_DEC:
        DEC A
        LD (MENU_ITEM),A
        JP MENU_REDRAW_SELECTION
MENU_DOWN:
        LD A,(MENU_ITEM)
        LD (MENU_OLD_ITEM),A
        CALL MENU_COUNT
        LD B,A
        LD A,(MENU_ITEM)
        INC A
        CP B
        JR C,MENU_DOWN_OK
        XOR A
MENU_DOWN_OK:
        LD (MENU_ITEM),A
        JP MENU_REDRAW_SELECTION

; Dispatch implemented actions. Functions belonging to the future
; interpreter ROM show an explicit message instead of silently failing.
MENU_EXECUTE:
        LD A,(MENU_SEL)
        OR A
        JP Z,MENU_FILE_EXEC
        CP 1
        JP Z,MENU_EDIT_EXEC
        CP 2
        JP Z,MENU_SEARCH_EXEC
        CP 3
        JP Z,NO_RUNTIME
        CP 4
        JP Z,MENU_UNIMPLEMENTED
        CP 5
        JP Z,MENU_OPTIONS_EXEC
        CP 6
        JP Z,MENU_WINDOW_EXEC
        ; Help
        LD A,(MENU_ITEM)
        OR A
        JP Z,SHOW_HELP
        LD HL,MSG_ABOUT
        JP SHOW_MESSAGE

MENU_FILE_EXEC:
        LD A,(MENU_ITEM)
        OR A
        JP Z,RESET_FILE
        CP 1
        JP Z,LOAD_FILE
        CP 2
        JP Z,SAVE_FILE
        JP EXIT_EDITOR

MENU_EDIT_EXEC:
        LD A,(MENU_ITEM)
        OR A
        JP Z,NEWLINE
        CP 1
        JP Z,DELETE_CHAR
        CP 2
        JR Z,MENU_HOME
        LD A,MAX_COL
        LD (CUR_COL),A
        JP FULL_REDRAW
MENU_HOME:
        XOR A
        LD (CUR_COL),A
        JP FULL_REDRAW

MENU_SEARCH_EXEC:
        LD A,(MENU_ITEM)
        OR A
        JP Z,GOTO_LINE
        LD HL,MSG_SEARCH
        JP SHOW_MESSAGE
MENU_UNIMPLEMENTED:
        LD HL,MSG_NOT_READY
        JP SHOW_MESSAGE
MENU_OPTIONS_EXEC:
        LD A,(MENU_ITEM)
        OR A
        JR Z,MENU_OPT_REDRAW
        CP 1
        JP Z,SHOW_HELP
        JP LOAD_LONG_DEMO
MENU_OPT_REDRAW:
        CALL SCREEN_INIT
        JP FULL_REDRAW
MENU_WINDOW_EXEC:
        LD A,(MENU_ITEM)
        OR A
        JP Z,FULL_REDRAW
        LD HL,MSG_VARIABLES
        JP SHOW_MESSAGE

; Popup locations max 58 so items do not wrap at column 80.
MENU_HEAD_X:
        DB 2,11,20,31,42,52,61,70
MENU_POP_X:
        DB 2,11,20,32,44,54,58,58
MENU_COUNTS:
        DB 4,4,3,2,2,3,2,2
MENU_HEAD_NAMES:
        DW HEAD_FILE,HEAD_EDIT,HEAD_SEARCH,HEAD_RUN
        DW HEAD_DEBUG,HEAD_OPTIONS,HEAD_WINDOW,HEAD_HELP
MENU_LISTS:
        DW ITEMS_FILE,ITEMS_EDIT,ITEMS_SEARCH,ITEMS_RUN
        DW ITEMS_DEBUG,ITEMS_OPTIONS,ITEMS_WINDOW,ITEMS_HELP
HEAD_FILE:    DB 'Fichier',0
HEAD_EDIT:    DB 'Edition',0
HEAD_SEARCH:  DB 'Recherche',0
HEAD_RUN:     DB 'Execution',0
HEAD_DEBUG:   DB 'Debogage',0
HEAD_OPTIONS: DB 'Options',0
HEAD_WINDOW:  DB 'Fenetre',0
HEAD_HELP:    DB 'Aide',0

ITEMS_FILE:
        DW FILE_NEW,FILE_OPEN,FILE_SAVE,FILE_EXIT
ITEMS_EDIT:
        DW EDIT_NEWLINE,EDIT_DELETE,EDIT_HOME,EDIT_END
ITEMS_SEARCH:
        DW SEARCH_GOTO,SEARCH_FIND,SEARCH_NEXT
ITEMS_RUN:
        DW RUN_EXEC,RUN_STOP
ITEMS_DEBUG:
        DW DEBUG_STEP,DEBUG_VARS
ITEMS_OPTIONS:
        DW OPT_MODE2,OPT_HELP,OPT_LONG
ITEMS_WINDOW:
        DW WIN_EDITOR,WIN_VARS
ITEMS_HELP:
        DW HELP_KEYS,HELP_ABOUT

FILE_NEW:       DB ' Nouveau            ',0
FILE_OPEN:      DB ' Ouvrir DEMO.BAS    ',0
FILE_SAVE:      DB ' Enregistrer        ',0
FILE_EXIT:      DB ' Quitter            ',0
EDIT_NEWLINE:   DB ' Inserer ligne      ',0
EDIT_DELETE:    DB ' Supprimer caract.  ',0
EDIT_HOME:      DB ' Debut de ligne     ',0
EDIT_END:       DB ' Fin de ligne       ',0
SEARCH_GOTO:    DB ' Aller a la ligne    ',0
SEARCH_FIND:    DB ' Rechercher         ',0
SEARCH_NEXT:    DB ' Suivant            ',0
RUN_EXEC:       DB ' Executer           ',0
RUN_STOP:       DB ' Arret              ',0
DEBUG_STEP:     DB ' Pas a pas          ',0
DEBUG_VARS:     DB ' Etat variables     ',0
OPT_MODE2:      DB ' Reafficher Mode 2  ',0
OPT_HELP:       DB ' Raccourcis         ',0
OPT_LONG:       DB ' Test 50 lignes     ',0
WIN_EDITOR:     DB ' Editeur            ',0
WIN_VARS:       DB ' Variables          ',0
HELP_KEYS:      DB ' Raccourcis         ',0
HELP_ABOUT:     DB ' A propos           ',0
MSG_GOTO_PROMPT: DB 'Aller a la ligne (1-1500):                                       Esc=Annuler',0
MSG_GOTO_RANGE: DB 'Numero de ligne inexistant. Tapez une touche pour continuer.               ',0
MSG_SEARCH:     DB 'Recherche: fonction prevue pour version suivante. Tapez une touche.        ',0
MSG_VARIABLES:  DB 'Fenetre Variables: disponible avec le moteur QBasic. Tapez une touche.     ',0
MSG_NOT_READY:  DB 'Cette commande sera ajoutee avec la ROM execution. Tapez une touche.       ',0
MSG_ABOUT:      DB 'QEDIT v0.4.2 - CPC6128 16Ko ROM - edition sans numeros de ligne.            ',0

HEADER_TEXT:
        DB ' Fichier  Edition  Recherche  Execution  Debogage  Options  Fenetre  Aide   ',0
TITLE_TEXT:
        DB '                         [ DEMO.BAS ]        QEDIT v0.4.2 B005',0
STATUS_TEXT:
        DB 'Ligne:       Col:     INS  |  MODE 2  |  1500 lignes max                 ',0
FOOTER_TEXT:
        DB ' Ctrl+T Menus  Ctrl+G Ligne  Ctrl+B Debut  Ctrl+E Fin  Ctrl+S Sauv  Esc Quitter ',0
MSG_HELP:
        DB 'Ctrl+T menus; Ctrl+G ligne; Ctrl+S/O/N; Esc     ',0
MSG_NO_RUN:
        DB 'Execution QBasic: ROM moteur non encore installee. Tapez une touche.       ',0
MSG_SAVED:
        DB 'DEMO.BAS enregistre. Tapez une touche.                                       ',0
MSG_LIMIT:
        DB 'Limite 1500 lignes / memoire compacte atteinte. Tapez une touche.              ',0
MSG_STORE_FULL:
        DB 'Memoire paginee pleine: enregistrer puis reouvrir pour compacter.            ',0
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
STRESS_TEXT:
        DB 'PRINT "1500 LIGNES TOKENISEES"',0
LONG_DEMO_TEXT:
        DB 'REM QEDIT CPC6128 - TEST DEFILEMENT LONG',0
        DB 'DIM i AS INTEGER',0
        DB 'CLS',0
        DB 'PRINT "TEST DEFILEMENT SUR 50 LIGNES"',0
        DB 'PRINT "DESCENDRE AU-DELA DE LA PAGE 21"',0
        DB 'FOR i = 1 TO 2',0
        DB '    PRINT "--- PASSAGE ---"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 01"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 02"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 03"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 04"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 05"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 06"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 07"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 08"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 09"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 10"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 11"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 12"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 13"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 14"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 15"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 16"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 17"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 18"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 19"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 20"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 21"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 22"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 23"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 24"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 25"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 26"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 27"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 28"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 29"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 30"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 31"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 32"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 33"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 34"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 35"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 36"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 37"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 38"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 39"',0
        DB '    PRINT "TEST DEFILEMENT - SECTION 40"',0
        DB 'NEXT i',0
        DB 'PRINT "FIN DU TEST - REMONTER AVEC FLECHE HAUT"',0
        DB 'END',255
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
