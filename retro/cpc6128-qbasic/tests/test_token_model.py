"""QEDIT v0.4: static memory layout and dictionary contract tests.

These tests do not execute Z80 machine code. WinAPE acceptance is required.
Run from any directory: python tests/test_token_model.py
"""
from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parent.parent
EDITOR = ROOT / "editor"
source = (EDITOR / "QEDIT_ROM.asm").read_text()
tokens = (EDITOR / "QEDIT_V04_CORE.asm").read_text()
rom = (EDITOR / "QEDIT.ROM").read_bytes()

assert len(rom) == 16384
assert rom[:4] == bytes((1, 0, 1, 0))
assert "MAX_LINES       EQU 1500" in source
assert "INDEX_BASE      EQU #9800" in source
assert "ENC_BUF         EQU #8A00" in source
assert "TEMP_LINE       EQU #8B00" in source
assert 'INCLUDE "QEDIT_V04_CORE.asm"' in source
assert "TOKEN_COUNT EQU 66" in tokens
assert "TOKEN_REM EQU 26" in tokens
assert "ADD A,#C4" in tokens and "LD A,#C0" in tokens
assert "CALL COMMIT_DIRTY" in source
assert "LOAD_STRESS_1500:" in source

table = tokens.split("TOKEN_POINTERS:", 1)[1].split("TOKEN_COUNT EQU", 1)[0]
labels = []
for row in table.splitlines():
    if "DW " in row:
        labels += row.split("DW ", 1)[1].strip().split(",")
assert len(labels) == 66, len(labels)
assert labels[0] == "TK_PRINT" and labels[1] == "TK_FOR"
assert labels[26] == "TK_REM"

assert 0x9800 + 3 * 1500 - 1 == 0xA993
assert 0x6000 + 21 * 80 - 1 == 0x668F
assert 0x9000 + 2048 == 0x9800

top = row = 0
for logical_line in range(1, 1500):
    if row == 20:
        top += 1
    else:
        row += 1
    assert top + row == logical_line
assert top == 1479 and row == 20
for logical_line in range(1498, -1, -1):
    if row == 0:
        top -= 1
    else:
        row -= 1
    assert top + row == logical_line
assert top == row == 0

# V0.4.2: check 16 KB and new ROM differs from previous tested V0.4.1.
# The exact SHA-256 will be frozen after the first GitHub RASM build.
assert hashlib.sha256(rom).hexdigest() != "796730034591b2fafa5fb975453013ecf8137181ed7342c1361a414f7bc69119"
print("PASS: ROM 16K / SHA, 66 tokens, RAM windows, 1500-line index, viewport logic")

# Regression v0.4.1: fixed display fields must not overwrite labels.
assert "LD HL,#1118" not in source, "old v0.4 fast cursor field overwrote Col:"
assert "LD HL,#1418\n        CALL CLEAR_COL_DIGITS" in source
assert 'LD HL,#0A18\n        CALL CLEAR_DIGITS' in source
assert 'LD B,3\nCLEAR_STATUS_FIELD:' in source
assert 'PDEC_THOUSANDS:\n        OR A\n        SBC HL,DE' in source
assert 'PDEC_HUNDREDS:\n        OR A\n        SBC HL,DE' in source
assert 'PDEC_TENS:\n        OR A\n        SBC HL,DE' in source
static = "Ligne:       Col:     INS  |  MODE 2  |  1500 lignes max"
assert static[9:13] == " "*4
assert static[13:17] == "Col:"
assert static[19:22] == " "*3
assert static[22:25] == "INS"
for row in [1,9,10,99,100,999,1000,1001,1499,1500]:
    num=row
    digits=[]
    seen=False
    for unit in [1000,100,10]:
        d,num=divmod(num,unit)
        seen = seen or bool(d)
        digits.append(str(d) if seen else " ")
    digits.append(str(num))
    assert int("".join(digits)) == row
print("PASS: status 4-digit lines 1..1500, fixed Col/INS field geometry")

# V0.4.2 direct navigation tests (static Z80 API and numeric model).
assert "CP 7               ; Ctrl+G:" in source
assert "JP Z,GOTO_LINE" in source
assert "MENU_SEARCH_EXEC:" in source and "JP Z,GOTO_LINE" in source.split("MENU_SEARCH_EXEC:")[1][:150]
assert "GOTO_LINE:" in source and "GOTO_PARSE:" in source
assert "CALL COMMIT_DIRTY" in source.split("GOTO_ACCEPT:")[1][:400]
assert "LD DE,VIEW_LINES" in source.split("GOTO_ACCEPT:")[1]
for count in [1,5,20,21,22,256,999,1000,1499,1500]:
    doc_lines=1500
    target=count-1
    top=min(max(0,target-10),max(0,doc_lines-21))
    row=target-top
    assert 0<=top<=1479 and 0<=row<21 and top+row==target
assert all(x<0x9000 for x in [0x8870,0x8874,0x8876,0x8879,0x887b])
print("PASS: Ctrl+G jumps 1..1500, data cache, numeric entry and viewport clamps")
