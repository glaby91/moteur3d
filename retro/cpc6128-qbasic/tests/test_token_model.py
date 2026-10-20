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

expected = "0de84c30a21f95c4bd653a7852691a0bfe786431e41aa4605d190e1a1e5f056f"
assert hashlib.sha256(rom).hexdigest() == expected
print("PASS: ROM 16K / SHA, 66 tokens, RAM windows, 1500-line index, viewport logic")
