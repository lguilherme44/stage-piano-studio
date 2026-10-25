#include "MainComponent.h"
#include "SampleImporter.hpp"
#include <chrono>
#include <filesystem>

MainComponent::MainComponent() {
    setLookAndFeel(&m_lookAndFeel);

    // Dynamic cross-platform library discovery (macOS, Windows, Linux)
    juce::File currentExe = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    juce::File cand1 = currentExe.getParentDirectory().getChildFile("library");
    juce::File cand2 = currentExe.getParentDirectory().getParentDirectory().getChildFile("library");
    juce::File cand3 = currentExe.getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("library");
    juce::File cand4 = currentExe.getParentDirectory().getParentDirectory().getParentDirectory().getParentDirectory().getChildFile("library");
    juce::File candMac = juce::File("/Volumes/SSD-EXT/StagePianoStudio/library");
    juce::File candWin = juce::File("C:/Users/lguil/wk/StagePianoStudio/library");

    if (cand1.isDirectory()) m_libDir = cand1.getFullPathName().toStdString();
    else if (cand2.isDirectory()) m_libDir = cand2.getFullPathName().toStdString();
    else if (cand3.isDirectory()) m_libDir = cand3.getFullPathName().toStdString();
    else if (cand4.isDirectory()) m_libDir = cand4.getFullPathName().toStdString();
    else if (candMac.isDirectory()) m_libDir = candMac.getFullPathName().toStdString();
    else if (candWin.isDirectory()) m_libDir = candWin.getFullPathName().toStdString();
    else m_libDir = cand1.getFullPathName().toStdString();

    // Initialize Multi-Layer Host
    m_host = std::make_shared<StagePianoHost>(44100.0f, m_libDir);

    m_host->setParamChangeListener([this](MidiCCParam param, float val) {
        juce::MessageManager::callAsync([this, param, val] {
            updateUIFromMidi(param, val);
        });
    });

    setupUI();
    initAudioAndMidi();

    m_deviceManager.addChangeListener(this);
    m_keyboardState.addListener(this);

    // Setup Virtual Piano Keyboard (61 keys from C2 to C7)
    m_keyboardComponent.setAvailableRange(36, 96);
    m_keyboardComponent.setScrollButtonsVisible(false);
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour(0xfff8fafc));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour(0xff181a20));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour(0xff272a33));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, juce::Colour(0x33f59e0b));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId, juce::Colour(0xfff59e0b));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::textLabelColourId, juce::Colour(0xff64748b));
    m_keyboardComponent.setColour(juce::MidiKeyboardComponent::shadowColourId, juce::Colours::black.withAlpha(0.6f));
    addAndMakeVisible(m_keyboardComponent);

    setSize(1140, 770);
    startTimerHz(30);
}

MainComponent::~MainComponent() {
    stopTimer();
    m_keyboardState.removeListener(this);
    m_deviceManager.removeChangeListener(this);
    m_deviceManager.removeAudioCallback(this);
    m_deviceManager.removeMidiInputDeviceCallback({}, this);
    setLookAndFeel(nullptr);
}

void MainComponent::handleNoteOn(juce::MidiKeyboardState* /*source*/, int /*midiChannel*/, int midiNoteNumber, float velocity) {
    m_host->noteOn(midiNoteNumber, juce::jlimit(1, 127, static_cast<int>(velocity * 127.0f)));
}

void MainComponent::handleNoteOff(juce::MidiKeyboardState* /*source*/, int /*midiChannel*/, int midiNoteNumber, float /*velocity*/) {
    m_host->noteOff(midiNoteNumber);
}

void MainComponent::updateUIFromMidi(MidiCCParam param, float val) {
    switch (param) {
        case MidiCCParam::PianoVolume:
            m_sliderPianoVol.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::SynthVolume:
            m_sliderSynthVol.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::DroneVolume:
            m_sliderDroneVol.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::SynthCutoff:
            m_sliderSynthCutoff.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::PianoEqLow:
            m_sliderPianoEqLow.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::PianoEqMid:
            m_sliderPianoEqMid.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::PianoEqHigh:
            m_sliderPianoEqHigh.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::SynthEqLow:
            m_sliderSynthEqLow.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::SynthEqMid:
            m_sliderSynthEqMid.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::SynthEqHigh:
            m_sliderSynthEqHigh.setValue(val, juce::dontSendNotification);
            break;
        case MidiCCParam::ChorusMix:
            m_sliderChorusMix.setValue(val * 100.0, juce::dontSendNotification);
            break;
        case MidiCCParam::DelayMix:
            m_sliderDelayMix.setValue(val * 100.0, juce::dontSendNotification);
            break;
        case MidiCCParam::ReverbMix:
            m_sliderReverbMix.setValue(val * 100.0, juce::dontSendNotification);
            break;
        default:
            break;
    }
    repaint();
}

void MainComponent::openAudioSettings() {
    auto* selector = new juce::AudioDeviceSelectorComponent(
        m_deviceManager,
        0, 0, // 0 inputs
        1, 2, // 1 to 2 outputs (Stereo)
        true, // Show MIDI inputs!
        false,// Hide MIDI outputs
        true, // Stereo pairs
        false // Standard options
    );
    selector->setSize(520, 480);

    juce::DialogWindow::LaunchOptions opt;
    opt.content.setOwned(selector);
    opt.dialogTitle = "Audio & MIDI Settings";
    opt.dialogBackgroundColour = StagePianoLookAndFeel::bgPanel;
    opt.escapeKeyTriggersCloseButton = true;
    opt.useNativeTitleBar = true;
    opt.resizable = false;
    opt.launchAsync();
}

void MainComponent::openImportDialog() {
    m_fileChooser = std::make_unique<juce::FileChooser>(
        "Select SoundFont (.sf2), SFZ (.sfz), or Sample Folder to Import...",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "*.sf2;*.sfz"
    );

    auto flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectDirectories;
    m_fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser) {
        auto result = chooser.getResult();
        if (result.isDirectory()) {
            std::string path = result.getFullPathName().toStdString();
            ImportResult res = SampleImporter::importSampleFolder(path, m_libDir);
            if (res.success) {
                populateInstrumentCombos();
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Import Successful",
                    "Imported folder: " + juce::String(res.instrumentName) + "\nSamples: " + juce::String(res.sampleCount)
                );
            } else {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Import Failed",
                    res.message
                );
            }
        } else if (result.existsAsFile()) {
            std::string path = result.getFullPathName().toStdString();
            std::string ext = result.getFileExtension().toLowerCase().toStdString();
            ImportResult res;
            if (ext == ".sf2") {
                res = SampleImporter::importSF2(path, m_libDir);
            } else if (ext == ".sfz") {
                res = SampleImporter::importSFZ(path, m_libDir);
            }

            if (res.success) {
                populateInstrumentCombos();
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::InfoIcon,
                    "Import Successful",
                    "Imported: " + juce::String(res.instrumentName) + "\nSamples: " + juce::String(res.sampleCount)
                );
            } else {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::AlertWindow::WarningIcon,
                    "Import Failed",
                    res.message
                );
            }
        }
    });
}

void MainComponent::populateInstrumentCombos() {
    static const InstItem catalogPianos[] = {
        {"Nord Bright Grand", "NS3_Bright_Grand"},
        {"Grand Imperial", "NS3_Grand_Imperial"},
        {"Italian Grand", "NS3_Italian_Grand"},
        {"Royal Grand 3D", "NS3_Royal_Grand_3D"},
        {"Silver Grand", "NS3_Silver_Grand"},
        {"Velvet Grand", "NS3_Velvet_Grand"},
        {"Concert Grand Ambient", "NS3_Concert_Grand_Ambient"},
        {"Concert Grand Close", "NS3_Concert_Grand_Close"},
        {"Grand Lady D", "NS3_Grand_Lady_D"},
        {"Studio Grand 1", "NS3_Studio_Grand_1"},
        {"Studio Grand 2", "NS3_Studio_Grand_2"},
        {"Damper Grand", "NS3_Damper_Grand"},
        {"Nord Grand Upright", "NS3_Grand_Upright"},
        {"Amber Upright", "NS3_Amber_Upright"},
        {"Baby Upright", "NS3_Baby_Upright"},
        {"Bambino Upright", "NS3_Bambino_Upright"},
        {"Black Upright", "NS3_Black_Upright"},
        {"Blue Swede Upright", "NS3_Blue_Swede"},
        {"Honky Tonk Upright", "NS3_HonkyTonky_Upright"},
        {"Mellow Upright", "NS3_Mellow_Upright"},
        {"Queen Upright", "NS3_Queen_Upright"},
        {"Rain Piano (Warm)", "NS3_Rain_Piano"},
        {"Romantic Upright", "NS3_Romantic_Upright"},
        {"Saloon Upright", "NS3_Saloon_Upright"},
        {"Damper Upright", "NS3_Damper_Upright"},
        {"Rhodes Stage Mark I (EP1)", "NS3_EP1"},
        {"Rhodes Mark I Deep (EP2)", "NS3_EP2"},
        {"Rhodes Mark II (EP3)", "NS3_EP3"},
        {"Rhodes Mark V (EP4)", "NS3_EP4"},
        {"Rhodes Bright (EP5)", "NS3_EP5"},
        {"Rhodes Sparkle Top (EP6)", "NS3_EP6"},
        {"Rhodes Dyno My Piano (EP7)", "NS3_EP7"},
        {"Rhodes Suitcase (EP8)", "NS3_EP8"},
        {"Ballad EP 1", "NS3_Ballad_EP1"},
        {"Ballad EP 2", "NS3_Ballad_EP2"},
        {"Hybrid EP", "NS3_Hybrid_EP"},
        {"Silver EP", "NS3_Silver_EP"},
        {"Wurlitzer 1", "NS3_Wurlitzer1"},
        {"Wurlitzer 2", "NS3_Wurlitzer2"},
        {"CP-80 Electric Grand", "NS3_Electric_Grand_CP80"},
        {"CP-80 Amped", "NS3_Electric_Grand_Amped"},
        {"CP-80 Gerbstedt", "NS3_Electric_Grand_Gerbstedt"},
        {"DX7 Full Tines FM", "NS3_DX_FullTines"},
        {"DX7 Rubba Rhodes", "NS3_DX_Rubba_Rhodes"},
        {"DigiGrand 1", "NS3_DigiGrand1"},
        {"DigiGrand 2", "NS3_DigiGrand2"}
    };

    static const InstItem catalogSynths[] = {
        {"Warm and Soft Pad", "Warm_and_Soft"},
        {"ChillPad 1", "ChillPad1"},
        {"Cloud Shimmer Pad", "Cloud_Shimmer"},
        {"Deep Sea Ambient", "DeepSea"},
        {"Huge Ambient 2", "HugeAmbient2"},
        {"Misty Vox Pad", "Misty_Vox"},
        {"Voicepad Ambient", "Voicepad"},
        {"Watergate Pad", "Watergate"},
        {"String Choir", "StringChoir"},
        {"Silver Rain", "Silver_Rain"},
        {"Access Virus Lead", "Access_Virus"},
        {"Crystal Saw Synth", "CrystalSaw"},
        {"Formanta Soviet Synth", "Formanta"},
        {"Munich Synth", "Munich"},
        {"Trance Saw Synth", "TranceSaw"},
        {"Unicorn Synth", "Unicorn"}
    };

    static const InstItem catalogDrones[] = {
        {"Warm and Soft (Drone)", "Warm_and_Soft"},
        {"ChillPad 1 (Drone)", "ChillPad1"},
        {"Cloud Shimmer (Drone)", "Cloud_Shimmer"},
        {"Deep Sea Ambient (Drone)", "DeepSea"},
        {"Huge Ambient 2 (Drone)", "HugeAmbient2"},
        {"Misty Vox Pad (Drone)", "Misty_Vox"},
        {"Voicepad Ambient (Drone)", "Voicepad"},
        {"Watergate Pad (Drone)", "Watergate"},
        {"String Choir (Drone)", "StringChoir"},
        {"Silver Rain (Drone)", "Silver_Rain"},
        {"Formanta Drone", "Formanta"},
        {"Access Virus Drone", "Access_Virus"}
    };

    // Scan for dynamic imported SF2 / SFZ in library/instruments
    std::vector<InstItem> customInstruments;
    try {
        juce::File instFolder(m_libDir + "/instruments");
        if (instFolder.isDirectory()) {
            auto files = instFolder.findChildFiles(juce::File::findFiles, false, "*.json");
            for (const auto& f : files) {
                std::string fname = f.getFileNameWithoutExtension().toStdString();
                if (fname.rfind("SF2_", 0) == 0 || fname.rfind("SFZ_", 0) == 0) {
                    customInstruments.push_back({f.getFileNameWithoutExtension(), juce::String(fname)});
                }
            }
        }
    } catch (...) {}

    // Populate Piano Combo
    m_availablePianos.clear();
    m_comboPiano.clear(juce::dontSendNotification);
    for (const auto& item : catalogPianos) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availablePianos.push_back(item);
            m_comboPiano.addItem(item.label, static_cast<int>(m_availablePianos.size()));
        }
    }
    for (const auto& item : customInstruments) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availablePianos.push_back(item);
            m_comboPiano.addItem(item.label, static_cast<int>(m_availablePianos.size()));
        }
    }
    if (m_availablePianos.empty()) {
        m_comboPiano.addItem("No installed pianos (syncing...)", 1);
        m_comboPiano.setItemEnabled(1, false);
    } else {
        m_comboPiano.setSelectedId(1, juce::dontSendNotification);
        m_host->loadPiano(m_availablePianos[0].id.toStdString());
    }

    // Populate Synth Combo
    m_availableSynths.clear();
    m_comboSynth.clear(juce::dontSendNotification);
    for (const auto& item : catalogSynths) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availableSynths.push_back(item);
            m_comboSynth.addItem(item.label, static_cast<int>(m_availableSynths.size()));
        }
    }
    for (const auto& item : customInstruments) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availableSynths.push_back(item);
            m_comboSynth.addItem(item.label, static_cast<int>(m_availableSynths.size()));
        }
    }
    if (m_availableSynths.empty()) {
        m_comboSynth.addItem("No installed synths (syncing...)", 1);
        m_comboSynth.setItemEnabled(1, false);
    } else {
        m_comboSynth.setSelectedId(1, juce::dontSendNotification);
        m_host->loadSynth(m_availableSynths[0].id.toStdString());
    }

    // Populate Drone Combo
    m_availableDrones.clear();
    m_comboDroneSound.clear(juce::dontSendNotification);
    for (const auto& item : catalogDrones) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availableDrones.push_back(item);
            m_comboDroneSound.addItem(item.label, static_cast<int>(m_availableDrones.size()));
        }
    }
    for (const auto& item : customInstruments) {
        if (m_host->isInstrumentInstalled(item.id.toStdString())) {
            m_availableDrones.push_back(item);
            m_comboDroneSound.addItem(item.label, static_cast<int>(m_availableDrones.size()));
        }
    }
    if (m_availableDrones.empty()) {
        m_comboDroneSound.addItem("No installed drone pads (syncing...)", 1);
        m_comboDroneSound.setItemEnabled(1, false);
    } else {
        m_comboDroneSound.setSelectedId(1, juce::dontSendNotification);
        m_host->loadDrone(m_availableDrones[0].id.toStdString());
    }

    repaint();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
    if (source == &m_deviceManager) {
        auto midiInputs = juce::MidiInput::getAvailableDevices();
        juce::String activeList;
        for (const auto& dev : midiInputs) {
            if (m_deviceManager.isMidiInputDeviceEnabled(dev.identifier)) {
                m_deviceManager.addMidiInputDeviceCallback(dev.identifier, this);
                if (activeList.isNotEmpty()) activeList += ", ";
                activeList += dev.name;
            }
        }
        if (activeList.isNotEmpty()) {
            m_activeMidiDevice = activeList.toStdString();
        } else {
            m_activeMidiDevice = "No MIDI input active (click CONFIG)";
        }
        repaint();
    }
}

void MainComponent::initAudioAndMidi() {
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.sampleRate = 44100.0;
    setup.bufferSize = 256;
    setup.inputChannels.clear();
    setup.outputChannels.setRange(0, 2, true);

    m_deviceManager.initialise(0, 2, nullptr, true, {}, &setup);
    m_deviceManager.addAudioCallback(this);

    auto midiInputs = juce::MidiInput::getAvailableDevices();
    juce::String activeList;
    for (const auto& dev : midiInputs) {
        m_deviceManager.setMidiInputDeviceEnabled(dev.identifier, true);
        m_deviceManager.addMidiInputDeviceCallback(dev.identifier, this);
        if (activeList.isNotEmpty()) activeList += ", ";
        activeList += dev.name;
    }

    if (activeList.isNotEmpty()) {
        m_activeMidiDevice = activeList.toStdString();
    } else {
        m_activeMidiDevice = "No MIDI input active (click CONFIG)";
    }
}

void MainComponent::setupUI() {
    auto setupRotary = [this](juce::Slider& s, const juce::String& suffix, double min, double max, double def) {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
        s.setTextValueSuffix(suffix);
        s.setRange(min, max, (max - min) > 100.0 ? 1.0 : 0.05);
        s.setValue(def, juce::dontSendNotification);
        s.setColour(juce::Slider::textBoxTextColourId, StagePianoLookAndFeel::accentAmber);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    auto setupFader = [this](juce::Slider& s, double min, double max, double def) {
        s.setSliderStyle(juce::Slider::LinearVertical);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 55, 14);
        s.setRange(min, max, 0.02);
        s.setValue(def, juce::dontSendNotification);
        s.setColour(juce::Slider::textBoxTextColourId, StagePianoLookAndFeel::accentAmber);
        s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible(s);
    };

    // 1. Piano Slot (Slot A)
    addAndMakeVisible(m_comboPiano);
    m_comboPiano.onChange = [this] {
        int idx = m_comboPiano.getSelectedId() - 1;
        if (idx >= 0 && idx < static_cast<int>(m_availablePianos.size())) {
            m_host->loadPiano(m_availablePianos[idx].id.toStdString());
        }
    };

    addAndMakeVisible(m_togglePiano);
    m_togglePiano.setToggleState(true, juce::dontSendNotification);
    m_togglePiano.onClick = [this] { m_host->setPianoEnabled(m_togglePiano.getToggleState()); };

    setupFader(m_sliderPianoVol, 0.0, 1.5, 1.0);
    m_sliderPianoVol.onValueChange = [this] { m_host->setPianoVolume(static_cast<float>(m_sliderPianoVol.getValue())); };

    // 2. Synth Slot (Slot B)
    addAndMakeVisible(m_comboSynth);
    m_comboSynth.onChange = [this] {
        int idx = m_comboSynth.getSelectedId() - 1;
        if (idx >= 0 && idx < static_cast<int>(m_availableSynths.size())) {
            m_host->loadSynth(m_availableSynths[idx].id.toStdString());
        }
    };

    addAndMakeVisible(m_toggleSynth);
    m_toggleSynth.setToggleState(true, juce::dontSendNotification);
    m_toggleSynth.onClick = [this] { m_host->setSynthEnabled(m_toggleSynth.getToggleState()); };

    setupFader(m_sliderSynthVol, 0.0, 1.5, 0.65);
    m_sliderSynthVol.onValueChange = [this] { m_host->setSynthVolume(static_cast<float>(m_sliderSynthVol.getValue())); };

    setupRotary(m_sliderSynthCutoff, " Hz", 300.0, 20000.0, 8500.0);
    m_sliderSynthCutoff.onValueChange = [this] { m_host->setSynthFilterCutoff(static_cast<float>(m_sliderSynthCutoff.getValue())); };

    // Velocity Curve Profile
    addAndMakeVisible(m_comboVelocityCurve);
    m_comboVelocityCurve.addItem("Velocity: Linear", 1);
    m_comboVelocityCurve.addItem("Velocity: Soft 1 (Light)", 2);
    m_comboVelocityCurve.addItem("Velocity: Soft 2 (Very Light)", 3);
    m_comboVelocityCurve.addItem("Velocity: Hard 1 (Weighted)", 4);
    m_comboVelocityCurve.addItem("Velocity: Hard 2 (Heavy)", 5);
    m_comboVelocityCurve.addItem("Velocity: Compressed (Live)", 6);
    m_comboVelocityCurve.setSelectedId(1, juce::dontSendNotification);
    m_comboVelocityCurve.onChange = [this] {
        int id = m_comboVelocityCurve.getSelectedId();
        switch (id) {
            case 1: m_host->setVelocityCurve(VelocityCurve::Linear); break;
            case 2: m_host->setVelocityCurve(VelocityCurve::Soft1); break;
            case 3: m_host->setVelocityCurve(VelocityCurve::Soft2); break;
            case 4: m_host->setVelocityCurve(VelocityCurve::Hard1); break;
            case 5: m_host->setVelocityCurve(VelocityCurve::Hard2); break;
            case 6: m_host->setVelocityCurve(VelocityCurve::Compressed); break;
            default: break;
        }
    };

    // 3. Ambient Continuous Drone Engine
    addAndMakeVisible(m_btnDroneToggle);
    m_btnDroneToggle.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
    m_btnDroneToggle.onClick = [this] {
        int rootNotes[] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
        int sel = m_comboDroneRoot.getSelectedId() - 1;
        int root = 36;
        if (sel >= 0 && sel < 12) root = rootNotes[sel];

        m_host->toggleDrone(root);
        bool active = m_host->isDroneActive();
        m_btnDroneToggle.setButtonText(active ? "DRONE LATCH [ACTIVE]" : "DRONE LATCH [OFF]");
        m_btnDroneToggle.setColour(juce::TextButton::buttonColourId, active ? StagePianoLookAndFeel::accentAmber : juce::Colour(0xff222530));
        m_btnDroneToggle.setColour(juce::TextButton::textColourOffId, active ? juce::Colours::black : StagePianoLookAndFeel::textLight);
    };

    addAndMakeVisible(m_comboDroneRoot);
    const char* rootNames[] = {
        "Root: C", "Root: C# / Db", "Root: D", "Root: D# / Eb", "Root: E", "Root: F",
        "Root: F# / Gb", "Root: G", "Root: G# / Ab", "Root: A", "Root: A# / Bb", "Root: B"
    };
    for (int i = 0; i < 12; ++i) {
        m_comboDroneRoot.addItem(rootNames[i], i + 1);
    }
    m_comboDroneRoot.setSelectedId(1, juce::dontSendNotification);
    m_comboDroneRoot.onChange = [this] {
        if (m_host->isDroneActive()) {
            int rootNotes[] = {36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47};
            int sel = m_comboDroneRoot.getSelectedId() - 1;
            if (sel >= 0 && sel < 12) {
                m_host->triggerDroneKey(rootNotes[sel]);
            }
        }
    };

    addAndMakeVisible(m_comboDroneSound);
    m_comboDroneSound.onChange = [this] {
        int idx = m_comboDroneSound.getSelectedId() - 1;
        if (idx >= 0 && idx < static_cast<int>(m_availableDrones.size())) {
            m_host->loadDrone(m_availableDrones[idx].id.toStdString());
        }
    };

    setupFader(m_sliderDroneVol, 0.0, 1.5, 0.70);
    m_sliderDroneVol.onValueChange = [this] { m_host->setDroneVolume(static_cast<float>(m_sliderDroneVol.getValue())); };

    // 4. Effects: Slot A (Piano) EQ, Slot B (Synth) EQ, Chorus, Delay, Reverb, Compressor
    // Slot A (Piano) 3-Band EQ
    setupRotary(m_sliderPianoEqLow, " dB", -12.0, 12.0, 0.0);
    m_sliderPianoEqLow.onValueChange = [this] { m_host->getPianoEqualizer().setLowGain(static_cast<float>(m_sliderPianoEqLow.getValue())); };

    setupRotary(m_sliderPianoEqMid, " dB", -12.0, 12.0, 0.0);
    m_sliderPianoEqMid.onValueChange = [this] { m_host->getPianoEqualizer().setMidGain(static_cast<float>(m_sliderPianoEqMid.getValue())); };

    setupRotary(m_sliderPianoEqHigh, " dB", -12.0, 12.0, 2.0);
    m_sliderPianoEqHigh.onValueChange = [this] { m_host->getPianoEqualizer().setHighGain(static_cast<float>(m_sliderPianoEqHigh.getValue())); };

    // Slot B (Synth) 3-Band EQ
    setupRotary(m_sliderSynthEqLow, " dB", -12.0, 12.0, 0.0);
    m_sliderSynthEqLow.onValueChange = [this] { m_host->getSynthEqualizer().setLowGain(static_cast<float>(m_sliderSynthEqLow.getValue())); };

    setupRotary(m_sliderSynthEqMid, " dB", -12.0, 12.0, 0.0);
    m_sliderSynthEqMid.onValueChange = [this] { m_host->getSynthEqualizer().setMidGain(static_cast<float>(m_sliderSynthEqMid.getValue())); };

    setupRotary(m_sliderSynthEqHigh, " dB", -12.0, 12.0, -1.5);
    m_sliderSynthEqHigh.onValueChange = [this] { m_host->getSynthEqualizer().setHighGain(static_cast<float>(m_sliderSynthEqHigh.getValue())); };

    addAndMakeVisible(m_toggleChorus);
    m_toggleChorus.setToggleState(false, juce::dontSendNotification);
    m_toggleChorus.onClick = [this] { m_host->getChorus().setEnabled(m_toggleChorus.getToggleState()); };
    setupRotary(m_sliderChorusMix, "%", 0.0, 100.0, 35.0);
    m_sliderChorusMix.onValueChange = [this] { m_host->getChorus().setMix(static_cast<float>(m_sliderChorusMix.getValue()) * 0.01f); };

    addAndMakeVisible(m_toggleDelay);
    m_toggleDelay.setToggleState(false, juce::dontSendNotification);
    m_toggleDelay.onClick = [this] { m_host->getDelay().setEnabled(m_toggleDelay.getToggleState()); };
    setupRotary(m_sliderDelayMix, "%", 0.0, 100.0, 25.0);
    m_sliderDelayMix.onValueChange = [this] { m_host->getDelay().setMix(static_cast<float>(m_sliderDelayMix.getValue()) * 0.01f); };

    addAndMakeVisible(m_toggleReverb);
    m_toggleReverb.setToggleState(true, juce::dontSendNotification);
    m_toggleReverb.onClick = [this] { m_host->getReverb().setEnabled(m_toggleReverb.getToggleState()); };
    setupRotary(m_sliderReverbMix, "%", 0.0, 100.0, 26.0);
    m_sliderReverbMix.onValueChange = [this] { m_host->getReverb().setMix(static_cast<float>(m_sliderReverbMix.getValue()) * 0.01f); };

    setupRotary(m_sliderCompThresh, " dB", -40.0, 0.0, -15.0);
    m_sliderCompThresh.onValueChange = [this] { m_host->getCompressor().setThreshold(static_cast<float>(m_sliderCompThresh.getValue())); };

    setupRotary(m_sliderCompMakeup, " dB", 0.0, 15.0, 2.5);
    m_sliderCompMakeup.onValueChange = [this] { m_host->getCompressor().setMakeupGain(static_cast<float>(m_sliderCompMakeup.getValue())); };

    // Header buttons
    addAndMakeVisible(m_btnPrevPreset);
    m_btnPrevPreset.onClick = [this] {
        int cur = m_comboPiano.getSelectedId();
        if (cur > 1) m_comboPiano.setSelectedId(cur - 1);
    };

    addAndMakeVisible(m_btnNextPreset);
    m_btnNextPreset.onClick = [this] {
        int cur = m_comboPiano.getSelectedId();
        if (cur < m_comboPiano.getNumItems()) m_comboPiano.setSelectedId(cur + 1);
    };

    addAndMakeVisible(m_btnSavePreset);
    m_btnSavePreset.setColour(juce::TextButton::buttonColourId, StagePianoLookAndFeel::accentNord);

    addAndMakeVisible(m_btnSettings);
    m_btnSettings.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2a2c38));
    m_btnSettings.onClick = [this] { openAudioSettings(); };

    // Skin / Theme Switcher
    addAndMakeVisible(m_comboTheme);
    m_comboTheme.addItem("Skin: Nord Red Studio", 1);
    m_comboTheme.addItem("Skin: Yamaha Motif Navy", 2);
    m_comboTheme.addItem("Skin: Roland V-Stage Obsidian", 3);
    m_comboTheme.setSelectedId(1, juce::dontSendNotification);
    m_comboTheme.onChange = [this] {
        int id = m_comboTheme.getSelectedId();
        switch (id) {
            case 1: m_lookAndFeel.setTheme(StagePianoLookAndFeel::Theme::NordStudio); break;
            case 2: m_lookAndFeel.setTheme(StagePianoLookAndFeel::Theme::YamahaMotif); break;
            case 3: m_lookAndFeel.setTheme(StagePianoLookAndFeel::Theme::RolandStage); break;
            default: break;
        }
        m_btnSavePreset.setColour(juce::TextButton::buttonColourId, StagePianoLookAndFeel::accentNord);
        repaint();
    };

    // Footer Buttons
    addAndMakeVisible(m_btnFooterConfig);
    m_btnFooterConfig.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
    m_btnFooterConfig.onClick = [this] { openAudioSettings(); };

    addAndMakeVisible(m_btnMidiLearn);
    m_btnMidiLearn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
    m_btnMidiLearn.onClick = [this] {
        if (m_host->isMidiLearning()) {
            m_host->cancelMidiLearn();
            m_btnMidiLearn.setButtonText("MIDI LEARN");
            m_btnMidiLearn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
            m_btnMidiLearn.setColour(juce::TextButton::textColourOffId, StagePianoLookAndFeel::textLight);
            return;
        }

        juce::PopupMenu menu;
        menu.addSectionHeader("Select parameter to map via MIDI CC:");
        menu.addItem(1, "Slot A Volume (Piano)");
        menu.addItem(2, "Slot B Volume (Synth)");
        menu.addItem(3, "Drone Volume");
        menu.addItem(4, "Synth Filter Cutoff");
        menu.addSeparator();
        menu.addItem(5, "Slot A (Piano) EQ Low");
        menu.addItem(6, "Slot A (Piano) EQ Mid");
        menu.addItem(7, "Slot A (Piano) EQ High");
        menu.addSeparator();
        menu.addItem(8, "Slot B (Synth) EQ Low");
        menu.addItem(9, "Slot B (Synth) EQ Mid");
        menu.addItem(10, "Slot B (Synth) EQ High");
        menu.addSeparator();
        menu.addItem(11, "Chorus Mix");
        menu.addItem(12, "Delay Mix");
        menu.addItem(13, "Reverb Mix");
        menu.addSeparator();
        menu.addItem(99, "Reset Default CCs (CC7, CC11, CC74, CC1, CC91, CC93)");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&m_btnMidiLearn),
            [this](int result) {
                if (result == 0) return;
                if (result == 99) {
                    m_host->setMidiBinding(7, MidiCCParam::PianoVolume);
                    m_host->setMidiBinding(11, MidiCCParam::SynthVolume);
                    m_host->setMidiBinding(74, MidiCCParam::SynthCutoff);
                    m_host->setMidiBinding(1, MidiCCParam::DroneVolume);
                    m_host->setMidiBinding(91, MidiCCParam::ReverbMix);
                    m_host->setMidiBinding(93, MidiCCParam::ChorusMix);
                    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "MIDI CC", "Default CC bindings restored!");
                    return;
                }

                MidiCCParam target = MidiCCParam::None;
                juce::String paramName = "";
                switch (result) {
                    case 1: target = MidiCCParam::PianoVolume; paramName = "Piano Vol"; break;
                    case 2: target = MidiCCParam::SynthVolume; paramName = "Synth Vol"; break;
                    case 3: target = MidiCCParam::DroneVolume; paramName = "Drone Vol"; break;
                    case 4: target = MidiCCParam::SynthCutoff; paramName = "Cutoff"; break;
                    case 5: target = MidiCCParam::PianoEqLow; paramName = "Piano Low"; break;
                    case 6: target = MidiCCParam::PianoEqMid; paramName = "Piano Mid"; break;
                    case 7: target = MidiCCParam::PianoEqHigh; paramName = "Piano High"; break;
                    case 8: target = MidiCCParam::SynthEqLow; paramName = "Synth Low"; break;
                    case 9: target = MidiCCParam::SynthEqMid; paramName = "Synth Mid"; break;
                    case 10: target = MidiCCParam::SynthEqHigh; paramName = "Synth High"; break;
                    case 11: target = MidiCCParam::ChorusMix; paramName = "Chorus"; break;
                    case 12: target = MidiCCParam::DelayMix; paramName = "Delay"; break;
                    case 13: target = MidiCCParam::ReverbMix; paramName = "Reverb"; break;
                    default: break;
                }

                if (target != MidiCCParam::None) {
                    m_host->startMidiLearn(target);
                    m_btnMidiLearn.setButtonText("MOVE HW CC (" + paramName + ")");
                    m_btnMidiLearn.setColour(juce::TextButton::buttonColourId, StagePianoLookAndFeel::accentAmber);
                    m_btnMidiLearn.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
                }
            });
    };

    addAndMakeVisible(m_btnImport);
    m_btnImport.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
    m_btnImport.onClick = [this] { openImportDialog(); };

    addAndMakeVisible(m_btnRefresh);
    m_btnRefresh.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
    m_btnRefresh.onClick = [this] { populateInstrumentCombos(); };

    populateInstrumentCombos();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* /*inputChannelData*/,
                                                     int /*numInputChannels*/,
                                                     float* const* outputChannelData,
                                                     int numOutputChannels,
                                                     int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& /*context*/) {
    if (numOutputChannels == 0 || numSamples == 0) return;

    auto start = std::chrono::high_resolution_clock::now();

    for (int ch = 0; ch < numOutputChannels; ++ch) {
        juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }

    std::vector<float> interleaved(numSamples * 2, 0.0f);
    m_host->render(interleaved.data(), static_cast<uint32_t>(numSamples), 2);

    for (int s = 0; s < numSamples; ++s) {
        if (numOutputChannels >= 1) outputChannelData[0][s] = interleaved[s * 2];
        if (numOutputChannels >= 2) outputChannelData[1][s] = interleaved[s * 2 + 1];
    }

    auto end = std::chrono::high_resolution_clock::now();
    double renderUs = std::chrono::duration<double, std::micro>(end - start).count();
    double budgetUs = (numSamples / 44100.0) * 1e6;
    m_dspLoad.store(static_cast<float>((renderUs / budgetUs) * 100.0));
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* /*device*/) {}
void MainComponent::audioDeviceStopped() {}

void MainComponent::handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) {
    if (source != nullptr) {
        m_activeMidiDevice = source->getName().toStdString();
    }

    m_keyboardState.processNextMidiEvent(message);

    if (message.isController()) {
        int cc = message.getControllerNumber();
        int val = message.getControllerValue();
        if (cc == 64) {
            bool down = (val >= 64);
            m_sustainDown = down;
            m_host->setSustainPedal(down);
        } else {
            bool wasLearning = m_host->isMidiLearning();
            m_host->handleMidiCC(cc, val);
            if (wasLearning) {
                juce::MessageManager::callAsync([this] {
                    m_btnMidiLearn.setButtonText("MIDI LEARN");
                    m_btnMidiLearn.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff222530));
                    m_btnMidiLearn.setColour(juce::TextButton::textColourOffId, StagePianoLookAndFeel::textLight);
                });
            }
        }
    }
}

void MainComponent::timerCallback() {
    repaint();
}

void MainComponent::paint(juce::Graphics& g) {
    g.fillAll(StagePianoLookAndFeel::bgChassis);

    auto bounds = getLocalBounds();

    // 1. Header Bar
    auto headerBounds = bounds.removeFromTop(80);
    g.setColour(juce::Colour(0xff181920));
    g.fillRect(headerBounds);
    g.setColour(StagePianoLookAndFeel::borderPanel);
    g.drawHorizontalLine(headerBounds.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

    // Brand Label
    g.setColour(StagePianoLookAndFeel::accentNord);
    g.fillRoundedRectangle(20.0f, 22.0f, 8.0f, 36.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("STAGE PIANO STUDIO", 36, 22, 220, 20, juce::Justification::left);
    g.setColour(StagePianoLookAndFeel::textDim);
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("DUAL-LAYER + CONTINUOUS DRONE HOST", 36, 42, 240, 16, juce::Justification::left);

    // OLED Screen Box
    auto oledRect = juce::Rectangle<int>(280, 14, 355, 52);
    g.setColour(juce::Colour(0xff090a0d));
    g.fillRoundedRectangle(oledRect.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff222530));
    g.drawRoundedRectangle(oledRect.toFloat(), 6.0f, 1.5f);

    g.setColour(StagePianoLookAndFeel::accentAmber);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("DUAL-LAYER LIVE SETUP", 295, 20, 180, 12, juce::Justification::left);

    g.setColour(juce::Colour(0xfffbbf24));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    juce::String layerStr = m_host->getPianoName() + " + " + m_host->getSynthName();
    g.drawText(layerStr, 295, 36, 190, 20, juce::Justification::left);

    g.setColour(StagePianoLookAndFeel::textDim);
    g.setFont(juce::FontOptions(10.0f));
    juce::String telemetry = "DSP: " + juce::String(m_dspLoad.load(), 1) + "% | VOICES: " + juce::String(m_host->getTotalActiveVoices());
    g.drawText(telemetry, 410, 28, 215, 20, juce::Justification::right);

    // 2. Footer Strip
    auto footerBounds = bounds.removeFromBottom(50);
    g.setColour(juce::Colour(0xff15161c));
    g.fillRect(footerBounds);
    g.setColour(StagePianoLookAndFeel::borderPanel);
    g.drawHorizontalLine(footerBounds.getY(), 0.0f, static_cast<float>(getWidth()));

    bool hasMidi = (m_activeMidiDevice != "No MIDI input active (click CONFIG)");
    g.setColour(hasMidi ? juce::Colour(0xff10b981) : juce::Colour(0xffef4444));
    g.fillEllipse(20.0f, footerBounds.getY() + 21.0f, 8.0f, 8.0f);
    g.setColour(StagePianoLookAndFeel::textLight);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MIDI: " + juce::String(m_activeMidiDevice), 34, footerBounds.getY() + 16, 320, 18, juce::Justification::left);

    g.setColour(m_sustainDown ? StagePianoLookAndFeel::accentAmber : StagePianoLookAndFeel::textDim);
    g.drawText(m_sustainDown ? "SUSTAIN: [DOWN]" : "SUSTAIN: [UP]", 360, footerBounds.getY() + 16, 110, 18, juce::Justification::left);

    // 3. Four Main Cards
    auto contentBounds = bounds.removeFromTop(385).reduced(16, 8);
    int colWidth = (contentBounds.getWidth() - 36) / 4;

    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    auto drawCard = [&g](juce::Rectangle<int> r, const juce::String& title, const juce::String& tag) {
        g.setColour(StagePianoLookAndFeel::bgPanel);
        g.fillRoundedRectangle(r.toFloat(), 8.0f);
        g.setColour(StagePianoLookAndFeel::borderPanel);
        g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);

        g.setColour(StagePianoLookAndFeel::textLight);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, r.getX() + 14, r.getY() + 12, 180, 16, juce::Justification::left);

        g.setColour(StagePianoLookAndFeel::textDim);
        g.setFont(juce::FontOptions(9.0f));
        g.drawText(tag, r.getRight() - 95, r.getY() + 12, 80, 16, juce::Justification::right);

        g.setColour(StagePianoLookAndFeel::borderPanel);
        g.drawHorizontalLine(r.getY() + 34, static_cast<float>(r.getX() + 14), static_cast<float>(r.getRight() - 14));
    };

    drawCard(r1, "PIANO & SYNTH LAYER", "DUAL SLOT");
    drawCard(r2, "AMBIENT DRONE ENGINE", "KEY LATCH");
    drawCard(r3, "EQ & MODULATION", "SLOT A / B");
    drawCard(r4, "REVERB & COMPRESSOR", "STUDIO FX");

    // Slot labels
    g.setColour(StagePianoLookAndFeel::textDim);
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("SLOT A: PIANO", r1.getX() + 14, r1.getY() + 38, 150, 14, juce::Justification::left);
    g.drawText("LEVEL", r1.getRight() - 75, r1.getY() + 72, 55, 12, juce::Justification::centred);

    g.drawText("SLOT B: SYNTH PAD", r1.getX() + 14, r1.getY() + 168, 150, 14, juce::Justification::left);
    g.drawText("CUTOFF", r1.getX() + 112, r1.getY() + 204, 68, 12, juce::Justification::centred);
    g.drawText("LEVEL", r1.getRight() - 75, r1.getY() + 204, 55, 12, juce::Justification::centred);

    g.drawText("KEY TOUCH RESPONSE", r1.getX() + 14, r1.getY() + 312, 220, 14, juce::Justification::left);

    g.drawText("DRONE HARMONIC ROOT", r2.getX() + 14, r2.getY() + 38, 180, 14, juce::Justification::left);
    g.drawText("DRONE AMBIENT SOUND", r2.getX() + 14, r2.getY() + 104, 180, 14, juce::Justification::left);
    g.drawText("DRONE LEVEL", r2.getX() + 14, r2.getY() + 210, 180, 14, juce::Justification::centred);

    // Card 3 labels
    g.drawText("SLOT A: PIANO EQ", r3.getX() + 14, r3.getY() + 38, 200, 14, juce::Justification::left);
    g.drawText("SLOT B: SYNTH EQ", r3.getX() + 14, r3.getY() + 122, 200, 14, juce::Justification::left);
    g.drawText("MODULATION", r3.getX() + 14, r3.getY() + 206, 200, 14, juce::Justification::left);

    // Gain Reduction Meter in Card 4
    auto meterBox = juce::Rectangle<int>(r4.getX() + 14, r4.getY() + 175, r4.getWidth() - 28, 26);
    g.setColour(juce::Colour(0xff090a0d));
    g.fillRoundedRectangle(meterBox.toFloat(), 4.0f);
    float gr = m_host->getCompressor().getGainReductionDb();
    g.setColour(StagePianoLookAndFeel::textDim);
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("GR: -" + juce::String(gr, 1) + " dB", meterBox.getX() + 6, meterBox.getY() + 3, 100, 10, juce::Justification::left);

    int numLeds = 12;
    int litLeds = juce::jlimit(0, numLeds, static_cast<int>(gr * 0.8f));
    int ledW = (meterBox.getWidth() - 12 - (numLeds * 2)) / numLeds;
    for (int i = 0; i < numLeds; ++i) {
        int lx = meterBox.getX() + 6 + i * (ledW + 2);
        int ly = meterBox.getY() + 13;
        bool lit = (i < litLeds);
        g.setColour(lit ? (i > 8 ? juce::Colour(0xffef4444) : StagePianoLookAndFeel::accentAmber) : juce::Colour(0xff222530));
        g.fillRect(lx, ly, ledW, 8);
    }

    // Keyboard chassis frame
    g.setColour(StagePianoLookAndFeel::borderPanel);
    g.drawRoundedRectangle(15.0f, 480.0f, static_cast<float>(getWidth() - 30), 226.0f, 6.0f, 1.5f);
}

void MainComponent::resized() {
    auto bounds = getLocalBounds();
    bounds.removeFromTop(80);
    auto footerArea = bounds.removeFromBottom(50);
    auto cardArea = bounds.removeFromTop(385);
    auto contentBounds = cardArea.reduced(16, 8);

    int colWidth = (contentBounds.getWidth() - 36) / 4;
    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    // Header buttons
    m_btnPrevPreset.setBounds(650, 22, 40, 36);
    m_btnNextPreset.setBounds(695, 22, 40, 36);
    m_btnSavePreset.setBounds(740, 22, 80, 36);
    m_btnSettings.setBounds(825, 22, 110, 36);
    m_comboTheme.setBounds(940, 22, 180, 36);

    // Footer buttons
    m_btnFooterConfig.setBounds(footerArea.getRight() - 95, footerArea.getY() + 10, 85, 30);
    m_btnMidiLearn.setBounds(footerArea.getRight() - 250, footerArea.getY() + 10, 145, 30);
    m_btnImport.setBounds(footerArea.getRight() - 415, footerArea.getY() + 10, 155, 30);
    m_btnRefresh.setBounds(footerArea.getRight() - 510, footerArea.getY() + 10, 85, 30);

    // Card 1: Slot A & Slot B + Velocity Curve
    m_comboPiano.setBounds(r1.getX() + 14, r1.getY() + 54, r1.getWidth() - 28, 26);
    m_togglePiano.setBounds(r1.getX() + 14, r1.getY() + 102, 110, 24);
    m_sliderPianoVol.setBounds(r1.getRight() - 75, r1.getY() + 86, 55, 78);

    m_comboSynth.setBounds(r1.getX() + 14, r1.getY() + 184, r1.getWidth() - 28, 26);
    m_toggleSynth.setBounds(r1.getX() + 14, r1.getY() + 230, 95, 24);
    m_sliderSynthCutoff.setBounds(r1.getX() + 112, r1.getY() + 218, 68, 62);
    m_sliderSynthVol.setBounds(r1.getRight() - 75, r1.getY() + 218, 55, 78);

    m_comboVelocityCurve.setBounds(r1.getX() + 14, r1.getY() + 328, r1.getWidth() - 28, 26);

    // Card 2: Ambient Drone Engine
    m_comboDroneRoot.setBounds(r2.getX() + 14, r2.getY() + 54, r2.getWidth() - 28, 26);
    m_comboDroneSound.setBounds(r2.getX() + 14, r2.getY() + 120, r2.getWidth() - 28, 26);
    m_btnDroneToggle.setBounds(r2.getX() + 14, r2.getY() + 160, r2.getWidth() - 28, 38);
    m_sliderDroneVol.setBounds(r2.getCentreX() - 30, r2.getY() + 226, 60, 128);

    // Card 3: Dedicated Slot A (Piano) EQ, Slot B (Synth) EQ & Modulation
    // Row 1: Piano EQ
    m_sliderPianoEqLow.setBounds(r3.getX() + 8, r3.getY() + 54, 72, 60);
    m_sliderPianoEqMid.setBounds(r3.getX() + 86, r3.getY() + 54, 72, 60);
    m_sliderPianoEqHigh.setBounds(r3.getX() + 164, r3.getY() + 54, 72, 60);

    // Row 2: Synth EQ
    m_sliderSynthEqLow.setBounds(r3.getX() + 8, r3.getY() + 138, 72, 60);
    m_sliderSynthEqMid.setBounds(r3.getX() + 86, r3.getY() + 138, 72, 60);
    m_sliderSynthEqHigh.setBounds(r3.getX() + 164, r3.getY() + 138, 72, 60);

    // Row 3: Modulation
    m_toggleChorus.setBounds(r3.getX() + 14, r3.getY() + 230, 90, 24);
    m_sliderChorusMix.setBounds(r3.getX() + 115, r3.getY() + 220, 65, 50);

    m_toggleDelay.setBounds(r3.getX() + 14, r3.getY() + 285, 90, 24);
    m_sliderDelayMix.setBounds(r3.getX() + 115, r3.getY() + 275, 65, 50);

    // Card 4: Reverb & Compressor
    m_toggleReverb.setBounds(r4.getX() + 14, r4.getY() + 45, 90, 24);
    m_sliderReverbMix.setBounds(r4.getX() + 110, r4.getY() + 40, 65, 50);

    m_sliderCompThresh.setBounds(r4.getX() + 15, r4.getY() + 100, 70, 70);
    m_sliderCompMakeup.setBounds(r4.getX() + 95, r4.getY() + 100, 70, 70);

    // Virtual Piano Keyboard (61 keys from C2 to C7 = 36 white keys)
    int kbX = 18;
    int kbY = 483;
    int kbW = getWidth() - 36;
    int kbH = 220;
    m_keyboardComponent.setBounds(kbX, kbY, kbW, kbH);
    m_keyboardComponent.setKeyWidth(static_cast<float>(kbW) / 36.0f);
}
