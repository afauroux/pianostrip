#!/usr/bin/env python3
"""
midi_to_note_array.py

Reads a MIDI file and converts it into a time-quantized array of symbols:
- "." for silence
- "C4", "F#3" for single notes
- "C4+E4+G4" for chords (multiple notes starting in the same time slot)

Quantization default: 16th-note grid (steps_per_beat=4).
"""

from __future__ import annotations

import argparse
from collections import defaultdict
from typing import List, Dict, Tuple, Set

import pretty_midi


NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def midi_pitch_to_name(pitch: int) -> str:
    """Convert MIDI pitch (0-127) to scientific pitch notation (e.g., 60 -> C4)."""
    name = NOTE_NAMES[pitch % 12]
    octave = (pitch // 12) - 1
    return f"{name}{octave}"


def choose_best_instrument(pm: pretty_midi.PrettyMIDI) -> int:
    """
    Pick the "main" instrument track:
    - prefer non-drum
    - prefer the one with most notes
    Returns index in pm.instruments.
    """
    candidates = [
        (i, inst) for i, inst in enumerate(pm.instruments) if not inst.is_drum
    ]
    if not candidates:
        return 0 if pm.instruments else -1
    return max(candidates, key=lambda x: len(x[1].notes))[0]


def quantize_time_to_step(t: float, step_s: float) -> int:
    """Round to nearest step index."""
    return int(round(t / step_s))


def midi_to_symbol_array(
    midi_path: str,
    *,
    steps_per_beat: int = 4,  # 4 => 16th notes
    symbol_silence: str = " ",
    instrument_index: int | None = None,
    mode: str = "onset",  # "onset" or "sustain"
    chord_join: str = "+",
) -> Tuple[List[str], float]:
    """
    Convert MIDI to a symbol array.

    mode:
      - "onset": mark only when notes start; other steps are silence unless new onset happens
      - "sustain": mark every step while note is held (good for piano-roll-like sequences)

    Returns: (symbols, step_duration_seconds)
    """
    pm = pretty_midi.PrettyMIDI(midi_path)

    if not pm.instruments:
        return [], 0.0

    if instrument_index is None:
        instrument_index = choose_best_instrument(pm)
    if instrument_index < 0 or instrument_index >= len(pm.instruments):
        raise ValueError(f"Invalid instrument_index={instrument_index}")

    inst = pm.instruments[instrument_index]

    # Determine step duration from estimated tempo:
    # beat duration = 60 / BPM
    # step duration = beat duration / steps_per_beat
    # If multiple tempi exist, we use pretty_midi's estimate for a practical grid.
    bpm = pm.estimate_tempo()
    beat_s = 60.0 / bpm
    step_s = beat_s / steps_per_beat

    # Collect events into steps
    # step -> set(note_names) to allow chords
    step_notes: Dict[int, Set[str]] = defaultdict(set)

    max_step = 0

    if mode not in ("onset", "sustain"):
        raise ValueError("mode must be 'onset' or 'sustain'")

    for n in inst.notes:
        start_step = quantize_time_to_step(n.start, step_s)
        end_step = quantize_time_to_step(n.end, step_s)
        note_name = midi_pitch_to_name(n.pitch)

        if mode == "onset":
            step_notes[start_step].add(note_name)
            max_step = max(max_step, start_step)
        else:  # sustain
            if end_step <= start_step:
                end_step = start_step + 1
            for s in range(start_step, end_step):
                step_notes[s].add(note_name)
            max_step = max(max_step, end_step - 1)

    symbols: List[str] = []
    for s in range(max_step + 1):
        notes_here = step_notes.get(s, set())
        if not notes_here:
            symbols.append(symbol_silence)
        else:
            # sort by pitch class+octave order by converting back from name
            # (simple lex sort works OK for "C#4" etc, but we'll do a safer pitch sort)
            def name_to_pitch(nn: str) -> int:
                # parse like "F#3" or "C4"
                # last char(s) are octave (can be -1, 0..9). handle "-1" too.
                # Find the first digit or '-' from the end.
                i = len(nn) - 1
                while i >= 0 and (nn[i].isdigit() or nn[i] == "-"):
                    i -= 1
                note_part = nn[: i + 1]
                octave_part = nn[i + 1 :]
                octave = int(octave_part)
                pc = NOTE_NAMES.index(note_part)
                return (octave + 1) * 12 + pc

            ordered = sorted(notes_here, key=name_to_pitch)
            symbols.append(chord_join.join(ordered))

    return symbols, step_s


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("midi", help="Path to .mid/.midi file")
    ap.add_argument(
        "--steps-per-beat", type=int, default=4, help="4=16th notes, 2=8th, 1=quarter"
    )
    ap.add_argument("--mode", choices=["onset", "sustain"], default="onset")
    ap.add_argument(
        "--instrument-index",
        type=int,
        default=None,
        help="Which track to use (default: auto best)",
    )
    ap.add_argument("--silence", default=" ", help="Symbol to use for silence")
    ap.add_argument("--join", default="+", help="Joiner for chords")
    ap.add_argument(
        "--print", dest="do_print", action="store_true", help="Print the array"
    )
    ap.add_argument(
        "--out", default=None, help="Optional output .txt file (one symbol per step)"
    )
    args = ap.parse_args()

    symbols, step_s = midi_to_symbol_array(
        args.midi,
        steps_per_beat=args.steps_per_beat,
        instrument_index=args.instrument_index,
        mode=args.mode,
        symbol_silence=args.silence,
        chord_join=args.join,
    )

    header = f"# steps={len(symbols)} step_s={step_s:.6f} mode={args.mode} steps_per_beat={args.steps_per_beat}"
    if args.do_print:
        print(header)
        print(symbols)

    i=0
    j=0;
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(header + "\n")
            w = 20
            while w*j+i<len(symbols):
                i=0
                for i in range(w): 
                    f.write(symbols[j*w+i] + ",")
                j+=1
                f.write("\n")
            f.write("\n")


if __name__ == "__main__":
    main()
