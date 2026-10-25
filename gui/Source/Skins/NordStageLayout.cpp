#include "NordStageLayout.h"
#include <cmath>

namespace {

// Mathematical 7-Segment LED Digit Renderer
void draw7SegmentDigit(juce::Graphics& g, float x, float y, float w, float h, int digit,
                       juce::Colour litColour, juce::Colour unlitColour) {
    // 7-segment bitmasks for digits 0-9
    // Segments: 0=A(top), 1=B(top-right), 2=C(bottom-right), 3=D(bottom), 4=E(bottom-left), 5=F(top-left), 6=G(mid)
    static const uint8_t masks[10] = {
        0b00111111, // 0: A B C D E F
        0b00000110, // 1: B C
        0b01011011, // 2: A B G E D
        0b01001111, // 3: A B G C D
        0b01100110, // 4: F G B C
        0b01101101, // 5: A F G C D
        0b01111101, // 6: A F E D C G
        0b00000111, // 7: A B C
        0b01111111, // 8: A B C D E F G
        0b01101111  // 9: A B C D F G
    };

    uint8_t mask = (digit >= 0 && digit <= 9) ? masks[digit] : 0;
    float t = juce::jmax(2.0f, w * 0.17f);
    float gap = 0.8f;
    float halfH = (h - t) * 0.5f;

    auto drawH = [&](float sx, float sy, float sw, bool lit) {
        g.setColour(lit ? litColour : unlitColour);
        juce::Path p;
        p.startNewSubPath(sx + t * 0.5f, sy);
        p.lineTo(sx + sw - t * 0.5f, sy);
        p.lineTo(sx + sw, sy + t * 0.5f);
        p.lineTo(sx + sw - t * 0.5f, sy + t);
        p.lineTo(sx + t * 0.5f, sy + t);
        p.lineTo(sx, sy + t * 0.5f);
        p.closeSubPath();
        g.fillPath(p);
    };

    auto drawV = [&](float sx, float sy, float sh, bool lit) {
        g.setColour(lit ? litColour : unlitColour);
        juce::Path p;
        p.startNewSubPath(sx + t * 0.5f, sy);
        p.lineTo(sx + t, sy + t * 0.5f);
        p.lineTo(sx + t, sy + sh - t * 0.5f);
        p.lineTo(sx + t * 0.5f, sy + sh);
        p.lineTo(sx, sy + sh - t * 0.5f);
        p.lineTo(sx, sy + t * 0.5f);
        p.closeSubPath();
        g.fillPath(p);
    };

    // A: top
    drawH(x + t * 0.5f, y, w - t, (mask & 1) != 0);
    // B: top-right
    drawV(x + w - t, y + t * 0.5f + gap, halfH - gap, (mask & 2) != 0);
    // C: bottom-right
    drawV(x + w - t, y + halfH + t * 0.5f + gap, halfH - gap, (mask & 4) != 0);
    // D: bottom
    drawH(x + t * 0.5f, y + h - t, w - t, (mask & 8) != 0);
    // E: bottom-left
    drawV(x, y + halfH + t * 0.5f + gap, halfH - gap, (mask & 16) != 0);
    // F: top-left
    drawV(x, y + t * 0.5f + gap, halfH - gap, (mask & 32) != 0);
    // G: middle
    drawH(x + t * 0.5f, y + halfH, w - t, (mask & 64) != 0);
}

// Recessed 7-Segment Digital LED Box with Glass Filter
void draw7SegmentBox(juce::Graphics& g, juce::Rectangle<float> bounds, int number) {
    // 1. Recessed dark bezel frame
    g.setColour(juce::Colour(0xff0e0202));
    g.fillRoundedRectangle(bounds, 3.5f);
    g.setColour(juce::Colour(0xff2d0606));
    g.drawRoundedRectangle(bounds, 3.5f, 1.0f);

    // 2. Dark red glass display filter
    auto glass = bounds.reduced(2.5f);
    g.setColour(juce::Colour(0xff1f0304));
    g.fillRect(glass);

    // 3. Grid line overlay on glass
    g.setColour(juce::Colour(0x15ff2222));
    for (float ly = glass.getY() + 3.0f; ly < glass.getBottom(); ly += 3.0f) {
        g.drawHorizontalLine(static_cast<int>(ly), glass.getX(), glass.getRight());
    }

    // 4. Digits
    auto digitArea = glass.reduced(3.0f, 2.5f);
    juce::Colour litCol = juce::Colour(0xffff2222);
    juce::Colour unlitCol = juce::Colour(0xff330607);

    int d1 = (number >= 10) ? (number / 10) % 10 : -1;
    int d2 = std::abs(number) % 10;

    if (d1 >= 0) {
        float dw = (digitArea.getWidth() - 2.0f) * 0.5f;
        draw7SegmentDigit(g, digitArea.getX(), digitArea.getY(), dw, digitArea.getHeight(), d1, litCol, unlitCol);
        draw7SegmentDigit(g, digitArea.getX() + dw + 2.0f, digitArea.getY(), dw, digitArea.getHeight(), d2, litCol, unlitCol);
    } else {
        float dw = digitArea.getWidth() * 0.65f;
        float dx = digitArea.getCentreX() - dw * 0.5f;
        draw7SegmentDigit(g, dx, digitArea.getY(), dw, digitArea.getHeight(), d2, litCol, unlitCol);
    }
}

// Vertical LED Ladder Meter
void drawLedLadder(juce::Graphics& g, juce::Rectangle<float> bounds, int totalLeds, int litLeds, juce::Colour litColour) {
    g.setColour(juce::Colour(0xff090a0d));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colour(0xff1e2129));
    g.drawRoundedRectangle(bounds, 2.0f, 0.8f);

    auto inner = bounds.reduced(1.5f);
    float ledH = (inner.getHeight() - (totalLeds - 1) * 1.5f) / static_cast<float>(totalLeds);
    float ledW = inner.getWidth();

    for (int i = 0; i < totalLeds; ++i) {
        float ly = inner.getBottom() - (i + 1) * ledH - i * 1.5f;
        bool isLit = (i < litLeds);
        g.setColour(isLit ? litColour : litColour.withAlpha(0.12f));
        g.fillRect(inner.getX(), ly, ledW, ledH);
        if (isLit) {
            g.setColour(litColour.withAlpha(0.35f));
            g.drawRect(inner.getX() - 0.5f, ly - 0.5f, ledW + 1.0f, ledH + 1.0f, 0.5f);
        }
    }
}

// Radial White Silkscreen Calibration Ticks for Knobs
void drawNordKnobTicks(juce::Graphics& g, float cx, float cy, float radius, float startAngle, float endAngle,
                       int numTicks, const juce::StringArray& labels = {}) {
    g.setColour(juce::Colour(0xffe2e8f0)); // Crisp white silkscreen
    for (int i = 0; i < numTicks; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(numTicks - 1);
        float angle = startAngle + t * (endAngle - startAngle);
        float cosA = std::cos(angle - juce::MathConstants<float>::halfPi);
        float sinA = std::sin(angle - juce::MathConstants<float>::halfPi);

        bool isMajor = (i == 0 || i == (numTicks - 1) || i == (numTicks / 2));
        float rInner = radius + 2.0f;
        float rOuter = radius + (isMajor ? 5.5f : 3.0f);

        g.drawLine(cx + cosA * rInner, cy + sinA * rInner,
                   cx + cosA * rOuter, cy + sinA * rOuter, isMajor ? 1.3f : 0.8f);

        if (isMajor && !labels.isEmpty()) {
            int labelIdx = (i == 0) ? 0 : (i == numTicks - 1 ? 2 : 1);
            if (labelIdx < labels.size() && labels[labelIdx].isNotEmpty()) {
                float rLabel = rOuter + 6.0f;
                float lx = cx + cosA * rLabel;
                float ly = cy + sinA * rLabel;
                if (i == (numTicks / 2)) {
                    ly -= 3.0f;
                }
                g.setFont(juce::FontOptions(7.5f, juce::Font::bold));
                g.drawText(labels[labelIdx],
                           static_cast<int>(lx - 15.0f),
                           static_cast<int>(ly - 6.0f),
                           30, 12, juce::Justification::centred);
            }
        }
    }
}

// Skeuomorphic Screw Head in panel corners
void drawChassisScrew(juce::Graphics& g, float cx, float cy) {
    g.setColour(juce::Colour(0xff1a1c22));
    g.fillEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f);
    g.setColour(juce::Colour(0xff4b5563));
    g.drawEllipse(cx - 3.5f, cy - 3.5f, 7.0f, 7.0f, 0.8f);
    g.setColour(juce::Colour(0xff111317));
    g.drawLine(cx - 2.0f, cy, cx + 2.0f, cy, 0.9f);
    g.drawLine(cx, cy - 2.0f, cx, cy + 2.0f, 0.9f);
}

// Raised Tactile Push Button with LED Dot
void drawTactileButton(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text,
                       bool isDown, bool isLedLit, juce::Colour ledColour = juce::Colour(0xffff2222)) {
    // 3D Beveled Button Body
    juce::Colour topCol = isDown ? juce::Colour(0xff131417) : juce::Colour(0xff22242b);
    juce::Colour botCol = isDown ? juce::Colour(0xff090a0d) : juce::Colour(0xff121316);
    juce::ColourGradient grad(topCol, bounds.getX(), bounds.getY(), botCol, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, 3.0f);

    g.setColour(isDown ? juce::Colour(0xff090a0c) : juce::Colour(0xff3f4452));
    g.drawRoundedRectangle(bounds, 3.0f, 0.9f);

    // Glowing LED Dot above button
    float ledRadius = 2.2f;
    float lx = bounds.getCentreX();
    float ly = bounds.getY() - 4.5f;

    g.setColour(isLedLit ? ledColour : juce::Colour(0xff3f0a0d));
    g.fillEllipse(lx - ledRadius, ly - ledRadius, ledRadius * 2.0f, ledRadius * 2.0f);
    if (isLedLit) {
        g.setColour(ledColour.withAlpha(0.5f));
        g.drawEllipse(lx - ledRadius - 1.0f, ly - ledRadius - 1.0f, (ledRadius + 1.0f) * 2.0f, (ledRadius + 1.0f) * 2.0f, 0.8f);
    }

    // Button label
    g.setColour(juce::Colour(0xffe2e8f0));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText(text, bounds.toNearestInt(), juce::Justification::centred);
}

} // namespace

void NordStageLayout::paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) {
    // 1. Clavia Nord Red Matte Chassis Base with Brushed Texture
    g.setColour(juce::Colour(0xff7d1519));
    g.fillRect(bounds);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff8c181d), 0.0f, 0.0f,
        juce::Colour(0xff600f13), 0.0f, static_cast<float>(bounds.getHeight()), false
    ));
    g.fillRect(bounds);

    // Classic Clavia Wooden Cheeks (Left & Right)
    auto drawWoodenCheek = [&](int x) {
        juce::ColourGradient woodGrad(juce::Colour(0xff4a1f12), static_cast<float>(x), 0.0f,
                                      juce::Colour(0xff1f0b06), static_cast<float>(x + 12), 0.0f, false);
        g.setGradientFill(woodGrad);
        g.fillRect(x, 0, 12, bounds.getHeight());
        // Subtle vertical wood grain lines
        g.setColour(juce::Colour(0x18000000));
        g.drawVerticalLine(x + 3, 0.0f, static_cast<float>(bounds.getHeight()));
        g.drawVerticalLine(x + 7, 0.0f, static_cast<float>(bounds.getHeight()));
        g.setColour(juce::Colour(0xff120603));
        g.drawVerticalLine(x == 0 ? 11 : x, 0.0f, static_cast<float>(bounds.getHeight()));
    };
    drawWoodenCheek(0);
    drawWoodenCheek(bounds.getRight() - 12);

    // 2. Header Strip
    auto headerBounds = bounds.removeFromTop(80).reduced(14, 0);
    g.setColour(juce::Colour(0xff121316));
    g.fillRoundedRectangle(headerBounds.toFloat().reduced(2.0f, 5.0f), 4.0f);
    g.setColour(juce::Colour(0xff2d2e36));
    g.drawRoundedRectangle(headerBounds.toFloat().reduced(2.0f, 5.0f), 4.0f, 1.0f);

    // Screws in header corners
    drawChassisScrew(g, 22.0f, 14.0f);
    drawChassisScrew(g, 22.0f, 68.0f);
    drawChassisScrew(g, bounds.getRight() - 22.0f, 14.0f);
    drawChassisScrew(g, bounds.getRight() - 22.0f, 68.0f);

    // Nord Red Badge Strip
    g.setColour(juce::Colour(0xffc52227));
    g.fillRoundedRectangle(30.0f, 18.0f, 6.0f, 44.0f, 2.0f);

    // Bauhaus / Rounded Typography Title
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("stage piano studio", 44, 18, 200, 24, juce::Justification::left);
    g.setColour(juce::Colour(0xffc52227));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("HD V1.3.7 - DFD STREAMING HOST", 44, 42, 200, 16, juce::Justification::left);

    // Central High-Contrast OLED / LCD Screen (Blue Framed with Glowing Cyan Text)
    auto screenBox = juce::Rectangle<int>(220, 12, 260, 56);
    g.setColour(juce::Colour(0xff03080e)); // Dark midnight LCD glass
    g.fillRoundedRectangle(screenBox.toFloat(), 5.0f);
    g.setColour(juce::Colour(0xff0284c7)); // Cyan Metallic Bezel
    g.drawRoundedRectangle(screenBox.toFloat(), 5.0f, 1.2f);

    // Screen scanlines
    g.setColour(juce::Colour(0x100284c7));
    for (int y = screenBox.getY() + 3; y < screenBox.getBottom(); y += 3) {
        g.drawHorizontalLine(y, static_cast<float>(screenBox.getX() + 2), static_cast<float>(screenBox.getRight() - 2));
    }

    // Cyan glowing program & layer text
    g.setColour(juce::Colour(0xff38bdf8)); // Bright Cyan Title
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText("Program " + juce::String(ctx.currentProgram), screenBox.getX() + 10, screenBox.getY() + 6, 120, 18, juce::Justification::left);

    g.setColour(juce::Colour(0xffa5f3fc));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("[PIANO] " + ctx.pianoName, screenBox.getX() + 10, screenBox.getY() + 25, 155, 14, juce::Justification::left);
    g.drawText("[SYNTH] " + ctx.synthName, screenBox.getX() + 10, screenBox.getY() + 39, 155, 14, juce::Justification::left);

    // Live Telemetry & Octave inside Screen Box (Right aligned)
    g.setColour(ctx.isLearningLowestC ? juce::Colour(0xfff59e0b) : juce::Colour(0xff38bdf8));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    juce::String octInfo = ctx.isLearningLowestC ? "CALIBRATING C" : (juce::String("OCT: ") + (ctx.globalOctave > 0 ? "+" : "") + juce::String(ctx.globalOctave));
    g.drawText(octInfo, screenBox.getRight() - 95, screenBox.getY() + 6, 88, 12, juce::Justification::right);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f));
    juce::String telem = "DSP: " + juce::String(ctx.dspLoad, 1) + "%\nDFD: " + juce::String(ctx.dfdLoad) + "%\nVOICES: " + juce::String(ctx.activeVoices);
    g.drawMultiLineText(telem, screenBox.getRight() - 95, screenBox.getY() + 20, 88, juce::Justification::right);

    // 3. Footer Strip
    auto footerBounds = bounds.removeFromBottom(50);
    g.setColour(juce::Colour(0xff121316));
    g.fillRect(footerBounds);
    g.setColour(juce::Colour(0xff2d2e36));
    g.drawHorizontalLine(footerBounds.getY(), 0.0f, static_cast<float>(bounds.getWidth()));

    bool hasMidi = (ctx.midiDevice != "No MIDI input active (click CONFIG)");
    g.setColour(hasMidi ? juce::Colour(0xff10b981) : juce::Colour(0xffef4444));
    g.fillEllipse(24.0f, footerBounds.getY() + 21.0f, 8.0f, 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MIDI: " + ctx.midiDevice, 38, footerBounds.getY() + 16, 320, 18, juce::Justification::left);

    g.setColour(ctx.sustainDown ? juce::Colour(0xfffbbf24) : juce::Colour(0xff64748b));
    g.drawText(ctx.sustainDown ? "SUSTAIN: [DOWN]" : "SUSTAIN: [UP]", 370, footerBounds.getY() + 16, 110, 18, juce::Justification::left);

    // 4. Main Hardware Module Cards
    auto contentBounds = bounds.removeFromTop(385).reduced(16, 6);
    int colWidth = (contentBounds.getWidth() - 36) / 4;

    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    auto drawModuleCard = [&g](juce::Rectangle<int> r, const juce::String& title, const juce::String& tag, bool showRedHeader = true) {
        g.setColour(juce::Colour(0xff141519));
        g.fillRoundedRectangle(r.toFloat(), 5.0f);
        g.setColour(juce::Colour(0xff2a2c35));
        g.drawRoundedRectangle(r.toFloat(), 5.0f, 1.0f);

        // Screws in panel corners
        drawChassisScrew(g, static_cast<float>(r.getX() + 6), static_cast<float>(r.getY() + 6));
        drawChassisScrew(g, static_cast<float>(r.getRight() - 6), static_cast<float>(r.getY() + 6));
        drawChassisScrew(g, static_cast<float>(r.getX() + 6), static_cast<float>(r.getBottom() - 6));
        drawChassisScrew(g, static_cast<float>(r.getRight() - 6), static_cast<float>(r.getBottom() - 6));

        // Header banner inside panel
        auto hRect = juce::Rectangle<float>(r.getX() + 2.0f, r.getY() + 2.0f, r.getWidth() - 4.0f, 26.0f);
        g.setColour(showRedHeader ? juce::Colour(0xff8a181c) : juce::Colour(0xff1e2027));
        g.fillRoundedRectangle(hRect, 4.0f);

        // Status LED on header
        float ledR = 2.5f;
        g.setColour(juce::Colour(0xffff2222));
        g.fillEllipse(r.getX() + 12.0f - ledR, r.getY() + 15.0f - ledR, ledR * 2.0f, ledR * 2.0f);

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, r.getX() + 22, r.getY() + 7, 180, 16, juce::Justification::left);

        g.setColour(juce::Colour(0xffe2e8f0));
        g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
        g.drawText(tag, r.getRight() - 85, r.getY() + 7, 75, 16, juce::Justification::right);
    };

    drawModuleCard(r1, "EQ PIANO & COMP", "ACOUSTIC", true);
    drawModuleCard(r2, "PIANO SECTION", "SLOT A", true);
    drawModuleCard(r3, "SYNTH & PROGRAMS", "SLOT B", true);
    drawModuleCard(r4, "EFFECTS & MASTER", "STUDIO FX", true);

    // --- CARD 1: EQ PIANO & COMPRESSOR ---
    // Section sub-header
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("TREBLE", r1.getX() + 10, r1.getY() + 40, 70, 12, juce::Justification::left);
    g.drawText("MID",    r1.getX() + 96, r1.getY() + 40, 70, 12, juce::Justification::left);
    g.drawText("BASS",   r1.getX() + 182, r1.getY() + 40, 70, 12, juce::Justification::left);

    // Radial Knob Ticks for EQ Piano
    float startA = juce::MathConstants<float>::pi * 1.25f;
    float endA   = juce::MathConstants<float>::pi * 2.75f;
    juce::StringArray eqLabels = { "-15", "0", "+15" };
    drawNordKnobTicks(g, r1.getX() + 44.0f,  r1.getY() + 82.0f, 20.0f, startA, endA, 9, eqLabels);
    drawNordKnobTicks(g, r1.getX() + 130.0f, r1.getY() + 82.0f, 20.0f, startA, endA, 9, eqLabels);
    drawNordKnobTicks(g, r1.getX() + 216.0f, r1.getY() + 82.0f, 20.0f, startA, endA, 9, eqLabels);

    // Horizontal Divider
    g.setColour(juce::Colour(0xff22242c));
    g.drawHorizontalLine(r1.getY() + 155, static_cast<float>(r1.getX() + 8), static_cast<float>(r1.getRight() - 8));

    // Studio Compressor
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("COMPRESSOR", r1.getX() + 12, r1.getY() + 162, 120, 14, juce::Justification::left);
    g.drawText("THRESH", r1.getX() + 24, r1.getY() + 182, 60, 12, juce::Justification::centred);
    g.drawText("MAKEUP", r1.getX() + 104, r1.getY() + 182, 60, 12, juce::Justification::centred);

    // Gain Reduction Meter in Card 1
    auto meterBox = juce::Rectangle<int>(r1.getX() + 12, r1.getY() + 265, r1.getWidth() - 24, 24);
    g.setColour(juce::Colour(0xff090a0d));
    g.fillRoundedRectangle(meterBox.toFloat(), 4.0f);
    float gr = ctx.gainReductionDb;
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("GR: -" + juce::String(gr, 1) + " dB", meterBox.getX() + 6, meterBox.getY() + 2, 90, 10, juce::Justification::left);

    int numLeds = 12;
    int litLeds = juce::jlimit(0, numLeds, static_cast<int>(gr * 0.8f));
    int ledW = (meterBox.getWidth() - 12 - (numLeds * 2)) / numLeds;
    for (int i = 0; i < numLeds; ++i) {
        int lx = meterBox.getX() + 6 + i * (ledW + 2);
        int ly = meterBox.getY() + 12;
        bool lit = (i < litLeds);
        g.setColour(lit ? (i > 8 ? juce::Colour(0xffef4444) : juce::Colour(0xfff59e0b)) : juce::Colour(0xff222530));
        g.fillRect(lx, ly, ledW, 8);
    }

    // Touch curve label
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("TOUCH CURVE (VELOCITY)", r1.getX() + 12, r1.getY() + 312, 180, 14, juce::Justification::left);

    // --- CARD 2: PIANO SECTION (SLOT A) ---
    // 7-Segment LED Model Display + Dual Vertical LED Ladders
    float pDisX = r2.getX() + 14.0f;
    float pDisY = r2.getY() + 36.0f;
    float pDisW = 40.0f;
    float pDisH = 34.0f;

    // Left Ladder (Purple/Magenta)
    drawLedLadder(g, juce::Rectangle<float>(pDisX - 8.0f, pDisY, 5.0f, pDisH), 6, 4, juce::Colour(0xffd946ef));
    // 7-Segment Box showing Model Index
    int pModel = juce::jlimit(1, 99, ctx.pianoModelIndex > 0 ? ctx.pianoModelIndex : 1);
    draw7SegmentBox(g, juce::Rectangle<float>(pDisX, pDisY, pDisW, pDisH), pModel);
    // Right Ladder (Bright Red)
    drawLedLadder(g, juce::Rectangle<float>(pDisX + pDisW + 3.0f, pDisY, 5.0f, pDisH), 6, 5, juce::Colour(0xffff2222));

    // Label: MODEL
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("MODEL", static_cast<int>(pDisX), static_cast<int>(pDisY + pDisH + 2.0f), static_cast<int>(pDisW), 12, juce::Justification::centred);

    // Level label above tall fader
    g.drawText("LEVEL", r2.getRight() - 68, r2.getY() + 54, 60, 12, juce::Justification::centred);

    // Acoustic Timbre & String Resonance Header
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("ACOUSTIC TIMBRE", r2.getX() + 14, r2.getY() + 165, 150, 14, juce::Justification::left);

    // --- CARD 3: SYNTH & PROGRAMS (SLOT B + CENTRAL CONTROLS) ---
    // 7-Segment LED Sample Display + Dual Vertical LED Ladders
    float sDisX = r3.getX() + 14.0f;
    float sDisY = r3.getY() + 36.0f;
    float sDisW = 40.0f;
    float sDisH = 34.0f;

    drawLedLadder(g, juce::Rectangle<float>(sDisX - 8.0f, sDisY, 5.0f, sDisH), 6, 3, juce::Colour(0xff38bdf8));
    int sSample = juce::jlimit(1, 99, ctx.synthSampleIndex > 0 ? ctx.synthSampleIndex : 1);
    draw7SegmentBox(g, juce::Rectangle<float>(sDisX, sDisY, sDisW, sDisH), sSample);
    drawLedLadder(g, juce::Rectangle<float>(sDisX + sDisW + 3.0f, sDisY, 5.0f, sDisH), 6, 4, juce::Colour(0xffff2222));

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("SAMPLE", static_cast<int>(sDisX), static_cast<int>(sDisY + sDisH + 2.0f), static_cast<int>(sDisW), 12, juce::Justification::centred);

    g.drawText("CUTOFF", r3.getX() + 115, r3.getY() + 64, 60, 12, juce::Justification::centred);
    g.drawText("LEVEL",  r3.getRight() - 68, r3.getY() + 54, 60, 12, juce::Justification::centred);

    // Radial ticks around VCF Cutoff knob
    drawNordKnobTicks(g, r3.getX() + 145.0f, r3.getY() + 112.0f, 20.0f, startA, endA, 9, { "40Hz", "1kHz", "18kHz" });

    // Synth EQ Header
    g.drawText("SYNTH 3-BAND EQ", r3.getX() + 12, r3.getY() + 165, 140, 14, juce::Justification::left);

    // Center Programs Bank Box (Classic 1..4 and 5..8 push switches)
    auto progBank = juce::Rectangle<float>(r3.getX() + 10, r3.getY() + 276, r3.getWidth() - 20, 78);
    g.setColour(juce::Colour(0xff111216));
    g.fillRoundedRectangle(progBank, 4.0f);
    g.setColour(juce::Colour(0xff22242c));
    g.drawRoundedRectangle(progBank, 4.0f, 1.0f);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(8.5f, juce::Font::bold));
    g.drawText("PROGRAM BANK (1-8)", progBank.getX() + 8, progBank.getY() + 4, 130, 12, juce::Justification::left);

    // --- CARD 4: EFFECTS & MASTER ---
    // Reverb, Modulation & Ambient Drone Engine
    g.setColour(juce::Colour(0xffcbd5e1));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("REVERB (VINTAGE HALL)", r4.getX() + 14, r4.getY() + 38, 160, 14, juce::Justification::left);

    // Reverb Knob Ticks
    drawNordKnobTicks(g, r4.getX() + 142.0f, r4.getY() + 65.0f, 20.0f, startA, endA, 9, { "0%", "50%", "100%" });

    // LED Ladders for Chorus & Delay
    drawLedLadder(g, juce::Rectangle<float>(r4.getX() + 175.0f, r4.getY() + 222.0f, 6.0f, 44.0f), 6, 4, juce::Colour(0xff38bdf8));
    drawLedLadder(g, juce::Rectangle<float>(r4.getX() + 175.0f, r4.getY() + 278.0f, 6.0f, 44.0f), 6, 3, juce::Colour(0xffd946ef));

    // Ambient Drone Section
    g.drawText("AMBIENT DRONE ENGINE", r4.getX() + 14, r4.getY() + 120, 180, 14, juce::Justification::left);

    // 5. Classic Clavia Crimson Red Felt Trim above 61-Key Keyboard
    g.setColour(juce::Colour(0xffb91c1c));
    g.fillRect(16, 477, bounds.getWidth() - 32, 4);
    // Subtle velvet highlight line
    g.setColour(juce::Colour(0xffdc2626));
    g.drawHorizontalLine(478, 16.0f, static_cast<float>(bounds.getWidth() - 16));

    // Keyboard chassis frame
    g.setColour(juce::Colour(0xff222530));
    g.drawRoundedRectangle(15.0f, 480.0f, static_cast<float>(bounds.getWidth() - 30), 226.0f, 6.0f, 1.5f);
}

void NordStageLayout::layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) {
    bounds.removeFromTop(80);
    auto footerArea = bounds.removeFromBottom(50);
    auto cardArea = bounds.removeFromTop(385);
    auto contentBounds = cardArea.reduced(16, 6);

    int colWidth = (contentBounds.getWidth() - 36) / 4;
    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    // Header buttons (Right aligned, perfectly non-overlapping)
    if (comps.btnPrevPreset)  comps.btnPrevPreset->setBounds(486, 22, 32, 36);
    if (comps.btnNextPreset)  comps.btnNextPreset->setBounds(522, 22, 32, 36);
    if (comps.btnSavePreset)  comps.btnSavePreset->setBounds(558, 22, 72, 36);
    if (comps.btnLearnC)      comps.btnLearnC->setBounds(634, 22, 94, 36);
    if (comps.btnOctaveDown)  comps.btnOctaveDown->setBounds(732, 22, 42, 36);
    if (comps.btnOctaveUp)    comps.btnOctaveUp->setBounds(778, 22, 42, 36);
    if (comps.comboTranspose) comps.comboTranspose->setBounds(824, 22, 86, 36);
    if (comps.btnSettings)    comps.btnSettings->setBounds(914, 22, 88, 36);
    if (comps.btnPanic)       comps.btnPanic->setBounds(1006, 22, 54, 36);
    if (comps.comboTheme)     comps.comboTheme->setBounds(1064, 22, 140, 36);

    // Footer buttons
    if (comps.btnFooterConfig) comps.btnFooterConfig->setBounds(footerArea.getRight() - 95, footerArea.getY() + 10, 85, 30);
    if (comps.btnMidiLearn)    comps.btnMidiLearn->setBounds(footerArea.getRight() - 250, footerArea.getY() + 10, 145, 30);
    if (comps.btnImport)       comps.btnImport->setBounds(footerArea.getRight() - 415, footerArea.getY() + 10, 155, 30);
    if (comps.btnRefresh)      comps.btnRefresh->setBounds(footerArea.getRight() - 510, footerArea.getY() + 10, 85, 30);

    // --- Section 1: EQ Piano & Compressor ---
    // 3 EQ Knobs
    if (comps.sliderPianoEqHigh) comps.sliderPianoEqHigh->setBounds(r1.getX() + 14,  r1.getY() + 54, 60, 58);
    if (comps.sliderPianoEqMid)  comps.sliderPianoEqMid->setBounds(r1.getX() + 100, r1.getY() + 54, 60, 58);
    if (comps.sliderPianoEqLow)  comps.sliderPianoEqLow->setBounds(r1.getX() + 186, r1.getY() + 54, 60, 58);

    // Compressor Knobs
    if (comps.sliderCompThresh) comps.sliderCompThresh->setBounds(r1.getX() + 24,  r1.getY() + 196, 60, 60);
    if (comps.sliderCompMakeup) comps.sliderCompMakeup->setBounds(r1.getX() + 104, r1.getY() + 196, 60, 60);

    if (comps.comboVelocityCurve) comps.comboVelocityCurve->setBounds(r1.getX() + 12, r1.getY() + 328, r1.getWidth() - 24, 26);

    // --- Section 2: Piano Section (Slot A) ---
    if (comps.comboPiano) comps.comboPiano->setBounds(r2.getX() + 66, r2.getY() + 38, r2.getWidth() - 146, 28);
    if (comps.btnPrevPiano) comps.btnPrevPiano->setBounds(r2.getRight() - 74, r2.getY() + 38, 28, 28);
    if (comps.btnNextPiano) comps.btnNextPiano->setBounds(r2.getRight() - 42, r2.getY() + 38, 28, 28);

    if (comps.togglePiano) comps.togglePiano->setBounds(r2.getX() + 14, r2.getY() + 86, 110, 24);
    // Tall Pro Console Volume Fader (180px tall throw)
    if (comps.sliderPianoVol) comps.sliderPianoVol->setBounds(r2.getRight() - 68, r2.getY() + 70, 60, 180);

    // Interactive Acoustic Timbre Buttons
    if (comps.toggleStrRes) comps.toggleStrRes->setBounds(r2.getX() + 14, r2.getY() + 185, 80, 26);
    if (comps.toggleSoft) comps.toggleSoft->setBounds(r2.getX() + 100, r2.getY() + 185, 80, 26);

    // --- Section 3: Synth Section (Slot B) ---
    if (comps.comboSynth) comps.comboSynth->setBounds(r3.getX() + 66, r3.getY() + 38, r3.getWidth() - 146, 28);
    if (comps.btnPrevSynth) comps.btnPrevSynth->setBounds(r3.getRight() - 74, r3.getY() + 38, 28, 28);
    if (comps.btnNextSynth) comps.btnNextSynth->setBounds(r3.getRight() - 42, r3.getY() + 38, 28, 28);

    if (comps.toggleSynth) comps.toggleSynth->setBounds(r3.getX() + 14, r3.getY() + 86, 95, 24);
    if (comps.sliderSynthCutoff) comps.sliderSynthCutoff->setBounds(r3.getX() + 115, r3.getY() + 80, 60, 60);
    // Tall Pro Console Volume Fader (180px tall throw)
    if (comps.sliderSynthVol) comps.sliderSynthVol->setBounds(r3.getRight() - 68, r3.getY() + 70, 60, 180);

    // Synth EQ Knobs
    if (comps.sliderSynthEqLow)  comps.sliderSynthEqLow->setBounds(r3.getX() + 8,   r3.getY() + 182, 60, 50);
    if (comps.sliderSynthEqMid)  comps.sliderSynthEqMid->setBounds(r3.getX() + 74,  r3.getY() + 182, 60, 50);
    if (comps.sliderSynthEqHigh) comps.sliderSynthEqHigh->setBounds(r3.getX() + 140, r3.getY() + 182, 60, 50);

    // Synth toggles
    if (comps.toggleLngRel) comps.toggleLngRel->setBounds(r3.getX() + 14, r3.getY() + 242, 85, 22);
    if (comps.toggleNoSens) comps.toggleNoSens->setBounds(r3.getX() + 105, r3.getY() + 242, 85, 22);

    // Program Bank (1..8) Interactive Push Buttons
    for (int i = 0; i < 4; ++i) {
        if (comps.btnPrograms[i]) {
            comps.btnPrograms[i]->setBounds(r3.getX() + 16 + i * 44, r3.getY() + 296, 38, 24);
        }
        if (comps.btnPrograms[i + 4]) {
            comps.btnPrograms[i + 4]->setBounds(r3.getX() + 16 + i * 44, r3.getY() + 324, 38, 24);
        }
    }

    // --- Section 4: Effects & Master ---
    if (comps.toggleReverb) comps.toggleReverb->setBounds(r4.getX() + 14, r4.getY() + 54, 90, 24);
    if (comps.sliderReverbMix) comps.sliderReverbMix->setBounds(r4.getX() + 112, r4.getY() + 45, 60, 58);

    // Drone controls
    if (comps.btnDroneToggle) comps.btnDroneToggle->setBounds(r4.getX() + 14, r4.getY() + 138, r4.getWidth() - 28, 30);
    if (comps.comboDroneRoot) comps.comboDroneRoot->setBounds(r4.getX() + 14, r4.getY() + 174, (r4.getWidth() - 34) / 3, 24);
    if (comps.comboDroneSound) comps.comboDroneSound->setBounds(r4.getX() + 18 + (r4.getWidth() - 34) / 3, r4.getY() + 174, (r4.getWidth() - 34) * 2 / 3, 24);

    // Modulation
    if (comps.toggleChorus) comps.toggleChorus->setBounds(r4.getX() + 14, r4.getY() + 230, 85, 24);
    if (comps.sliderChorusMix) comps.sliderChorusMix->setBounds(r4.getX() + 105, r4.getY() + 220, 65, 48);

    if (comps.toggleDelay) comps.toggleDelay->setBounds(r4.getX() + 14, r4.getY() + 285, 85, 24);
    if (comps.sliderDelayMix) comps.sliderDelayMix->setBounds(r4.getX() + 105, r4.getY() + 275, 65, 48);

    // Tall Drone Volume Fader (140px tall throw)
    if (comps.sliderDroneVol) comps.sliderDroneVol->setBounds(r4.getRight() - 68, r4.getY() + 215, 60, 140);

    // Virtual Keyboard
    int kbX = 18;
    int kbY = 483;
    int kbW = bounds.getWidth() - 36;
    int kbH = 220;
    if (comps.keyboardComponent) {
        comps.keyboardComponent->setBounds(kbX, kbY, kbW, kbH);
    }
}
