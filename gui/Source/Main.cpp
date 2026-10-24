#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

class StagePianoStudioApplication : public juce::JUCEApplication {
public:
    StagePianoStudioApplication() = default;

    const juce::String getApplicationName() override       { return "Stage Piano Studio"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise(const juce::String& /*commandLine*/) override {
        m_mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override {
        m_mainWindow = nullptr;
    }

    void systemRequestedQuit() override {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override {}

    class MainWindow : public juce::DocumentWindow {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                             StagePianoLookAndFeel::bgChassis,
                             DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);
            setContentOwned(new MainComponent(), true);
            setResizable(false, false);
            centreWithSize(getWidth(), getHeight());
            setVisible(true);
        }

        void closeButtonPressed() override {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> m_mainWindow;
};

START_JUCE_APPLICATION(StagePianoStudioApplication)
