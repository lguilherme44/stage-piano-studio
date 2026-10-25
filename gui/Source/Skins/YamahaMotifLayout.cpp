#include "YamahaMotifLayout.h"

void YamahaMotifLayout::paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) {
    // 1. Midnight Metallic Navy Blue Chassis Base
    g.setColour(juce::Colour(0xff0b1120));
    g.fillRect(bounds);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff141c2e), 0.0f, 0.0f,
        juce::Colour(0xff080d18), 0.0f, static_cast<float>(bounds.getHeight()), false
    ));
    g.fillRect(bounds);

    // Brushed Silver Aluminum Side Trim
    g.setColour(juce::Colour(0xff334155));
    g.fillRect(0, 0, 10, bounds.getHeight());
    g.fillRect(bounds.getRight() - 10, 0, 10, bounds.getHeight());
    g.setColour(juce::Colour(0xff64748b));
    g.drawVerticalLine(9, 0.0f, static_cast<float>(bounds.getHeight()));
    g.drawVerticalLine(bounds.getRight() - 10, 0.0f, static_cast<float>(bounds.getHeight()));

    // 2. Header Strip
    auto headerBounds = bounds.removeFromTop(80).reduced(12, 0);
    g.setColour(juce::Colour(0xff0f172a));
    g.fillRoundedRectangle(headerBounds.toFloat().reduced(4.0f, 6.0f), 4.0f);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(headerBounds.toFloat().reduced(4.0f, 6.0f), 4.0f, 1.0f);

    // Yamaha Tuning Fork Emblem Style Label
    g.setColour(juce::Colour(0xff0284c7)); // Cyan Metallic
    g.fillRoundedRectangle(26.0f, 22.0f, 6.0f, 36.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("MOTIF WORKSTATION", 40, 22, 210, 20, juce::Justification::left);
    g.setColour(juce::Colour(0xff38bdf8));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("ADVANCED WAVE SAMPLER HOST", 40, 42, 210, 16, juce::Justification::left);

    // Motif Green/Cyan Backlit Graphic LCD Screen
    auto lcdRect = juce::Rectangle<int>(255, 14, 280, 52);
    g.setColour(juce::Colour(0xff022c22)); // Deep Emerald Dark Background
    g.fillRoundedRectangle(lcdRect.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff059669)); // Bright Emerald Bezel
    g.drawRoundedRectangle(lcdRect.toFloat(), 4.0f, 1.0f);

    // Grid lines for retro LCD effect
    g.setColour(juce::Colour(0x2210b981));
    for (int y = lcdRect.getY() + 4; y < lcdRect.getBottom(); y += 4) {
        g.drawHorizontalLine(y, static_cast<float>(lcdRect.getX()), static_cast<float>(lcdRect.getRight()));
    }

    g.setColour(juce::Colour(0xff34d399)); // Luminous LCD text
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("MULTI-PART PERFORMANCE [SONG 01]", 268, 18, 200, 12, juce::Justification::left);

    g.setColour(juce::Colour(0xffa7f3d0));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    juce::String layerStr = ctx.pianoName + " + " + ctx.synthName;
    g.drawText(layerStr, 268, 34, 160, 20, juce::Justification::left);

    g.setColour(juce::Colour(0xff6ee7b7));
    g.setFont(juce::FontOptions(9.5f));
    juce::String telemetry = "DSP: " + juce::String(ctx.dspLoad, 1) + "% | DFD: " + juce::String(ctx.dfdLoad) + "%"
                           + " | VOICES: " + juce::String(ctx.activeVoices);
    g.drawText(telemetry, 330, 24, 195, 20, juce::Justification::right);

    // 3. Footer Strip
    auto footerBounds = bounds.removeFromBottom(50);
    g.setColour(juce::Colour(0xff090e17));
    g.fillRect(footerBounds);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawHorizontalLine(footerBounds.getY(), 0.0f, static_cast<float>(bounds.getWidth()));

    bool hasMidi = (ctx.midiDevice != "No MIDI input active (click CONFIG)");
    g.setColour(hasMidi ? juce::Colour(0xff10b981) : juce::Colour(0xffef4444));
    g.fillEllipse(22.0f, footerBounds.getY() + 21.0f, 8.0f, 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MIDI: " + ctx.midiDevice, 36, footerBounds.getY() + 16, 320, 18, juce::Justification::left);

    g.setColour(ctx.sustainDown ? juce::Colour(0xff34d399) : juce::Colour(0xff64748b));
    g.drawText(ctx.sustainDown ? "SUSTAIN: [DOWN]" : "SUSTAIN: [UP]", 370, footerBounds.getY() + 16, 110, 18, juce::Justification::left);

    // 4. Main 4 Panel Cards (Motif Layout: 4-Fader Bank on Left, Generators in Center, FX on Right)
    auto contentBounds = bounds.removeFromTop(385).reduced(18, 8);
    int colWidth = (contentBounds.getWidth() - 36) / 4;

    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    auto drawCard = [&g](juce::Rectangle<int> r, const juce::String& title, const juce::String& tag) {
        g.setColour(juce::Colour(0xff111827));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff1e293b));
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);

        // Header banner inside panel
        g.setColour(juce::Colour(0xff1e293b));
        g.fillRoundedRectangle(r.getX() + 2.0f, r.getY() + 2.0f, r.getWidth() - 4.0f, 28.0f, 4.0f);

        g.setColour(juce::Colour(0xfff8fafc));
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, r.getX() + 12, r.getY() + 8, 180, 16, juce::Justification::left);

        g.setColour(juce::Colour(0xff38bdf8));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(tag, r.getRight() - 95, r.getY() + 8, 82, 16, juce::Justification::right);
    };

    drawCard(r1, "MULTI-MIXER BANK", "4-FADERS");
    drawCard(r2, "WAVE GENERATORS", "SLOT A / B");
    drawCard(r3, "DUAL EQUALIZERS", "ISOLATION");
    drawCard(r4, "VCM STUDIO EFFECTS", "DSP MASTER");

    // Fader labels for Card 1 (Multi-Fader Bank)
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("CH 1: PIANO", r1.getX() + 10, r1.getY() + 40, 75, 14, juce::Justification::centred);
    g.drawText("CH 2: SYNTH", r1.getX() + 95, r1.getY() + 40, 75, 14, juce::Justification::centred);
    g.drawText("CH 3: DRONE", r1.getX() + 180, r1.getY() + 40, 75, 14, juce::Justification::centred);

    // Card 2 labels
    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("SLOT A: PIANO VOICE", r2.getX() + 12, r2.getY() + 36, 150, 14, juce::Justification::left);
    g.drawText("TOUCH CURVE", r2.getX() + 12, r2.getY() + 102, 150, 14, juce::Justification::left);
    g.drawText("SLOT B: SYNTH VOICE", r2.getX() + 12, r2.getY() + 172, 150, 14, juce::Justification::left);
    g.drawText("VCF CUTOFF", r2.getX() + 12, r2.getY() + 240, 150, 14, juce::Justification::left);
    g.drawText("AMBIENT DRONE SOUND", r2.getX() + 12, r2.getY() + 308, 150, 14, juce::Justification::left);

    // Card 3 labels
    g.drawText("SLOT A PIANO EQ", r3.getX() + 12, r3.getY() + 36, 150, 14, juce::Justification::left);
    g.drawText("SLOT B SYNTH EQ", r3.getX() + 12, r3.getY() + 160, 150, 14, juce::Justification::left);
    g.drawText("MODULATION FX", r3.getX() + 12, r3.getY() + 276, 150, 14, juce::Justification::left);

    // Gain Reduction Meter in Card 4
    auto meterBox = juce::Rectangle<int>(r4.getX() + 14, r4.getY() + 175, r4.getWidth() - 28, 26);
    g.setColour(juce::Colour(0xff090a0d));
    g.fillRoundedRectangle(meterBox.toFloat(), 4.0f);
    float gr = ctx.gainReductionDb;
    g.setColour(juce::Colour(0xff34d399));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("VCM COMP GR: -" + juce::String(gr, 1) + " dB", meterBox.getX() + 6, meterBox.getY() + 3, 140, 10, juce::Justification::left);

    int numLeds = 12;
    int litLeds = juce::jlimit(0, numLeds, static_cast<int>(gr * 0.8f));
    int ledW = (meterBox.getWidth() - 12 - (numLeds * 2)) / numLeds;
    for (int i = 0; i < numLeds; ++i) {
        int lx = meterBox.getX() + 6 + i * (ledW + 2);
        int ly = meterBox.getY() + 13;
        bool lit = (i < litLeds);
        g.setColour(lit ? (i > 8 ? juce::Colour(0xffef4444) : juce::Colour(0xff10b981)) : juce::Colour(0xff1e293b));
        g.fillRect(lx, ly, ledW, 8);
    }

    // 5. Cyan/Blue Felt Trim above Keyboard
    g.setColour(juce::Colour(0xff0284c7));
    g.fillRect(16, 477, bounds.getWidth() - 32, 4);
    g.setColour(juce::Colour(0xff1e293b));
    g.drawRoundedRectangle(15.0f, 480.0f, static_cast<float>(bounds.getWidth() - 30), 226.0f, 6.0f, 1.5f);
}

void YamahaMotifLayout::layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) {
    bounds.removeFromTop(80);
    auto footerArea = bounds.removeFromBottom(50);
    auto cardArea = bounds.removeFromTop(385);
    auto contentBounds = cardArea.reduced(18, 8);

    int colWidth = (contentBounds.getWidth() - 36) / 4;
    auto r1 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r2 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r3 = contentBounds.removeFromLeft(colWidth);
    contentBounds.removeFromLeft(12);
    auto r4 = contentBounds;

    // Header buttons (Right aligned)
    int hX = bounds.getWidth() - 20;
    if (comps.comboTheme)     { hX -= 175; comps.comboTheme->setBounds(hX, 22, 175, 36); }
    if (comps.btnPanic)        { hX -= 70;  comps.btnPanic->setBounds(hX, 22, 60, 36); }
    if (comps.btnSettings)     { hX -= 105; comps.btnSettings->setBounds(hX, 22, 95, 36); }
    if (comps.comboTranspose)  { hX -= 105; comps.comboTranspose->setBounds(hX, 22, 95, 36); }
    if (comps.btnSavePreset)   { hX -= 90;  comps.btnSavePreset->setBounds(hX, 22, 80, 36); }
    if (comps.btnNextPreset)   { hX -= 45;  comps.btnNextPreset->setBounds(hX, 22, 38, 36); }
    if (comps.btnPrevPreset)   { hX -= 45;  comps.btnPrevPreset->setBounds(hX, 22, 38, 36); }

    // Footer buttons
    if (comps.btnFooterConfig) comps.btnFooterConfig->setBounds(footerArea.getRight() - 95, footerArea.getY() + 10, 85, 30);
    if (comps.btnMidiLearn)    comps.btnMidiLearn->setBounds(footerArea.getRight() - 250, footerArea.getY() + 10, 145, 30);
    if (comps.btnImport)       comps.btnImport->setBounds(footerArea.getRight() - 415, footerArea.getY() + 10, 155, 30);
    if (comps.btnRefresh)      comps.btnRefresh->setBounds(footerArea.getRight() - 510, footerArea.getY() + 10, 85, 30);

    // Section 1: Multi-Fader Bank (Yamaha Mixing Console Style!)
    if (comps.togglePiano) comps.togglePiano->setBounds(r1.getX() + 8, r1.getY() + 60, 78, 24);
    if (comps.sliderPianoVol) comps.sliderPianoVol->setBounds(r1.getX() + 18, r1.getY() + 90, 58, 220);

    if (comps.toggleSynth) comps.toggleSynth->setBounds(r1.getX() + 95, r1.getY() + 60, 78, 24);
    if (comps.sliderSynthVol) comps.sliderSynthVol->setBounds(r1.getX() + 105, r1.getY() + 90, 58, 220);

    if (comps.btnDroneToggle) comps.btnDroneToggle->setBounds(r1.getX() + 180, r1.getY() + 60, 80, 24);
    if (comps.sliderDroneVol) comps.sliderDroneVol->setBounds(r1.getX() + 190, r1.getY() + 90, 58, 220);

    // Section 2: Wave Generators (Voices & Pitch Tuning)
    if (comps.comboPiano) comps.comboPiano->setBounds(r2.getX() + 12, r2.getY() + 54, r2.getWidth() - 24, 26);
    if (comps.comboVelocityCurve) comps.comboVelocityCurve->setBounds(r2.getX() + 12, r2.getY() + 122, r2.getWidth() - 24, 26);

    if (comps.comboSynth) comps.comboSynth->setBounds(r2.getX() + 12, r2.getY() + 192, r2.getWidth() - 24, 26);
    if (comps.sliderSynthCutoff) comps.sliderSynthCutoff->setBounds(r2.getX() + 12, r2.getY() + 258, r2.getWidth() - 24, 45);

    if (comps.comboDroneSound) comps.comboDroneSound->setBounds(r2.getX() + 12, r2.getY() + 328, (r2.getWidth() - 30) * 2 / 3, 26);
    if (comps.comboDroneRoot) comps.comboDroneRoot->setBounds(r2.getX() + 16 + (r2.getWidth() - 30) * 2 / 3, r2.getY() + 328, (r2.getWidth() - 30) / 3, 26);

    // Section 3: Dual Equalizers
    if (comps.sliderPianoEqLow) comps.sliderPianoEqLow->setBounds(r3.getX() + 8, r3.getY() + 60, 72, 60);
    if (comps.sliderPianoEqMid) comps.sliderPianoEqMid->setBounds(r3.getX() + 86, r3.getY() + 60, 72, 60);
    if (comps.sliderPianoEqHigh) comps.sliderPianoEqHigh->setBounds(r3.getX() + 164, r3.getY() + 60, 72, 60);

    if (comps.sliderSynthEqLow) comps.sliderSynthEqLow->setBounds(r3.getX() + 8, r3.getY() + 185, 72, 60);
    if (comps.sliderSynthEqMid) comps.sliderSynthEqMid->setBounds(r3.getX() + 86, r3.getY() + 185, 72, 60);
    if (comps.sliderSynthEqHigh) comps.sliderSynthEqHigh->setBounds(r3.getX() + 164, r3.getY() + 185, 72, 60);

    // Modulation
    if (comps.toggleChorus) comps.toggleChorus->setBounds(r3.getX() + 12, r3.getY() + 300, 85, 24);
    if (comps.sliderChorusMix) comps.sliderChorusMix->setBounds(r3.getX() + 95, r3.getY() + 295, 55, 45);
    if (comps.toggleDelay) comps.toggleDelay->setBounds(r3.getX() + 155, r3.getY() + 300, 75, 24);
    if (comps.sliderDelayMix) comps.sliderDelayMix->setBounds(r3.getX() + 225, r3.getY() + 295, 55, 45);

    // Section 4: VCM Effects & Mastering
    if (comps.toggleReverb) comps.toggleReverb->setBounds(r4.getX() + 14, r4.getY() + 45, 90, 24);
    if (comps.sliderReverbMix) comps.sliderReverbMix->setBounds(r4.getX() + 110, r4.getY() + 40, 65, 50);

    if (comps.sliderCompThresh) comps.sliderCompThresh->setBounds(r4.getX() + 15, r4.getY() + 100, 70, 70);
    if (comps.sliderCompMakeup) comps.sliderCompMakeup->setBounds(r4.getX() + 95, r4.getY() + 100, 70, 70);

    // Virtual Keyboard
    int kbX = 18;
    int kbY = 483;
    int kbW = bounds.getWidth() - 36;
    int kbH = 220;
    if (comps.keyboardComponent) {
        comps.keyboardComponent->setBounds(kbX, kbY, kbW, kbH);
    }
}
