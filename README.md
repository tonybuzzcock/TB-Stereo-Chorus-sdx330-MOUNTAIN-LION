# TB SDX-330 (Mountain Lion VST)

A model of the **Roland SDX-330 Dimensional Expander** (1994), a 16-bit, 44.1 kHz digital
multi-chorus with 16 algorithms, as a **VST 2.4, 64-bit Intel plugin for Mac OS X 10.8.5**
(Pro Tools 11 through Blue Cat PatchWork). Non-commercial, for personal use.

There is **no schematic**. The sound is a behavioural model built from the Owner's Manual:
each algorithm follows its block diagram and parameter list (chapter 3) and the parameter
descriptions (chapter 4). The front panel follows a photo of the unit
(`Docs/SDX-330_front_panel.png`).

The modern Mac version (CLAP / VST3 / AU) will live in `TB-Stereo-Chorus-sdx330-2026-MAC`.

## The panel

The 19" 1U face (482 x 44 mm, about 11:1) at **half the width and twice the height**
(2.74:1, 1100 x 402), in the unit's own colours: black face, amber SDX-330 lettering, black
knobs, the green LED program display and the yellow-green LCD. The TB electronic logo (red
TB, "electronic" in white) is where the maker's badge was.

| Section | Control | What it does |
|---|---|---|
| | INPUT LEVEL (concentric: inner L, outer R) | 0 to MAX, unity at 12 o'clock, +12 dB fully up |
| | OUTPUT LEVEL | the same |
| L INPUT R | meter | OVER, 0, -6, -12, -18, -24 per side at the converter; MIDI lamp (no MIDI in the plugin) |
| PROGRAM | 3-digit display | program number (SYS in system mode); its dot lights when the settings differ from the stored program |
| | LCD, 17 x 2 | program name and the top-screen parameter, or the edit pages |
| | PROGRAM | play mode: second line shows the algorithm instead of the parameter; elsewhere back to play |
| | FUNC 1-3 | edit mode: pick the parameter in that LCD column |
| | alpha dial, increment / decrement | play: choose the program (1-300); edit: page or value; system: Direct Mix; write: destination |
| | EDIT | parameter edit mode (EDIT and PAGE lamps lit) |
| | PAGE | edit: the dial turns pages again |
| | SYSTEM | system mode: Direct Mix (PROG / ON / OFF), as the unit's system function 11 |
| | EXIT | back to play mode |
| | MEMORY | write: choose a user program 1-200 with the dial, MEMORY again writes it, then type its name and press Return |
| | BYPASS | the effect fades out over about 10 ms, then the input passes untouched. It's this plugin only, not the host's bypass, so PatchWork is unaffected; programs leave it alone |
| | POWER | always on |

Strip: **PARAMETERS** drops a drawer over the face with a knob for every parameter of the
current algorithm (named as on the LCD) and the 3-band EQ, and arrows to change the
algorithm. The unit itself only edits through the LCD.

Knobs: drag up / down (Shift for fine), double-click for the default, scroll wheel. Every
parameter is automatable and saved with the session, so reopening a session brings the
plugin back exactly as it was left.

## Programs

As on the unit: **User Area 1-200** and **Preset Area 201-300**.

- Presets 201-300 are the manual's Preset Name Table, names and algorithms as printed. Roland
  never published their settings, so each one is **voiced by ear from its name and
  algorithm** (e.g. 204 SDD-320 Dimension, 233 SPACE CHORUS, 282 VINTAGE CE-1, 290 MODEL 147).
  Two values are the unit's own: 201 STEREO CHORUS shows Rate 0.65 on the manual's power-on
  screen, 226 SMOOTH 3D CHORUS shows Rate 0.14 in the photo. Retune them against your unit and
  write your own versions to the user area.
- User programs that haven't been written hold a copy of the presets (1-100 and 101-200 both
  copy 201-300), as the unit does after its factory initialization.
- Names are up to **17 characters** (the LCD's width) in the LCD's own ASCII characters. Click
  the LCD while a written user program is showing to rename it; Esc keeps the old name.
- User programs are kept in
  `~/Library/Application Support/TB/TB SDX-330/Programs.txt`, shared by every instance.
- The plugin powers on at 201 STEREO CHORUS, as the unit does.

## The 16 algorithms

| # | Algorithm | Parameters (LCD names) | How it's modelled |
|---|---|---|---|
| 1 | Stereo Chorus | Rate Depth PrDly LFO Phase XMix Pan-L Pan-R | two modulated delays, one LFO with an L/R phase offset, cross mix, pans |
| 2 | Stereo 3D Chorus | Rate Depth PrDly LFO Phase AzmtL ElevL AzmtR ElevR | the stereo chorus, each side placed by the 3D positioner |
| 3 | 3D Panner | Speed Dir Start PrDly | L+R, pre delay, a source circling the listener |
| 4 | Space Chorus | Mode (1 2 3 4 1+4 2+4 3+4) Dirct T.Lvl | after the SDD-320 Dimension D: two delays swept by one triangle LFO in opposite directions, each side's wet fed inverted to the other |
| 5 | 2 Band Chorus | Lo-/Hi- Rate Depth PD LFO Ph Mix, Xover | a chorus per band (4th-order Linkwitz-Riley crossover) |
| 6 | 3 Band Chorus | Lo-/Md-/Hi- ..., LMXov MHXov | the same, three bands |
| 7 | 4 Band Chorus | Lo-/LM-/HM-/Hi- ..., LoxLM LMxHM HMxHi | four bands, mono in, stereo out |
| 8 | Stereo 8 Phase Chorus | Rate LFO, Dpt / PD / Ph / Pol / Mix 1-8 | eight taps per side from one LFO |
| 9 | 16 Phase Chorus | Mode 1-6 Rate Depth PrDly LFO | sixteen taps, LFO phases 1/16 apart, mono in |
| 10 | Detune Chorus | L1-L3, R1-R3 Pit (cents) Pan Lvl, PrDly | six pitch shifters (two crossfaded taps sweeping a long window) |
| 11 | Ensemble | Mode 1-6 | a string-machine ensemble per side: three taps 120 degrees apart, slow + fast LFOs |
| 12 | Wave Chorus | LFO (TRI SIN EXP OCW1-3) Rate Depth PrDly Phase XMix | the stereo chorus with six LFO shapes |
| 13 | Vintage Chorus | Intns Mode 1-4 | after the CE-1: Intensity raises speed and depth together; the manual's four output modes (4 = direct left, effect right) |
| 14 | Stereo Flanger | Rate Depth Manu Resnc LFO (TRI SIN EXP OFW1 OFW2) Phase | Manual sets the delay, the LFO sweeps around it, Resonance feeds back (negative inverts) |
| 15 | Rotary | Speed HFast RFast HSlow RSlow rise / fall times R:H Mode (Off/On Mic) depths, tremolos, overdrive, Difsn | horn and rotor with Doppler and tremolo, speed ramps, two mics, valve-like overdrive (Gain 0 = no sound, as the manual says) |
| 16 | Ambience Chorus | chorus + ChFil ChFrq ChLvl AmDcy AmDmp AmDns AmFil AmFrq AmLvl | the chorus plus a small room (diffusers and a 4-line feedback delay network) |

Every program also has **E.Lvl**, **Dirct**, **Input** (STEREO / MONO) and the **3-band EQ**
(low and high shelving or peaking, mid peaking; frequencies, gains and Qs as the manual).
Changing program mutes the effect for about 12 ms while it changes over (the direct sound
carries on), so program changes don't click.

Not modelled: MIDI (program change, Control Assign); in a DAW every parameter can be
automated instead. Display order (ORD) and the LCD contrast setting are left out too.

### Estimates (not fixed by the manual)

- Chorus delay: Pre Delay + 1.5 ms + a sweep of up to +-3 ms at Depth 100, reduced at fast
  rates so Depth 100 never bends pitch more than about 0.6 semitone.
- Space Chorus modes: LFO 0.25 Hz (modes 1-3), 0.5 Hz (mode 4), 0.37 Hz (combined), sweep
  0.3-0.9 ms around 3.2 ms, 9 kHz bandwidth, cross feed 0.42.
- 16 Phase Chorus modes: 1 all taps to both sides, 2 alternate taps left / right, 3 spread
  across the field, 4 left in phase / right with every other tap inverted, 5 two LFO speeds,
  6 staggered pre delays spread wide.
- Ensemble modes: 1 classic strings (0.63 / 6.3 Hz), 2 slow and deep, 3 bright shimmer,
  4 with regeneration, 5 wide and diffused, 6 lush.
- Vintage Chorus: centre 3.5 ms, Intensity 0-100 = 0.25-5 Hz and 0.4-2.8 ms, 7 kHz bandwidth.
- Detune: 45 ms splice window.
- Flanger: Manual 0-100 = 0.05-10 ms, Resonance +-100 = +-0.93 feedback.
- Rotary: 800 Hz crossover, horn Doppler +-0.6 ms, rotor +-0.35 ms, rise / fall 1-100 =
  0.05-5 s.
- Ambience: Decay 1-30 = 0.1-3 s, HF Damp -100 = 1 kHz in the loop.
- 3D: interaural delay up to 0.65 ms, head shadow, rear darkening, elevation peak / notch,
  and a crosstalk canceller; Roland's RSS processing isn't published. 3D Panner Speed 1-10 =
  one turn in 20 s to two turns a second.
- LFO shapes EXP, OCW1-3 and OFW1-2 are drawn from the manual's small pictures.
- Meter: "0" = 10 dB under full scale.

## Install

Actions -> latest run -> Artifacts -> `TB-Stereo-Chorus-sdx330-mountain-lion-vst` ->
download, unzip twice, copy `TB SDX-330.vst` to `/Library/Audio/Plug-Ins/VST/`, rescan in
PatchWork.

## Building and testing

- `make -C Tests run vst-run` (Linux or Mac): program store, DSP tests (parameter table, all
  100 programs, every algorithm, EQ, stability, automation, program changes, 96 kHz, demo
  renders) and a mini VST host test. `make -C Tests asan` runs them under the sanitizers.
- `scripts/build_vst_mac.sh` builds the bundle on macOS and checks it declares 10.8.5 and
  links only 10.8-era libraries.
- GitHub Actions builds on `macos-15-intel`, loads the bundle, opens the panel offscreen and
  drives its controls, and publishes the screenshots to the `gui-preview` branch.

Plain C++98 DSP, no allocation while processing, no latency.
