# Auto-Master

An automatic mastering VST3 plugin for FL Studio's master track (Windows x64).

Built with [JUCE 8.0.6](https://github.com/juce-framework/JUCE) and built automatically by GitHub Actions.

## Features

- **LUFS metering** per ITU-R BS.1770: momentary (400 ms), short-term (3 s), integrated (gated), and LRA (loudness range)
- **Auto Gain** — steers short-term loudness toward your target LUFS (default -14 LUFS, streaming-safe)
- **De-Harsh** — dynamic EQ that reduces harsh 2–6 kHz energy only when it dominates the mix
- **Glue compression** — gentle bus-style compression to hold the mix together
- **Warmth / Air EQ** — low-shelf warmth, high-shelf air
- **Stereo Width** — mid/side width control
- **Ceiling limiter** — keeps peaks under your set ceiling (default -0.3 dB)

## Controls

| Parameter | Range | Default |
|---|---|---|
| Mastering On | on/off | on |
| Target LUFS | -16 to -8 | -14 |
| Amount | 0–1 | 0.5 |
| Glue Comp | 0–1 | 0.4 |
| De-Harsh | 0–1 | 0.3 |
| Harsh Freq | 2–6 kHz | 3.5 kHz |
| Warmth | ±6 dB | 0 |
| Air | 0–6 dB | 0 |
| Width | 0–2 | 1 |
| Ceiling | -3 to 0 dB | -0.3 |
| Auto Gain | on/off | on |

## Building

The CI workflow (`.github/workflows/build-windows.yml`) builds the VST3 on every push. Download the artifact from the **Actions** tab, then copy the `Auto-Master.vst3` folder to:

```
C:\Program Files\Common Files\VST3\
```

and rescan plugins in FL Studio.

Local build:

```
cmake -S . -B build -A x64
cmake --build build --config Release --target AutoMaster_VST3
```
