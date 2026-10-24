#pragma once

#include <CoreMIDI/CoreMIDI.h>
#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <iostream>

enum class MidiEventType {
    NoteOn,
    NoteOff,
    ControlChange,
    PitchBend,
    ProgramChange
};

struct MidiEvent {
    MidiEventType type;
    uint8_t channel;
    uint8_t data1; // note number or CC number
    uint8_t data2; // velocity or CC value
    int16_t pitchBend; // -8192 .. 8191
};

class MidiEngine {
public:
    using MidiCallback = std::function<void(const MidiEvent& event)>;

    MidiEngine();
    ~MidiEngine();

    bool init();
    void setMidiCallback(MidiCallback callback) {
        m_callback = callback;
    }

    std::vector<std::string> getConnectedDevices();
    bool connectDevice(int sourceIndex);
    bool connectDeviceByName(const std::string& partialName);
    void disconnect();

    std::string getActiveDeviceName() const { return m_activeDeviceName; }

private:
    static void notifyProc(const MIDINotification* message, void* refCon);
    static void readProc(const MIDIPacketList* pktlist, void* readProcRefCon, void* srcConnRefCon);

    void handlePacketList(const MIDIPacketList* pktlist);
    void checkAndAutoConnect();

    MIDIClientRef m_client = 0;
    MIDIPortRef m_inputPort = 0;
    MIDIEndpointRef m_sourceEndpoint = 0;
    MIDIEndpointRef m_virtualDestination = 0;
    std::string m_activeDeviceName = "None";
    MidiCallback m_callback = nullptr;
};
