#include "TimbrePreset.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>

static std::string escapeJson(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else res += c;
    }
    return res;
}

static std::string extractString(const std::string& json, const std::string& key) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return "";
    size_t q1 = json.find('"', pos + pattern.length());
    if (q1 == std::string::npos) return "";
    size_t q2 = json.find('"', q1 + 1);
    if (q2 == std::string::npos) return "";
    return json.substr(q1 + 1, q2 - q1 - 1);
}

static float extractFloat(const std::string& json, const std::string& key, float defaultVal) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defaultVal;
    size_t start = json.find_first_of("-0123456789.", pos + pattern.length());
    if (start == std::string::npos) return defaultVal;
    size_t end = json.find_first_not_of("0123456789.-+eE", start);
    try {
        return std::stof(json.substr(start, end - start));
    } catch (...) {
        return defaultVal;
    }
}

static bool extractBool(const std::string& json, const std::string& key, bool defaultVal) {
    std::string pattern = "\"" + key + "\":";
    size_t pos = json.find(pattern);
    if (pos == std::string::npos) return defaultVal;
    size_t vpos = json.find_first_not_of(" \t\r\n", pos + pattern.length());
    if (vpos == std::string::npos) return defaultVal;
    if (json.compare(vpos, 4, "true") == 0) return true;
    if (json.compare(vpos, 5, "false") == 0) return false;
    return defaultVal;
}

std::string TimbrePreset::serialize() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n"
       << "  \"name\": \"" << escapeJson(name) << "\",\n"
       << "  \"base_instrument\": \"" << escapeJson(baseInstrument) << "\",\n"
       << "  \"filter\": {\n"
       << "    \"cutoff_hz\": " << filterCutoff << ",\n"
       << "    \"resonance\": " << filterResonance << "\n"
       << "  },\n"
       << "  \"envelope\": {\n"
       << "    \"attack_ms\": " << attackMs << ",\n"
       << "    \"decay_ms\": " << decayMs << ",\n"
       << "    \"sustain_level\": " << sustainLevel << ",\n"
       << "    \"release_ms\": " << releaseMs << "\n"
       << "  },\n"
       << "  \"equalizer\": {\n"
       << "    \"enabled\": " << (eqEnabled ? "true" : "false") << ",\n"
       << "    \"low_gain_db\": " << eqLowGainDb << ",\n"
       << "    \"low_freq_hz\": " << eqLowFreqHz << ",\n"
       << "    \"mid_gain_db\": " << eqMidGainDb << ",\n"
       << "    \"mid_freq_hz\": " << eqMidFreqHz << ",\n"
       << "    \"mid_q\": " << eqMidQ << ",\n"
       << "    \"high_gain_db\": " << eqHighGainDb << ",\n"
       << "    \"high_freq_hz\": " << eqHighFreqHz << "\n"
       << "  },\n"
       << "  \"compressor\": {\n"
       << "    \"enabled\": " << (compEnabled ? "true" : "false") << ",\n"
       << "    \"threshold_db\": " << compThresholdDb << ",\n"
       << "    \"ratio\": " << compRatio << ",\n"
       << "    \"attack_ms\": " << compAttackMs << ",\n"
       << "    \"release_ms\": " << compReleaseMs << ",\n"
       << "    \"makeup_db\": " << compMakeupDb << ",\n"
       << "    \"knee_db\": " << compKneeDb << "\n"
       << "  },\n"
       << "  \"master\": {\n"
       << "    \"volume\": " << masterVolume << "\n"
       << "  }\n"
       << "}\n";
    return ss.str();
}

bool TimbrePreset::deserialize(const std::string& json) {
    if (json.empty()) return false;

    std::string n = extractString(json, "name");
    if (!n.empty()) name = n;

    std::string b = extractString(json, "base_instrument");
    if (!b.empty()) baseInstrument = b;

    // Filter
    filterCutoff = extractFloat(json, "cutoff_hz", filterCutoff);
    filterResonance = extractFloat(json, "resonance", filterResonance);

    // Envelope
    attackMs = extractFloat(json, "attack_ms", attackMs);
    decayMs = extractFloat(json, "decay_ms", decayMs);
    sustainLevel = extractFloat(json, "sustain_level", sustainLevel);
    releaseMs = extractFloat(json, "release_ms", releaseMs);

    // EQ
    eqEnabled = extractBool(json, "enabled", eqEnabled);
    eqLowGainDb = extractFloat(json, "low_gain_db", eqLowGainDb);
    eqLowFreqHz = extractFloat(json, "low_freq_hz", eqLowFreqHz);
    eqMidGainDb = extractFloat(json, "mid_gain_db", eqMidGainDb);
    eqMidFreqHz = extractFloat(json, "mid_freq_hz", eqMidFreqHz);
    eqMidQ = extractFloat(json, "mid_q", eqMidQ);
    eqHighGainDb = extractFloat(json, "high_gain_db", eqHighGainDb);
    eqHighFreqHz = extractFloat(json, "high_freq_hz", eqHighFreqHz);

    // Compressor
    compThresholdDb = extractFloat(json, "threshold_db", compThresholdDb);
    compRatio = extractFloat(json, "ratio", compRatio);
    compAttackMs = extractFloat(json, "attack_ms", compAttackMs);
    compReleaseMs = extractFloat(json, "release_ms", compReleaseMs);
    compMakeupDb = extractFloat(json, "makeup_db", compMakeupDb);
    compKneeDb = extractFloat(json, "knee_db", compKneeDb);

    // Master
    masterVolume = extractFloat(json, "volume", masterVolume);

    return true;
}

bool TimbrePreset::saveToFile(const std::string& path) const {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "[Preset] Error opening file for writing: " << path << std::endl;
        return false;
    }
    out << serialize();
    return true;
}

bool TimbrePreset::loadFromFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "[Preset] Error opening file for reading: " << path << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return deserialize(buffer.str());
}
