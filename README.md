# Auto-Master

An automatic mastering VST3 plugin for FL Studio's master track, built with JUCE 8.

## Features
- **ITU-R BS.1770 K-weighted LUFS meter** — momentary, short-term, integrated, and LRA
- **Auto gain** — steers short-term loudness to a target LUFS (-16 to -8)
- **De-harsh dynamic EQ** — tames resonant harshness around a chosen frequency (2–6 kHz)
- **Glue compressor** — smooths and tightens the mix
- **Warmth / Air tone shaping** — low-shelf and high-shelf EQ
- **Stereo width** — mid/side widening
- **Ceiling limiter** — keeps peaks under your set ceiling

## Building (CI)
Push to `main` or run the "Build Windows VST3" workflow — the compiled plugin appears as an Actions artifact.

## Installing
1. Download the artifact from the Actions run and extract it.
2. Copy the `AutoMaster.vst3` folder to `C:\Program Files\Common Files\VST3\`.
3. Rescan plugins in FL Studio (Options → File → Manage plugins), then add **Auto-Master** to your master track.

## Manual build
```bash
cmake -S . -B build -A x64
cmake --build build --config Release --target AutoMaster_VST3
```
