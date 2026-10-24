# Stage Piano Studio

A lightweight, cross-platform C++ desktop instrument designed for live stage keyboardists. Combines an acoustic/electric piano layer, an atmospheric synth layer, and a continuous harmonic drone engine with an onboard studio effects rack.

Built on JUCE 8 and modern C++17. Runs standalone on macOS, Windows, and Linux with low-latency audio drivers (CoreAudio, WASAPI/ASIO, ALSA/JACK).

---

## Why this exists

Most live keyboard setups suffer from one of two problems:
1. **Heavy sampler hosts** that take gigabytes of RAM, take 30+ seconds to launch, and add unnecessary complexity for gigging musicians who just want a great acoustic piano layered with a warm pad.
2. **Lack of continuous ambient drone support**: worship and ambient players often need an infinite sustained pad bed underneath chord progressions without having to hold a foot pedal down or juggle extra hardware pedals.

Stage Piano Studio was built to solve this: instant lazy loading, sub-5ms audio callback latency, dual-layer mixing, and a continuous crossfade drone engine that runs indefinitely without audio dropouts or clicking.

---

## Features

- **Dual-Layer Architecture:**
  - **Slot A (Piano):** Acoustic Grands, Uprights, Electric Pianos (Rhodes Mark I–V, Dyno), Wurlitzers, CP-80, and FM Digital pianos.
  - **Slot B (Synth Pad):** Warm analog pads, vocal choirs, shimmer textures, and lead synths.
  - Independent volume, octave transpose (-2 to +2), and resonant low-pass filter per slot.

- **Ambient Continuous Drone Engine:**
  - Latch mode with seamless equal-power crossfade looping (no end-of-sample clicks or dropouts).
  - Harmonic root key selector (all 12 semitones: C through B).
  - Smooth 1.5-second crossfades when transitioning between keys during live performance.
  - Dedicated tone filter to keep the pad warm and separated from piano chords.

- **Studio DSP Chain:**
  - **3-Band EQ:** Low Shelf (80 Hz), Parametric Mid (1.2 kHz with Q), and High Shelf (8 kHz) using RBJ transposed biquad filters.
  - **Stereo Quadrature Chorus:** 90-degree phase offset modulation for wide stereo image.
  - **Ping-Pong Delay:** Cross-feedback delay with high-frequency feedback damping.
  - **Studio Reverb:** Algorithmic plate/hall network with damping and stereo spread.
  - **Dynamic Compressor:** Peak/RMS envelope detector, soft knee, makeup gain, and real-time 12-segment gain reduction LED meter.

- **Live Playing & Hardware Control:**
  - Automatic detection and routing for any class-compliant USB MIDI keyboard.
  - Low-latency buffer tuning (64 to 256 samples).
  - Built-in 61-key on-screen virtual keyboard with mouse clicking, QWERTY playback, and real-time visual MIDI feedback.
  - Damper / Sustain pedal (CC#64) support with smooth voice release tails.

---

## Architecture

The project is structured in clean modular layers:

```
StagePianoStudio/
├── CMakeLists.txt              # Root CMake configuration (auto-fetches JUCE if missing)
├── .github/workflows/          # Cross-platform CI/CD (macOS, Windows, Linux builds)
├── engine/                     # Core DSP & Audio Engine (Standard C++17)
│   ├── include/
│   │   ├── Sampler.hpp         # Polyphonic multi-velocity sampler with crossfade looping
│   │   ├── StagePianoHost.hpp  # Master host coordinating Slot A, Slot B, Drone & FX
│   │   ├── Equalizer.hpp       # 3-band parametric/shelving biquad EQ
│   │   ├── Compressor.hpp      # Soft-knee dynamics compressor with live GR metering
│   │   ├── Chorus.hpp          # Stereo quadrature dimension chorus
│   │   ├── Delay.hpp           # Stereo ping-pong cross-feedback delay
│   │   ├── Reverb.hpp          # Algorithmic studio hall reverb
│   │   └── TimbrePreset.hpp    # JSON preset serialization
│   └── src/
├── gui/                        # Native JUCE 8 User Interface
│   ├── Source/
│   │   ├── Main.cpp            # Application entry point
│   │   ├── MainComponent.cpp   # Studio rack UI layout, virtual keyboard, and audio loop
│   │   └── StagePianoLookAndFeel.cpp # Custom dark metallic / Nord studio styling
│   └── CMakeLists.txt
├── library/                    # Instrument mappings and presets
│   ├── instruments/            # Multi-velocity note zone mappings (JSON)
│   └── presets/                # User timbre presets (JSON)
├── tools/                      # Audio format utilities & extraction tools
│   └── nkx_extractor.cpp       # Monolith archive unpacker and audio decoder
└── docs/
    └── MIDI_SETUP.md           # MIDI keyboard connection guide
```

---

## Building from Source

### Prerequisites
- CMake 3.22+
- A C++17 compatible compiler:
  - **macOS:** Xcode Command Line Tools (`clang`)
  - **Windows:** Visual Studio 2022 (MSVC) or Build Tools with C++ workload
  - **Linux:** GCC 11+ or Clang 13+

### Linux Dependencies
```bash
sudo apt-get update
sudo apt-get install -y libasound2-dev libjack-jackd2-dev libfreetype-dev \
    libx11-dev libxinerama-dev libxrandr-dev libxcursor-dev libgl1-mesa-dev
```

### Build Commands (All Platforms)
```bash
# Clone the repository
git clone https://github.com/your-username/StagePianoStudio.git
cd StagePianoStudio

# Configure (CMake will automatically fetch JUCE 8.0.4 if not present locally)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build executable
cmake --build build --config Release --parallel
```

The compiled application will be located in:
- **macOS:** `build/gui/StagePianoStudio_artefacts/Release/Stage Piano Studio.app`
- **Windows:** `build/gui/StagePianoStudio_artefacts/Release/Stage Piano Studio.exe`
- **Linux:** `build/gui/StagePianoStudio_artefacts/Release/Stage Piano Studio`

---

## Sample Library Format

Instrument definitions use transparent JSON mapping files in `library/instruments/`:

```json
{
  "name": "Nord_Bright_Grand",
  "sample_rate": 44100,
  "channels": 2,
  "notes": {
    "60": [
      {
        "min_velocity": 1,
        "max_velocity": 64,
        "target_velocity": 32,
        "sample": "Piano_C4_pp.wav"
      },
      {
        "min_velocity": 65,
        "max_velocity": 127,
        "target_velocity": 127,
        "sample": "Piano_C4_ff.wav"
      }
    ]
  }
}
```

> **Note on audio samples:** This repository contains the source code, DSP engine, extraction utilities, and JSON instrument definition maps. Due to copyright restrictions, proprietary commercial audio recordings (.wav) are **not** hosted in this repository. Place your legally acquired sample recordings into `library/samples/` or configure the path in the Settings dialog.

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
All reverse engineering tools and format decoders in this repository were developed strictly for interoperability and educational study.
