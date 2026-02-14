/**
 * @file themis_bass.h
 * @brief Bass voice generator - root-note bass lines with preset patterns
 *
 * Generates bass lines that:
 * - Always play the root note of the current chord
 * - Use 16 preset rhythm patterns (triggers, accents, holds)
 * - Change patterns at chord progression cycle boundaries
 * - Support freeze to lock pattern selection
 */

#ifndef THEMIS_BASS_H
#define THEMIS_BASS_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// PATTERN TABLE SIZE
// ============================================================================

constexpr uint8_t NUM_BASS_PATTERNS = 22;
constexpr uint8_t NUM_BASS_PITCH_PATTERNS = 16;
constexpr uint8_t NUM_BASS_FILLS = 8;

/**
 * @brief Count set bits in a 32-bit pattern (up to 'length' bits from MSB)
 */
inline uint8_t CountTriggersVar(uint32_t pattern, uint8_t length) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < length; i++) {
        if ((pattern >> (length - 1 - i)) & 0x01) count++;
    }
    return count;
}

/**
 * @brief Check if a step is active in a variable-length pattern
 * @param pattern Bit pattern (bit [length-1] = step 0)
 * @param step Step number (0 to length-1)
 * @param length Pattern length (8, 16, or 32)
 */
inline bool IsStepActiveVar(uint32_t pattern, uint8_t step, uint8_t length) {
    return (pattern >> (length - 1 - step)) & 0x01;
}

// ============================================================================
// PATTERN STRUCTURE
// ============================================================================

/**
 * @struct BassPattern
 * @brief Rhythm pattern preset with triggers, accents, and holds
 *
 * Supports 8, 16, or 32 step patterns. Stored as bit fields for efficiency.
 */
struct BassPattern
{
    uint32_t triggers;      ///< Bit pattern: which steps trigger (bit [length-1] = step 0)
    uint32_t accents;       ///< Bit pattern: which triggers are accented
    uint32_t holds;         ///< Bit pattern: which notes hold longer
    uint8_t length;         ///< Pattern length: 8, 16, or 32
    const char* name;       ///< Pattern name for display
};

/**
 * @struct BassPitchPattern
 * @brief Pitch pattern preset: up to 32 steps of pitch type selections
 */
struct BassPitchPattern
{
    BassPitchType steps[32]; ///< Pitch type per step (unused steps = ROOT)
    uint8_t length;          ///< Pattern length: 8, 16, or 32
    const char* name;
};

// ============================================================================
// PATTERN TABLE (defined in themis_bass.cpp)
// ============================================================================

extern const BassPattern bassPatterns[NUM_BASS_PATTERNS];
extern const BassPitchPattern bassPitchPatterns[NUM_BASS_PITCH_PATTERNS];
extern const uint8_t bassFillsHalf[NUM_BASS_FILLS];

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

/**
 * @brief Calculate bass note for a given step (pitch only, no trigger check)
 *
 * Extracts pitch calculation from ProcessBassStep so both normal and fill
 * paths can share the same note logic.
 *
 * @param config Bass voice configuration
 * @param state Bass voice runtime state
 * @param chordCtx Current chord context
 * @param melodyRoot Global melody root note
 * @param step Current sequencer step (0-31)
 * @return MIDI note number
 */
int8_t CalculateBassNote(
    const BassConfig& config,
    const BassState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint8_t step);

/**
 * @brief Resolve a pitch type to a MIDI note offset from root
 *
 * @param type The pitch type to resolve
 * @param baseNote Base MIDI note (chord root with octave)
 * @param chordCtx Current chord context for interval lookups
 * @return Resolved MIDI note, clamped to 24-96
 */
int8_t ResolveBassPitchType(BassPitchType type, int8_t baseNote, const ChordContext& chordCtx);

/**
 * @brief Process one step of the bass voice
 *
 * @param config Bass voice configuration
 * @param state Bass voice runtime state (modified)
 * @param chordCtx Current chord context for root note
 * @param melodyRoot Global melody root note
 * @param step Current sequencer step (0-31)
 * @param outNote Output: MIDI note to play (-1 if no trigger)
 * @param outVelocity Output: MIDI velocity (70 normal, 120 accent)
 * @param outGateLength Output: gate length in steps (1 short, 3 long)
 * @return true if a note should be triggered
 */
bool ProcessBassStep(
    const BassConfig& config,
    BassState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint8_t step,
    int8_t& outNote,
    uint8_t& outVelocity,
    uint8_t& outGateLength
);

/**
 * @brief Randomize bass pattern selection
 *
 * @param state Bass voice state (modified)
 * @param config Bass voice config
 * @param seed Random seed
 */
void RandomizeBassPattern(
    BassState& state,
    const BassConfig& config,
    uint32_t seed
);

} // namespace themis

#endif // THEMIS_BASS_H
