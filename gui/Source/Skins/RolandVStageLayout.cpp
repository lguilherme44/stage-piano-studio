#include "RolandVStageLayout.h"

void RolandVStageLayout::paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) {
    // 1. Sleek Obsidian Black Metallic Chassis
    g.setColour(juce::Colour(0xff09090b));
    g.fillRect(bounds);

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xff18181b), 0.0f, 0.0f,
        juce::Colour(0xff050507), 0.0f, static_cast<float>(bounds.getHeight()), false
    ));
    g.fillRect(bounds);

    // Luxury Champagne Gold Side Edge Trims
    g.setColour(juce::Colour(0xffb45309));
    g.fillRect(0, 0, 10, bounds.getHeight());
    g.fillRect(bounds.getRight() - 10, 0, 10, bounds.getHeight());
    g.setColour(juce::Colour(0xffd97706));
    g.drawVerticalLine(9, 0.0f, static_cast<float>(bounds.getHeight()));
    g.drawVerticalLine(bounds.getRight() - 10, 0.0f, static_cast<float>(bounds.getHeight()));

    // 2. Header Strip
    auto headerBounds = bounds.removeFromTop(80).reduced(12, 0);
    g.setColour(juce::Colour(0xff121215));
    g.fillRoundedRectangle(headerBounds.toFloat().reduced(4.0f, 6.0f), 4.0f);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(headerBounds.toFloat().reduced(4.0f, 6.0f), 4.0f, 1.0f);

    // Roland Pro Gold Ribbon Indicator
    g.setColour(juce::Colour(0xfff59e0b)); // Bright Gold
    g.fillRoundedRectangle(26.0f, 22.0f, 6.0f, 36.0f, 2.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("V-STAGE OBSIDIAN", 40, 22, 210, 20, juce::Justification::left);
    g.setColour(juce::Colour(0xfffbbf24));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("SUPERNATURAL STAGE PIANO", 40, 42, 210, 16, juce::Justification::left);

    // High Contrast Stage OLED Screen
    auto oledRect = juce::Rectangle<int>(255, 14, 280, 52);
    g.setColour(juce::Colour(0xff000000));
    g.fillRoundedRectangle(oledRect.toFloat(), 4.0f);
    g.setColour(juce::Colour(0xffd97706)); // Gold rim
    g.drawRoundedRectangle(oledRect.toFloat(), 4.0f, 1.0f);

    g.setColour(juce::Colour(0xfffbbf24));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("LIVE SCENE / ACOUSTIC + SYNTH", 268, 18, 200, 12, juce::Justification::left);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    juce::String layerStr = ctx.pianoName + " + " + ctx.synthName;
    g.drawText(layerStr, 268, 34, 160, 20, juce::Justification::left);

    g.setColour(juce::Colour(0xfffef08a));
    g.setFont(juce::FontOptions(9.5f));
    juce::String telemetry = "DSP: " + juce::String(ctx.dspLoad, 1) + "% | DFD: " + juce::String(ctx.dfdLoad) + "%"
                           + " | VOICES: " + juce::String(ctx.activeVoices);
    g.drawText(telemetry, 330, 24, 195, 20, juce::Justification::right);

    // 3. Footer Strip
    auto footerBounds = bounds.removeFromBottom(50);
    g.setColour(juce::Colour(0xff0a0a0c));
    g.fillRect(footerBounds);
    g.setColour(juce::Colour(0xff27272a));
    g.drawHorizontalLine(footerBounds.getY(), 0.0f, static_cast<float>(bounds.getWidth()));

    bool hasMidi = (ctx.midiDevice != "No MIDI input active (click CONFIG)");
    g.setColour(hasMidi ? juce::Colour(0xff10b981) : juce::Colour(0xffef4444));
    g.fillEllipse(22.0f, footerBounds.getY() + 21.0f, 8.0f, 8.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("MIDI: " + ctx.midiDevice, 36, footerBounds.getY() + 16, 320, 18, juce::Justification::left);

    g.setColour(ctx.sustainDown ? juce::Colour(0xfffbbf24) : juce::Colour(0xff71717a));
    g.drawText(ctx.sustainDown ? "SUSTAIN: [DOWN]" : "SUSTAIN: [UP]", 370, footerBounds.getY() + 16, 110, 18, juce::Justification::left);

    // 4. Main 4 Panel Cards (V-Stage Pro Modules)
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
        g.setColour(juce::Colour(0xff141418));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff27272a));
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.0f);

        // Header banner inside panel with subtle gold line
        g.setColour(juce::Colour(0xff1c1c22));
        g.fillRoundedRectangle(r.getX() + 2.0f, r.getY() + 2.0f, r.getWidth() - 4.0f, 28.0f, 4.0f);
        g.setColour(juce::Colour(0x66f59e0b));
        g.drawHorizontalLine(r.getY() + 30, static_cast<float>(r.getX() + 6), static_cast<float>(r.getRight() - 6));

        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title, r.getX() + 12, r.getY() + 8, 180, 16, juce::Justification::left);

        g.setColour(juce::Colour(0xfffbbf24));
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(tag, r.getRight() - 95, r.getY() + 8, 82, 16, juce::Justification::right);
    };

    drawCard(r1, "ACOUSTIC ENGINE", "SLOT A");
    drawCard(r2, "SYNTHESIZER CORE", "SLOT B");
    drawCard(r3, "ATMOSPHERE & DRONE", "PAD LATCH");
    drawCard(r4, "MASTERING RACK", "STUDIO DSP");

    // Labels Card 1
    g.setColour(juce::Colour(0xffd4d4d8));
    g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    g.drawText("PIANO MODEL", r1.getX() + 12, r1.getY() + 36, 120, 14, juce::Justification::left);
    g.drawText("LEVEL", r1.getRight() - 75, r1.getY() + 72, 55, 12, juce::Justification::centred);
    g.drawText("ACOUSTIC 3-BAND EQ", r1.getX() + 12, r1.getY() + 172, 160, 14, juce::Justification::left);
    g.drawText("HAMMER TOUCH ACTION", r1.getX() + 12, r1.getY() + 308, 180, 14, juce::Justification::left);

    // Labels Card 2
    g.drawText("SYNTHESIS TIMBRE", r2.getX() + 12, r2.getY() + 36, 140, 14, juce::Justification::left);
    g.drawText("FILTER", r2.getX() + 112, r2.getY() + 72, 68, 12, juce::Justification::centred);
    g.drawText("LEVEL", r2.getRight() - 75, r2.getY() + 72, 55, 12, juce::Justification::centred);
    g.drawText("SYNTH 3-BAND EQ", r2.getX() + 12, r2.getY() + 172, 160, 14, juce::Justification::left);
    g.drawText("MODULATION", r2.getX() + 12, r2.getY() + 276, 160, 14, juce::Justification::left);

    // Labels Card 3
    g.drawText("DRONE TONIC ROOT", r3.getX() + 12, r3.getY() + 36, 160, 14, juce::Justification::left);
    g.drawText("PAD TIMBRE TEXTURE", r3.getX() + 12, r3.getY() + 102, 160, 14, juce::Justification::left);
    g.drawText("PAD LEVEL", r3.getX() + 12, r3.getY() + 208, 160, 14, juce::Justification::centred);

    // Gain Reduction Meter in Card 4
    auto meterBox = juce::Rectangle<int>(r4.getX() + 14, r4.getY() + 175, r4.getWidth() - 28, 26);
    g.setColour(juce::Colour(0xff09090b));
    g.fillRoundedRectangle(meterBox.toFloat(), 4.0f);
    float gr = ctx.gainReductionDb;
    g.setColour(juce::Colour(0xfffbbf24));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("OPTICAL COMP GR: -" + juce::String(gr, 1) + " dB", meterBox.getX() + 6, meterBox.getY() + 3, 160, 10, juce::Justification::left);

    int numLeds = 12;
    int litLeds = juce::jlimit(0, numLeds, static_cast<int>(gr * 0.8f));
    int ledW = (meterBox.getWidth() - 12 - (numLeds * 2)) / numLeds;
    for (int i = 0; i < numLeds; ++i) {
        int lx = meterBox.getX() + 6 + i * (ledW + 2);
        int ly = meterBox.getY() + 13;
        bool lit = (i < litLeds);
        g.setColour(lit ? (i > 8 ? juce::Colour(0xffef4444) : juce::Colour(0xfff59e0b)) : juce::Colour(0xff27272a));
        g.fillRect(lx, ly, ledW, 8);
    }

    // 5. Gold Foil Felt Line above Keyboard
    g.setColour(juce::Colour(0xffd97706));
    g.fillRect(16, 477, bounds.getWidth() - 32, 4);
    g.setColour(juce::Colour(0xff27272a));
    g.drawRoundedRectangle(15.0f, 480.0f, static_cast<float>(bounds.getWidth() - 30), 226.0f, 6.0f, 1.5f);
}

void RolandVStageLayout::layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) {
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

    // Section 1: Acoustic Engine (Piano Slot A + Touch)
    if (comps.comboPiano) comps.comboPiano->setBounds(r1.getX() + 12, r1.getY() + 52, r1.getWidth() - 24, 26);
    if (comps.togglePiano) comps.togglePiano->setBounds(r1.getX() + 12, r1.getY() + 94, 110, 24);
    if (comps.sliderPianoVol) comps.sliderPianoVol->setBounds(r1.getRight() - 75, r1.getY() + 86, 55, 78);

    if (comps.sliderPianoEqLow) comps.sliderPianoEqLow->setBounds(r1.getX() + 8, r1.getY() + 192, 72, 60);
    if (comps.sliderPianoEqMid) comps.sliderPianoEqMid->setBounds(r1.getX() + 86, r1.getY() + 192, 72, 60);
    if (comps.sliderPianoEqHigh) comps.sliderPianoEqHigh->setBounds(r1.getX() + 164, r1.getY() + 192, 72, 60);

    if (comps.comboVelocityCurve) comps.comboVelocityCurve->setBounds(r1.getX() + 12, r1.getY() + 328, r1.getWidth() - 24, 26);

    // Section 2: Synthesizer Core (Synth Slot B)
    if (comps.comboSynth) comps.comboSynth->setBounds(r2.getX() + 12, r2.getY() + 52, r2.getWidth() - 24, 26);
    if (comps.toggleSynth) comps.toggleSynth->setBounds(r2.getX() + 12, r2.getY() + 94, 95, 24);
    if (comps.sliderSynthCutoff) comps.sliderSynthCutoff->setBounds(r2.getX() + 112, r2.getY() + 86, 68, 62);
    if (comps.sliderSynthVol) comps.sliderSynthVol->setBounds(r2.getRight() - 75, r2.getY() + 86, 55, 78);

    if (comps.sliderSynthEqLow) comps.sliderSynthEqLow->setBounds(r2.getX() + 8, r2.getY() + 192, 72, 60);
    if (comps.sliderSynthEqMid) comps.sliderSynthEqMid->setBounds(r2.getX() + 86, r2.getY() + 192, 72, 60);
    if (comps.sliderSynthEqHigh) comps.sliderSynthEqHigh->setBounds(r2.getX() + 164, r2.getY() + 192, 72, 60);

    // Modulation
    if (comps.toggleChorus) comps.toggleChorus->setBounds(r2.getX() + 12, r2.getY() + 300, 85, 24);
    if (comps.sliderChorusMix) comps.sliderChorusMix->setBounds(r2.getX() + 105, r2.getY() + 290, 65, 50);
    if (comps.toggleDelay) comps.toggleDelay->setBounds(r2.getX() + 175, r2.getY() + 300, 85, 24);
    if (comps.sliderDelayMix) comps.sliderDelayMix->setBounds(r2.getX() + 250, r2.getY() + 290, 65, 50);

    // Section 3: Atmosphere & Drone Pad
    if (comps.comboDroneRoot) comps.comboDroneRoot->setBounds(r3.getX() + 12, r3.getY() + 54, r3.getWidth() - 24, 26);
    if (comps.comboDroneSound) comps.comboDroneSound->setBounds(r3.getX() + 12, r3.getY() + 120, r3.getWidth() - 24, 26);
    if (comps.btnDroneToggle) comps.btnDroneToggle->setBounds(r3.getX() + 12, r3.getY() + 160, r3.getWidth() - 24, 38);
    if (comps.sliderDroneVol) comps.sliderDroneVol->setBounds(r3.getCentreX() - 30, r3.getY() + 226, 60, 128);

    // Section 4: Mastering Rack (Studio DSP)
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
