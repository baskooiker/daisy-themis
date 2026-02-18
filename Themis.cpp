/**
 * @file Themis.cpp
 * @brief Master Clock & Generative Drum Sequencer for Daisy Patch
 * @version 1.0
 *
 * Main application file containing audio callback and main loop.
 * All functionality is split into modules:
 *
 *   - types.h      : Type definitions, enums, structs
 *   - globals.h/cpp: Global variables and const data
 *   - groove.h/cpp : Trigger queues and groove timing
 *   - drums.h/cpp  : Drum pattern generation and processing
 *   - melody.h/cpp : Melody generation
 *   - display.h/cpp: OLED display rendering
 *   - config.h/cpp : Settings, controls, MIDI handling
 */

#include "daisy_patch.h"
#include "daisysp.h"
#include <string>

#include "types.h"
#include "globals.h"
#include "groove.h"
#include "drums.h"
#include "melody.h"
#include "display.h"
#include "config.h"

using namespace daisy;
using namespace daisysp;

// External declaration for trigger flag (defined in config.cpp)
extern volatile bool trigger16thNote;

// ============================================================================
// AUDIO CALLBACK
// ============================================================================

/**
 * @brief Audio callback - outputs gate signals and processes trigger queues
 *
 * Called at 48kHz sample rate. Handles:
 * - Sample-accurate drum trigger queue processing
 * - Sample-accurate melody trigger queue processing
 * - Gate output timing for all audio outputs
 * - Internal clock Metro timing
 */
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        // Process scheduled MIDI triggers (sample-accurate groove timing)
        ProcessTriggerQueue();

        // Process scheduled melody triggers (sample-accurate groove timing)
        ProcessMelodyQueue();

        // Increment global sample counter
        globalSampleCounter++;

        // Process Metro for timing (only in internal clock mode)
        if(!externalClockMode && isRunning)
        {
            if(clockMetro.Process())
            {
                trigger16thNote = true; // Signal main loop to process patterns
                lastBeatSample = globalSampleCounter; // Record exact sample time of beat
            }
        }

        // Update gate pulse counters and turn off gates after pulse width
        if(gate16th)
        {
            gate16thCounter++;
            if(gate16thCounter >= GATE_PULSE_SAMPLES)
                gate16th = false;
        }

        if(gateReset)
        {
            gateResetCounter++;
            if(gateResetCounter >= RESET_PULSE_SAMPLES)
                gateReset = false;
        }

        if(melodyGate)
        {
            melodyGateCounter++;
            if(melodyGateCounter >= GATE_PULSE_SAMPLES)
                melodyGate = false;
        }

        if(bassGate)
        {
            bassGateCounter++;
            if(bassGateCounter >= GATE_PULSE_SAMPLES)
                bassGate = false;
        }

        if(analogDrumGate)
        {
            analogDrumGateCounter++;
            if(analogDrumGateCounter >= GATE_PULSE_SAMPLES)
                analogDrumGate = false;
        }

        // Output gate signals (5V = 1.0f, 0V = 0.0f for CV outputs)
        out[0][i] = gate16th ? 1.0f : 0.0f;       // OUT1: 16th note clock
        out[1][i] = gateReset ? 1.0f : 0.0f;      // OUT2: Reset trigger
        out[2][i] = melodyGate ? 1.0f : 0.0f;     // OUT3: Melody gate
        out[3][i] = bassGate ? 1.0f : 0.0f;       // OUT4: Bass gate
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(void)
{
    // Initialize hardware
    hw.Init();
    float samplerate = hw.AudioSampleRate();

    // Load saved settings from persistent storage
    LoadSettings();

    // Initialize clock metro
    clockMetro.Init(1.0f, samplerate);
    UpdateClockFrequency();

    // Display initial state
    hw.display.Fill(false);
    std::string str = "Themis";
    char*       cstr = &str[0];
    hw.display.WriteString(cstr, Font_6x8, true);
    hw.display.Update();

    // Start MIDI and Audio
    hw.midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Main loop
    uint32_t lastDisplayUpdate = System::GetNow();
    const uint32_t DISPLAY_UPDATE_RATE = 100; // Update display every 100ms

    for(;;)
    {
        uint32_t now = System::GetNow();

        // Process controls
        ProcessControls();

        // Process clock and gates
        ProcessClock();

        // Handle MIDI input
        hw.midi.Listen();
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        // Update display at regular intervals
        if(now - lastDisplayUpdate > DISPLAY_UPDATE_RATE)
        {
            UpdateDisplay();
            lastDisplayUpdate = now;
        }

        // Small delay to prevent tight loop
        System::Delay(1);
    }
}
