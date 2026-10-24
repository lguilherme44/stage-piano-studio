#include "StagePianoLookAndFeel.h"

const juce::Colour StagePianoLookAndFeel::bgChassis   = juce::Colour(0xff121317);
const juce::Colour StagePianoLookAndFeel::bgPanel     = juce::Colour(0xff181a20);
const juce::Colour StagePianoLookAndFeel::borderPanel = juce::Colour(0xff272a33);
const juce::Colour StagePianoLookAndFeel::accentAmber = juce::Colour(0xfff59e0b);
const juce::Colour StagePianoLookAndFeel::accentNord  = juce::Colour(0xffd02b2b);
const juce::Colour StagePianoLookAndFeel::textLight   = juce::Colour(0xfff1f5f9);
const juce::Colour StagePianoLookAndFeel::textDim     = juce::Colour(0xff94a3b8);

StagePianoLookAndFeel::StagePianoLookAndFeel() {
    setColour(juce::ResizableWindow::backgroundColourId, bgChassis);
    setColour(juce::DialogWindow::backgroundColourId, bgPanel);

    // Labels
    setColour(juce::Label::textColourId, textLight);

    // Combo Boxes
    setColour(juce::ComboBox::backgroundColourId, bgPanel);
    setColour(juce::ComboBox::textColourId, textLight);
    setColour(juce::ComboBox::outlineColourId, borderPanel);
    setColour(juce::ComboBox::arrowColourId, accentAmber);

    // Popup Menu
    setColour(juce::PopupMenu::backgroundColourId, bgPanel);
    setColour(juce::PopupMenu::textColourId, textLight);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2a2d39));
    setColour(juce::PopupMenu::highlightedTextColourId, accentAmber);

    // Toggle Buttons / Checkboxes
    setColour(juce::ToggleButton::textColourId, textLight);
    setColour(juce::ToggleButton::tickColourId, accentAmber);
    setColour(juce::ToggleButton::tickDisabledColourId, textDim);

    // ListBox
    setColour(juce::ListBox::backgroundColourId, bgPanel);
    setColour(juce::ListBox::outlineColourId, borderPanel);
}

void StagePianoLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& /*slider*/) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    auto center = bounds.getCentre();

    // 1. Dark outer drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillEllipse(center.x - radius, center.y - radius + 2.0f, radius * 2.0f, radius * 2.0f);

    // 2. Metallic Bezel Ring
    juce::ColourGradient bezelGrad(juce::Colour(0xff333742), center.x - radius, center.y - radius,
                                   juce::Colour(0xff1c1e24), center.x + radius, center.y + radius, false);
    g.setGradientFill(bezelGrad);
    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

    // 3. Inner Dark Face
    auto innerRadius = radius * 0.88f;
    juce::ColourGradient faceGrad(juce::Colour(0xff282b35), center.x, center.y - innerRadius,
                                  juce::Colour(0xff14151a), center.x, center.y + innerRadius, false);
    g.setGradientFill(faceGrad);
    g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerRadius * 2.0f, innerRadius * 2.0f);

    // 4. Subtle Radial Track (Background arc)
    juce::Path bgArc;
    bgArc.addCentredArc(center.x, center.y, radius * 0.94f, radius * 0.94f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff232630));
    g.strokePath(bgArc, juce::PathStrokeType(2.0f));

    // 5. Active Value Arc (Glow Amber)
    if (sliderPosProportional > 0.0f) {
        juce::Path valArc;
        valArc.addCentredArc(center.x, center.y, radius * 0.94f, radius * 0.94f, 0.0f, rotaryStartAngle, toAngle, true);
        g.setColour(accentAmber.withAlpha(0.4f));
        g.strokePath(valArc, juce::PathStrokeType(3.5f));
        g.setColour(accentAmber);
        g.strokePath(valArc, juce::PathStrokeType(2.0f));
    }

    // 6. Amber Indicator Notch / Pointer
    juce::Path pointer;
    auto pointerLength = innerRadius * 0.75f;
    auto pointerThickness = 2.5f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -innerRadius, pointerThickness, pointerLength * 0.45f, 1.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));

    g.setColour(accentAmber.withAlpha(0.5f));
    g.strokePath(pointer, juce::PathStrokeType(1.5f));
    g.setColour(accentAmber);
    g.fillPath(pointer);
}

void StagePianoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerSize = 5.0f;

    juce::Colour bg = backgroundColour;
    if (shouldDrawButtonAsDown) {
        bg = bg.darker(0.2f);
    } else if (shouldDrawButtonAsHighlighted) {
        bg = bg.brighter(0.1f);
    }

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, cornerSize);

    // Border
    if (button.getToggleState()) {
        g.setColour(accentNord);
        g.drawRoundedRectangle(bounds, cornerSize, 1.5f);
    } else {
        g.setColour(borderPanel);
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }
}
