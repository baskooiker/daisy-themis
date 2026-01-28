/**
 * @file themis_acid.h
 * @brief Acid voice generator - 303-style bass line patterns
 *
 * Generates acid bass lines with:
 * - Pattern presets (rhythm + melody as separate tables)
 * - Slide triggering via overlapping MIDI notes
 * - Accents via velocity (64 normal, 127 accent)
 * - Probabilistic variation (trigger, accent, slide, octave, fills)
 * - Chord-aware note selection
 */

#ifndef THEMIS_ACID_H
#define THEMIS_ACID_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// PATTERN TABLE SIZES
// ============================================================================

constexpr uint8_t NUM_ACID_RHYTHM_PATTERNS = 16;
constexpr uint8_t NUM_ACID_MELODY_PATTERNS = 16;
constexpr uint8_t ACID_PATTERN_LENGTH = 16;

// ============================================================================
// PATTERN STRUCTURES
// ============================================================================

/**
 * @struct AcidRhythmPattern
 * @brief Rhythm pattern preset with triggers, accents, slides, holds
 *
 * Each pattern is 16 steps. Stored as bit fields for efficiency.
 */
struct AcidRhythmPattern
{
    uint16_t triggers;      ///< Bit pattern: which steps trigger (MSB = step 0)
    uint16_t accents;       ///< Bit pattern: which triggers are accented
    uint16_t slides;        ///< Bit pattern: which notes slide to next
    uint16_t holds;         ///< Bit pattern: which notes have extended gate
    const char* name;       ///< Pattern name for display
};

/**
 * @struct AcidMelodyPattern
 * @brief Melody pattern preset with scale degree offsets
 *
 * Each step contains a scale degree offset (-8 to +7).
 * 0 = root, positive = up the scale, negative = down.
 */
struct AcidMelodyPattern
{
    int8_t notes[ACID_PATTERN_LENGTH];  ///< Scale degree offsets
    const char* name;                    ///< Pattern name for display
};

// ============================================================================
// PATTERN TABLES (defined in themis_acid.cpp)
// ============================================================================

extern const AcidRhythmPattern acidRhythmPatterns[NUM_ACID_RHYTHM_PATTERNS];
extern const AcidMelodyPattern acidMelodyPatterns[NUM_ACID_MELODY_PATTERNS];
extern const char* acidActivityNames[NUM_ACID_ACTIVITIES];
extern const char* acidModeNames[NUM_ACID_MODES];

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

/**
 * @brief Process one step of the acid voice
 *
 * @param config Acid voice configuration
 * @param state Acid voice runtime state (modified)
 * @param chordCtx Current chord context for note mapping
 * @param melodyRoot Global melody root note
 * @param step Current sequencer step (0-31)
 * @param seed Random seed for probabilistic elements
 * @param outNote Output: MIDI note to play (-1 if no trigger)
 * @param outVelocity Output: MIDI velocity (64 or 127)
 * @param outSlide Output: true if this note should slide to next
 * @param outGateLength Output: gate length in steps (1-4)
 * @return true if a note should be triggered
 */
bool ProcessAcidStep(
    const AcidConfig& config,
    AcidState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint8_t step,
    uint32_t seed,
    int8_t& outNote,
    uint8_t& outVelocity,
    bool& outSlide,
    uint8_t& outGateLength
);

/**
 * @brief Check if current step should trigger an end-of-bar fill
 *
 * @param state Acid voice state
 * @param step Current step (0-31)
 * @param fillProb Fill probability (0-100)
 * @param seed Random seed
 * @return true if fill should start
 */
bool ShouldTriggerFill(
    const AcidState& state,
    uint8_t step,
    uint8_t fillProb,
    uint32_t seed
);

/**
 * @brief Generate a fill pattern step
 *
 * @param state Acid voice state
 * @param chordCtx Current chord context
 * @param melodyRoot Global melody root
 * @param seed Random seed
 * @param outNote Output: MIDI note
 * @param outVelocity Output: MIDI velocity
 * @param outSlide Output: slide flag
 * @return true if fill step triggers a note
 */
bool ProcessFillStep(
    AcidState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint32_t seed,
    int8_t& outNote,
    uint8_t& outVelocity,
    bool& outSlide
);

/**
 * @brief Map a scale degree offset to MIDI note
 *
 * @param scaleDegree Scale degree offset (0 = root)
 * @param chordCtx Current chord context
 * @param melodyRoot Global melody root
 * @param octaveOffset Base octave offset
 * @param octaveShift Additional octave shift from probability
 * @return MIDI note number
 */
int8_t MapAcidNoteToMidi(
    int8_t scaleDegree,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    int8_t octaveOffset,
    int8_t octaveShift
);

/**
 * @brief Randomize pattern selection (for auto mode)
 *
 * @param state Acid voice state (modified)
 * @param config Acid voice config
 * @param seed Random seed
 */
void RandomizeAcidPatterns(
    AcidState& state,
    const AcidConfig& config,
    uint32_t seed
);

/**
 * @brief Get pattern name for display
 *
 * @param rhythmIdx Rhythm pattern index
 * @param melodyIdx Melody pattern index
 * @param buffer Output buffer for combined name
 * @param bufSize Buffer size
 */
void GetAcidPatternName(
    uint8_t rhythmIdx,
    uint8_t melodyIdx,
    char* buffer,
    uint8_t bufSize
);

} // namespace themis

#endif // THEMIS_ACID_H
