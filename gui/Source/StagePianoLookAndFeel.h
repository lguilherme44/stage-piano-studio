#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class StagePianoLookAndFeel : public juce::LookAndFeel_V4 {
public:
    StagePianoLookAndFeel();
    ~StagePianoLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    // Palette Constants
    static const juce::Colour bgChassis;
    static const juce::Colour bgPanel;
    static const juce::Colour borderPanel;
    static const juce::Colour accentAmber;
    static const juce::Colour accentNord;
    static const juce::Colour textLight;
    static const juce::Colour textDim;
};
