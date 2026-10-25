#pragma once

#include "SkinLayoutStrategy.h"

class RolandVStageLayout : public SkinLayoutStrategy {
public:
    RolandVStageLayout() = default;
    ~RolandVStageLayout() override = default;

    SkinType getType() const override { return SkinType::RolandVStage; }
    juce::String getName() const override { return "Roland V-Stage Obsidian"; }

    void paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) override;
    void layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) override;
};
