#include "AudioEngine.hpp"
#include <iostream>

AudioEngine::AudioEngine() {}

AudioEngine::~AudioEngine() {
    stop();
    if (m_audioUnit) {
        AudioComponentInstanceDispose(m_audioUnit);
        m_audioUnit = nullptr;
    }
}

OSStatus AudioEngine::audioCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData) {
    auto* self = static_cast<AudioEngine*>(inRefCon);
    if (!self || !self->m_callback) {
        // Output silence
        for (UInt32 i = 0; i < ioData->mNumberBuffers; ++i) {
            memset(ioData->mBuffers[i].mData, 0, ioData->mBuffers[i].mDataByteSize);
        }
        return noErr;
    }

    // Standard stereo interleaved or deinterleaved float buffer
    if (ioData->mNumberBuffers == 2) {
        // De-interleaved stereo
        std::vector<float> tempInterleaved(inNumberFrames * 2, 0.0f);
        self->m_callback(tempInterleaved.data(), inNumberFrames, 2);
        
        float* left = static_cast<float*>(ioData->mBuffers[0].mData);
        float* right = static_cast<float*>(ioData->mBuffers[1].mData);
        for (UInt32 i = 0; i < inNumberFrames; ++i) {
            left[i] = tempInterleaved[i * 2];
            right[i] = tempInterleaved[i * 2 + 1];
        }
    } else if (ioData->mNumberBuffers == 1) {
        // Interleaved
        float* out = static_cast<float*>(ioData->mBuffers[0].mData);
        memset(out, 0, ioData->mBuffers[0].mDataByteSize);
        self->m_callback(out, inNumberFrames, ioData->mBuffers[0].mNumberChannels);
    }

    return noErr;
}

bool AudioEngine::init(uint32_t sampleRate, uint32_t bufferSize) {
    m_sampleRate = sampleRate;
    m_bufferSize = bufferSize;

    AudioComponentDescription desc;
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

    AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
    if (!comp) {
        std::cerr << "[AudioEngine] Error: Could not find default output AudioComponent\n";
        return false;
    }

    OSStatus status = AudioComponentInstanceNew(comp, &m_audioUnit);
    if (status != noErr) {
        std::cerr << "[AudioEngine] Error: AudioComponentInstanceNew failed with status: " << status << "\n";
        return false;
    }

    // Set Stream Format: 32-bit Float, Stereo, 44100Hz
    AudioStreamBasicDescription asbd;
    memset(&asbd, 0, sizeof(asbd));
    asbd.mSampleRate = static_cast<Float64>(m_sampleRate);
    asbd.mFormatID = kAudioFormatLinearPCM;
    asbd.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagsNativeEndian;
    asbd.mBitsPerChannel = 32;
    asbd.mChannelsPerFrame = 2;
    asbd.mBytesPerFrame = asbd.mChannelsPerFrame * sizeof(Float32);
    asbd.mFramesPerPacket = 1;
    asbd.mBytesPerPacket = asbd.mBytesPerFrame;

    status = AudioUnitSetProperty(m_audioUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &asbd,
                                  sizeof(asbd));
    if (status != noErr) {
        std::cerr << "[AudioEngine] Warning: Setting stream format failed: " << status << "\n";
    }

    // Set Render Callback
    AURenderCallbackStruct callbackStruct;
    callbackStruct.inputProc = audioCallback;
    callbackStruct.inputProcRefCon = this;

    status = AudioUnitSetProperty(m_audioUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input,
                                  0,
                                  &callbackStruct,
                                  sizeof(callbackStruct));
    if (status != noErr) {
        std::cerr << "[AudioEngine] Error: Setting render callback failed: " << status << "\n";
        return false;
    }

    // Set Buffer Frame Size
    UInt32 frames = m_bufferSize;
    AudioUnitSetProperty(m_audioUnit,
                         kAudioDevicePropertyBufferFrameSize,
                         kAudioUnitScope_Global,
                         0,
                         &frames,
                         sizeof(frames));

    status = AudioUnitInitialize(m_audioUnit);
    if (status != noErr) {
        std::cerr << "[AudioEngine] Error: AudioUnitInitialize failed: " << status << "\n";
        return false;
    }

    return true;
}

bool AudioEngine::start() {
    if (!m_audioUnit) return false;
    OSStatus status = AudioOutputUnitStart(m_audioUnit);
    if (status == noErr) {
        m_running = true;
        return true;
    }
    std::cerr << "[AudioEngine] Error starting AudioUnit: " << status << "\n";
    return false;
}

bool AudioEngine::stop() {
    if (m_audioUnit && m_running) {
        AudioOutputUnitStop(m_audioUnit);
        m_running = false;
        return true;
    }
    return false;
}
