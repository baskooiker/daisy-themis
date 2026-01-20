/**
 * @file platform_desktop.h
 * @brief Desktop platform implementation for Themis
 */

#ifndef PLATFORM_DESKTOP_H
#define PLATFORM_DESKTOP_H

#include "themis_platform.h"
#include <chrono>
#include <functional>

/**
 * @class DesktopPlatform
 * @brief Platform implementation for desktop (SDL2/ImGui)
 */
class DesktopPlatform : public themis::Platform {
public:
    DesktopPlatform();

    uint32_t GetMicroseconds() override;
    uint32_t GetRandomSeed() override;
    float GetSampleRate() override;

    void SendMidiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override;
    void SendMidiNoteOff(uint8_t channel, uint8_t note) override;
    void SendMidiClock() override;
    void SendMidiStart() override;
    void SendMidiStop() override;

    void SetCVOutput(uint8_t channel, float voltage) override;
    void SetGateOutput(uint8_t channel, bool high) override;

    // Desktop-specific configuration
    void SetSampleRate(float rate) { sampleRate = rate; }

    // Callbacks for MIDI (set by midi_out module)
    std::function<void(uint8_t, uint8_t, uint8_t)> midiNoteOnCallback;
    std::function<void(uint8_t, uint8_t)> midiNoteOffCallback;
    std::function<void()> midiClockCallback;
    std::function<void()> midiStartCallback;
    std::function<void()> midiStopCallback;

    // CV/Gate state (readable by UI)
    float cvOutputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool gateOutputs[4] = {false, false, false, false};

private:
    std::chrono::high_resolution_clock::time_point startTime;
    float sampleRate = 48000.0f;
    uint32_t randomState = 12345;
};

// Global desktop platform instance
extern DesktopPlatform* g_desktopPlatform;

#endif // PLATFORM_DESKTOP_H
