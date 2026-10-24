# MIDI Keyboard & Controller Setup

Stage Piano Studio is class-compliant and connects automatically to any USB MIDI keyboard or interface.

## Quick Start

1. Plug your MIDI keyboard or USB interface into your computer.
2. If your keyboard has a driver setting in its menu (e.g. `Vender` vs `Generic`), make sure it is set to **`Generic` / `Class-Compliant`**.
3. Open Stage Piano Studio.
4. Click **`AUDIO / MIDI`** (or **`CONFIG`** in the footer) to open settings.
5. In the **Active MIDI Inputs** list, check your keyboard.

## macOS
CoreMIDI detects hotplugged USB keyboards automatically. When a note is played, the footer indicator updates to display your device name.

## Windows
Windows MIDI drivers and WASAPI/ASIO audio devices are enumerated in the Audio & MIDI settings panel. Select your low-latency audio driver (ASIO or Windows Audio Exclusive) for minimum buffer delay (~64-128 samples).

## Linux
ALSA and JACK MIDI ports are supported. Make sure your user belongs to the `audio` group for realtime permissions.
