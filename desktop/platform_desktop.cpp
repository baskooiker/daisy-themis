/**
 * @file platform_desktop.cpp
 * @brief Desktop platform implementation
 */

#include "platform_desktop.h"

DesktopPlatform* g_desktopPlatform = nullptr;

DesktopPlatform::DesktopPlatform()
{
    startTime = std::chrono::high_resolution_clock::now();

    // Initialize random state with time-based seed
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    randomState = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(epoch).count()
    );
}

uint32_t DesktopPlatform::GetMicroseconds()
{
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime);
    return static_cast<uint32_t>(duration.count());
}

uint32_t DesktopPlatform::GetRandomSeed()
{
    // Simple xorshift random number generator
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;

    // Mix in some time for additional entropy
    randomState ^= GetMicroseconds();

    return randomState;
}

float DesktopPlatform::GetSampleRate()
{
    return sampleRate;
}

void DesktopPlatform::SendMidiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity)
{
    if (midiNoteOnCallback) {
        midiNoteOnCallback(channel, note, velocity);
    }
}

void DesktopPlatform::SendMidiNoteOff(uint8_t channel, uint8_t note)
{
    if (midiNoteOffCallback) {
        midiNoteOffCallback(channel, note);
    }
}

void DesktopPlatform::SendMidiClock()
{
    if (midiClockCallback) {
        midiClockCallback();
    }
}

void DesktopPlatform::SendMidiStart()
{
    if (midiStartCallback) {
        midiStartCallback();
    }
}

void DesktopPlatform::SendMidiStop()
{
    if (midiStopCallback) {
        midiStopCallback();
    }
}

void DesktopPlatform::SetCVOutput(uint8_t channel, float voltage)
{
    if (channel < 4) {
        cvOutputs[channel] = voltage;
    }
}

void DesktopPlatform::SetGateOutput(uint8_t channel, bool high)
{
    if (channel < 4) {
        gateOutputs[channel] = high;
    }
}
