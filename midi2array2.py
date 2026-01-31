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
import os
import re
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


def sanitize_name(name: str) -> str:
    cleaned = re.sub(r"[^a-zA-Z0-9_]+", "_", name.strip())
    cleaned = cleaned.strip("_")
    return cleaned or "Song"


def emit_header(songs: List[Tuple[str, List[str], float]], header_path: str) -> None:
    lines: List[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("#include <avr/pgmspace.h>")
    lines.append("")
    lines.append("struct Song {")
    lines.append("  const char* name;")
    lines.append("  const char* const* steps;")
    lines.append("  size_t stepCount;")
    lines.append("  float stepSeconds;")
    lines.append("};")
    lines.append("")

    for song_name, symbols, step_s in songs:
        array_name = f"kSong_{sanitize_name(song_name)}"
        for idx, symbol in enumerate(symbols):
            token = symbol.replace("\"", "\\\"")
            lines.append(f"static const char {array_name}_{idx}[] PROGMEM = \"{token}\";")
        lines.append("")
        lines.append(f"static const char* const {array_name}[] PROGMEM = {{")
        for idx, _symbol in enumerate(symbols):
            lines.append(f"  {array_name}_{idx},")
        lines.append("};")
        lines.append("")

    for song_name, _symbols, _step_s in songs:
        name_id = f"kSongName_{sanitize_name(song_name)}"
        lines.append(f"static const char {name_id}[] PROGMEM = \"{song_name}\";")
    lines.append("")

    lines.append("static const Song kSongs[] = {")
    for song_name, symbols, step_s in songs:
        array_name = f"kSong_{sanitize_name(song_name)}"
        name_id = f"kSongName_{sanitize_name(song_name)}"
        lines.append("  {")
        lines.append(f"    {name_id},")
        lines.append(f"    {array_name},")
        lines.append(f"    sizeof({array_name}) / sizeof({array_name}[0]),")
        lines.append(f"    {step_s:.6f}f")
        lines.append("  },")
    lines.append("};")
    lines.append("")
    lines.append("static const size_t kSongCount = sizeof(kSongs) / sizeof(kSongs[0]);")
    lines.append("")

    with open(header_path, \"w\", encoding=\"utf-8\") as f:
        f.write(\"\\n\".join(lines))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("midi", nargs="+", help="Path(s) to .mid/.midi file(s)")
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
    ap.add_argument(
        "--out-header",
        default=None,
        help="Optional output .h file with arrays and a kSongs meta array",
    )
    ap.add_argument(
        "--song-name",
        action="append",
        default=None,
        help="Override song name (repeatable, matches order of MIDI inputs)",
    )
    args = ap.parse_args()

    songs: List[Tuple[str, List[str], float]] = []

    for index, midi_path in enumerate(args.midi):
        symbols, step_s = midi_to_symbol_array(
            midi_path,
            steps_per_beat=args.steps_per_beat,
            instrument_index=args.instrument_index,
            mode=args.mode,
            symbol_silence=args.silence,
            chord_join=args.join,
        )
        if args.song_name and index < len(args.song_name):
            song_name = args.song_name[index]
        else:
            song_name = os.path.splitext(os.path.basename(midi_path))[0]
        songs.append((song_name, symbols, step_s))

        header = (
            f"# steps={len(symbols)} step_s={step_s:.6f} mode={args.mode} "
            f"steps_per_beat={args.steps_per_beat}"
        )
        if args.do_print:
            print(header)
            print(symbols)

        if args.out:
            base = os.path.splitext(os.path.basename(midi_path))[0]
            txt_path = args.out
            if len(args.midi) > 1:
                root, ext = os.path.splitext(args.out)
                txt_path = f\"{root}_{sanitize_name(base)}{ext or '.txt'}\"
            with open(txt_path, "w", encoding="utf-8") as f:
                f.write(header + "\n")
                w = 20
                i = 0
                j = 0
                while w * j + i < len(symbols):
                    i = 0
                    for i in range(w):
                        idx = j * w + i
                        if idx >= len(symbols):
                            break
                        f.write(symbols[idx] + ",")
                    j += 1
                    f.write("\n")
                f.write("\n")

    if args.out_header:
        emit_header(songs, args.out_header)


if __name__ == "__main__":
    main()
