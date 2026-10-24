#include "SampleImporter.hpp"
#include <iostream>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>

namespace fs = std::filesystem;

// Create a small 16-bit sine wave WAV file for testing
static void createTestWav(const std::string& path, float freq = 440.0f, float durationSec = 0.1f) {
    int sampleRate = 44100;
    int numFrames = static_cast<int>(sampleRate * durationSec);
    std::ofstream out(path, std::ios::binary);
    
    uint32_t dataBytes = numFrames * sizeof(short);
    uint32_t riffSize = 36 + dataBytes;
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1;
    uint16_t channels = 1;
    uint32_t sRate = sampleRate;
    uint32_t byteRate = sRate * sizeof(short);
    uint16_t blockAlign = sizeof(short);
    uint16_t bitsPerSample = 16;

    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riffSize), 4);
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    out.write(reinterpret_cast<const char*>(&fmtSize), 4);
    out.write(reinterpret_cast<const char*>(&audioFormat), 2);
    out.write(reinterpret_cast<const char*>(&channels), 2);
    out.write(reinterpret_cast<const char*>(&sRate), 4);
    out.write(reinterpret_cast<const char*>(&byteRate), 4);
    out.write(reinterpret_cast<const char*>(&blockAlign), 2);
    out.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&dataBytes), 4);

    std::vector<short> pcm(numFrames);
    for (int i = 0; i < numFrames; ++i) {
        float t = static_cast<float>(i) / sampleRate;
        float s = std::sin(2.0f * 3.14159265f * freq * t) * 0.8f;
        pcm[i] = static_cast<short>(s * 32767.0f);
    }
    out.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
}

int main() {
    std::cout << "[Test Importer] Running SFZ & Sample Importer tests..." << std::endl;

    fs::path tempDir = fs::temp_directory_path() / "sps_importer_test";
    fs::create_directories(tempDir / "source_sfz");
    fs::create_directories(tempDir / "target_lib");

    // 1. Create source test WAV samples
    std::string wavC4 = (tempDir / "source_sfz" / "Sine_C4.wav").string();
    std::string wavG4 = (tempDir / "source_sfz" / "Sine_G4.wav").string();
    createTestWav(wavC4, 261.63f, 0.2f);
    createTestWav(wavG4, 392.00f, 0.2f);

    // 2. Create test .sfz file
    std::string sfzFile = (tempDir / "source_sfz" / "TestSinePiano.sfz").string();
    {
        std::ofstream out(sfzFile);
        out << "<group>\n";
        out << "<region> sample=Sine_C4.wav lokey=48 hikey=63 pitch_keycenter=60 lovel=1 hivel=127\n";
        out << "<region> sample=Sine_G4.wav lokey=64 hikey=84 pitch_keycenter=67 lovel=1 hivel=127\n";
    }

    // 3. Test importSFZ
    ImportResult res = SampleImporter::importSFZ(sfzFile, (tempDir / "target_lib").string());
    std::cout << "[Test Importer] Result: " << res.message << std::endl;
    assert(res.success == true);
    assert(res.sampleCount == 2);

    // 4. Verify target instrument JSON
    fs::path targetJson = tempDir / "target_lib" / "instruments" / (res.instrumentId + ".json");
    assert(fs::exists(targetJson));
    std::cout << "[Test Importer] Generated JSON: " << targetJson << std::endl;

    // 5. Test importSampleFolder
    fs::create_directories(tempDir / "sample_folder");
    createTestWav((tempDir / "sample_folder" / "Grand_C3_pp.wav").string(), 130.81f, 0.1f);
    createTestWav((tempDir / "sample_folder" / "Grand_C3_ff.wav").string(), 130.81f, 0.1f);
    createTestWav((tempDir / "sample_folder" / "Grand_G3_mf.wav").string(), 196.00f, 0.1f);

    ImportResult folderRes = SampleImporter::importSampleFolder((tempDir / "sample_folder").string(), (tempDir / "target_lib").string());
    std::cout << "[Test Importer] Folder Import Result: " << folderRes.message << std::endl;
    assert(folderRes.success == true);
    assert(folderRes.sampleCount == 3);

    fs::path folderJson = tempDir / "target_lib" / "instruments" / (folderRes.instrumentId + ".json");
    assert(fs::exists(folderJson));
    std::cout << "[Test Importer] Generated Folder JSON: " << folderJson << std::endl;

    // Clean up
    fs::remove_all(tempDir);
    std::cout << "[Test Importer] ALL TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
