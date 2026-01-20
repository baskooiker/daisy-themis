/**
 * @file midi_out.cpp
 * @brief MIDI output implementation using RtMidi
 */

#ifdef THEMIS_ENABLE_MIDI

#include "midi_out.h"
#include <rtmidi/RtMidi.h>
#include <iostream>

namespace themis_midi {

// Global instance
MidiOutput g_midiOutput;

MidiOutput::MidiOutput()
{
    try {
        midiOut = std::make_unique<RtMidiOut>();
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
        midiOut = nullptr;
    }
}

MidiOutput::~MidiOutput()
{
    Close();
}

std::vector<std::string> MidiOutput::GetAvailablePorts()
{
    std::vector<std::string> ports;

    if (!midiOut) {
        return ports;
    }

    try {
        unsigned int numPorts = midiOut->getPortCount();
        for (unsigned int i = 0; i < numPorts; i++) {
            ports.push_back(midiOut->getPortName(i));
        }
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }

    return ports;
}

bool MidiOutput::OpenPort(int index)
{
    if (!midiOut) {
        return false;
    }

    Close();

    try {
        unsigned int numPorts = midiOut->getPortCount();
        if (index < 0 || index >= (int)numPorts) {
            return false;
        }

        midiOut->openPort(index);
        currentPortName = midiOut->getPortName(index);
        portOpen = true;
        return true;

    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
        portOpen = false;
        return false;
    }
}

void MidiOutput::Close()
{
    if (midiOut && portOpen) {
        try {
            midiOut->closePort();
        } catch (RtMidiError& error) {
            std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
        }
    }
    portOpen = false;
    currentPortName.clear();
}

void MidiOutput::NoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {
            static_cast<unsigned char>(0x90 | (channel & 0x0F)),
            static_cast<unsigned char>(note & 0x7F),
            static_cast<unsigned char>(velocity & 0x7F)
        };
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

void MidiOutput::NoteOff(uint8_t channel, uint8_t note)
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {
            static_cast<unsigned char>(0x80 | (channel & 0x0F)),
            static_cast<unsigned char>(note & 0x7F),
            0
        };
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

void MidiOutput::Clock()
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {0xF8};
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

void MidiOutput::Start()
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {0xFA};
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

void MidiOutput::Stop()
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {0xFC};
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

void MidiOutput::Continue()
{
    if (!midiOut || !portOpen) return;

    try {
        std::vector<unsigned char> message = {0xFB};
        midiOut->sendMessage(&message);
    } catch (RtMidiError& error) {
        std::cerr << "RtMidi error: " << error.getMessage() << std::endl;
    }
}

} // namespace themis_midi

#endif // THEMIS_ENABLE_MIDI
