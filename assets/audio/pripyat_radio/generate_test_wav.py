#!/usr/bin/env python3
"""Generate an ORIGINAL 30s test tone + radio noise WAV for the Windows sound player.

No external dependencies; this file is a technical placeholder, NOT a Ukrainian
broadcast or a licensed song. Production audio remains a separate optional asset.
"""
import argparse
import math
import os
import struct
import wave

RATE = 16000
SECONDS = 30

def generate(target):
    os.makedirs(os.path.dirname(os.path.abspath(target)), exist_ok=True)
    # Stable pseudorandom radio hiss; avoid nondeterministic test artifacts.
    seed = 0x72616469
    phase1 = phase2 = 0.0
    with wave.open(target, "wb") as wav:
        wav.setnchannels(1)
        wav.setsampwidth(2)
        wav.setframerate(RATE)
        block = bytearray()
        for i in range(RATE * SECONDS):
            t = i / RATE
            seed = (1664525 * seed + 1013904223) & 0xffffffff
            noise = ((seed >> 16) / 32768.0) - 1.0
            # Gentle test signal, not a reconstruction of a real station.
            envelope = 0.5 + 0.5 * math.sin(t * 0.31)
            active = (int(t) % 9) < 6
            pitch = 310.0 + 35.0 * math.sin(t * 0.6)
            phase1 += 2.0 * math.pi * pitch / RATE
            phase2 += 2.0 * math.pi * 490.0 / RATE
            signal = (0.08 * math.sin(phase1) + 0.027 * math.sin(phase2)) * (1.0 if active else 0.0)
            sample = max(-0.8, min(0.8, 0.035 * noise * envelope + signal))
            block.extend(struct.pack("<h", round(sample * 32767)))
            if len(block) >= 32768:
                wav.writeframesraw(block)
                block.clear()
        if block:
            wav.writeframesraw(block)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", help="Output WAV path")
    args = parser.parse_args()
    generate(args.output)
    print(args.output)
