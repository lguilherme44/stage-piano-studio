#include "StagePianoHost.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

int main() {
    std::cout << "[TestEngine] Initializing StagePianoHost test suite...\n";

    StagePianoHost host(44100.0f, "/Volumes/SSD-EXT/StagePianoStudio/library");

    // 1. Test Default MIDI CC bindings
    std::cout << "[TestEngine] Checking default MIDI CC bindings...\n";
    assert(host.getMidiBinding(7) == MidiCCParam::PianoVolume);
    assert(host.getMidiBinding(11) == MidiCCParam::SynthVolume);
    assert(host.getMidiBinding(74) == MidiCCParam::SynthCutoff);
    assert(host.getMidiBinding(1) == MidiCCParam::DroneVolume);
    assert(host.getMidiBinding(91) == MidiCCParam::ReverbMix);
    assert(host.getMidiBinding(93) == MidiCCParam::ChorusMix);

    // 2. Test Parameter callback via CC dispatch
    std::cout << "[TestEngine] Testing MIDI CC parameter dispatch & listener...\n";
    MidiCCParam lastParam = MidiCCParam::None;
    float lastVal = 0.0f;
    int callbackCount = 0;

    host.setParamChangeListener([&](MidiCCParam param, float val) {
        lastParam = param;
        lastVal = val;
        callbackCount++;
    });

    // Send CC 7 (Piano Volume) at max (127 -> 1.5f)
    host.handleMidiCC(7, 127);
    assert(lastParam == MidiCCParam::PianoVolume);
    assert(std::abs(lastVal - 1.5f) < 0.05f);
    assert(std::abs(host.getPianoVolume() - 1.5f) < 0.05f);

    // Send CC 1 (Drone Volume) at 64 -> ~0.75f
    host.handleMidiCC(1, 64);
    assert(lastParam == MidiCCParam::DroneVolume);
    assert(std::abs(host.getDroneVolume() - (64.0f / 127.0f * 1.5f)) < 0.05f);

    // 3. Test MIDI Learn
    std::cout << "[TestEngine] Testing MIDI Learn functionality...\n";
    assert(!host.isMidiLearning());
    host.startMidiLearn(MidiCCParam::DelayMix);
    assert(host.isMidiLearning());
    assert(host.getMidiLearnTarget() == MidiCCParam::DelayMix);

    // Send CC 23 during learn mode
    host.handleMidiCC(23, 100);
    assert(!host.isMidiLearning());
    assert(host.getMidiBinding(23) == MidiCCParam::DelayMix);

    // Now CC 23 should control Delay Mix
    host.handleMidiCC(23, 127);
    assert(lastParam == MidiCCParam::DelayMix);
    assert(std::abs(lastVal - (1.0f * 0.8f)) < 0.05f);

    // 4. Test Velocity Curves
    std::cout << "[TestEngine] Testing Velocity Curve profiles...\n";
    host.setVelocityCurve(VelocityCurve::Linear);
    assert(host.getVelocityCurve() == VelocityCurve::Linear);

    host.setVelocityCurve(VelocityCurve::Soft1);
    assert(host.getVelocityCurve() == VelocityCurve::Soft1);

    host.setVelocityCurve(VelocityCurve::Hard2);
    assert(host.getVelocityCurve() == VelocityCurve::Hard2);

    host.setVelocityCurve(VelocityCurve::Compressed);
    assert(host.getVelocityCurve() == VelocityCurve::Compressed);

    // 5. Test Dedicated Slot A & Slot B Equalizers
    std::cout << "[TestEngine] Testing Dedicated Piano & Synth Equalizers...\n";
    host.getPianoEqualizer().setLowGain(4.0f);
    assert(std::abs(host.getPianoEqualizer().getLowGain() - 4.0f) < 0.01f);
    host.getSynthEqualizer().setHighGain(-3.0f);
    assert(std::abs(host.getSynthEqualizer().getHighGain() - (-3.0f)) < 0.01f);

    // 6. Test Global Transpose
    std::cout << "[TestEngine] Testing Global Transpose...\n";
    assert(host.getGlobalTranspose() == 0);
    host.setGlobalTranspose(3); // Transpose +3 semitones (e.g. C -> Eb)
    assert(host.getGlobalTranspose() == 3);
    host.setGlobalTranspose(-2); // Transpose -2 semitones (e.g. C -> Bb)
    assert(host.getGlobalTranspose() == -2);
    host.setGlobalTranspose(0);

    // 7. Test DFD Streaming Engine Diagnostics
    std::cout << "[TestEngine] Testing DFD Streaming Engine diagnostics...\n";
    assert(host.getDfdLoadPercent() >= 0 && host.getDfdLoadPercent() <= 100);
    assert(host.getDfdActiveRequests() >= 0);
    assert(host.getDfdUnderrunCount() == 0);

    // 8. Test Panic Button & Emergency Reset
    std::cout << "[TestEngine] Testing Emergency Panic...\n";
    host.noteOn(60, 100);
    host.noteOn(64, 100);
    host.noteOn(67, 100);
    host.panic();
    assert(host.getTotalActiveVoices() == 0);

    std::cout << "[TestEngine] All StagePianoHost engine tests PASSED successfully!\n";
    return 0;
}
