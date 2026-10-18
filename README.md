# Loam DaBoom

**[Play it in your browser](https://vox4vulpes.github.io/loam-daboom/)** ·
**[Download the VST3 plugin and standalone app](https://github.com/vox4vulpes/loam-daboom/releases/latest)** ·
part of the [Loam Synth Bundle](https://github.com/vox4vulpes/loam-synth-vst)

小鷺 · Rhythm Composer — an eight-voice drum machine in the spirit of the
TR-808 and TR-909, running entirely in the browser via the Web Audio API.
Every sound is synthesised; there are no samples.

Open `index.html` in any modern browser to play it.

## Voices

| Voice | Knobs |
|---|---|
| Kick | Tune, Decay, Punch (beater click), Level |
| Snare | Tune, Decay, Snappy (noise), Level |
| Clap | Tone, Decay, Spread (between the slaps), Level |
| Closed hat | Tone, Decay, Level — chokes the open hat |
| Open hat | Tone, Decay, Level |
| Tom | Tune, Decay, Level |
| Rim | Tune, Decay, Level |
| Cowbell | Tune, Decay, Level |

Hats and cowbell are built the 808 way, from square waves at inharmonic
ratios through band-pass filters.

## Playing it

Flip the power switch, then hit the pads or the keys `Z X C V B N M ,`
(matched by position, so any keyboard layout works). A MIDI keyboard works
too in Chrome and Edge, on General MIDI drum notes (C1 kick, D1 snare,
F#1 closed hat...).

## Sequencer

Sixteen steps per voice. Click a step once for a hit, again for an accent,
again to clear. **PLAY** (or `Space`) runs it at **BPM** with optional
**SWING**; four patterns, **A–D**, switch at the end of the bar while
playing. **M** mutes a row. **MASTER** has volume, drive and reverb.

## Pattern library

**LIBRARY** loads a groove into the pattern you're on (A–D): factory ones —
House, Boom Bap, Breakbeat, Trap, Techno, Garage, Lazy Groove, Half-Time —
or your own. To keep a pattern, type a name and **SAVE**; **DELETE** removes
one of yours. Grooves are saved separately from kits, so any groove plays on
any kit.

## Kits

The **KIT** menu loads a factory kit (808, 909, Lo-Fi) or one of yours,
without touching the patterns. To build your own, pick each voice's version
from its name above the pad — say the 909 kick with the Lo-Fi snare and the
808 hats — tweak the knobs, type a name and **SAVE**. Kits hold sounds only.
Kits, grooves and your current patterns are remembered in the browser.

And the little egret next to the name has something to say.

## VST3 plugin

[`vst/`](vst/) is the same drum machine as a VST3 plugin and standalone app,
built with JUCE: the same voices, kits, pattern library and panel, and the same
sound — every voice is synthesised from the web version's formulas (down to
Chrome's band-limited square waves), checked against it level for level.

- **In a DAW** the sequencer plays along with the transport, in its tempo, while
  **PLAY** is lit (it is when the plugin loads); the BPM knob dims, since the DAW
  sets the tempo. Patterns A–D, the kit and PLAY are saved with the project.
- **Standalone**, PLAY (or `Space`) runs the sequencer at **BPM**.
- MIDI notes play the voices on General MIDI drum notes; every knob, mute,
  BPM, swing and the master are automatable parameters.
- Your kits and grooves are files under your user app-data folder, in
  `Loam/Loam DaBoom/Kits` and `Loam/Loam DaBoom/Grooves`.

To build it yourself (C++17; on Linux a few dev packages, see
[`.github/workflows/build-vst.yml`](.github/workflows/build-vst.yml)):

```sh
cmake -S vst -B vst/build -DCMAKE_BUILD_TYPE=Release
cmake --build vst/build --config Release
```

`vst/Tests/EngineTest.cpp` checks the sound engine and the sequencer clocks
without any GUI dependencies (`-DLOAM_BUILD_PLUGIN=OFF`).

## Releases

What changed in each version is in [CHANGELOG.md](CHANGELOG.md). To cut a
release, add a `## [x.y.z]` section there and push a version tag; the
[Release workflow](.github/workflows/release.yml) builds the VST3 plugin on
Windows, macOS and Linux and publishes the zips and the installer.

## License

[GNU AGPLv3](LICENSE). Copyright (c) 2026 vox4vulpes
