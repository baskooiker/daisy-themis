/**
 * @file themis_rhythm.h
 * @brief Generative rhythm player - supporting accompaniment voice
 *
 * The rhythm player provides rhythmic accompaniment that morphs between
 * different playing styles: chords, arpeggios, bass lines, and polyrhythmic
 * patterns. It follows the chord context with a supporting personality.
 */

#ifndef THEMIS_RHYTHM_H
#define THEMIS_RHYTHM_H

#include "themis_types.h"

namespace themis {

// Forward declarations
struct ChordContext;

// ============================================================================
// RHYTHM PLAYER CONSTANTS
// ============================================================================

constexpr uint8_t RHYTHM_QUEUE_SIZE = 8;       ///< Trigger queue size
constexpr uint8_t RHYTHM_MAX_NOTES = 6;        ///< Max simultaneous notes
constexpr uint8_t MORPH_DURATION_BARS = 4;     ///< Bars to complete style morph

// ============================================================================
// RHYTHM PATTERN TABLES
// ============================================================================

/**
 * @brief Pre-defined rhythm patterns for different styles
 *
 * Patterns are 16-bit values (1 bar of 16th notes).
 * Bit 15 = step 0, Bit 0 = step 15.
 */
struct RhythmPatterns
{
    uint16_t chordSparse[4];      ///< Sparse chord stabs
    uint16_t chordMedium[4];      ///< Medium chord activity
    uint16_t chordBusy[4];        ///< Busy chord comping
    uint16_t bassGroove[4];       ///< Bass line patterns
};

// ============================================================================
// RHYTHM PLAYER FUNCTIONS
// ============================================================================

/**
 * @brief Process one step of rhythm player voice
 *
 * Called every 16th note. Decides whether to trigger notes based on
 * current style, pattern position, and intensity.
 *
 * @param config Rhythm player configuration
 * @param state Rhythm player runtime state (modified)
 * @param chordCtx Current chord context from poly voice
 * @param kickActive True if kick triggers this step
 * @param step Current step (0-31 within 2-bar phrase)
 * @param seed Random seed for this step
 * @param outNotes Array to fill with notes to play (max 6)
 * @param outNumNotes Number of notes written to outNotes
 * @return True if notes should trigger, false for rest
 */
bool ProcessRhythmStep(
    const RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    const ChordContext& chordCtx,
    bool kickActive,
    uint8_t step,
    uint32_t seed,
    int8_t* outNotes,
    uint8_t& outNumNotes
);

/**
 * @brief Calculate note velocity based on intensity and beat position
 *
 * @param state Current rhythm player state
 * @param step Current step (0-31)
 * @param seed Random seed for variation
 * @return MIDI velocity (1-127)
 */
uint8_t CalculateRhythmVelocity(
    const RhythmPlayerState& state,
    uint8_t step,
    uint32_t seed
);

/**
 * @brief Calculate note duration in steps based on articulation
 *
 * @param config Rhythm player configuration
 * @param state Current rhythm player state
 * @param seed Random seed for variation
 * @return Number of steps to hold notes (1-8)
 */
uint8_t CalculateRhythmDuration(
    const RhythmPlayerConfig& config,
    const RhythmPlayerState& state,
    uint32_t seed
);

/**
 * @brief Update style morphing state
 *
 * Manages smooth transitions between playing styles in morph mode.
 * Style changes only happen at bar boundaries (step 0 or 16).
 *
 * @param config Rhythm player configuration
 * @param state Rhythm player state (modified)
 * @param step Current step (0-31)
 * @param seed Random seed for style selection
 */
void UpdateRhythmMorph(
    const RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    uint8_t step,
    uint32_t seed
);

/**
 * @brief Update intensity contour
 *
 * Smoothly transitions intensity toward target for musical dynamics.
 *
 * @param state Rhythm player state (modified)
 * @param seed Random seed for target changes
 */
void UpdateRhythmIntensity(RhythmPlayerState& state, uint32_t seed);

/**
 * @brief Randomize rhythm player parameters for morph mode
 *
 * @param config Rhythm player config (modified)
 * @param state Rhythm player state (modified)
 * @param seed Random seed
 */
void RandomizeRhythmParams(
    RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    uint32_t seed
);

/**
 * @brief Generate chord voicing from chord context
 *
 * Creates a chord voicing with proper voice leading from previous chord.
 *
 * @param chordCtx Current chord context
 * @param octaveOffset Octave adjustment
 * @param inversion Inversion (0=root, 1=first, 2=second)
 * @param outNotes Array to fill with notes (max 6)
 * @param outNumNotes Number of notes written
 */
void GenerateChordVoicing(
    const ChordContext& chordCtx,
    int8_t octaveOffset,
    uint8_t inversion,
    int8_t* outNotes,
    uint8_t& outNumNotes
);

/**
 * @brief Apply inversion to a set of chord notes
 *
 * Moves the lowest note(s) up an octave based on inversion level.
 *
 * @param notes Array of MIDI note numbers (modified in place)
 * @param numNotes Number of notes in the array
 * @param inversion Inversion level (0=root, 1=first, 2=second)
 */
void ApplyInversion(int8_t* notes, uint8_t numNotes, uint8_t inversion);

/**
 * @brief Get the next inversion value based on pattern
 *
 * @param state Current rhythm player state (modified)
 * @param seed Random seed for random pattern
 * @return Next inversion value (0-2)
 */
uint8_t GetNextInversion(RhythmPlayerState& state, uint32_t seed);

/**
 * @brief Process polyrhythm pattern
 *
 * Generates notes based on polyrhythmic layers (e.g., 3-against-4).
 *
 * @param chordCtx Current chord context
 * @param state Current rhythm player state (modified)
 * @param octaveOffset Octave adjustment
 * @param step Current step
 * @param outNotes Array to fill with notes
 * @param outNumNotes Number of notes written
 * @return True if any note triggers
 */
bool ProcessPolyrhythm(
    const ChordContext& chordCtx,
    RhythmPlayerState& state,
    int8_t octaveOffset,
    uint8_t step,
    int8_t* outNotes,
    uint8_t& outNumNotes
);

/**
 * @brief Check if rhythm should play on this step
 *
 * Evaluates pattern, style, and random factors.
 *
 * @param config Rhythm player configuration
 * @param state Current rhythm player state
 * @param kickActive True if kick triggers this step
 * @param step Current step (0-31)
 * @param seed Random seed
 * @return True if rhythm should trigger notes
 */
bool ShouldRhythmPlay(
    const RhythmPlayerConfig& config,
    const RhythmPlayerState& state,
    bool kickActive,
    uint8_t step,
    uint32_t seed
);

/**
 * @brief Get rhythm pattern for current style and activity
 *
 * @param style Playing style
 * @param activity Activity level
 * @param patternIndex Which pattern variant (0-3)
 * @return 16-bit rhythm pattern
 */
uint16_t GetRhythmPattern(
    RhythmPlayStyle style,
    RhythmActivity activity,
    uint8_t patternIndex
);

} // namespace themis

#endif // THEMIS_RHYTHM_H
