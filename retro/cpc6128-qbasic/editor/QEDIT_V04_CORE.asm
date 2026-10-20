; ============================================================
; QEDIT v0.4 banked token document engine.
; 1500 entries at #9800 (bank id + little endian record address).
; Four 16K CPC6128 expanded banks C4-C7 map at #4000-#7FFF.
; Each compact record: byte token_length, then token_length bytes.
; Records are immutable, modified lines append; an old record is retired.
; ROM contains token dictionary; screen cache stays at #6000.
; ============================================================

; Get pointer to index entry for logical line HL (0..1499): #9800+3*HL.
INDEX_ADDRESS:
        PUSH HL
        ADD HL,HL
        POP DE
        ADD HL,DE
        LD DE,INDEX_BASE
        ADD HL,DE
        RET

; Input HL=logical line, DE=destination plain-text row.
LOAD_LOGICAL_LINE:
        XOR A
        LD (TOK_INVALID_FLAG),A
        LD (REQUEST_INDEX),HL
        LD (REQUEST_DEST),DE
        LD H,D
        LD L,E
        LD (HL),32
        INC DE
        LD BC,75
        LDIR
        LD HL,(REQUEST_INDEX)
        CALL INDEX_ADDRESS
        LD A,(HL)
        CP #FF
        RET Z
        LD (TOK_TMP),A
        INC HL
        LD E,(HL)
        INC HL
        LD D,(HL)
        EX DE,HL
        ; HL=bank address, bank selector in TOK_TMP.
        PUSH HL
        LD A,(TOK_TMP)
        CALL SELECT_EXP_BANK
        POP HL
        LD A,(HL)
        LD (DECODE_LEN),A
        INC HL
        LD E,A
        LD D,0
        LD BC,0
        LD C,E
        LD DE,ENC_BUF
        LDIR
        CALL SELECT_BASE_BANK
        JP DECODE_LINE

; Carry set on success. Input HL=logical line and DE=plain-text ptr.
WRITE_LOGICAL_LINE:
        LD (REQUEST_INDEX),HL
        LD (REQUEST_SOURCE),DE
        CALL ENCODE_LINE
        LD A,(ENC_LEN)
        OR A
        JR NZ,WRITE_NONEMPTY
        LD HL,(REQUEST_INDEX)
        CALL INDEX_ADDRESS
        LD (HL),#FF
        SCF
        RET
WRITE_NONEMPTY:
        LD E,A
        INC E                 ; include length header
        LD D,0
        LD HL,(STORE_ADDR)
        ADD HL,DE
        LD A,H
        CP #80
        JR C,WRITE_FITS
        LD A,(STORE_BANK)
        INC A
        CP 4
        JR NC,WRITE_FULL
        LD (STORE_BANK),A
        LD HL,#4000
        LD (STORE_ADDR),HL
WRITE_FITS:
        LD HL,(STORE_ADDR)
        LD (DECODE_PTR),HL   ; remember record start
        LD A,(ENC_LEN)
        LD E,A
        LD D,0
        INC DE
        ADD HL,DE
        LD (STORE_ADDR),HL
        LD A,(STORE_BANK)
        CALL SELECT_EXP_BANK
        LD HL,(DECODE_PTR)
        LD A,(ENC_LEN)
        LD (HL),A
        INC HL
        EX DE,HL
        LD HL,ENC_BUF
        LD A,(ENC_LEN)
        LD C,A
        LD B,0
        LDIR
        CALL SELECT_BASE_BANK
        LD HL,(REQUEST_INDEX)
        CALL INDEX_ADDRESS
        LD A,(STORE_BANK)
        LD (HL),A
        INC HL
        LD DE,(DECODE_PTR)
        LD (HL),E
        INC HL
        LD (HL),D
        SCF
        RET
WRITE_FULL:
        LD A,1
        LD (STORE_ERROR),A
        OR A
        RET

; 7Fxx Gate Array memory config. Input A=0..3 => C4..C7.
SELECT_EXP_BANK:
        ADD A,#C4
        LD BC,#7F00
        OUT (C),A
        RET
SELECT_BASE_BANK:
        LD A,#C0
        LD BC,#7F00
        OUT (C),A
        RET

; Flush current row only when it has changed, preserving fast cursor motion.
COMMIT_DIRTY:
        LD A,(DIRTY_FLAG)
        OR A
        SCF
        RET Z
        CALL CURRENT_ABS_INDEX
        LD A,(CUR_LINE)
        PUSH HL
        CALL LINE_POINTER
        EX DE,HL
        POP HL
        CALL WRITE_LOGICAL_LINE
        RET NC
        XOR A
        LD (DIRTY_FLAG),A
        SCF
        RET

; Rebuild screen cache from packed store; screen is not drawn here.
CACHE_BUILD:
        XOR A
        LD (TMP_ROW),A
CACHE_LOOP:
        LD HL,(TOP_LINE)
        LD A,(TMP_ROW)
        LD E,A
        LD D,0
        ADD HL,DE
        PUSH HL
        LD A,(TMP_ROW)
        CALL LINE_POINTER
        EX DE,HL
        POP HL
        CALL LOAD_LOGICAL_LINE
        LD A,(TMP_ROW)
        LD E,A
        LD D,0
        LD HL,TOK_ERRORS
        ADD HL,DE
        LD A,(TOK_INVALID_FLAG)
        LD (HL),A
        LD A,(TMP_ROW)
        INC A
        LD (TMP_ROW),A
        CP VIEW_LINES
        JR NZ,CACHE_LOOP
        RET

; ENCODE_LINE: tokenize keywords outside strings, REM and apostrophe comments.
; The raw source is always exactly 76 visible characters, padded spaces.
ENCODE_LINE:
        LD HL,(REQUEST_SOURCE)
        LD DE,75
        ADD HL,DE
        LD B,76
ENC_TRIM:
        LD A,(HL)
        CP 32
        JR NZ,ENC_TRIM_DONE
        DEC HL
        DJNZ ENC_TRIM
ENC_TRIM_DONE:
        LD A,B
        LD (ENC_LEFT),A
        LD HL,ENC_BUF
        LD (ENC_PTR),HL
        XOR A
        LD (ENC_QUOTED),A
        LD (ENC_PREV_WORD),A
        LD HL,(REQUEST_SOURCE)
        LD (ENC_SOURCE),HL
ENC_LOOP:
        LD A,(ENC_LEFT)
        OR A
        JR Z,ENC_FINISH
        LD HL,(ENC_SOURCE)
        LD A,(HL)
        CP 34                    ; double quote
        JR Z,ENC_RAW
        CP 39                    ; apostrophe comment
        JR Z,ENC_RAW
        LD A,(ENC_QUOTED)
        OR A
        JR NZ,ENC_RAW
        LD A,(ENC_PREV_WORD)
        OR A
        JR NZ,ENC_RAW
        CALL MATCH_KEYWORD
        JR C,ENC_LOOP
ENC_RAW:
        LD HL,(ENC_SOURCE)
        LD A,(HL)
        LD (TMP_CHAR),A
        CP 34
        JR NZ,ENC_CHECK_COMMENT
        LD A,(ENC_QUOTED)
        CP 2
        JR Z,ENC_EMIT_RAW
        XOR 1
        LD (ENC_QUOTED),A
        JR ENC_EMIT_RAW
ENC_CHECK_COMMENT:
        CP 39
        JR NZ,ENC_EMIT_RAW
        LD A,(ENC_QUOTED)
        OR A
        JR NZ,ENC_EMIT_RAW
        LD A,2
        LD (ENC_QUOTED),A
ENC_EMIT_RAW:
        LD A,(TMP_CHAR)
        LD HL,(ENC_PTR)
        LD (HL),A
        INC HL
        LD (ENC_PTR),HL
        LD HL,(ENC_SOURCE)
        INC HL
        LD (ENC_SOURCE),HL
        LD A,(ENC_LEFT)
        DEC A
        LD (ENC_LEFT),A
        LD A,(TMP_CHAR)
        CALL IS_WORD_CHAR
        LD A,0
        JR NC,ENC_PREV_STORE
        INC A
ENC_PREV_STORE:
        LD (ENC_PREV_WORD),A
        JR ENC_LOOP
ENC_FINISH:
        LD HL,(ENC_PTR)
        LD DE,ENC_BUF
        OR A
        SBC HL,DE
        LD A,L
        LD (ENC_LEN),A
        RET

; MATCH_KEYWORD: use ROM table; only at word boundaries.
; success => emit one token and advance input by exact keyword length.
MATCH_KEYWORD:
        XOR A
        LD (TOK_IDX),A
MATCH_NEXT_WORD:
        LD A,(TOK_IDX)
        CP TOKEN_COUNT
        JP NC,MATCH_FAIL
        LD L,A
        LD H,0
        ADD HL,HL
        LD DE,TOKEN_POINTERS
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        LD (TOK_WORD_PTR),DE
        LD HL,(ENC_SOURCE)
        LD B,0
MATCH_CHAR:
        LD A,(DE)
        OR A
        JR Z,MATCH_BOUNDARY
        LD C,A
        LD A,(ENC_LEFT)
        CP B
        JR Z,MATCH_SKIP
        LD A,(HL)
        CP 'a'
        JR C,MATCH_COMPARE
        CP 'z'+1
        JR NC,MATCH_COMPARE
        SUB 32
MATCH_COMPARE:
        CP C
        JR NZ,MATCH_SKIP
        INC HL
        INC DE
        INC B
        JR MATCH_CHAR
MATCH_BOUNDARY:
        LD A,(ENC_LEFT)
        CP B
        JR Z,MATCH_ACCEPT
        LD A,(HL)
        CALL IS_WORD_CHAR
        JR C,MATCH_SKIP
MATCH_ACCEPT:
        LD HL,(ENC_PTR)
        LD A,(TOK_IDX)
        CP 128
        JR C,MATCH_LEGACY_EMIT
        LD (HL),TOKEN_EXT_PREFIX
        INC HL
        SUB 128
        JR MATCH_EMIT_ID
MATCH_LEGACY_EMIT:
        ADD A,#80
MATCH_EMIT_ID:
        LD (HL),A
        INC HL
        LD (ENC_PTR),HL
        LD HL,(ENC_SOURCE)
        LD E,B
        LD D,0
        ADD HL,DE
        LD (ENC_SOURCE),HL
        LD A,(ENC_LEFT)
        SUB B
        LD (ENC_LEFT),A
        LD A,1
        LD (ENC_PREV_WORD),A
        LD A,(TOK_IDX)
        CP TOKEN_REM
        JR NZ,MATCH_SUCCESS
        LD A,2
        LD (ENC_QUOTED),A
MATCH_SUCCESS:
        SCF
        RET
MATCH_SKIP:
        LD A,(TOK_IDX)
        INC A
        LD (TOK_IDX),A
        JP MATCH_NEXT_WORD
MATCH_FAIL:
        OR A
        RET

; Word characters prevent substitution inside names PRINTED, MY_FOR or X$.
; Carry=1 if ASCII letter/digit/underscore/dollar.
IS_WORD_CHAR:
        CP 'A'
        JR C,WORD_LOWER
        CP 'Z'+1
        JR C,WORD_YES
WORD_LOWER:
        CP 'a'
        JR C,WORD_DIGIT
        CP 'z'+1
        JR C,WORD_YES
WORD_DIGIT:
        CP '0'
        JR C,WORD_OTHER
        CP '9'+1
        JR C,WORD_YES
WORD_OTHER:
        CP '_'
        JR Z,WORD_YES
        CP '$'
        JR Z,WORD_YES
        OR A
        RET
WORD_YES:
        SCF
        RET

; Decode ENC_BUF[DECODE_LEN] into REQUEST_DEST, padded to 76 columns.
DECODE_LINE:
        LD HL,ENC_BUF
        LD (DECODE_PTR),HL
        LD HL,(REQUEST_DEST)
        LD (DECODE_DEST),HL
        XOR A
        LD (ENC_QUOTED),A       ; expanded char count
DECODE_LOOP:
        LD A,(DECODE_LEN)
        OR A
        RET Z
        DEC A
        LD (DECODE_LEN),A
        LD HL,(DECODE_PTR)
        LD A,(HL)
        INC HL
        LD (DECODE_PTR),HL
        CP TOKEN_EXT_PREFIX
        JR Z,DECODE_EXT
        CP #80
        JR C,DECODE_RAW
        SUB #80
        JR DECODE_TOKEN_ID
DECODE_EXT:
        LD A,(DECODE_LEN)
        OR A
        JR Z,DECODE_UNKNOWN
        DEC A
        LD (DECODE_LEN),A
        LD HL,(DECODE_PTR)
        LD A,(HL)
        INC HL
        LD (DECODE_PTR),HL
        ADD A,128
DECODE_TOKEN_ID:
        CP TOKEN_COUNT
        JR NC,DECODE_UNKNOWN
        LD L,A
        LD H,0
        ADD HL,HL
        LD DE,TOKEN_POINTERS
        ADD HL,DE
        LD E,(HL)
        INC HL
        LD D,(HL)
        EX DE,HL
DECODE_WORD:
        LD A,(HL)
        OR A
        JR Z,DECODE_LOOP
        PUSH HL
        CALL DECODE_ONE_CHAR
        POP HL
        INC HL
        JR DECODE_WORD
DECODE_UNKNOWN:
        LD A,1
        LD (TOK_INVALID_FLAG),A
        LD A,'?'
DECODE_RAW:
        CALL DECODE_ONE_CHAR
        JR DECODE_LOOP
DECODE_ONE_CHAR:
        LD (TOK_TMP),A
        LD A,(ENC_QUOTED)
        CP 76
        RET NC
        INC A
        LD (ENC_QUOTED),A
        LD HL,(DECODE_DEST)
        LD A,(TOK_TMP)
        LD (HL),A
        INC HL
        LD (DECODE_DEST),HL
        RET

; Immutable dictionary v1: token values 80h...; keep IDs stable for ROM 2.
; Important: avoid inventing line-number tokens; QBasic source is numberless.
TOKEN_POINTERS:
        DW TK_PRINT,TK_FOR,TK_NEXT,TK_IF,TK_THEN,TK_ELSE,TK_END
        DW TK_DIM,TK_AS,TK_INTEGER,TK_STRING,TK_CLS,TK_INPUT
        DW TK_TO,TK_STEP,TK_WHILE,TK_WEND,TK_DO,TK_LOOP
        DW TK_UNTIL,TK_SUB,TK_FUNCTION,TK_RETURN,TK_SELECT,TK_CASE
        DW TK_EXIT,TK_REM,TK_AND,TK_OR,TK_NOT,TK_MOD,TK_GOTO
        DW TK_GOSUB,TK_RANDOMIZE,TK_RND,TK_LEN,TK_MID
        DW TK_LEFT,TK_RIGHT,TK_CHR,TK_ASC,TK_SLEEP,TK_BEEP
        DW TK_COLOR,TK_LOCATE,TK_OPEN,TK_CLOSE,TK_WRITE
        DW TK_READ,TK_DATA,TK_RESTORE,TK_SHARED,TK_STATIC
        DW TK_BYVAL,TK_CONST,TK_DOUBLE,TK_SINGLE,TK_LONG
        DW TK_REDIM,TK_PRESERVE,TK_OPTION,TK_BASE,TK_ON
        DW TK_ERROR,TK_RESUME,TK_TIMER
        DW TK_QBX_00,TK_QBX_01,TK_QBX_02,TK_QBX_03,TK_QBX_04,TK_QBX_05,TK_QBX_06,TK_QBX_07
        DW TK_QBX_08,TK_QBX_09,TK_QBX_10,TK_QBX_11,TK_QBX_12,TK_QBX_13,TK_QBX_14,TK_QBX_15
        DW TK_QBX_16,TK_QBX_17,TK_QBX_18,TK_QBX_19,TK_QBX_20,TK_QBX_21,TK_QBX_22,TK_QBX_23
        DW TK_QBX_24,TK_QBX_25,TK_QBX_26,TK_QBX_27,TK_QBX_28,TK_QBX_29,TK_QBX_30,TK_QBX_31
        DW TK_QBX_32,TK_QBX_33,TK_QBX_34,TK_QBX_35,TK_QBX_36,TK_QBX_37,TK_QBX_38,TK_QBX_39
        DW TK_QBX_40,TK_QBX_41,TK_QBX_42,TK_QBX_43,TK_QBX_44,TK_QBX_45,TK_QBX_46,TK_QBX_47
        DW TK_QBX_48,TK_QBX_49,TK_QBX_50,TK_QBX_51,TK_QBX_52,TK_QBX_53,TK_QBX_54,TK_QBX_55
        DW TK_QBX_56,TK_QBX_57,TK_QBX_58,TK_QBX_59,TK_QBX_60,TK_QBX_61
        DW TK_EXT_000,TK_EXT_001,TK_EXT_002,TK_EXT_003,TK_EXT_004,TK_EXT_005,TK_EXT_006,TK_EXT_007
        DW TK_EXT_008,TK_EXT_009,TK_EXT_010,TK_EXT_011,TK_EXT_012,TK_EXT_013,TK_EXT_014,TK_EXT_015
        DW TK_EXT_016,TK_EXT_017,TK_EXT_018,TK_EXT_019,TK_EXT_020,TK_EXT_021,TK_EXT_022,TK_EXT_023
        DW TK_EXT_024,TK_EXT_025,TK_EXT_026,TK_EXT_027,TK_EXT_028,TK_EXT_029,TK_EXT_030,TK_EXT_031
        DW TK_EXT_032,TK_EXT_033,TK_EXT_034,TK_EXT_035,TK_EXT_036,TK_EXT_037,TK_EXT_038,TK_EXT_039
        DW TK_EXT_040,TK_EXT_041,TK_EXT_042,TK_EXT_043,TK_EXT_044,TK_EXT_045,TK_EXT_046,TK_EXT_047
        DW TK_EXT_048,TK_EXT_049,TK_EXT_050,TK_EXT_051,TK_EXT_052,TK_EXT_053,TK_EXT_054,TK_EXT_055
        DW TK_EXT_056,TK_EXT_057,TK_EXT_058,TK_EXT_059,TK_EXT_060,TK_EXT_061,TK_EXT_062,TK_EXT_063
        DW TK_EXT_064,TK_EXT_065,TK_EXT_066,TK_EXT_067
TOKEN_COUNT EQU 196
TOKEN_EXT_PREFIX EQU #01
TOKEN_REM EQU 26
TK_PRINT:     DB 'PRINT',0
TK_FOR:       DB 'FOR',0
TK_NEXT:      DB 'NEXT',0
TK_IF:        DB 'IF',0
TK_THEN:      DB 'THEN',0
TK_ELSE:      DB 'ELSE',0
TK_END:       DB 'END',0
TK_DIM:       DB 'DIM',0
TK_AS:        DB 'AS',0
TK_INTEGER:   DB 'INTEGER',0
TK_STRING:    DB 'STRING',0
TK_CLS:       DB 'CLS',0
TK_INPUT:     DB 'INPUT',0
TK_TO:        DB 'TO',0
TK_STEP:      DB 'STEP',0
TK_WHILE:     DB 'WHILE',0
TK_WEND:      DB 'WEND',0
TK_DO:        DB 'DO',0
TK_LOOP:      DB 'LOOP',0
TK_UNTIL:     DB 'UNTIL',0
TK_SUB:       DB 'SUB',0
TK_FUNCTION:  DB 'FUNCTION',0
TK_RETURN:    DB 'RETURN',0
TK_SELECT:    DB 'SELECT',0
TK_CASE:      DB 'CASE',0
TK_EXIT:      DB 'EXIT',0
TK_REM:       DB 'REM',0
TK_AND:       DB 'AND',0
TK_OR:        DB 'OR',0
TK_NOT:       DB 'NOT',0
TK_MOD:       DB 'MOD',0
TK_GOTO:      DB 'GOTO',0
TK_GOSUB:     DB 'GOSUB',0
TK_RANDOMIZE: DB 'RANDOMIZE',0
TK_RND:       DB 'RND',0
TK_LEN:       DB 'LEN',0
TK_MID:       DB 'MID$',0
TK_LEFT:      DB 'LEFT$',0
TK_RIGHT:     DB 'RIGHT$',0
TK_CHR:       DB 'CHR$',0
TK_ASC:       DB 'ASC',0
TK_SLEEP:     DB 'SLEEP',0
TK_BEEP:      DB 'BEEP',0
TK_COLOR:     DB 'COLOR',0
TK_LOCATE:    DB 'LOCATE',0
TK_OPEN:      DB 'OPEN',0
TK_CLOSE:     DB 'CLOSE',0
TK_WRITE:     DB 'WRITE',0
TK_READ:      DB 'READ',0
TK_DATA:      DB 'DATA',0
TK_RESTORE:   DB 'RESTORE',0
TK_SHARED:    DB 'SHARED',0
TK_STATIC:    DB 'STATIC',0
TK_BYVAL:     DB 'BYVAL',0
TK_CONST:     DB 'CONST',0
TK_DOUBLE:    DB 'DOUBLE',0
TK_SINGLE:    DB 'SINGLE',0
TK_LONG:      DB 'LONG',0
TK_REDIM:     DB 'REDIM',0
TK_PRESERVE:  DB 'PRESERVE',0
TK_OPTION:    DB 'OPTION',0
TK_BASE:      DB 'BASE',0
TK_ON:        DB 'ON',0
TK_ERROR:     DB 'ERROR',0
TK_RESUME:    DB 'RESUME',0
TK_TIMER:     DB 'TIMER',0

; Additional QBX 7.1 keywords: IDs C2h..FFh (original 80h..C1h unchanged).
TK_QBX_00: DB 'ABS',0
TK_QBX_01: DB 'SGN',0
TK_QBX_02: DB 'INT',0
TK_QBX_03: DB 'FIX',0
TK_QBX_04: DB 'SQR',0
TK_QBX_05: DB 'SIN',0
TK_QBX_06: DB 'COS',0
TK_QBX_07: DB 'TAN',0
TK_QBX_08: DB 'ATN',0
TK_QBX_09: DB 'LOG',0
TK_QBX_10: DB 'EXP',0
TK_QBX_11: DB 'SCREEN',0
TK_QBX_12: DB 'PSET',0
TK_QBX_13: DB 'PRESET',0
TK_QBX_14: DB 'LINE',0
TK_QBX_15: DB 'CIRCLE',0
TK_QBX_16: DB 'PAINT',0
TK_QBX_17: DB 'DRAW',0
TK_QBX_18: DB 'POINT',0
TK_QBX_19: DB 'VIEW',0
TK_QBX_20: DB 'WINDOW',0
TK_QBX_21: DB 'GET',0
TK_QBX_22: DB 'PUT',0
TK_QBX_23: DB 'PCOPY',0
TK_QBX_24: DB 'INSTR',0
TK_QBX_25: DB 'VAL',0
TK_QBX_26: DB 'STR$',0
TK_QBX_27: DB 'STRING$',0
TK_QBX_28: DB 'SPACE$',0
TK_QBX_29: DB 'UCASE$',0
TK_QBX_30: DB 'LCASE$',0
TK_QBX_31: DB 'LTRIM$',0
TK_QBX_32: DB 'RTRIM$',0
TK_QBX_33: DB 'HEX$',0
TK_QBX_34: DB 'OCT$',0
TK_QBX_35: DB 'INKEY$',0
TK_QBX_36: DB 'INPUT$',0
TK_QBX_37: DB 'CSRLIN',0
TK_QBX_38: DB 'POS',0
TK_QBX_39: DB 'WIDTH',0
TK_QBX_40: DB 'KEY',0
TK_QBX_41: DB 'ELSEIF',0
TK_QBX_42: DB 'STOP',0
TK_QBX_43: DB 'SWAP',0
TK_QBX_44: DB 'CALL',0
TK_QBX_45: DB 'DECLARE',0
TK_QBX_46: DB 'TYPE',0
TK_QBX_47: DB 'COMMON',0
TK_QBX_48: DB 'ERASE',0
TK_QBX_49: DB 'LBOUND',0
TK_QBX_50: DB 'UBOUND',0
TK_QBX_51: DB 'PEEK',0
TK_QBX_52: DB 'POKE',0
TK_QBX_53: DB 'SOUND',0
TK_QBX_54: DB 'PLAY',0
TK_QBX_55: DB 'CINT',0
TK_QBX_56: DB 'CLNG',0
TK_QBX_57: DB 'CSNG',0
TK_QBX_58: DB 'CDBL',0
TK_QBX_59: DB 'CSTR',0
TK_QBX_60: DB 'EOF',0
TK_QBX_61: DB 'LOF',0

; B005 extended QBX7.1/CPC words: 01h,index-128; existing tokens unchanged.
TK_EXT_000: DB 'CHDIR',0
TK_EXT_001: DB 'CLEAR',0
TK_EXT_002: DB 'CURDIR$',0
TK_EXT_003: DB 'CURRENCY',0
TK_EXT_004: DB 'CVC',0
TK_EXT_005: DB 'CVD',0
TK_EXT_006: DB 'CVI',0
TK_EXT_007: DB 'CVL',0
TK_EXT_008: DB 'CVS',0
TK_EXT_009: DB 'DATE$',0
TK_EXT_010: DB 'DEF',0
TK_EXT_011: DB 'DEFCUR',0
TK_EXT_012: DB 'DEFDBL',0
TK_EXT_013: DB 'DEFINT',0
TK_EXT_014: DB 'DEFLNG',0
TK_EXT_015: DB 'DEFSNG',0
TK_EXT_016: DB 'DEFSTR',0
TK_EXT_017: DB 'EQV',0
TK_EXT_018: DB 'ERL',0
TK_EXT_019: DB 'ERR',0
TK_EXT_020: DB 'FIELD',0
TK_EXT_021: DB 'FILES',0
TK_EXT_022: DB 'FRE',0
TK_EXT_023: DB 'FREEFILE',0
TK_EXT_024: DB 'IMP',0
TK_EXT_025: DB 'INP',0
TK_EXT_026: DB 'KILL',0
TK_EXT_027: DB 'LET',0
TK_EXT_028: DB 'LIGHTPEN',0
TK_EXT_029: DB 'LOC',0
TK_EXT_030: DB 'LOCK',0
TK_EXT_031: DB 'LSET',0
TK_EXT_032: DB 'MKC$',0
TK_EXT_033: DB 'MKD$',0
TK_EXT_034: DB 'MKDIR',0
TK_EXT_035: DB 'MKI$',0
TK_EXT_036: DB 'MKL$',0
TK_EXT_037: DB 'MKS$',0
TK_EXT_038: DB 'NAME',0
TK_EXT_039: DB 'OUT',0
TK_EXT_040: DB 'PALETTE',0
TK_EXT_041: DB 'PMAP',0
TK_EXT_042: DB 'RESET',0
TK_EXT_043: DB 'RMDIR',0
TK_EXT_044: DB 'RSET',0
TK_EXT_045: DB 'RUN',0
TK_EXT_046: DB 'SADD',0
TK_EXT_047: DB 'SEEK',0
TK_EXT_048: DB 'STICK',0
TK_EXT_049: DB 'STRIG',0
TK_EXT_050: DB 'TIME$',0
TK_EXT_051: DB 'UNLOCK',0
TK_EXT_052: DB 'VARPTR',0
TK_EXT_053: DB 'XOR',0
TK_EXT_054: DB 'SHELL',0
TK_EXT_055: DB 'ENVIRON$',0
TK_EXT_056: DB 'SYSTEM',0
TK_EXT_057: DB 'CHAIN',0
TK_EXT_058: DB 'BLOAD',0
TK_EXT_059: DB 'BSAVE',0
TK_EXT_060: DB 'MODE',0
TK_EXT_061: DB 'INK',0
TK_EXT_062: DB 'BORDER',0
TK_EXT_063: DB 'PEN',0
TK_EXT_064: DB 'PAPER',0
TK_EXT_065: DB 'PLOT',0
TK_EXT_066: DB 'SYMBOL',0
TK_EXT_067: DB 'TAG',0
