#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

enum class SkinType {
    NordStage = 1,
    YamahaMotif = 2,
    RolandVStage = 3
};

struct SkinUIComponents {
    // Header controls
    juce::Button* btnPrevPreset = nullptr;
    juce::Button* btnNextPreset = nullptr;
    juce::Button* btnSavePreset = nullptr;
    juce::Button* btnSettings = nullptr;
    juce::Button* btnPanic = nullptr;
    juce::ComboBox* comboTranspose = nullptr;
    juce::ComboBox* comboTheme = nullptr;

    // Slot A (Piano Layer)
    juce::ComboBox* comboPiano = nullptr;
    juce::Slider* sliderPianoVol = nullptr;
    juce::ToggleButton* togglePiano = nullptr;
    juce::ComboBox* comboVelocityCurve = nullptr;
    juce::Slider* sliderPianoEqLow = nullptr;
    juce::Slider* sliderPianoEqMid = nullptr;
    juce::Slider* sliderPianoEqHigh = nullptr;

    // Slot B (Synth Pad Layer)
    juce::ComboBox* comboSynth = nullptr;
    juce::Slider* sliderSynthVol = nullptr;
    juce::Slider* sliderSynthCutoff = nullptr;
    juce::ToggleButton* toggleSynth = nullptr;
    juce::Slider* sliderSynthEqLow = nullptr;
    juce::Slider* sliderSynthEqMid = nullptr;
    juce::Slider* sliderSynthEqHigh = nullptr;

    // Ambient Drone Engine
    juce::Button* btnDroneToggle = nullptr;
    juce::ComboBox* comboDroneRoot = nullptr;
    juce::ComboBox* comboDroneSound = nullptr;
    juce::Slider* sliderDroneVol = nullptr;

    // Studio FX Chain
    juce::ToggleButton* toggleChorus = nullptr;
    juce::Slider* sliderChorusMix = nullptr;
    juce::ToggleButton* toggleDelay = nullptr;
    juce::Slider* sliderDelayMix = nullptr;
    juce::ToggleButton* toggleReverb = nullptr;
    juce::Slider* sliderReverbMix = nullptr;
    juce::Slider* sliderCompThresh = nullptr;
    juce::Slider* sliderCompMakeup = nullptr;

    // Footer Strip Controls
    juce::Button* btnFooterConfig = nullptr;
    juce::Button* btnMidiLearn = nullptr;
    juce::Button* btnImport = nullptr;
    juce::Button* btnRefresh = nullptr;

    // Virtual Keyboard Component
    juce::Component* keyboardComponent = nullptr;
};

struct SkinContext {
    juce::String pianoName;
    juce::String synthName;
    float dspLoad = 0.0f;
    int dfdLoad = 0;
    int activeVoices = 0;
    juce::String midiDevice;
    bool sustainDown = false;
    float gainReductionDb = 0.0f;
};

class SkinLayoutStrategy {
public:
    virtual ~SkinLayoutStrategy() = default;

    virtual SkinType getType() const = 0;
    virtual juce::String getName() const = 0;

    // Render chassis, silkscreens, OLED/LCD displays and hardware boundaries
    virtual void paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) = 0;

    // Reorganize all hardware buttons, knobs and sliders according to this skin's philosophy
    virtual void layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) = 0;
};
