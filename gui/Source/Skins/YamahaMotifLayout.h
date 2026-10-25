#pragma once

#include "SkinLayoutStrategy.h"

class YamahaMotifLayout : public SkinLayoutStrategy {
public:
    YamahaMotifLayout() = default;
    ~YamahaMotifLayout() override = default;

    SkinType getType() const override { return SkinType::YamahaMotif; }
    juce::String getName() const override { return "Yamaha Motif Navy"; }

    void paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) override;
    void layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) override;
};
