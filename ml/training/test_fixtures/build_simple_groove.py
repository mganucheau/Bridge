#!/usr/bin/env python3
"""Write simple_groove.mid (1 bar 4/4 drum groove). Run: python build_simple_groove.py"""
from __future__ import annotations

import io
import os
import struct


def _write_varlen(bio: io.BytesIO, value: int) -> None:
    arr = bytearray()
    arr.append(value & 0x7F)
    value >>= 7
    while value > 0:
        arr.insert(0, (value & 0x7F) | 0x80)
        value >>= 7
    bio.write(arr)


def _midi_track_simple_groove(ticks_per_quarter: int = 480) -> bytes:
    bio = io.BytesIO()
    us_per_quarter = 500_000  # 120 BPM
    bio.write(b"\x00\xff\x51\x03")
    bio.write(bytes([(us_per_quarter >> 16) & 0xFF, (us_per_quarter >> 8) & 0xFF, us_per_quarter & 0xFF]))
    bio.write(b"\x00\xff\x58\x04\x04\x02\x18\x08")  # 4/4

    sixteenth = ticks_per_quarter // 4
    ch_drum = 0x99  # channel 10
    t_cursor = 0
    events: list[tuple[int, bytes]] = []

    def add_note_abs_tick(tick: int, pitch: int, vel: int, dur: int) -> None:
        events.append((tick, bytes([ch_drum, pitch & 0x7F, vel & 0x7F])))
        events.append((tick + dur, bytes([0x89, pitch & 0x7F, 0])))

    for i in range(16):
        tick = i * sixteenth
        add_note_abs_tick(tick, 42, 70 if i % 2 == 0 else 55, sixteenth - 1)
        if i in (0, 5, 10, 15):
            add_note_abs_tick(tick, 36, 100, sixteenth - 1)
        if i in (4, 12):
            add_note_abs_tick(tick, 38, 95, sixteenth - 1)

    events.sort(key=lambda x: x[0])
    prev = 0
    for tick, msg in events:
        delta = tick - prev
        _write_varlen(bio, delta)
        bio.write(msg)
        prev = tick

    bar_end = 16 * sixteenth
    if prev < bar_end:
        _write_varlen(bio, bar_end - prev)
    bio.write(b"\x00\xff\x2f\x00")
    return bio.getvalue()


def main() -> None:
    here = os.path.dirname(os.path.abspath(__file__))
    out = os.path.join(here, "simple_groove.mid")
    tq = 480
    track = _midi_track_simple_groove(tq)
    with open(out, "wb") as f:
        f.write(b"MThd" + struct.pack(">IHHH", 6, 0, 1, tq))
        f.write(b"MTrk" + struct.pack(">I", len(track)) + track)
    print("wrote", out, os.path.getsize(out), "bytes")


if __name__ == "__main__":
    main()
