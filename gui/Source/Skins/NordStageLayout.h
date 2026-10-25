#pragma once

#include "SkinLayoutStrategy.h"

class NordStageLayout : public SkinLayoutStrategy {
public:
    NordStageLayout() = default;
    ~NordStageLayout() override = default;

    SkinType getType() const override { return SkinType::NordStage; }
    juce::String getName() const override { return "Nord Stage Red"; }

    void paintBackground(juce::Graphics& g, juce::Rectangle<int> bounds, const SkinContext& ctx) override;
    void layoutComponents(juce::Rectangle<int> bounds, const SkinUIComponents& comps) override;
};
