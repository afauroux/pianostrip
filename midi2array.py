#!/usr/bin/env python3
"""
midi_to_note_array.py

Convert a MIDI file into a time-quantized array of note names:
- Each time step contains either "SIL" or a note like "C4", "F#3".
- Uses a fixed grid (e.g., 16th notes or 10 ms).
- Handles tempo changes via midi ticks -> seconds conversion.
- Handles overlaps with a selectable policy: "loudest", "highest", "lowest", "first".

Dependencies:
  pip install mido python-rtmidi
(You don't strictly need python-rtmidi unless you also do realtime I/O.)

Usage:
  python midi_to_note_array.py input.mid --grid 16th --policy highest
  python midi_to_note_array.py input.mid --grid-ms 10 --policy loudest
"""

from __future__ import annotations

import argparse
import math
from dataclasses import dataclass
from typing import Dict, List, Tuple, Optional

import mido


NOTE_NAMES_SHARP = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def midi_note_to_name(note: int) -> str:
    """
    MIDI note number -> name like C4, F#3.
    MIDI standard: note 60 = C4.
    octave = (note // 12) - 1
    """
    if not (0 <= note <= 127):
        raise ValueError(f"Invalid MIDI note number: {note}")
    name = NOTE_NAMES_SHARP[note % 12]
    octave = (note // 12) - 1
    return f"{name}{octave}"


@dataclass(frozen=True)
class NoteEvent:
    start_s: float
    end_s: float
    note: int
    velocity: int
    channel: int
    track: int


def extract_note_events(mid: mido.MidiFile) -> List[NoteEvent]:
    """
    Parse MIDI into note events with start/end in seconds.
    Uses mido's tick->second conversion with tempo changes.
    """
    ticks_per_beat = mid.ticks_per_beat

    # We'll build a "global" tempo map by iterating over the merged track.
    # But to correctly end notes per track/channel, we also need per-track note-on bookkeeping.
    # Approach:
    # 1) Convert all messages to absolute ticks in *their track*.
    # 2) Collect tempo changes from merged absolute ticks.
    # 3) Provide a function ticks->seconds using the tempo map.
    # 4) Re-walk each track, translate abs ticks to seconds, and match note_on/note_off.

    # Step 1: absolute ticks per track
    track_abs_msgs: List[List[Tuple[int, mido.Message]]] = []
    for ti, track in enumerate(mid.tracks):
        abs_t = 0
        out = []
        for msg in track:
            abs_t += msg.time
            out.append((abs_t, msg))
        track_abs_msgs.append(out)

    # Step 2: gather tempo changes across all tracks (use set_tempo meta)
    tempo_changes: List[Tuple[int, int]] = [(0, 500000)]  # (abs_tick, tempo_us_per_beat) default 120 bpm
    for tr in track_abs_msgs:
        for abs_t, msg in tr:
            if msg.type == "set_tempo":
                tempo_changes.append((abs_t, msg.tempo))

    tempo_changes.sort(key=lambda x: x[0])

    # Remove duplicates at same tick by keeping the last one
    dedup: List[Tuple[int, int]] = []
    for t, tempo in tempo_changes:
        if dedup and dedup[-1][0] == t:
            dedup[-1] = (t, tempo)
        else:
            dedup.append((t, tempo))
    tempo_changes = dedup

    # Precompute cumulative seconds at each tempo change for fast conversion
    # segments: [ (tick_i, tempo_i, sec_at_tick_i), ... ]
    segments: List[Tuple[int, int, float]] = []
    sec_acc = 0.0
    prev_tick, prev_tempo = tempo_changes[0]
    segments.append((prev_tick, prev_tempo, sec_acc))

    for tick, tempo in tempo_changes[1:]:
        dticks = tick - prev_tick
        sec_acc += mido.tick2second(dticks, ticks_per_beat, prev_tempo)
        segments.append((tick, tempo, sec_acc))
        prev_tick, prev_tempo = tick, tempo

    def ticks_to_seconds(abs_tick: int) -> float:
        # Find last segment with tick_i <= abs_tick
        # Linear scan is OK for small maps; for huge maps you can binary search.
        idx = 0
        for i, (t, tempo, sec0) in enumerate(segments):
            if t <= abs_tick:
                idx = i
            else:
                break
        tick0, tempo0, sec0 = segments[idx]
        return sec0 + mido.tick2second(abs_tick - tick0, ticks_per_beat, tempo0)

    # Step 4: collect note events by matching note_on/note_off
    events: List[NoteEvent] = []
    # active[(track, channel, note)] = (start_tick, start_sec, velocity)
    active: Dict[Tuple[int, int, int], Tuple[int, float, int]] = {}

    for ti, tr in enumerate(track_abs_msgs):
        for abs_t, msg in tr:
            if msg.type == "note_on" and msg.velocity > 0:
                start_s = ticks_to_seconds(abs_t)
                active[(ti, msg.channel, msg.note)] = (abs_t, start_s, msg.velocity)

            elif msg.type in ("note_off", "note_on") and (msg.type == "note_off" or msg.velocity == 0):
                key = (ti, msg.channel, msg.note)
                if key in active:
                    start_tick, start_s, vel = active.pop(key)
                    end_s = ticks_to_seconds(abs_t)
                    if end_s > start_s:  # ignore zero/negative
                        events.append(
                            NoteEvent(
                                start_s=start_s,
                                end_s=end_s,
                                note=msg.note,
                                velocity=vel,
                                channel=msg.channel,
                                track=ti,
                            )
                        )

    # If notes are left "hanging", close them at end of file
    # Estimate end time using max tick across all tracks
    max_tick = 0
    for tr in track_abs_msgs:
        if tr:
            max_tick = max(max_tick, tr[-1][0])
    end_file_s = ticks_to_seconds(max_tick)

    for (ti, ch, note), (_start_tick, start_s, vel) in list(active.items()):
        if end_file_s > start_s:
            events.append(
                NoteEvent(
                    start_s=start_s,
                    end_s=end_file_s,
                    note=note,
                    velocity=vel,
                    channel=ch,
                    track=ti,
                )
            )

    # Sort by start time
    events.sort(key=lambda e: (e.start_s, e.end_s, e.track, e.channel, e.note))
    return events


def choose_note(candidates: List[NoteEvent], policy: str) -> Optional[int]:
    """
    Pick one note for a time slice when multiple overlap.
    """
    if not candidates:
        return None

    if policy == "first":
        return candidates[0].note
    if policy == "highest":
        return max(candidates, key=lambda e: e.note).note
    if policy == "lowest":
        return min(candidates, key=lambda e: e.note).note
    if policy == "loudest":
        return max(candidates, key=lambda e: (e.velocity, e.note)).note

    raise ValueError(f"Unknown policy: {policy}")


def grid_step_seconds_from_musical(mid: mido.MidiFile, grid: str) -> float:
    """
    Convert a musical grid like '16th' or '8th' to seconds using the *initial* tempo.
    If the piece has tempo changes, this is still a fixed-time grid.
    """
    # initial tempo (default 120 bpm)
    tempo = 500000
    for tr in mid.tracks:
        for msg in tr:
            if msg.type == "set_tempo":
                tempo = msg.tempo
                break
        else:
            continue
        break

    beat_s = mido.tempo2bpm(tempo)  # careful: tempo2bpm returns bpm, not seconds
    # Let's do correctly: seconds per beat = tempo(us/beat) * 1e-6
    sec_per_beat = tempo * 1e-6

    grid = grid.lower().strip()
    if grid in ("quarter", "1/4"):
        return sec_per_beat
    if grid in ("8th", "eighth", "1/8"):
        return sec_per_beat / 2
    if grid in ("16th", "sixteenth", "1/16"):
        return sec_per_beat / 4
    if grid in ("32nd", "1/32"):
        return sec_per_beat / 8

    raise ValueError(f"Unsupported musical grid: {grid}")


def events_to_array(
    events: List[NoteEvent],
    step_s: float,
    policy: str = "highest",
    silence_token: str = "SIL",
) -> List[str]:
    """
    Convert note events into a time-quantized array.
    Each bin covers [i*step_s, (i+1)*step_s).
    """
    if not events:
        return []

    t_end = max(e.end_s for e in events)
    n = int(math.ceil(t_end / step_s))
    out = [silence_token] * n

    # For each slice, find overlapping events.
    # This is O(N_slices * N_events) if done naively; we'll do a sweep.
    events_sorted = sorted(events, key=lambda e: e.start_s)
    active: List[NoteEvent] = []
    j = 0

    for i in range(n):
        t0 = i * step_s
        t1 = t0 + step_s

        # Add events that start before t1
        while j < len(events_sorted) and events_sorted[j].start_s < t1:
            active.append(events_sorted[j])
            j += 1

        # Remove events that ended at/before t0
        active = [e for e in active if e.end_s > t0]

        # Candidates overlap this bin if they are active now
        note = choose_note(active, policy=policy)
        if note is not None:
            out[i] = midi_note_to_name(note)

    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("midi_file", help="Path to .mid file")
    ap.add_argument("--grid", default=None, help="Musical grid: 16th, 8th, quarter, etc.")
    ap.add_argument("--grid-ms", type=float, default=None, help="Fixed time grid in milliseconds (e.g., 10)")
    ap.add_argument("--policy", default="highest", choices=["highest", "lowest", "loudest", "first"])
    ap.add_argument("--silence", default="SIL", help="Silence token (default: SIL)")
    ap.add_argument("--print", action="store_true", help="Print the resulting array, one per line with time index")
    ap.add_argument("--save", default=None, help="Save output as a text file (one token per line)")
    args = ap.parse_args()

    mid = mido.MidiFile(args.midi_file)

    if (args.grid is None) == (args.grid_ms is None):
        raise SystemExit("Choose exactly one: --grid (musical) OR --grid-ms (fixed time).")

    if args.grid_ms is not None:
        step_s = args.grid_ms / 1000.0
    else:
        step_s = grid_step_seconds_from_musical(mid, args.grid)

    events = extract_note_events(mid)
    arr = events_to_array(events, step_s=step_s, policy=args.policy, silence_token=args.silence)

    if args.print:
        for i, token in enumerate(arr):
            t = i * step_s
            print(f"{t:10.4f}s  {token}")

    if args.save:
        with open(args.save, "w", encoding="utf-8") as f:
            for token in arr:
                f.write(token + "\n")

    if not args.print and not args.save:
        # Default: print a short summary + first 50 tokens
        print(f"Steps: {len(arr)}  step_s={step_s:.6f}  policy={args.policy}")
        print("First 50 tokens:", arr[:50])


if __name__ == "__main__":
    main()
