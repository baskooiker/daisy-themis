/**
 * @file themis_chords.h
 * @brief Chord shapes and progression definitions
 */

#ifndef THEMIS_CHORDS_H
#define THEMIS_CHORDS_H

#include <cstdint>
#include "themis_types.h"

namespace themis {

// ============================================================================
// CHORD TYPES
// ============================================================================

enum ChordType {
    CHORD_MAJOR,        // {0, 4, 7}
    CHORD_MINOR,        // {0, 3, 7}
    CHORD_DIM,          // {0, 3, 6}
    CHORD_AUG,          // {0, 4, 8}
    CHORD_SUS2,         // {0, 2, 7}
    CHORD_SUS4,         // {0, 5, 7}
    CHORD_MAJ7,         // {0, 4, 7, 11}
    CHORD_MIN7,         // {0, 3, 7, 10}
    CHORD_DOM7,         // {0, 4, 7, 10}
    CHORD_DIM7,         // {0, 3, 6, 9}
    CHORD_MIN7B5,       // {0, 3, 6, 10} (half-diminished)
    CHORD_ADD9,         // {0, 4, 7, 14}
    CHORD_MADD9,        // {0, 3, 7, 14}
    NUM_CHORD_TYPES
};

// ============================================================================
// CHORD SHAPE
// ============================================================================

struct ChordShape {
    int8_t intervals[6];  // Intervals from root in semitones, -128 = unused
    uint8_t numNotes;     // How many notes in the chord
};

// ============================================================================
// PROGRESSION
// ============================================================================

struct ProgressionStep {
    int8_t scaleDegree;       // 0-6 for diatonic (I-VII), or semitone offset for non-diatonic
    bool isDiatonic;          // If true, scaleDegree is a scale degree; if false, it's semitones
    ChordType chordType;      // Chord quality
    int8_t octaveOffset;      // -1, 0, +1 octave adjustment
};

enum ProgressionFeel {
    FEEL_HAPPY,
    FEEL_SAD,
    FEEL_EPIC,
    FEEL_TENSE,
    FEEL_DREAMY,
    FEEL_JAZZY,
    FEEL_NOSTALGIC,
    FEEL_DARK,
    FEEL_BRIGHT,
    FEEL_EMOTIONAL,
    FEEL_MELANCHOLIC,
    NUM_PROGRESSION_FEELS
};

struct ChordProgression {
    const char* name;
    ProgressionStep steps[8];  // Max 8 chords in a progression
    uint8_t length;            // Number of chords in progression
    ProgressionFeel feel;      // Categorization
};

// ============================================================================
// CHORD RATE
// ============================================================================

enum ChordRate {
    CHORD_RATE_2_BARS,    // Change every 64 steps (2 bars of 32 steps)
    CHORD_RATE_1_BAR,     // Change every 32 steps (1 bar)
    CHORD_RATE_HALF_BAR,  // Change every 16 steps (half bar)
    CHORD_RATE_QUARTER,   // Change every 8 steps (quarter bar / 2 beats)
    NUM_CHORD_RATES
};

// ============================================================================
// CONSTANTS
// ============================================================================

static const uint8_t NUM_PROGRESSIONS = 26;

// Steps per chord for each rate
extern const uint8_t chordRateSteps[NUM_CHORD_RATES];

// Chord rate names for UI
extern const char* chordRateNames[NUM_CHORD_RATES];

// Chord shape definitions
extern const ChordShape chordShapes[NUM_CHORD_TYPES];

// Progression definitions
extern const ChordProgression progressions[NUM_PROGRESSIONS];

// ============================================================================
// FUNCTIONS
// ============================================================================

/**
 * @brief Calculate actual MIDI notes for a chord
 * @param progression The chord progression to use
 * @param chordIndex Which chord in the progression (0-7)
 * @param rootNote Global root note (0-11, C=0)
 * @param scale Current scale type
 * @param octaveOffset Additional octave offset
 * @param outNotes Output array for MIDI notes (max 6)
 * @return Number of notes in chord
 */
uint8_t GetChordNotes(
    const ChordProgression* progression,
    uint8_t chordIndex,
    uint8_t rootNote,
    uint8_t scale,
    int8_t octaveOffset,
    int8_t* outNotes
);

// ============================================================================
// CHORD-AWARE MELODY MAPPING
// ============================================================================

/**
 * @brief Get array of compatible semitones for a chord and compat mode
 * @param ctx Chord context (root, type, diatonic status)
 * @param mode Compatibility mode
 * @param outNotes Output array for semitones (relative to chord root)
 * @param maxNotes Maximum number of notes to return
 * @return Number of notes placed in outNotes
 */
uint8_t GetCompatibleNotes(const ChordContext& ctx, MelodyCompatMode mode,
                           int8_t* outNotes, uint8_t maxNotes);

/**
 * @brief Quantize a note to the nearest note in a set, preserving octave
 * @param note Semitone value (any octave)
 * @param notes Array of semitones (0-11) to quantize to
 * @param count Number of notes in array
 * @return Quantized note in same octave range as original
 */
int8_t QuantizeToNearestNote(int8_t note, const int8_t* notes, uint8_t count);

/**
 * @brief Map a melody note to chord-compatible note
 * @param originalNote Original melody note (semitone offset from C2)
 * @param ctx Current chord context
 * @param mode Compatibility mode
 * @return Mapped note (semitone offset from C2)
 */
int8_t MapNoteToChord(int8_t originalNote, const ChordContext& ctx,
                      MelodyCompatMode mode);

} // namespace themis

#endif // THEMIS_CHORDS_H
