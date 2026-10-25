#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "StagePianoLookAndFeel.h"
#include "StagePianoHost.hpp"
#include "Skins/SkinLayoutStrategy.h"
#include "Skins/SkinFactory.h"

class MainComponent : public juce::Component,
                      public juce::AudioIODeviceCallback,
                      public juce::MidiInputCallback,
                      public juce::Timer,
                      public juce::ChangeListener,
                      public juce::MidiKeyboardState::Listener {
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                          int numInputChannels,
                                          float* const* outputChannelData,
                                          int numOutputChannels,
                                          int numSamples,
                                          const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

    // MidiInputCallback
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    // MidiKeyboardState::Listener
    void handleNoteOn(juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;
    void handleNoteOff(juce::MidiKeyboardState* source, int midiChannel, int midiNoteNumber, float velocity) override;

    // Timer
    void timerCallback() override;

    // Keyboard Hotkeys (Esc -> Panic)
    bool keyPressed(const juce::KeyPress& key) override;

    // ChangeListener
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // MouseListener
    void mouseDown(const juce::MouseEvent& event) override;

    void openAudioSettings();
    void setSkin(SkinType type);
    SkinType getCurrentSkinType() const { return m_currentSkin ? m_currentSkin->getType() : SkinType::NordStage; }

private:
    void initAudioAndMidi();
    void setupUI();

    StagePianoLookAndFeel m_lookAndFeel;
    std::unique_ptr<SkinLayoutStrategy> m_currentSkin;
    juce::AudioDeviceManager m_deviceManager;
    std::shared_ptr<StagePianoHost> m_host;

    std::string m_rootDir;
    std::string m_libDir;

    // Telemetry state
    std::atomic<float> m_dspLoad{0.0f};
    std::string m_activeMidiDevice = "No MIDI input active (click CONFIG)";
    bool m_sustainDown = false;

    // On-Screen Virtual Piano Keyboard
    juce::MidiKeyboardState m_keyboardState;
    juce::MidiKeyboardComponent m_keyboardComponent{m_keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard};

    // Header Components
    juce::TextButton m_btnPrevPreset{"<"};
    juce::TextButton m_btnNextPreset{">"};
    juce::TextButton m_btnSavePreset{"Save Preset"};
    juce::TextButton m_btnSettings{"AUDIO / MIDI"};
    juce::TextButton m_btnPanic{"PANIC"};
    juce::ComboBox m_comboTranspose;
    juce::TextButton m_btnOctaveDown{"-1 Oct"};
    juce::TextButton m_btnOctaveUp{"+1 Oct"};
    juce::TextButton m_btnLearnC{"LEARN C"};

    juce::TextButton m_btnFooterConfig{"CONFIG"};
    juce::TextButton m_btnRefresh{"REFRESH"};
    juce::TextButton m_btnImport{"IMPORT SF2 / SFZ"};
    juce::TextButton m_btnMidiLearn{"MIDI LEARN"};

    // Performance & Skin Controls
    juce::ComboBox m_comboVelocityCurve;
    juce::ComboBox m_comboTheme;

    // Interactive Model Selectors & Nord Badges
    juce::TextButton m_btnPrevPiano{"<"};
    juce::TextButton m_btnNextPiano{">"};
    juce::ToggleButton m_toggleStrRes{"STR RES"};
    juce::ToggleButton m_toggleSoft{"SOFT"};

    juce::TextButton m_btnPrevSynth{"<"};
    juce::TextButton m_btnNextSynth{">"};
    juce::ToggleButton m_toggleLngRel{"LNG REL"};
    juce::ToggleButton m_toggleNoSens{"NO SENS"};

    // Program Bank Buttons (1..8)
    std::array<juce::TextButton, 8> m_btnPrograms;
    int m_currentProgram = 1;

    struct InstItem {
        juce::String label;
        juce::String id;
    };
    std::vector<InstItem> m_availablePianos;
    std::vector<InstItem> m_availableSynths;
    std::vector<InstItem> m_availableDrones;

    void populateInstrumentCombos();
    void openImportDialog();
    void updateUIFromMidi(MidiCCParam param, float val);
    void updateOctaveDisplay();
    void cancelMidiLearnState();
    MidiCCParam getParamForComponent(juce::Component* target, juce::String& outName);
    bool m_isWaitingForControlClick = false;
    std::unique_ptr<juce::FileChooser> m_fileChooser;

    // Slot A (Piano)
    juce::ComboBox m_comboPiano;
    juce::Slider m_sliderPianoVol;
    juce::ToggleButton m_togglePiano{"Piano Layer"};

    // Slot B (Synth Pad)
    juce::ComboBox m_comboSynth;
    juce::Slider m_sliderSynthVol;
    juce::Slider m_sliderSynthCutoff;
    juce::ToggleButton m_toggleSynth{"Synth Layer"};

    // Ambient Drone Engine
    juce::TextButton m_btnDroneToggle{"DRONE LATCH [OFF]"};
    juce::ComboBox m_comboDroneRoot;
    juce::ComboBox m_comboDroneSound;
    juce::Slider m_sliderDroneVol;

    // Slot A (Piano) EQ
    juce::Slider m_sliderPianoEqLow;
    juce::Slider m_sliderPianoEqMid;
    juce::Slider m_sliderPianoEqHigh;

    // Slot B (Synth) EQ
    juce::Slider m_sliderSynthEqLow;
    juce::Slider m_sliderSynthEqMid;
    juce::Slider m_sliderSynthEqHigh;

    juce::ToggleButton m_toggleChorus{"Chorus"};
    juce::Slider m_sliderChorusMix;

    juce::ToggleButton m_toggleDelay{"Delay"};
    juce::Slider m_sliderDelayMix;

    juce::ToggleButton m_toggleReverb{"Reverb"};
    juce::Slider m_sliderReverbMix;

    juce::Slider m_sliderCompThresh;
    juce::Slider m_sliderCompMakeup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
