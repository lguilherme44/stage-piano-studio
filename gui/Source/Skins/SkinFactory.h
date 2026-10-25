#pragma once

#include "NordStageLayout.h"
#include "YamahaMotifLayout.h"
#include "RolandVStageLayout.h"
#include <memory>

class SkinFactory {
public:
    static std::unique_ptr<SkinLayoutStrategy> create(SkinType type) {
        switch (type) {
            case SkinType::NordStage:
                return std::make_unique<NordStageLayout>();
            case SkinType::YamahaMotif:
                return std::make_unique<YamahaMotifLayout>();
            case SkinType::RolandVStage:
                return std::make_unique<RolandVStageLayout>();
            default:
                return std::make_unique<NordStageLayout>();
        }
    }
};
