# Stage Piano Studio - Roadmap

Planned features and technical milestones for upcoming releases.

## Phase 1: Expanded Sample & Format Import
- [ ] **SoundFont 2 (`.sf2`) Parser & Importer**: Direct loading and playback of standard SF2 banks without external tools.
- [ ] **SFZ Format Reader**: Parsing `.sfz` text mapping definitions to map custom instrument multisamples.
- [ ] **Drag & Drop Sample Importer**: Automatic parsing of sample filenames (e.g. `Grand_C3_v127.wav`) to infer root note, velocity range, and loop points.
- [ ] **Sample Library Compression**: Real-time FLAC or lightweight lossless compression option to reduce disk footprint.

## Phase 2: Acoustic Realism & Expression
- [ ] **Round-Robin Cycling**: Multi-sample alternating hits to avoid the machine-gun effect on rapid keystrokes.
- [ ] **Release Samples & Damper Resonances**: Key-off release triggers and pedal down sympathetic resonance.
- [ ] **Velocity Curve Profiles**: Selectable response curves (Soft, Normal, Hard, Custom Spline) to match any weighted or semi-weighted keyboard action.

## Phase 3: Live Performance & Gig Workflow
- [ ] **Seamless Patch Transition (Patch Remain)**: Sustain previously played chords across preset switches without audio cuts.
- [ ] **MIDI Program Change (PC) Routing**: Quick bank selection via physical keyboard buttons or footswitches.
- [ ] **Keyboard Split Mode**: Split point to route lower keys to Bass/Drone and upper keys to Piano/Lead.

## Phase 4: Plugin Ecosystem
- [ ] **VST3 & AU Plugin Targets**: Exporting both Standalone executable and DAW plugins from the same unified CMake codebase.
- [ ] **Headless Linux Daemon**: Minimal footprint mode for dedicated headless Raspberry Pi / embedded live sound modules.
