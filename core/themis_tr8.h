/**
 * @file themis_tr8.h
 * @brief TR-8 drum machine voice - external drum machine via MIDI
 *
 * Controls a Roland TR-8 (or compatible) drum machine over MIDI.
 * 11 voices with predefined techno/house-style kit patterns,
 * accent support, AB variation, and random fills.
 * MIDI-only voice (no internal synth).
 */

#ifndef THEMIS_TR8_H
#define THEMIS_TR8_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// TR-8 VOICE DEFINITIONS
// ============================================================================

enum TR8Voice : uint8_t {
    TR8_BD = 0,   // Bass Drum
    TR8_RS,       // Rim Shot
    TR8_SD,       // Snare Drum
    TR8_HC,       // Hand Clap
    TR8_CH,       // Closed HiHat
    TR8_LT,       // Low Tom
    TR8_OH,       // Open HiHat
    TR8_MT,       // Mid Tom
    TR8_CC,       // Crash Cymbal
    TR8_HT,       // High Tom
    TR8_RC,       // Ride Cymbal
    NUM_TR8_VOICES
};

constexpr uint8_t NUM_TR8_KITS = 16;
constexpr uint8_t NUM_TR8_FILLS = 8;

extern const uint8_t tr8MidiNotes[NUM_TR8_VOICES];
extern const char* tr8VoiceNames[NUM_TR8_VOICES];

// ============================================================================
// TR-8 KIT STRUCTURE
// ============================================================================

/**
 * @struct TR8Kit
 * @brief One complete 16-step kit pattern for all 11 TR-8 voices
 *
 * Patterns are 16-step (MSB = step 0), using uint16_t.
 * Accents use a separate bitmask per voice.
 */
struct TR8Kit {
    uint16_t triggers[NUM_TR8_VOICES];  ///< 16-step trigger patterns
    uint16_t accents[NUM_TR8_VOICES];   ///< 16-step accent patterns
    const char* name;
};

/**
 * @struct TR8FillKit
 * @brief 8-step fill pattern for snare, toms, and crash
 */
struct TR8FillKit {
    uint8_t snare;    ///< Snare fill pattern (8-step, MSB = step 0)
    uint8_t tom;      ///< Tom cascade pattern (distributed across LT/MT/HT)
    uint8_t crash;    ///< Crash/ride accent pattern
    const char* name;
};

extern const TR8Kit tr8Kits[NUM_TR8_KITS];
extern const TR8FillKit tr8Fills[NUM_TR8_FILLS];

// ============================================================================
// TR-8 CONFIG AND STATE
// ============================================================================

/**
 * @struct TR8Config
 * @brief User-configurable TR-8 voice settings
 */
struct TR8Config {
    bool active;                    ///< Is TR-8 voice enabled
    uint8_t midiChannel;            ///< MIDI output channel (0-15), default 9 (ch10)
    uint8_t kitIndex;               ///< Current kit A index
    bool freezeKit;                 ///< Prevent auto-randomization
    bool fillsEnabled;              ///< Enable drum fills
    VariationConfig variation;      ///< AB kit switching

    void Init()
    {
        active = false;             // Off by default (external hardware)
        midiChannel = 9;            // Channel 10 (0-indexed)
        kitIndex = 0;
        freezeKit = false;
        fillsEnabled = true;
        variation.mode = VAR_MODE_OFF;
        variation.sequence = VAR_SEQ_AAAB;
        variation.granularity = VAR_GRAN_BAR;
    }
};

/**
 * @struct TR8State
 * @brief Runtime state for TR-8 voice
 */
struct TR8State {
    uint8_t currentKitA;            ///< Active A kit index
    uint8_t currentKitB;            ///< Active B kit index
    bool fillActive;                ///< Is a fill currently scheduled
    uint8_t fillPatternIndex;       ///< Which fill pattern to use
    uint8_t fillStartStep;          ///< Total step where fill begins
    uint8_t chordCyclesUntilChange; ///< Chord cycles before next kit randomization

    void Init()
    {
        currentKitA = 0;
        currentKitB = 1;
        fillActive = false;
        fillPatternIndex = 0;
        fillStartStep = 0;
        chordCyclesUntilChange = 0;
    }
};

// ============================================================================
// TR-8 FUNCTIONS
// ============================================================================

/**
 * @brief Process one step of the TR-8 voice
 *
 * Checks triggers and accents for the current step across all 11 voices,
 * handles fill patterns, and fires the callback for each triggered voice.
 *
 * @param config TR-8 configuration
 * @param state TR-8 runtime state (modified for fills)
 * @param currentStep Current sequencer step (0-31)
 * @param barCounter Current bar within cycle
 * @param callback Function to call for each triggered voice (midiNote, velocity)
 */
/// Callback type for TR-8 MIDI trigger output
typedef void (*TR8TriggerCallback)(uint8_t midiNote, uint8_t velocity);

void ProcessTR8Step(
    const TR8Config& config,
    TR8State& state,
    uint8_t currentStep,
    uint8_t barCounter,
    TR8TriggerCallback callback
);

/**
 * @brief Randomize TR-8 kit selection
 *
 * Selects random A and B kits (ensuring they're different),
 * randomizes variation config and resets state.
 *
 * @param config TR-8 config (modified: variation)
 * @param state TR-8 state (modified: kit indices, fill state)
 * @param seed Random seed
 */
void RandomizeTR8Kit(
    TR8Config& config,
    TR8State& state,
    uint32_t seed
);

} // namespace themis

#endif // THEMIS_TR8_H
