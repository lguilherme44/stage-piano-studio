#pragma once

#include <string>
#include <vector>

struct ImportResult {
    bool success = false;
    std::string message;
    std::string instrumentId;
    std::string instrumentName;
    int sampleCount = 0;
};

class SampleImporter {
public:
    static ImportResult importSFZ(const std::string& sfzFilePath, const std::string& targetLibraryDir);
    static ImportResult importSF2(const std::string& sf2FilePath, const std::string& targetLibraryDir);
    static ImportResult importSampleFolder(const std::string& folderPath, const std::string& targetLibraryDir);
};
