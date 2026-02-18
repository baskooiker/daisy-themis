/**
 * @file themis_tr8.cpp
 * @brief TR-8 drum machine voice implementation
 */

#include "themis_tr8.h"
#include "themis_platform.h"
#include "themis_patterns.h"

namespace themis {

// ============================================================================
// MIDI NOTE MAPPING (Roland TR-8 standard)
// ============================================================================

const uint8_t tr8MidiNotes[NUM_TR8_VOICES] = {
    36,  // BD - Bass Drum
    37,  // RS - Rim Shot
    38,  // SD - Snare Drum
    39,  // HC - Hand Clap
    42,  // CH - Closed HiHat
    43,  // LT - Low Tom
    46,  // OH - Open HiHat
    47,  // MT - Mid Tom
    49,  // CC - Crash Cymbal
    50,  // HT - High Tom
    51,  // RC - Ride Cymbal
};

const char* tr8VoiceNames[NUM_TR8_VOICES] = {
    "BD", "RS", "SD", "HC", "CH", "LT", "OH", "MT", "CC", "HT", "RC"
};

// ============================================================================
// KIT PATTERN DATA (16 predefined techno/house kits)
// ============================================================================
// 16-step patterns, MSB = step 0
// Beat:  1  e  &  a  2  e  &  a  3  e  &  a  4  e  &  a
// Step:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
//
// Voices: BD RS SD HC CH LT OH MT CC HT RC

const TR8Kit tr8Kits[NUM_TR8_KITS] = {
    // 0: Four on Floor - Classic house kick, hats on 8ths, SD on 2/4
    {
        {   // triggers
            0b1000100010001000, // BD: four on floor
            0b0000000000000000, // RS: off
            0b0000100000001000, // SD: beats 2 and 4
            0b0000000000000000, // HC: off
            0b1010101010101010, // CH: 8th notes
            0b0000000000000000, // LT: off
            0b0100010001000100, // OH: upbeats
            0b0000000000000000, // MT: off
            0b0000000000000000, // CC: off
            0b0000000000000000, // HT: off
            0b0000000000000000, // RC: off
        },
        {   // accents
            0b1000000010000000, // BD: 1 and 3 accented
            0b0000000000000000,
            0b0000100000001000, // SD: all accented
            0b0000000000000000,
            0b1000100010001000, // CH: downbeats accented
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "4 on Floor"
    },

    // 1: Minimal Techno - Sparse kick, rim + clap, hat groove
    {
        {
            0b1000000010010000, // BD: 1, 3, syncopated
            0b0010000000100000, // RS: offbeats
            0b0000000000000000, // SD: off
            0b0000100000000000, // HC: beat 2 clap
            0b1010101010101010, // CH: 8ths
            0b0000000000000000,
            0b0001000000010000, // OH: sparse upbeats
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000100000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Min Techno"
    },

    // 2: Deep House - Swung hats, syncopated claps, ghost snares
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0000000000000000,
            0b0000100100001000, // SD: 2, 2-a, 4
            0b0000010000000100, // HC: swung claps
            0b1001101010011010, // CH: swung pattern
            0b0000000000000000,
            0b0100010001000100, // OH: upbeats
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4 accented
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Deep House"
    },

    // 3: Breakbeat - Broken kick, heavy snare/clap, ride
    {
        {
            0b1000001010000010, // BD: broken pattern
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000100000001000, // HC: layer with snare
            0b1010101010101010, // CH: 8ths
            0b0000000000000000,
            0b0001000000010000, // OH: offbeats
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1010101010101010, // RC: ride 8ths
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000100000001000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1000100010001000,
        },
        "Breakbeat"
    },

    // 4: Industrial - Aggressive kick, layered snare+clap, toms
    {
        {
            0b1001100010011000, // BD: aggressive doubles
            0b0010000000100000, // RS: ghost notes
            0b0000100000001000, // SD: 2 and 4
            0b0000100000001000, // HC: layer with snare
            0b1010101010101010, // CH: 8ths
            0b0000000010000001, // LT: end of bar hits
            0b0100000001000000, // OH: sparse opens
            0b0000001000000000, // MT: ghost hit
            0b0000000000000000,
            0b0000000100000000, // HT: accent
            0b0000000000000000,
        },
        {
            0b1000100010001000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000100000001000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000100000000,
            0b0000000000000000,
        },
        "Industrial"
    },

    // 5: Acid House - 909-style, open hats, rimshots
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0010001000100010, // RS: 16th offbeats
            0b0000100000001000, // SD: 2 and 4
            0b0000000000000000,
            0b1010101010101010, // CH: 8ths
            0b0000000000000000,
            0b0100010001000100, // OH: upbeats
            0b0000000000000000,
            0b1000000000000000, // CC: crash on 1
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0010000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0100000000000000,
            0b0000000000000000,
            0b1000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Acid House"
    },

    // 6: Electro - 808-style with syncopation
    {
        {
            0b1000001010001000, // BD: syncopated kick
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000100000000100, // HC: offbeat claps
            0b1011101011101010, // CH: busy hats
            0b0000000000000000,
            0b0000010000000000, // OH: single open
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000010000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Electro"
    },

    // 7: Dub Techno - Sparse, dubbed out
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0000000000100000, // RS: ghost rim
            0b0000000000001000, // SD: only beat 4
            0b0000010000000000, // HC: single clap
            0b1001001010010010, // CH: shuffled
            0b0000000000000000,
            0b0100000001000000, // OH: wide opens
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1010101010101010, // RC: steady ride
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000000000001000,
            0b0000010000000000,
            0b1000000010000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1000100010001000,
        },
        "Dub Techno"
    },

    // 8: Tribal - Tom-heavy, polyrhythmic
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000000000000000,
            0b1010101010101010, // CH: 8ths
            0b1001000000000010, // LT: polyrhythm
            0b0000010000000100, // OH: offbeats
            0b0000001001000000, // MT: polyrhythm
            0b0000000000000000,
            0b0100000010000000, // HT: polyrhythm
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b1000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0100000000000000,
            0b0000000000000000,
        },
        "Tribal"
    },

    // 9: Garage - 2-step feel with shuffled hats
    {
        {
            0b1000000010010000, // BD: 2-step
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000100000000000, // HC: clap on 2
            0b1001101010011010, // CH: shuffled
            0b0000000000000000,
            0b0100010001000100, // OH: upbeats
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000100000000000,
            0b1000000010000000,
            0b0000000000000000,
            0b0100000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Garage"
    },

    // 10: Hard Techno - Pounding kick, ride-driven
    {
        {
            0b1010100010101000, // BD: driving doubles
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000100000001000, // HC: layer snare
            0b1010101010101010, // CH: 8ths
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1000000000000000, // CC: crash on 1
            0b0000000000000000,
            0b1010101010101010, // RC: ride 8ths
        },
        {
            0b1000100010001000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000100000001000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1000000000000000,
            0b0000000000000000,
            0b1000100010001000,
        },
        "Hard Techno"
    },

    // 11: Chicago House - Classic Chicago jack
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0000000000000000,
            0b0000000000000000,
            0b0000100000001000, // HC: clap on 2/4
            0b1010101010101010, // CH: 8ths
            0b0000000000000000,
            0b0100010001000100, // OH: upbeats
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000100000001000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "Chicago"
    },

    // 12: EBM - Electronic Body Music, driving
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0010001000100010, // RS: 16th offbeats
            0b0000100000001000, // SD: 2 and 4
            0b0000000000000000,
            0b1111111111111111, // CH: 16ths
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
        },
        "EBM"
    },

    // 13: Italo Disco - Snappy, with toms and crash
    {
        {
            0b1000100010001000, // BD: four on floor
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000000000000000,
            0b1010101010101010, // CH: 8ths
            0b0000000000000001, // LT: fill into next bar
            0b0100010001000100, // OH: upbeats
            0b0000000000000010, // MT: syncopated
            0b1000000000000000, // CC: crash on 1
            0b0000000000010000, // HT: accent
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b1000000000000000,
            0b0000000000010000,
            0b0000000000000000,
        },
        "Italo"
    },

    // 14: Afro Tech - African-influenced techno
    {
        {
            0b1000001010001000, // BD: syncopated
            0b0000000000000000,
            0b0000100000001000, // SD: 2 and 4
            0b0000000000000000,
            0b1010101010101010, // CH: 8ths
            0b1001000000000010, // LT: clave-like
            0b0001000100000100, // OH: offbeat opens
            0b0000001000100000, // MT: cross-rhythm
            0b0000000000000000,
            0b0100000010000000, // HT: accent pattern
            0b0000000000000000,
        },
        {
            0b1000000010000000,
            0b0000000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b1000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0100000000000000,
            0b0000000000000000,
        },
        "Afro Tech"
    },

    // 15: Perc Workout - Percussion-heavy, minimal kick
    {
        {
            0b1000000010000000, // BD: just 1 and 3
            0b0010010000100100, // RS: busy rimshots
            0b0000100000001000, // SD: 2 and 4
            0b0000010000000100, // HC: offbeat claps
            0b1010101010101010, // CH: 8ths
            0b1000000000001000, // LT: sparse
            0b0001000000010000, // OH: opens
            0b0000100100000000, // MT: ghost toms
            0b0000000000000000,
            0b0100000010000000, // HT: accents
            0b1010101010101010, // RC: ride 8ths
        },
        {
            0b1000000010000000,
            0b0010000000000000,
            0b0000100000001000,
            0b0000000000000000,
            0b1000100010001000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0000000000000000,
            0b0100000010000000,
            0b1000100010001000,
        },
        "Perc Work"
    },
};

// ============================================================================
// FILL PATTERNS (8-step, MSB = step 0)
// ============================================================================

const TR8FillKit tr8Fills[NUM_TR8_FILLS] = {
    {0b11111111, 0b10101010, 0b10000000, "Run"},       // Snare roll, tom pulse, crash on 1
    {0b00011111, 0b00001111, 0b00000001, "Build"},      // Builds to downbeat
    {0b10110111, 0b01010101, 0b10000000, "Bounce"},     // Bouncy syncopation
    {0b11001110, 0b11001100, 0b10000000, "Stutter"},    // Stuttered pairs
    {0b10101111, 0b10100101, 0b10000001, "Skip"},       // Skip then dense
    {0b11010110, 0b01101001, 0b10000000, "Broken"},     // Broken rhythm
    {0b11111111, 0b11111111, 0b10000001, "Full"},       // All voices, max energy
    {0b10010011, 0b01001011, 0b10000000, "Swing"},      // Swung fill
};

// ============================================================================
// PROCESSING
// ============================================================================

void ProcessTR8Step(
    const TR8Config& config,
    TR8State& state,
    uint8_t currentStep,
    uint8_t barCounter,
    TR8TriggerCallback callback)
{
    if (!config.active || !callback) return;

    // Determine which kit to use via AB variation
    uint8_t activeKitIdx = state.currentKitA;
    if (config.variation.mode != VAR_MODE_OFF) {
        uint8_t var = GetCurrentVariation(&config.variation, currentStep, barCounter);
        if (var >= 1) {
            activeKitIdx = state.currentKitB;
        }
    }

    if (activeKitIdx >= NUM_TR8_KITS) activeKitIdx = 0;
    const TR8Kit& kit = tr8Kits[activeKitIdx];

    // Check if we're in a fill window
    uint8_t totalStep = (barCounter * 32) + currentStep;
    bool inFill = state.fillActive
                  && (totalStep >= state.fillStartStep)
                  && (totalStep < state.fillStartStep + 8);

    // Clear fill flag once the window has passed
    if (state.fillActive && totalStep >= state.fillStartStep + 8) {
        state.fillActive = false;
    }

    // Map currentStep (0-31) to 16-step pattern position
    uint8_t patStep = currentStep % 16;

    if (inFill) {
        // Fill mode: override snare, toms, crash with fill pattern
        uint8_t fillStep = totalStep - state.fillStartStep;
        if (state.fillPatternIndex >= NUM_TR8_FILLS) state.fillPatternIndex = 0;
        const TR8FillKit& fill = tr8Fills[state.fillPatternIndex];

        // Process normal voices that aren't overridden by fill
        // BD and CH continue from kit during fill
        if (IsStepActive16(kit.triggers[TR8_BD], patStep)) {
            bool accent = IsStepActive16(kit.accents[TR8_BD], patStep);
            callback(tr8MidiNotes[TR8_BD], accent ? 120 : 90);
        }
        if (IsStepActive16(kit.triggers[TR8_CH], patStep)) {
            bool accent = IsStepActive16(kit.accents[TR8_CH], patStep);
            callback(tr8MidiNotes[TR8_CH], accent ? 120 : 90);
        }

        // Fill: snare
        if (IsStepActive8(fill.snare, fillStep)) {
            callback(tr8MidiNotes[TR8_SD], 115);
        }

        // Fill: toms (distribute across LT/MT/HT based on fill step)
        if (IsStepActive8(fill.tom, fillStep)) {
            // Cascade: HT -> MT -> LT pattern
            uint8_t tomVoice;
            if (fillStep < 3) tomVoice = TR8_HT;
            else if (fillStep < 6) tomVoice = TR8_MT;
            else tomVoice = TR8_LT;
            callback(tr8MidiNotes[tomVoice], 110);
        }

        // Fill: crash
        if (IsStepActive8(fill.crash, fillStep)) {
            callback(tr8MidiNotes[TR8_CC], 120);
        }
    } else {
        // Normal mode: play kit patterns
        for (uint8_t v = 0; v < NUM_TR8_VOICES; v++) {
            if (IsStepActive16(kit.triggers[v], patStep)) {
                bool accent = IsStepActive16(kit.accents[v], patStep);
                callback(tr8MidiNotes[v], accent ? 120 : 90);
            }
        }
    }
}

void RandomizeTR8Kit(
    TR8Config& config,
    TR8State& state,
    uint32_t seed)
{
    // Select kit A
    state.currentKitA = seed % NUM_TR8_KITS;

    // Select kit B (different from A)
    state.currentKitB = (seed >> 8) % NUM_TR8_KITS;
    if (state.currentKitB == state.currentKitA) {
        state.currentKitB = (state.currentKitB + 1) % NUM_TR8_KITS;
    }

    // Sync config kitIndex to current A
    config.kitIndex = state.currentKitA;

    // Randomize variation (50% off, 50% AB)
    config.variation.mode = ((seed >> 16) % 2 == 0) ? VAR_MODE_OFF : VAR_MODE_AB;
    config.variation.sequence = (VariationSequence)((seed >> 18) % 4);  // AAAA-ABAB

    // Reset fill state
    state.fillActive = false;
    state.fillPatternIndex = (seed >> 22) % NUM_TR8_FILLS;
}

} // namespace themis
