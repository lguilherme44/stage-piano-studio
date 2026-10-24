#pragma once

#include <CoreAudio/CoreAudio.h>
#include <AudioToolbox/AudioToolbox.h>
#include <functional>
#include <vector>
#include <string>

class AudioEngine {
public:
    using RenderCallback = std::function<void(float* outputBuffer, uint32_t numFrames, uint32_t numChannels)>;

    AudioEngine();
    ~AudioEngine();

    bool init(uint32_t sampleRate = 44100, uint32_t bufferSize = 256);
    bool start();
    bool stop();

    void setRenderCallback(RenderCallback callback) {
        m_callback = callback;
    }

    uint32_t getSampleRate() const { return m_sampleRate; }
    uint32_t getBufferSize() const { return m_bufferSize; }
    std::string getDeviceName() const { return m_deviceName; }

private:
    static OSStatus audioCallback(void* inRefCon,
                                  AudioUnitRenderActionFlags* ioActionFlags,
                                  const AudioTimeStamp* inTimeStamp,
                                  UInt32 inBusNumber,
                                  UInt32 inNumberFrames,
                                  AudioBufferList* ioData);

    AudioComponentInstance m_audioUnit = nullptr;
    uint32_t m_sampleRate = 44100;
    uint32_t m_bufferSize = 256;
    std::string m_deviceName = "Default Output";
    RenderCallback m_callback = nullptr;
    bool m_running = false;
};
