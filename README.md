# Loam DaBoom

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
playing. **M** mutes a row. The **KIT** menu (808, 909, Lo-Fi) resets every
voice's sound without touching the patterns, and **MASTER** has volume,
drive and reverb. Patterns and sounds are remembered in the browser.

And the little egret next to the name has something to say.

## License

GNU AGPLv3, like the rest of the repo.
