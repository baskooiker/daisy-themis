/**
 * @file themis_platform.h
 * @brief Platform abstraction interface for Themis
 *
 * This interface allows Themis core algorithms to run on different platforms
 * (Daisy hardware, desktop, etc.) by abstracting hardware-specific functionality.
 */

#ifndef THEMIS_PLATFORM_H
#define THEMIS_PLATFORM_H

#include <cstdint>

namespace themis {

/**
 * @class Platform
 * @brief Abstract interface for platform-specific functionality
 *
 * Each platform (Daisy firmware, desktop app) implements this interface
 * to provide hardware-specific services to the core algorithms.
 */
class Platform {
public:
    virtual ~Platform() = default;

    // ========================================================================
    // Random / Time
    // ========================================================================

    /**
     * @brief Get current time in microseconds
     * @return Microseconds since startup (or epoch on desktop)
     */
    virtual uint32_t GetMicroseconds() = 0;

    /**
     * @brief Get a seed value for random number generation
     * @return Pseudo-random seed based on time or hardware entropy
     */
    virtual uint32_t GetRandomSeed() = 0;

    // ========================================================================
    // Audio
    // ========================================================================

    /**
     * @brief Get the audio sample rate
     * @return Sample rate in Hz (typically 48000)
     */
    virtual float GetSampleRate() = 0;

    // ========================================================================
    // MIDI Output
    // ========================================================================

    /**
     * @brief Send a MIDI Note On message
     * @param channel MIDI channel (0-15)
     * @param note MIDI note number (0-127)
     * @param velocity Note velocity (1-127)
     */
    virtual void SendMidiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) = 0;

    /**
     * @brief Send a MIDI Note Off message
     * @param channel MIDI channel (0-15)
     * @param note MIDI note number (0-127)
     */
    virtual void SendMidiNoteOff(uint8_t channel, uint8_t note) = 0;

    /**
     * @brief Send a MIDI Clock message
     */
    virtual void SendMidiClock() = 0;

    /**
     * @brief Send a MIDI Start message
     */
    virtual void SendMidiStart() = 0;

    /**
     * @brief Send a MIDI Stop message
     */
    virtual void SendMidiStop() = 0;

    // ========================================================================
    // CV/Gate Output (optional, only on hardware)
    // ========================================================================

    /**
     * @brief Set CV output voltage
     * @param channel CV channel (0-based)
     * @param voltage Voltage value (0-5V typically)
     */
    virtual void SetCVOutput(uint8_t channel, float voltage) = 0;

    /**
     * @brief Set gate output state
     * @param channel Gate channel (0-based)
     * @param high True for gate high, false for gate low
     */
    virtual void SetGateOutput(uint8_t channel, bool high) = 0;
};

/**
 * @brief Global platform instance
 *
 * Set this at startup to point to the platform-specific implementation.
 * All core algorithms use this pointer to access platform services.
 */
extern Platform* g_platform;

} // namespace themis

#endif // THEMIS_PLATFORM_H
