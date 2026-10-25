#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class StagePianoLookAndFeel : public juce::LookAndFeel_V4 {
public:
    enum class Theme {
        NordStudio = 0,
        YamahaMotif,
        RolandStage
    };

    StagePianoLookAndFeel();
    ~StagePianoLookAndFeel() override = default;

    void setTheme(Theme theme);
    Theme getTheme() const { return m_theme; }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    // Palette Constants
    static juce::Colour bgChassis;
    static juce::Colour bgPanel;
    static juce::Colour borderPanel;
    static juce::Colour accentAmber;
    static juce::Colour accentNord;
    static juce::Colour textLight;
    static juce::Colour textDim;

private:
    Theme m_theme = Theme::NordStudio;
};
