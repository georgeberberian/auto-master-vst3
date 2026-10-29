# Auto-Master

An automatic mastering VST3 plugin for FL Studio. Put it on your master track and let it master for you.

## Features
- **ITU-R BS.1770 LUFS metering** - Momentary, Short-Term, Integrated, and LRA (Loudness Range)
- **Auto Gain** - continuously steers short-term loudness toward your Target LUFS
- **De-Harsh** - dynamic EQ that automatically ducks a chosen harshness band (2-6 kHz)
- **Glue Comp** - stereo-linked glue compression with adjustable amount
- **Warmth / Air** - low-shelf and high-shelf tone shaping
- **Width** - mid/side stereo width control
- **Ceiling** - true look-ahead brickwall limiter (5 ms look-ahead, latency reported to host)

## Parameters
| Parameter | Range | Default |
|---|---|---|
| Mastering On | off/on | on |
| Target LUFS | -16 to -8 LUFS | -14 |
| Amount | 0 to 1 | 0.5 |
| Glue Comp | 0 to 1 | 0.4 |
| De-Harsh | 0 to 1 | 0.3 |
| Harsh Freq | 2-6 kHz | 3.5 kHz |
| Warmth | -6 to +6 dB | 0 |
| Air | 0 to +6 dB | 0 |
| Width | 0 to 2 | 1 |
| Ceiling | -3 to 0 dB | -0.3 |
| Auto Gain | off/on | on |

## Build (Windows)
A GitHub Actions workflow builds the VST3 automatically on every push to `main`, `master`, or `auto-master`.

1. Download the `AutoMaster-Windows-VST3` artifact from the latest successful Actions run.
2. Copy the `AutoMaster.vst3` folder to `C:\Program Files\Common Files\VST3\`.
3. In FL Studio, rescan plugins (Options > File > Manage plugins > Rescan) and add Auto-Master to your master track.

## Manual Build
```bash
cmake -S . -B build -A x64
cmake --build build --config Release --target AutoMaster_VST3
```

## Requirements
- CMake 3.22+
- JUCE 8.0.6 (fetched automatically via FetchContent)
