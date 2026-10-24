#include "SampleImporter.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>

#define TSF_IMPLEMENTATION
#include "tsf.h"

namespace fs = std::filesystem;

static bool writeWav16FromFloat(const std::string& path, const float* floatSamples, size_t numFrames, int numChannels, int sampleRate) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    uint32_t dataBytes = static_cast<uint32_t>(numFrames * numChannels * sizeof(short));
    uint32_t riffSize = 36 + dataBytes;
    uint32_t fmtSize = 16;
    uint16_t audioFormat = 1; // PCM
    uint16_t channels = static_cast<uint16_t>(numChannels);
    uint32_t sRate = static_cast<uint32_t>(sampleRate);
    uint32_t byteRate = sRate * channels * sizeof(short);
    uint16_t blockAlign = channels * sizeof(short);
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

    std::vector<short> pcm(numFrames * numChannels);
    for (size_t i = 0; i < numFrames * numChannels; ++i) {
        float s = std::clamp(floatSamples[i], -1.0f, 1.0f);
        pcm[i] = static_cast<short>(std::round(s * 32767.0f));
    }
    out.write(reinterpret_cast<const char*>(pcm.data()), dataBytes);
    return true;
}

static std::string sanitizeName(const std::string& input) {
    std::string s = input;
    for (char& c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    // Remove duplicate underscores
    std::string clean;
    bool lastUnderscore = false;
    for (char c : s) {
        if (c == '_') {
            if (!lastUnderscore) clean += c;
            lastUnderscore = true;
        } else {
            clean += c;
            lastUnderscore = false;
        }
    }
    while (!clean.empty() && clean.front() == '_') clean.erase(clean.begin());
    while (!clean.empty() && clean.back() == '_') clean.pop_back();
    return clean.empty() ? "SampleInstrument" : clean;
}

ImportResult SampleImporter::importSF2(const std::string& sf2FilePath, const std::string& targetLibraryDir) {
    ImportResult result;
    if (!fs::exists(sf2FilePath)) {
        result.message = "File not found: " + sf2FilePath;
        return result;
    }

    tsf* font = tsf_load_filename(sf2FilePath.c_str());
    if (!font) {
        result.message = "Failed to parse SoundFont2 format: " + sf2FilePath;
        return result;
    }

    int presetCount = tsf_get_presetcount(font);
    if (presetCount == 0 || font->presetNum == 0) {
        tsf_close(font);
        result.message = "No presets found in SoundFont2 file";
        return result;
    }

    fs::path p(sf2FilePath);
    std::string baseName = sanitizeName(p.stem().string());
    std::string samplesDir = targetLibraryDir + "/samples";
    std::string instDir = targetLibraryDir + "/instruments";
    fs::create_directories(samplesDir);
    fs::create_directories(instDir);

    // Import primary preset (preset index 0) or main preset
    int targetPresetIdx = 0;
    const tsf_preset& preset = font->presets[targetPresetIdx];
    std::string presetName = preset.presetName;
    while (!presetName.empty() && (presetName.back() == ' ' || presetName.back() == '\0')) {
        presetName.pop_back();
    }
    if (presetName.empty()) presetName = "Default";

    std::string instId = "SF2_" + baseName + "_" + sanitizeName(presetName);
    result.instrumentId = instId;
    result.instrumentName = presetName + " (SF2)";

    struct LayerJson {
        int minVel;
        int maxVel;
        int targetVel;
        std::string filename;
    };
    std::map<int, std::vector<LayerJson>> noteZones;
    int savedSamples = 0;

    for (int r = 0; r < preset.regionNum; ++r) {
        const tsf_region& reg = preset.regions[r];
        if (reg.end <= reg.offset || reg.offset >= 100000000) continue;

        size_t numFrames = reg.end - reg.offset;
        std::string sampleFileName = instId + "_r" + std::to_string(r) + ".wav";
        std::string sampleFullPath = samplesDir + "/" + sampleFileName;

        if (font->fontSamples && (reg.offset + numFrames) <= 100000000) {
            writeWav16FromFloat(sampleFullPath, font->fontSamples + reg.offset, numFrames, 1, reg.sample_rate ? reg.sample_rate : 44100);
            savedSamples++;
        }

        int keyCenter = (reg.pitch_keycenter > 0 && reg.pitch_keycenter <= 127) ? reg.pitch_keycenter : ((reg.lokey + reg.hikey) / 2);
        int loKey = std::clamp(static_cast<int>(reg.lokey), 0, 127);
        int hiKey = std::clamp(static_cast<int>(reg.hikey), 0, 127);
        int loVel = std::clamp(static_cast<int>(reg.lovel), 1, 127);
        int hiVel = std::clamp(static_cast<int>(reg.hivel), 1, 127);
        int targetVel = (loVel + hiVel) / 2;

        LayerJson layer{loVel, hiVel, targetVel, sampleFileName};

        // Map root note or span
        for (int k = loKey; k <= hiKey; ++k) {
            noteZones[k].push_back(layer);
        }
    }

    tsf_close(font);

    if (noteZones.empty()) {
        result.message = "No valid playable regions found in SoundFont preset";
        return result;
    }

    // Write Instrument JSON definition
    std::string jsonPath = instDir + "/" + instId + ".json";
    std::ofstream out(jsonPath);
    if (!out) {
        result.message = "Failed to write instrument JSON to: " + jsonPath;
        return result;
    }

    out << "{\n";
    out << "  \"name\": \"" << result.instrumentName << "\",\n";
    out << "  \"type\": \"SoundFont2\",\n";
    out << "  \"notes\": {\n";

    size_t zoneIdx = 0;
    for (const auto& [note, layers] : noteZones) {
        out << "    \"" << note << "\": [\n";
        for (size_t l = 0; l < layers.size(); ++l) {
            out << "      {\n";
            out << "        \"min_velocity\": " << layers[l].minVel << ",\n";
            out << "        \"max_velocity\": " << layers[l].maxVel << ",\n";
            out << "        \"target_velocity\": " << layers[l].targetVel << ",\n";
            out << "        \"file\": \"" << layers[l].filename << "\"\n";
            out << "      }" << (l + 1 < layers.size() ? "," : "") << "\n";
        }
        out << "    ]" << (++zoneIdx < noteZones.size() ? "," : "") << "\n";
    }
    out << "  }\n";
    out << "}\n";

    result.success = true;
    result.sampleCount = savedSamples;
    result.message = "Successfully imported " + result.instrumentName + " (" + std::to_string(savedSamples) + " samples).";
    return result;
}

static int parseMidiNote(const std::string& token) {
    if (token.empty()) return 60;
    if (std::isdigit(static_cast<unsigned char>(token[0]))) {
        return std::clamp(std::stoi(token), 0, 127);
    }
    // Parse note name like C4, F#3, Bb2
    static const char* noteNames[] = {"c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"};
    std::string lower = token;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    int semitone = 0;
    size_t octIdx = 1;
    if (lower.size() >= 2 && (lower[1] == '#' || lower[1] == 'b')) {
        octIdx = 2;
        if (lower[1] == '#') {
            for (int i = 0; i < 12; ++i) {
                if (lower.substr(0, 2) == noteNames[i]) { semitone = i; break; }
            }
        } else if (lower[1] == 'b') {
            // Flat
            char base = lower[0];
            int natural = 0;
            for (int i = 0; i < 12; ++i) {
                if (noteNames[i][0] == base && noteNames[i][1] == '\0') { natural = i; break; }
            }
            semitone = (natural + 11) % 12;
        }
    } else {
        for (int i = 0; i < 12; ++i) {
            if (noteNames[i][0] == lower[0] && noteNames[i][1] == '\0') { semitone = i; break; }
        }
    }

    int octave = 4;
    try {
        octave = std::stoi(lower.substr(octIdx));
    } catch (...) {}

    return std::clamp((octave + 1) * 12 + semitone, 0, 127);
}

ImportResult SampleImporter::importSFZ(const std::string& sfzFilePath, const std::string& targetLibraryDir) {
    ImportResult result;
    if (!fs::exists(sfzFilePath)) {
        result.message = "File not found: " + sfzFilePath;
        return result;
    }

    std::ifstream in(sfzFilePath);
    if (!in) {
        result.message = "Cannot open SFZ file: " + sfzFilePath;
        return result;
    }

    fs::path sfzPath(sfzFilePath);
    fs::path sfzDir = sfzPath.parent_path();
    std::string baseName = sanitizeName(sfzPath.stem().string());
    std::string instId = "SFZ_" + baseName;
    result.instrumentId = instId;
    result.instrumentName = sfzPath.stem().string() + " (SFZ)";

    std::string samplesDir = targetLibraryDir + "/samples";
    std::string instDir = targetLibraryDir + "/instruments";
    fs::create_directories(samplesDir);
    fs::create_directories(instDir);

    struct LayerJson {
        int minVel;
        int maxVel;
        int targetVel;
        std::string filename;
    };
    std::map<int, std::vector<LayerJson>> noteZones;
    int importedSamples = 0;

    std::string line;
    std::string defaultPath = "";
    
    struct Region {
        std::string sample = "";
        int lokey = 0;
        int hikey = 127;
        int pitch_keycenter = 60;
        int lovel = 1;
        int hivel = 127;
    } currentRegion;

    bool inRegion = false;

    auto commitRegion = [&]() {
        if (!currentRegion.sample.empty()) {
            fs::path srcSample = sfzDir / defaultPath / currentRegion.sample;
            if (!fs::exists(srcSample)) {
                srcSample = sfzDir / currentRegion.sample;
            }

            if (fs::exists(srcSample)) {
                std::string destFileName = instId + "_" + srcSample.filename().string();
                std::string destFullPath = samplesDir + "/" + destFileName;
                try {
                    if (!fs::exists(destFullPath)) {
                        fs::copy_file(srcSample, destFullPath, fs::copy_options::overwrite_existing);
                    }
                    importedSamples++;

                    LayerJson layer{currentRegion.lovel, currentRegion.hivel, (currentRegion.lovel + currentRegion.hivel) / 2, destFileName};
                    for (int k = currentRegion.lokey; k <= currentRegion.hikey; ++k) {
                        noteZones[k].push_back(layer);
                    }
                } catch (...) {}
            }
        }
    };

    while (std::getline(in, line)) {
        // Strip comments //
        size_t commentPos = line.find("//");
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);

        std::stringstream ss(line);
        std::string token;
        while (ss >> token) {
            if (token == "<region>") {
                if (inRegion) commitRegion();
                inRegion = true;
                currentRegion = Region{};
            } else if (token == "<group>" || token == "<global>" || token == "<control>") {
                if (inRegion) commitRegion();
                inRegion = false;
            } else {
                size_t eq = token.find('=');
                if (eq != std::string::npos) {
                    std::string key = token.substr(0, eq);
                    std::string val = token.substr(eq + 1);
                    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c){ return std::tolower(c); });

                    if (key == "default_path") defaultPath = val;
                    else if (key == "sample") currentRegion.sample = val;
                    else if (key == "key") {
                        int k = parseMidiNote(val);
                        currentRegion.lokey = k;
                        currentRegion.hikey = k;
                        currentRegion.pitch_keycenter = k;
                    }
                    else if (key == "lokey") currentRegion.lokey = parseMidiNote(val);
                    else if (key == "hikey") currentRegion.hikey = parseMidiNote(val);
                    else if (key == "pitch_keycenter") currentRegion.pitch_keycenter = parseMidiNote(val);
                    else if (key == "lovel") currentRegion.lovel = std::clamp(std::stoi(val), 1, 127);
                    else if (key == "hivel") currentRegion.hivel = std::clamp(std::stoi(val), 1, 127);
                }
            }
        }
    }
    if (inRegion) commitRegion();

    if (noteZones.empty()) {
        result.message = "No samples or note regions found in SFZ file";
        return result;
    }

    // Write JSON
    std::string jsonPath = instDir + "/" + instId + ".json";
    std::ofstream out(jsonPath);
    if (!out) {
        result.message = "Failed to write instrument JSON: " + jsonPath;
        return result;
    }

    out << "{\n";
    out << "  \"name\": \"" << result.instrumentName << "\",\n";
    out << "  \"type\": \"SFZ\",\n";
    out << "  \"notes\": {\n";

    size_t zoneIdx = 0;
    for (const auto& [note, layers] : noteZones) {
        out << "    \"" << note << "\": [\n";
        for (size_t l = 0; l < layers.size(); ++l) {
            out << "      {\n";
            out << "        \"min_velocity\": " << layers[l].minVel << ",\n";
            out << "        \"max_velocity\": " << layers[l].maxVel << ",\n";
            out << "        \"target_velocity\": " << layers[l].targetVel << ",\n";
            out << "        \"file\": \"" << layers[l].filename << "\"\n";
            out << "      }" << (l + 1 < layers.size() ? "," : "") << "\n";
        }
        out << "    ]" << (++zoneIdx < noteZones.size() ? "," : "") << "\n";
    }
    out << "  }\n";
    out << "}\n";

    result.success = true;
    result.sampleCount = importedSamples;
    result.message = "Successfully imported " + result.instrumentName + " (" + std::to_string(importedSamples) + " samples).";
    return result;
}

static bool parseSampleFileNaming(const std::string& stem, int& outNote, int& outVelocity) {
    std::string lower = stem;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // 1. Velocity extraction (look for v### or dynamic tokens)
    outVelocity = 100;
    static const struct { const char* tag; int vel; } dyns[] = {
        {"fff", 127}, {"ff", 118}, {"f", 105}, {"mf", 88},
        {"mp", 72}, {"ppp", 20}, {"pp", 40}, {"p", 60}
    };
    for (const auto& d : dyns) {
        size_t pos = lower.rfind(d.tag);
        if (pos != std::string::npos) {
            bool leftOk = (pos == 0 || lower[pos - 1] == '_' || lower[pos - 1] == '-' || lower[pos - 1] == ' ' || std::isdigit(lower[pos - 1]));
            bool rightOk = (pos + std::strlen(d.tag) == lower.size() || lower[pos + std::strlen(d.tag)] == '_' || lower[pos + std::strlen(d.tag)] == '-' || lower[pos + std::strlen(d.tag)] == ' ');
            if (leftOk && rightOk) {
                outVelocity = d.vel;
                break;
            }
        }
    }

    // Check v### pattern e.g. v127, vel90
    size_t vpos = lower.find("_v");
    if (vpos == std::string::npos) vpos = lower.find("-v");
    if (vpos != std::string::npos && vpos + 2 < lower.size() && std::isdigit(lower[vpos + 2])) {
        try {
            outVelocity = std::clamp(std::stoi(lower.substr(vpos + 2)), 1, 127);
        } catch (...) {}
    }

    // 2. Note extraction
    outNote = -1;
    // Check note tokens e.g. c4, f#3, bb2, a0
    static const char* noteNames[] = {"c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"};
    for (int oct = -1; oct <= 9; ++oct) {
        std::string octStr = std::to_string(oct);
        for (int semi = 0; semi < 12; ++semi) {
            std::string nStr = std::string(noteNames[semi]) + octStr;
            size_t npos = lower.find(nStr);
            if (npos != std::string::npos) {
                bool leftOk = (npos == 0 || !std::isalpha(lower[npos - 1]));
                bool rightOk = (npos + nStr.size() == lower.size() || !std::isalpha(lower[npos + nStr.size()]));
                if (leftOk && rightOk) {
                    outNote = (oct + 1) * 12 + semi;
                    break;
                }
            }
        }
        if (outNote != -1) break;
    }

    // If not found by name, check 3-digit MIDI note e.g. _060_
    if (outNote == -1) {
        for (size_t i = 0; i + 3 <= lower.size(); ++i) {
            if (std::isdigit(lower[i]) && std::isdigit(lower[i + 1]) && std::isdigit(lower[i + 2])) {
                bool leftOk = (i == 0 || !std::isdigit(lower[i - 1]));
                bool rightOk = (i + 3 == lower.size() || !std::isdigit(lower[i + 3]));
                if (leftOk && rightOk) {
                    int val = std::stoi(lower.substr(i, 3));
                    if (val >= 21 && val <= 108) {
                        outNote = val;
                        break;
                    }
                }
            }
        }
    }

    return (outNote >= 0 && outNote <= 127);
}

ImportResult SampleImporter::importSampleFolder(const std::string& folderPath, const std::string& targetLibraryDir) {
    ImportResult result;
    if (!fs::is_directory(folderPath)) {
        result.message = "Directory not found: " + folderPath;
        return result;
    }

    fs::path p(folderPath);
    std::string baseName = sanitizeName(p.filename().string());
    std::string instId = "WAV_" + baseName;
    result.instrumentId = instId;
    result.instrumentName = p.filename().string() + " (Folder)";

    std::string samplesDir = targetLibraryDir + "/samples";
    std::string instDir = targetLibraryDir + "/instruments";
    fs::create_directories(samplesDir);
    fs::create_directories(instDir);

    struct ParsedSample {
        int note;
        int velocity;
        fs::path srcPath;
    };
    std::map<int, std::vector<ParsedSample>> samplesByNote;
    int sampleCount = 0;

    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        if (ext != ".wav" && ext != ".aif" && ext != ".aiff") continue;

        int note = -1, vel = 100;
        if (parseSampleFileNaming(entry.path().stem().string(), note, vel)) {
            samplesByNote[note].push_back({note, vel, entry.path()});
            sampleCount++;
        }
    }

    if (samplesByNote.empty()) {
        result.message = "No valid audio files with recognized note names (e.g. C4, 060) found in folder";
        return result;
    }

    struct LayerJson {
        int minVel;
        int maxVel;
        int targetVel;
        std::string filename;
    };
    std::map<int, std::vector<LayerJson>> noteZones;

    for (auto& [note, list] : samplesByNote) {
        std::sort(list.begin(), list.end(), [](const ParsedSample& a, const ParsedSample& b) {
            return a.velocity < b.velocity;
        });

        int prevVel = 1;
        for (size_t i = 0; i < list.size(); ++i) {
            bool isLast = (i + 1 == list.size());
            int minV = (i == 0) ? 1 : prevVel;
            int maxV = isLast ? 127 : list[i].velocity;
            prevVel = maxV + 1;

            std::string destFileName = instId + "_" + list[i].srcPath.filename().string();
            std::string destFullPath = samplesDir + "/" + destFileName;
            try {
                if (!fs::exists(destFullPath)) {
                    fs::copy_file(list[i].srcPath, destFullPath, fs::copy_options::overwrite_existing);
                }
            } catch (...) {}

            noteZones[note].push_back({minV, maxV, list[i].velocity, destFileName});
        }
    }

    // Write JSON
    std::string jsonPath = instDir + "/" + instId + ".json";
    std::ofstream out(jsonPath);
    if (!out) {
        result.message = "Failed to write instrument JSON: " + jsonPath;
        return result;
    }

    out << "{\n";
    out << "  \"name\": \"" << result.instrumentName << "\",\n";
    out << "  \"type\": \"FolderImport\",\n";
    out << "  \"notes\": {\n";

    size_t zoneIdx = 0;
    for (const auto& [note, layers] : noteZones) {
        out << "    \"" << note << "\": [\n";
        for (size_t l = 0; l < layers.size(); ++l) {
            out << "      {\n";
            out << "        \"min_velocity\": " << layers[l].minVel << ",\n";
            out << "        \"max_velocity\": " << layers[l].maxVel << ",\n";
            out << "        \"target_velocity\": " << layers[l].targetVel << ",\n";
            out << "        \"file\": \"" << layers[l].filename << "\"\n";
            out << "      }" << (l + 1 < layers.size() ? "," : "") << "\n";
        }
        out << "    ]" << (++zoneIdx < noteZones.size() ? "," : "") << "\n";
    }
    out << "  }\n";
    out << "}\n";

    result.success = true;
    result.sampleCount = sampleCount;
    result.message = "Successfully imported " + result.instrumentName + " (" + std::to_string(sampleCount) + " samples).";
    return result;
}
