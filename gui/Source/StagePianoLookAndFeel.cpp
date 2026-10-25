#include "StagePianoLookAndFeel.h"

juce::Colour StagePianoLookAndFeel::bgChassis   = juce::Colour(0xff121317);
juce::Colour StagePianoLookAndFeel::bgPanel     = juce::Colour(0xff181a20);
juce::Colour StagePianoLookAndFeel::borderPanel = juce::Colour(0xff272a33);
juce::Colour StagePianoLookAndFeel::accentAmber = juce::Colour(0xfff59e0b);
juce::Colour StagePianoLookAndFeel::accentNord  = juce::Colour(0xffd02b2b);
juce::Colour StagePianoLookAndFeel::textLight   = juce::Colour(0xfff1f5f9);
juce::Colour StagePianoLookAndFeel::textDim     = juce::Colour(0xff94a3b8);

StagePianoLookAndFeel::StagePianoLookAndFeel() {
    setTheme(Theme::NordStudio);
}

void StagePianoLookAndFeel::setTheme(Theme theme) {
    m_theme = theme;
    switch (theme) {
        case Theme::NordStudio:
            bgChassis   = juce::Colour(0xff121317);
            bgPanel     = juce::Colour(0xff181a20);
            borderPanel = juce::Colour(0xff272a33);
            accentAmber = juce::Colour(0xfff59e0b);
            accentNord  = juce::Colour(0xffd02b2b);
            break;
        case Theme::YamahaMotif:
            bgChassis   = juce::Colour(0xff0d131f);
            bgPanel     = juce::Colour(0xff141c2c);
            borderPanel = juce::Colour(0xff222e44);
            accentAmber = juce::Colour(0xff38bdf8);
            accentNord  = juce::Colour(0xff2563eb);
            break;
        case Theme::RolandStage:
            bgChassis   = juce::Colour(0xff09090b);
            bgPanel     = juce::Colour(0xff121215);
            borderPanel = juce::Colour(0xff27272a);
            accentAmber = juce::Colour(0xffeab308);
            accentNord  = juce::Colour(0xffb91c1c);
            break;
    }

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

    if (m_theme == Theme::NordStudio) {
        // --- Authentic Clavia Nord Hardware Fluted Knob ---
        // 1. Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.65f));
        g.fillEllipse(center.x - radius, center.y - radius + 2.0f, radius * 2.0f, radius * 2.0f);

        // 2. Fluted outer gear / ridges (side tactile grip notches)
        int numRidges = 18;
        for (int i = 0; i < numRidges; ++i) {
            float angle = (static_cast<float>(i) / numRidges) * juce::MathConstants<float>::twoPi;
            float rx = center.x + std::cos(angle) * (radius - 1.2f);
            float ry = center.y + std::sin(angle) * (radius - 1.2f);
            g.setColour(juce::Colour(0xff0d0e12));
            g.fillEllipse(rx - 1.8f, ry - 1.8f, 3.6f, 3.6f);
        }

        // 3. Beveled Body Ring
        juce::ColourGradient bodyGrad(juce::Colour(0xff2d2f38), center.x - radius, center.y - radius,
                                      juce::Colour(0xff141518), center.x + radius, center.y + radius, false);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(center.x - radius + 1.5f, center.y - radius + 1.5f, (radius - 1.5f) * 2.0f, (radius - 1.5f) * 2.0f);

        // 4. Nord Red Inner Ring (signature Clavia hardware badge detail)
        float redRingRadius = radius * 0.85f;
        g.setColour(juce::Colour(0xff991b1e));
        g.drawEllipse(center.x - redRingRadius, center.y - redRingRadius, redRingRadius * 2.0f, redRingRadius * 2.0f, 1.4f);

        // 5. Active Value Track Arc
        if (sliderPosProportional > 0.0f) {
            juce::Path valArc;
            valArc.addCentredArc(center.x, center.y, radius * 0.95f, radius * 0.95f, 0.0f, rotaryStartAngle, toAngle, true);
            g.setColour(juce::Colour(0xffef4444).withAlpha(0.4f));
            g.strokePath(valArc, juce::PathStrokeType(3.5f));
            g.setColour(juce::Colour(0xffef4444));
            g.strokePath(valArc, juce::PathStrokeType(1.8f));
        }

        // 6. Flat Matte Black Top Cap
        float capRadius = radius * 0.76f;
        juce::ColourGradient capGrad(juce::Colour(0xff22242a), center.x, center.y - capRadius,
                                     juce::Colour(0xff0d0e11), center.x, center.y + capRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f);

        // Cap Bevel
        g.setColour(juce::Colour(0xff474d5d));
        g.drawEllipse(center.x - capRadius, center.y - capRadius, capRadius * 2.0f, capRadius * 2.0f, 1.0f);

        // 7. Crisp White Pointer Line (Nord chickenhead / line indicator)
        juce::Path pointer;
        float ptrLen = capRadius * 0.82f;
        pointer.addRoundedRectangle(-1.25f, -capRadius, 2.5f, ptrLen, 0.8f);
        pointer.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center.x, center.y));

        g.setColour(juce::Colours::white);
        g.fillPath(pointer);
        return;
    }

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

void StagePianoLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                            const juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/) {
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
    float centerX = bounds.getCentreX();

    // 1. Calibration notch lines on left & right with dB silkscreen
    g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
    static const char* dbLabels[7] = { "+6", "0", "-6", "-12", "-18", "-24", "-inf" };
    for (int i = 0; i <= 6; ++i) {
        float ly = bounds.getY() + 10.0f + i * ((bounds.getHeight() - 20.0f) / 6.0f);
        bool isZero = (i == 1);
        g.setColour(isZero ? juce::Colour(0xffef4444) : juce::Colour(0xff475569));
        g.drawHorizontalLine(static_cast<int>(ly), centerX - 15.0f, centerX - 8.0f);
        g.drawHorizontalLine(static_cast<int>(ly), centerX + 8.0f, centerX + 15.0f);

        // dB text cleanly within component left margin
        g.setColour(isZero ? juce::Colour(0xfff87171) : juce::Colour(0xff94a3b8));
        g.drawText(dbLabels[i], static_cast<int>(bounds.getX()), static_cast<int>(ly - 5.0f),
                   static_cast<int>(centerX - 17.0f), 10, juce::Justification::right);
    }

    // 2. Center recessed track (pro audio metallic groove)
    auto trackRect = juce::Rectangle<float>(centerX - 3.0f, bounds.getY() + 6.0f, 6.0f, bounds.getHeight() - 12.0f);
    g.setColour(juce::Colour(0xff06070a));
    g.fillRoundedRectangle(trackRect, 3.0f);
    g.setColour(juce::Colour(0xff1f222a));
    g.drawRoundedRectangle(trackRect, 3.0f, 1.2f);

    // 3. Pro Console Fader Cap (Generous width: 36px, height: 24px)
    float capW = 36.0f;
    float capH = 22.0f;
    float capY = std::clamp(sliderPos - (capH * 0.5f), bounds.getY() + 4.0f, bounds.getBottom() - capH - 4.0f);
    auto capRect = juce::Rectangle<float>(centerX - (capW * 0.5f), capY, capW, capH);

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.75f));
    g.fillRoundedRectangle(capRect.translated(0.0f, 2.5f), 3.5f);

    // Metallic body with horizontal tactile ridges
    juce::ColourGradient capGrad(juce::Colour(0xff484e5d), capRect.getX(), capRect.getY(),
                                juce::Colour(0xff15171d), capRect.getX(), capRect.getBottom(), false);
    g.setGradientFill(capGrad);
    g.fillRoundedRectangle(capRect, 3.5f);

    // Subtle tactile grip ridges on the fader cap
    g.setColour(juce::Colour(0x25ffffff));
    g.drawHorizontalLine(static_cast<int>(capY + 5.0f), capRect.getX() + 4.0f, capRect.getRight() - 4.0f);
    g.drawHorizontalLine(static_cast<int>(capY + 17.0f), capRect.getX() + 4.0f, capRect.getRight() - 4.0f);

    // Bevel frame
    g.setColour(juce::Colour(0xff60677c));
    g.drawRoundedRectangle(capRect, 3.5f, 1.0f);

    // Center indicator line (Crisp amber Nord notch)
    g.setColour(juce::Colour(0xfffbbf24));
    g.fillRect(capRect.getX() + 4.0f, capY + (capH * 0.5f) - 1.0f, capW - 8.0f, 2.0f);
}

void StagePianoLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto cornerSize = 4.0f;

    if (m_theme == Theme::NordStudio) {
        // --- Authentic Clavia Nord Tactile Push Button with Status LED Dot ---
        juce::Colour topCol = shouldDrawButtonAsDown ? juce::Colour(0xff15161a)
                                                     : (shouldDrawButtonAsHighlighted ? juce::Colour(0xff2d2f38) : juce::Colour(0xff21232a));
        juce::Colour botCol = shouldDrawButtonAsDown ? juce::Colour(0xff0b0c0e) : juce::Colour(0xff131417);

        juce::ColourGradient btnGrad(topCol, bounds.getX(), bounds.getY(),
                                     botCol, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(btnGrad);
        g.fillRoundedRectangle(bounds, cornerSize);

        // Metallic/plastic bevel border
        g.setColour(shouldDrawButtonAsDown ? juce::Colour(0xff0a0b0d) : juce::Colour(0xff3f4452));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

        // If button is active/toggled on, draw glowing red status LED dot in top-right or center-top
        bool isLit = button.getToggleState();
        if (button.getClickingTogglesState() || isLit) {
            float ledR = 2.5f;
            float lx = bounds.getRight() - 10.0f;
            float ly = bounds.getY() + 7.0f;

            g.setColour(isLit ? juce::Colour(0xffff2222) : juce::Colour(0xff450d0f));
            g.fillEllipse(lx - ledR, ly - ledR, ledR * 2.0f, ledR * 2.0f);

            if (isLit) {
                g.setColour(juce::Colour(0xffff4444).withAlpha(0.5f));
                g.drawEllipse(lx - ledR - 1.0f, ly - ledR - 1.0f, (ledR + 1.0f) * 2.0f, (ledR + 1.0f) * 2.0f, 0.8f);
            }
        }
        return;
    }

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
