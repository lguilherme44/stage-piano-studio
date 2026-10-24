#include "MidiEngine.hpp"
#include <iostream>
#include <algorithm>

MidiEngine::MidiEngine() {}

MidiEngine::~MidiEngine() {
    disconnect();
    if (m_virtualDestination) {
        MIDIEndpointDispose(m_virtualDestination);
        m_virtualDestination = 0;
    }
    if (m_inputPort) {
        MIDIPortDispose(m_inputPort);
        m_inputPort = 0;
    }
    if (m_client) {
        MIDIClientDispose(m_client);
        m_client = 0;
    }
}

bool MidiEngine::init() {
    OSStatus status = MIDIClientCreateWithBlock(CFSTR("StagePianoStudioMidiClient"), &m_client, ^(const MIDINotification* msg) {
        notifyProc(msg, this);
    });

    if (status != noErr) {
        std::cerr << "[MidiEngine] Error creating MIDIClient: " << status << "\n";
        return false;
    }

    status = MIDIInputPortCreateWithBlock(m_client, CFSTR("StagePianoStudioInputPort"), &m_inputPort, ^(const MIDIPacketList* pktlist, void* srcConnRefCon) {
        (void)srcConnRefCon;
        handlePacketList(pktlist);
    });

    if (status != noErr) {
        std::cerr << "[MidiEngine] Error creating MIDIInputPort: " << status << "\n";
        return false;
    }

    // Create Virtual Destination for DAWs and software MIDI input
    status = MIDIDestinationCreateWithBlock(m_client, CFSTR("StagePianoStudio"), &m_virtualDestination, ^(const MIDIPacketList* pktlist, void* srcConnRefCon) {
        (void)srcConnRefCon;
        handlePacketList(pktlist);
    });
    if (status == noErr) {
        std::cout << "[MidiEngine] Virtual MIDI Input 'StagePianoStudio' active (available to all macOS apps).\n";
    }

    // Auto-connect to any available MIDI controller
    checkAndAutoConnect();

    return true;
}

void MidiEngine::notifyProc(const MIDINotification* message, void* refCon) {
    auto* self = static_cast<MidiEngine*>(refCon);
    if (!self) return;

    if (message->messageID == kMIDIMsgObjectAdded || message->messageID == kMIDIMsgSetupChanged) {
        std::cout << "[MidiEngine] Hardware configuration changed. Scanning for MIDI devices...\n";
        self->checkAndAutoConnect();
    } else if (message->messageID == kMIDIMsgObjectRemoved) {
        std::cout << "[MidiEngine] MIDI device disconnected.\n";
        self->checkAndAutoConnect();
    }
}

std::vector<std::string> MidiEngine::getConnectedDevices() {
    std::vector<std::string> devices;
    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; ++i) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (src) {
            CFStringRef nameRef = nullptr;
            MIDIObjectGetStringProperty(src, kMIDIPropertyName, &nameRef);
            if (nameRef) {
                char buf[256];
                CFStringGetCString(nameRef, buf, sizeof(buf), kCFStringEncodingUTF8);
                CFRelease(nameRef);
                devices.push_back(std::string(buf));
            } else {
                devices.push_back("Unknown MIDI Device " + std::to_string(i));
            }
        }
    }
    return devices;
}

bool MidiEngine::connectDevice(int sourceIndex) {
    ItemCount count = MIDIGetNumberOfSources();
    if (sourceIndex < 0 || sourceIndex >= static_cast<int>(count)) {
        return false;
    }

    disconnect();

    MIDIEndpointRef src = MIDIGetSource(sourceIndex);
    if (!src) return false;

    OSStatus status = MIDIPortConnectSource(m_inputPort, src, nullptr);
    if (status == noErr) {
        m_sourceEndpoint = src;
        CFStringRef nameRef = nullptr;
        MIDIObjectGetStringProperty(src, kMIDIPropertyName, &nameRef);
        if (nameRef) {
            char buf[256];
            CFStringGetCString(nameRef, buf, sizeof(buf), kCFStringEncodingUTF8);
            CFRelease(nameRef);
            m_activeDeviceName = std::string(buf);
        } else {
            m_activeDeviceName = "Device " + std::to_string(sourceIndex);
        }
        std::cout << "[MidiEngine] Successfully connected to: " << m_activeDeviceName << "\n";
        return true;
    }

    return false;
}

bool MidiEngine::connectDeviceByName(const std::string& partialName) {
    ItemCount count = MIDIGetNumberOfSources();
    std::string lowerQuery = partialName;
    std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

    for (ItemCount i = 0; i < count; ++i) {
        MIDIEndpointRef src = MIDIGetSource(i);
        if (src) {
            CFStringRef nameRef = nullptr;
            MIDIObjectGetStringProperty(src, kMIDIPropertyName, &nameRef);
            if (nameRef) {
                char buf[256];
                CFStringGetCString(nameRef, buf, sizeof(buf), kCFStringEncodingUTF8);
                CFRelease(nameRef);
                std::string devName(buf);
                std::string lowerDev = devName;
                std::transform(lowerDev.begin(), lowerDev.end(), lowerDev.begin(), ::tolower);

                if (lowerDev.find(lowerQuery) != std::string::npos) {
                    return connectDevice(static_cast<int>(i));
                }
            }
        }
    }
    return false;
}

void MidiEngine::disconnect() {
    if (m_sourceEndpoint && m_inputPort) {
        MIDIPortDisconnectSource(m_inputPort, m_sourceEndpoint);
        m_sourceEndpoint = 0;
        m_activeDeviceName = "None";
    }
}

void MidiEngine::checkAndAutoConnect() {
    auto devices = getConnectedDevices();
    if (devices.empty()) {
        std::cout << "[MidiEngine] No MIDI devices detected. Connect a USB MIDI keyboard or controller.\n";
        disconnect();
        return;
    }

    std::cout << "[MidiEngine] Available MIDI Devices:\n";
    for (size_t i = 0; i < devices.size(); ++i) {
        std::cout << "  [" << i << "] " << devices[i] << "\n";
    }

    // Auto-connect to the first available hardware controller if not already connected
    if (!m_sourceEndpoint && !devices.empty()) {
        connectDevice(0);
    }
}

void MidiEngine::handlePacketList(const MIDIPacketList* pktlist) {
    if (!pktlist || !m_callback) return;

    const MIDIPacket* packet = &pktlist->packet[0];
    for (UInt32 i = 0; i < pktlist->numPackets; ++i) {
        UInt16 length = packet->length;
        const Byte* data = packet->data;

        for (UInt16 b = 0; b < length; ) {
            Byte status = data[b];
            
            // Check for channel voice messages (0x80 .. 0xEF)
            if (status >= 0x80 && status <= 0xEF) {
                Byte msgType = status & 0xF0;
                Byte channel = status & 0x0F;

                if (msgType == 0x90) { // Note On
                    if (b + 2 < length) {
                        Byte note = data[b + 1];
                        Byte vel = data[b + 2];
                        b += 3;
                        if (vel == 0) {
                            m_callback({MidiEventType::NoteOff, channel, note, 0, 0});
                        } else {
                            m_callback({MidiEventType::NoteOn, channel, note, vel, 0});
                        }
                        continue;
                    }
                } else if (msgType == 0x80) { // Note Off
                    if (b + 2 < length) {
                        Byte note = data[b + 1];
                        Byte vel = data[b + 2];
                        b += 3;
                        m_callback({MidiEventType::NoteOff, channel, note, vel, 0});
                        continue;
                    }
                } else if (msgType == 0xB0) { // Control Change
                    if (b + 2 < length) {
                        Byte cc = data[b + 1];
                        Byte val = data[b + 2];
                        b += 3;
                        m_callback({MidiEventType::ControlChange, channel, cc, val, 0});
                        continue;
                    }
                } else if (msgType == 0xE0) { // Pitch Bend
                    if (b + 2 < length) {
                        Byte lsb = data[b + 1];
                        Byte msb = data[b + 2];
                        b += 3;
                        int16_t bend = static_cast<int16_t>((msb << 7) | lsb) - 8192;
                        m_callback({MidiEventType::PitchBend, channel, 0, 0, bend});
                        continue;
                    }
                } else if (msgType == 0xC0) { // Program Change
                    if (b + 1 < length) {
                        Byte prog = data[b + 1];
                        b += 2;
                        m_callback({MidiEventType::ProgramChange, channel, prog, 0, 0});
                        continue;
                    }
                }
            }
            b++;
        }
        packet = MIDIPacketNext(packet);
    }
}
