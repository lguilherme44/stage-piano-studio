#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "MainComponent.h"
#include <iostream>

int main(int argc, char* argv[]) {
    juce::ScopedJuceInitialiser_GUI guiInit;

    juce::File outDir = (argc > 1) ? juce::File(argv[1]) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    std::cout << "[RENDER] Initializing MainComponent..." << std::endl;
    auto comp = std::make_unique<MainComponent>();
    comp->setSize(1220, 770);

    struct SkinTarget {
        SkinType type;
        const char* name;
        const char* filename;
    };

    std::vector<SkinTarget> skins = {
        { SkinType::NordStage, "Nord Stage Red", "nord_stage_skin.png" },
        { SkinType::YamahaMotif, "Yamaha Motif Navy", "yamaha_motif_skin.png" },
        { SkinType::RolandVStage, "Roland V-Stage Obsidian", "roland_vstage_skin.png" }
    };

    // Save current initial state
    {
        std::cout << "[RENDER] Capturing current screen state..." << std::endl;
        comp->setSize(1220, 770);
        comp->resized();
        comp->repaint();

        juce::Image img(juce::Image::ARGB, 1220, 770, true);
        juce::Graphics g(img);
        comp->paintEntireComponent(g, true);

        juce::File outFile = outDir.getChildFile("current_skin.png");
        outFile.deleteFile();
        juce::FileOutputStream stream(outFile);
        if (stream.openedOk()) {
            juce::PNGImageFormat png;
            png.writeImageToStream(img, stream);
            std::cout << " -> Saved: " << outFile.getFullPathName() << std::endl;
        }
    }

    for (const auto& s : skins) {
        std::cout << "[RENDER] Switching and rendering " << s.name << "..." << std::endl;
        comp->setSkin(s.type);
        comp->setSize(1220, 770);
        comp->resized();
        comp->repaint();

        juce::Image img(juce::Image::ARGB, 1220, 770, true);
        juce::Graphics g(img);
        comp->paintEntireComponent(g, true);

        juce::File outFile = outDir.getChildFile(s.filename);
        outFile.deleteFile();
        juce::FileOutputStream stream(outFile);
        if (stream.openedOk()) {
            juce::PNGImageFormat png;
            png.writeImageToStream(img, stream);
            std::cout << " -> Saved: " << outFile.getFullPathName() << std::endl;
        } else {
            std::cerr << " -> Failed to write: " << outFile.getFullPathName() << std::endl;
        }
    }

    std::cout << "[RENDER] All skins successfully rendered to PNG!" << std::endl;
    return 0;
}
