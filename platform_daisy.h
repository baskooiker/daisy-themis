/**
 * @file platform_daisy.h
 * @brief Daisy platform implementation for Themis
 */

#ifndef PLATFORM_DAISY_H
#define PLATFORM_DAISY_H

#include "core/themis_platform.h"
#include "daisy_patch.h"

using namespace daisy;

/**
 * @class DaisyPlatform
 * @brief Platform implementation for Daisy Patch hardware
 */
class DaisyPlatform : public themis::Platform {
public:
    DaisyPlatform(DaisyPatch* hardware) : hw(hardware) {}

    uint32_t GetMicroseconds() override
    {
        return System::GetUs();
    }

    uint32_t GetRandomSeed() override
    {
        return System::GetUs();
    }

    float GetSampleRate() override
    {
        return hw->AudioSampleRate();
    }

    void SendMidiNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override
    {
        uint8_t msg[3] = {
            static_cast<uint8_t>(0x90 | channel),
            note,
            velocity
        };
        hw->midi.SendMessage(msg, 3);
    }

    void SendMidiNoteOff(uint8_t channel, uint8_t note) override
    {
        uint8_t msg[3] = {
            static_cast<uint8_t>(0x80 | channel),
            note,
            0
        };
        hw->midi.SendMessage(msg, 3);
    }

    void SendMidiClock() override
    {
        uint8_t msg[1] = {0xF8};
        hw->midi.SendMessage(msg, 1);
    }

    void SendMidiStart() override
    {
        uint8_t msg[1] = {0xFA};
        hw->midi.SendMessage(msg, 1);
    }

    void SendMidiStop() override
    {
        uint8_t msg[1] = {0xFC};
        hw->midi.SendMessage(msg, 1);
    }

    void SetCVOutput(uint8_t channel, float voltage) override
    {
        // Convert voltage (0-5V) to DAC value (0-4095)
        uint16_t dacValue = (uint16_t)(voltage / 5.0f * 4095.0f);
        if(dacValue > 4095) dacValue = 4095;

        DacHandle::Channel dacChannel = (channel == 0)
            ? DacHandle::Channel::ONE
            : DacHandle::Channel::TWO;
        hw->seed.dac.WriteValue(dacChannel, dacValue);
    }

    void SetGateOutput(uint8_t channel, bool high) override
    {
        // Gate outputs are directly controlled via hw->gate_output
        // Channel 0 = Gate Out
        // This is handled in the main code for now
        (void)channel;
        (void)high;
    }

private:
    DaisyPatch* hw;
};

#endif // PLATFORM_DAISY_H
