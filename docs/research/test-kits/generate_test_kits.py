#!/usr/bin/env python3
"""Generate Nord Drum 3P test kit presets for song-load debugging."""

from __future__ import annotations

from pathlib import Path

KIT_HEADER = """<?xml version="1.0" encoding="UTF-8"?>
<kit
\tfirmwareVersion="c1.6.0"
\tearliestCompatibleFirmware="4.1.0-alpha"
\tmodFXCurrentParam="feedback"
\tcurrentFilterType="lpf"
\tmodFXType="none"
\tlpfMode="24dB"
\thpfMode="HPLadder"
\tfilterRoute="H2L">
\t<defaultParams
\t\treverbAmount="0x80000000"
\t\tvolume="0x00000000"
\t\tpan="0x00000000"
\t\tsidechainCompressorShape="0xDC28F5B2"
\t\tmodFXDepth="0x00000000"
\t\tmodFXRate="0xE0000000"
\t\tstutterRate="0x00000000"
\t\tsampleRateReduction="0x80000000"
\t\tbitCrush="0x80000000"
\t\tmodFXOffset="0x00000000"
\t\tmodFXFeedback="0x80000000"
\t\tcompressorThreshold="0x00000000"
\t\tarpeggiatorGate="0x7FFFFFFF"
\t\ttempo="0x00000000">
\t\t<delay rate="0x00000000" feedback="0x80000000" />
\t\t<lpf frequency="0x7FFFFFFF" resonance="0x80000000" />
\t\t<hpf frequency="0x80000000" resonance="0x80000000" />
\t\t<equalizer bass="0x00000000" treble="0x00000000" bassFrequency="0x00000000" trebleFrequency="0x00000000" />
\t</defaultParams>
\t<delay pingPong="1" analog="0" syncLevel="7" syncType="0" />
\t<sidechain attack="327244" release="936" syncLevel="6" syncType="0" />
\t<audioCompressor attack="83886080" release="83886080" thresh="0" ratio="1073741824" compHPF="0" compBlend="2147483647" />
\t<stutter quantized="1" reverse="0" pingPong="0" />
\t<soundSources>
"""

KIT_FOOTER = """\t</soundSources>
\t<selectedDrumIndex>0</selectedDrumIndex>
</kit>
"""

MODKNOBS = [
    10,
    7,
    53,
    52,
    21,
    50,
    47,
    44,
    48,
    12,
    30,
    31,
    14,
    17,
    18,
    19,
]

DEF_PATH = "MIDI_DEVICES/DEFINITION/Nord Drum 3P.xml"

INLINE_CCLABELS = """\t\t\t<ccLabels
\t\t\t\t0="Bank Select MSB"
\t\t\t\t7="Level"
\t\t\t\t10="PAN"
\t\t\t\t12="Reverb Type"
\t\t\t\t14="Noise Filter Frequency"
\t\t\t\t17="Noise Filter Resonance"
\t\t\t\t18="Noise Level"
\t\t\t\t19="Tone Level"
\t\t\t\t21="Noise Decay"
\t\t\t\t30="Tone Spectra"
\t\t\t\t31="Tone Pitch MSB"
\t\t\t\t44="Delay Rate"
\t\t\t\t47="Delay Amount"
\t\t\t\t48="Reverb Amount"
\t\t\t\t50="Tone Decay"
\t\t\t\t52="Tone Freq"
\t\t\t\t53="Tone DYN Filter"
\t\t\t\t70="Channel Select" />"""

OUTPUT = 'outputDevice="2" outputDeviceName="H4MIDI-WC port 1"'


def mod_knobs_block(indent: str = "\t\t") -> str:
    lines = [f"{indent}<modKnobs>"]
    for cc in MODKNOBS:
        lines.append(f'{indent}\t<modKnob cc="{cc}" />')
    lines.append(f"{indent}</modKnobs>")
    return "\n".join(lines)


def definition_block(indent: str = "\t\t") -> str:
    return (
        f"{indent}<midiDevice>\n"
        f'{indent}\t<definitionFile name="{DEF_PATH}" />\n'
        f"{indent}</midiDevice>"
    )


def inline_labels_block(indent: str = "\t\t") -> str:
    return f"{indent}<midiDevice>\n{INLINE_CCLABELS}\n{indent}</midiDevice>"


def drum_row(
    channel: int,
    *,
    mod_knobs: bool = False,
    definition: bool = False,
    inline_labels: bool = False,
    minimal: bool = False,
) -> str:
    attrs = f'channel="{channel}" note="0" {OUTPUT}'
    if minimal:
        return f"\t\t<midiOutput {attrs} />"

    parts = [f"\t\t<midiOutput {attrs}>"]
    if mod_knobs:
        parts.append(mod_knobs_block())
    if definition:
        parts.append(definition_block())
    elif inline_labels:
        parts.append(inline_labels_block())
    parts.append("\t\t</midiOutput>")
    return "\n".join(parts)


def build_kit(rows: list[str]) -> str:
    return KIT_HEADER + "\n".join(rows) + "\n" + KIT_FOOTER


PRESETS: dict[str, tuple[str, list[str]]] = {
    "ND3P Test Minimal.XML": (
        "6 bare MIDI rows — no modKnobs, no definition. Baseline row count.",
        [drum_row(ch, minimal=True) for ch in range(6)],
    ),
    "ND3P Test 2Row.XML": (
        "Only 2 rows (ch 0–1). Simplest multi-row case.",
        [drum_row(ch, minimal=True) for ch in range(2)],
    ),
    "ND3P Test All Meta.XML": (
        "Correct format: modKnobs + definitionFile on every row.",
        [drum_row(ch, mod_knobs=True, definition=True) for ch in range(6)],
    ),
    "ND3P Test Row0 Meta.XML": (
        "Metadata only on row 0 — tests load-time propagation from first drum.",
        [drum_row(0, mod_knobs=True, definition=True)]
        + [drum_row(ch, minimal=False) for ch in range(1, 6)],
    ),
    "ND3P Test Row5 Meta.XML": (
        "Metadata only on row 5 — simulates old broken song-save format.",
        [drum_row(ch, minimal=False) for ch in range(5)]
        + [drum_row(5, mod_knobs=True, definition=True)],
    ),
    "ND3P Test Inline Labels.XML": (
        "Inline ccLabels on row 0 only, modKnobs on all rows, no definition file.",
        [drum_row(0, mod_knobs=True, inline_labels=True)]
        + [drum_row(ch, mod_knobs=True) for ch in range(1, 6)],
    ),
    "ND3P Test DIN.XML": (
        "6 rows via DIN ports instead of H4 — rules out USB port matching.",
        [
            f'\t\t<midiOutput channel="{ch}" note="0" outputDevice="1" outputDeviceName="DIN ports" />'
            for ch in range(6)
        ],
    ),
}


def main() -> None:
    repo_dir = Path(__file__).parent
    sd_dir = Path("/Volumes/DELUGE/KITS/_CG/External")
    sd_ok = sd_dir.is_dir()

    readme_lines = [
        "# Nord Drum 3P test kit presets",
        "",
        "Generated by `generate_test_kits.py`. Copy to SD: `KITS/_CG/External/`.",
        "",
        "## How to use",
        "",
        "1. Load a test kit preset on the Deluge.",
        "2. Confirm row count + CC names in kit view.",
        "3. Save a new song (e.g. `SONGS/ND3P-Test-<name>.XML`).",
        "4. Reload the song — note which presets survive save/load.",
        "",
        "| Preset | Purpose |",
        "|--------|---------|",
    ]

    for filename, (purpose, rows) in PRESETS.items():
        content = build_kit(rows)
        (repo_dir / filename).write_text(content)
        if sd_ok:
            (sd_dir / filename).write_text(content)
        readme_lines.append(f"| `{filename}` | {purpose} |")
        print(f"Wrote {filename} ({len(rows)} drums)")

    readme_lines.extend(
        [
            "",
            "## Expected results (hypothesis)",
            "",
            "- **Minimal / 2Row**: If song load fails here too → row-count bug unrelated to modKnobs/definition.",
            "- **All Meta**: Should match production `Nord Drum 3P.XML` behaviour.",
            "- **Row0 / Row5 Meta**: Tests `propagateSharedSettingsAcrossKit()` on load.",
            "- **Inline Labels**: Tests path without SD definition file.",
            "- **DIN**: Tests output-device matching in `claimOutput`.",
            "",
            "Regenerate: `python3 docs/research/test-kits/generate_test_kits.py`",
        ]
    )
    (repo_dir / "README.md").write_text("\n".join(readme_lines) + "\n")

    if sd_ok:
        print(f"\nAlso copied to {sd_dir}")
    else:
        print("\nSD not mounted — repo copies only")


if __name__ == "__main__":
    main()
