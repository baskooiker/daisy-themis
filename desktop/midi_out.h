/**
 * @file midi_out.h
 * @brief MIDI output wrapper using RtMidi
 */

#ifndef THEMIS_MIDI_OUT_H
#define THEMIS_MIDI_OUT_H

#ifdef THEMIS_ENABLE_MIDI

#include <string>
#include <vector>
#include <memory>

// Forward declaration
class RtMidiOut;

namespace themis_midi {

/**
 * @class MidiOutput
 * @brief RtMidi wrapper for MIDI output
 */
class MidiOutput {
public:
    MidiOutput();
    ~MidiOutput();

    /**
     * @brief Get list of available MIDI output ports
     */
    std::vector<std::string> GetAvailablePorts();

    /**
     * @brief Open a MIDI output port by index
     * @param index Port index from GetAvailablePorts()
     * @return true if successful
     */
    bool OpenPort(int index);

    /**
     * @brief Close current port
     */
    void Close();

    /**
     * @brief Check if a port is currently open
     */
    bool IsOpen() const { return portOpen; }

    /**
     * @brief Get name of current port
     */
    std::string GetCurrentPortName() const { return currentPortName; }

    /**
     * @brief Send MIDI Note On message
     */
    void NoteOn(uint8_t channel, uint8_t note, uint8_t velocity);

    /**
     * @brief Send MIDI Note Off message
     */
    void NoteOff(uint8_t channel, uint8_t note);

    /**
     * @brief Send MIDI Clock message
     */
    void Clock();

    /**
     * @brief Send MIDI Start message
     */
    void Start();

    /**
     * @brief Send MIDI Stop message
     */
    void Stop();

    /**
     * @brief Send MIDI Continue message
     */
    void Continue();

private:
    std::unique_ptr<RtMidiOut> midiOut;
    bool portOpen = false;
    std::string currentPortName;
};

// Global MIDI output instance
extern MidiOutput g_midiOutput;

} // namespace themis_midi

#endif // THEMIS_ENABLE_MIDI

#endif // THEMIS_MIDI_OUT_H
