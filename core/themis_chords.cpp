/**
 * @file themis_chords.cpp
 * @brief Chord shapes and progression data implementation
 */

#include "themis_chords.h"
#include "themis_melody.h"

namespace themis {

// ============================================================================
// CHORD RATE DATA
// ============================================================================

const uint8_t chordRateSteps[NUM_CHORD_RATES] = {64, 32, 16, 8};

const char* chordRateNames[NUM_CHORD_RATES] = {
    "2 Bars",
    "1 Bar",
    "Half",
    "Quarter"
};

// ============================================================================
// VIBE SYSTEM NAMES
// ============================================================================

const char* vibeNames[NUM_VIBE_TYPES] = {
    "Minor",
    "Whole-Tone",
    "Major"
};

const char* progCategoryNames[NUM_PROG_CATEGORIES] = {
    "Steady",
    "Cadence",
    "Full"
};

// ============================================================================
// CHORD SHAPE DATA
// ============================================================================

const ChordShape chordShapes[NUM_CHORD_TYPES] = {
    // CHORD_MAJOR: Major triad
    {{0, 4, 7, -128, -128, -128}, 3},

    // CHORD_MINOR: Minor triad
    {{0, 3, 7, -128, -128, -128}, 3},

    // CHORD_DIM: Diminished triad
    {{0, 3, 6, -128, -128, -128}, 3},

    // CHORD_AUG: Augmented triad
    {{0, 4, 8, -128, -128, -128}, 3},

    // CHORD_SUS2: Suspended 2nd
    {{0, 2, 7, -128, -128, -128}, 3},

    // CHORD_SUS4: Suspended 4th
    {{0, 5, 7, -128, -128, -128}, 3},

    // CHORD_MAJ7: Major 7th
    {{0, 4, 7, 11, -128, -128}, 4},

    // CHORD_MIN7: Minor 7th
    {{0, 3, 7, 10, -128, -128}, 4},

    // CHORD_DOM7: Dominant 7th
    {{0, 4, 7, 10, -128, -128}, 4},

    // CHORD_DIM7: Diminished 7th
    {{0, 3, 6, 9, -128, -128}, 4},

    // CHORD_MIN7B5: Half-diminished (minor 7 flat 5)
    {{0, 3, 6, 10, -128, -128}, 4},

    // CHORD_ADD9: Major add 9
    {{0, 4, 7, 14, -128, -128}, 4},

    // CHORD_MADD9: Minor add 9
    {{0, 3, 7, 14, -128, -128}, 4},
};

// ============================================================================
// PROGRESSION DATA
// ============================================================================

const ChordProgression progressions[NUM_PROGRESSIONS] = {
    // ========================================================================
    // MINOR VIBE - STEADY (for transitions)
    // ========================================================================

    // 0: Minor Drone - Single i chord (steady)
    {"Minor Drone",
     {{0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     1, FEEL_DARK, VIBE_MINOR, PROG_STEADY},

    // 1: Minor 7 Drone - Rich minor drone
    {"Min7 Drone",
     {{0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0}},
     1, FEEL_DARK, VIBE_MINOR, PROG_STEADY},

    // ========================================================================
    // MINOR VIBE - CADENCES (2 chords)
    // ========================================================================

    // 2: Minor iv-i - Plagal cadence
    {"Minor iv-i",
     {{3, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {3, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     2, FEEL_SAD, VIBE_MINOR, PROG_CADENCE},

    // 3: Minor V-i - Authentic cadence
    {"Minor V-i",
     {{4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MINOR, 0},
      {4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     2, FEEL_DARK, VIBE_MINOR, PROG_CADENCE},

    // ========================================================================
    // MINOR VIBE - FULL PROGRESSIONS (4+ chords)
    // ========================================================================

    // 4: Epic Minor (i - VI - III - VII)
    {"Epic Minor",
     {{0, true, CHORD_MINOR, 0}, {5, true, CHORD_MAJOR, 0},
      {2, true, CHORD_MAJOR, 0}, {6, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_EPIC, VIBE_MINOR, PROG_FULL},

    // 5: Natural Minor (i - iv - v - i)
    {"Natural Minor",
     {{0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {4, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_SAD, VIBE_MINOR, PROG_FULL},

    // 6: Andalusian (i - VII - VI - V)
    {"Andalusian",
     {{0, true, CHORD_MINOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {-4, false, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_TENSE, VIBE_MINOR, PROG_FULL},

    // 7: Minor Plagal (i - iv - bVII - i)
    {"Minor Plagal",
     {{0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {-2, false, CHORD_MAJOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_MELANCHOLIC, VIBE_MINOR, PROG_FULL},

    // 8: Techno Pulse - Minimal movement i-iv
    {"Techno Pulse",
     {{0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK, VIBE_MINOR, PROG_FULL},

    // 9: Dark Phrygian - i to bII
    {"Dark Phrygian",
     {{0, true, CHORD_MINOR, 0}, {1, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK, VIBE_MINOR, PROG_FULL},

    // ========================================================================
    // WHOLE-TONE VIBE - STEADY (for transitions)
    // ========================================================================

    // 10: Augmented Drone
    {"Aug Drone",
     {{0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0}},
     1, FEEL_DREAMY, VIBE_WHOLE_TONE, PROG_STEADY},

    // 11: Sus4 Drone - Tension without resolution
    {"Sus4 Drone",
     {{0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0}},
     1, FEEL_TENSE, VIBE_WHOLE_TONE, PROG_STEADY},

    // ========================================================================
    // WHOLE-TONE VIBE - CADENCES (2 chords)
    // ========================================================================

    // 12: Aug-Dim resolving
    {"Aug-Dim",
     {{0, true, CHORD_AUG, 0}, {0, true, CHORD_DIM, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_DIM, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0},
      {0, true, CHORD_AUG, 0}, {0, true, CHORD_AUG, 0}},
     2, FEEL_TENSE, VIBE_WHOLE_TONE, PROG_CADENCE},

    // 13: Tritone resolving
    {"Tritone Res",
     {{0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     2, FEEL_TENSE, VIBE_WHOLE_TONE, PROG_CADENCE},

    // ========================================================================
    // WHOLE-TONE VIBE - FULL PROGRESSIONS (4+ chords)
    // ========================================================================

    // 14: Tritone - Dissonant tritone movement
    {"Tritone",
     {{0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_TENSE, VIBE_WHOLE_TONE, PROG_FULL},

    // 15: Industrial - i-bII-i-bVII
    {"Industrial",
     {{0, true, CHORD_MINOR, 0}, {1, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK, VIBE_WHOLE_TONE, PROG_FULL},

    // 16: Dreamy 7ths (Imaj7 - IVmaj7 - vi7 - V7)
    {"Dreamy 7ths",
     {{0, true, CHORD_MAJ7, 0}, {3, true, CHORD_MAJ7, 0},
      {5, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_DREAMY, VIBE_WHOLE_TONE, PROG_FULL},

    // 17: Dim7 Drone - Eerie diminished drone
    {"Dim7 Drone",
     {{0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0}},
     1, FEEL_TENSE, VIBE_WHOLE_TONE, PROG_STEADY},

    // ========================================================================
    // MAJOR VIBE - STEADY (for transitions)
    // ========================================================================

    // 18: Major Drone
    {"Major Drone",
     {{0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     1, FEEL_BRIGHT, VIBE_MAJOR, PROG_STEADY},

    // 19: Sus2 Drone
    {"Sus2 Drone",
     {{0, true, CHORD_SUS2, 0}, {0, true, CHORD_SUS2, 0},
      {0, true, CHORD_SUS2, 0}, {0, true, CHORD_SUS2, 0},
      {0, true, CHORD_SUS2, 0}, {0, true, CHORD_SUS2, 0},
      {0, true, CHORD_SUS2, 0}, {0, true, CHORD_SUS2, 0}},
     1, FEEL_DREAMY, VIBE_MAJOR, PROG_STEADY},

    // ========================================================================
    // MAJOR VIBE - CADENCES (2 chords)
    // ========================================================================

    // 20: Major IV-I - Plagal cadence
    {"Major IV-I",
     {{3, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {3, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     2, FEEL_BRIGHT, VIBE_MAJOR, PROG_CADENCE},

    // 21: Major V-I - Authentic cadence
    {"Major V-I",
     {{4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     2, FEEL_HAPPY, VIBE_MAJOR, PROG_CADENCE},

    // ========================================================================
    // MAJOR VIBE - FULL PROGRESSIONS (4+ chords)
    // ========================================================================

    // 22: Pop (I - V - vi - IV)
    {"Pop I-V-vi-IV",
     {{0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_HAPPY, VIBE_MAJOR, PROG_FULL},

    // 23: Rock (I - IV - V - IV)
    {"Rock I-IV-V",
     {{0, true, CHORD_MAJOR, 0}, {3, true, CHORD_MAJOR, 0},
      {4, true, CHORD_MAJOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_BRIGHT, VIBE_MAJOR, PROG_FULL},

    // 24: Fifties (I - vi - IV - V)
    {"Fifties",
     {{0, true, CHORD_MAJOR, 0}, {5, true, CHORD_MINOR, 0},
      {3, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_NOSTALGIC, VIBE_MAJOR, PROG_FULL},

    // 25: Mixolydian Rock (I - bVII - IV - I)
    {"Mixolydian",
     {{0, true, CHORD_MAJOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {3, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_BRIGHT, VIBE_MAJOR, PROG_FULL},

    // 26: Jazz ii-V-I
    {"Jazz ii-V-I",
     {{1, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_JAZZY, VIBE_MAJOR, PROG_FULL},

    // 27: Jazz Standard (I - vi - ii - V)
    {"Jazz Standard",
     {{0, true, CHORD_MAJ7, 0}, {5, true, CHORD_MIN7, 0},
      {1, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_JAZZY, VIBE_MAJOR, PROG_FULL},

    // 28: Modern Pop Add9
    {"Modern Add9",
     {{0, true, CHORD_ADD9, 0}, {4, true, CHORD_ADD9, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_ADD9, 0}, {0, true, CHORD_ADD9, 0},
      {0, true, CHORD_ADD9, 0}, {0, true, CHORD_ADD9, 0}},
     4, FEEL_DREAMY, VIBE_MAJOR, PROG_FULL},

    // 29: Extended Pop (I - V - vi - IV - I - V - iii - IV)
    {"Extended Pop",
     {{0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {2, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0}},
     8, FEEL_HAPPY, VIBE_MAJOR, PROG_FULL},

    // 30: Circle of 5ths (vi - ii - V - I - IV - vii° - iii - vi)
    {"Circle of 5ths",
     {{5, true, CHORD_MINOR, 0}, {1, true, CHORD_MINOR, 0},
      {4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {3, true, CHORD_MAJOR, 0}, {6, true, CHORD_DIM, 0},
      {2, true, CHORD_MINOR, 0}, {5, true, CHORD_MINOR, 0}},
     8, FEEL_JAZZY, VIBE_MAJOR, PROG_FULL},
};

// ============================================================================
// FUNCTIONS
// ============================================================================

uint8_t GetChordNotes(
    const ChordProgression* progression,
    uint8_t chordIndex,
    uint8_t rootNote,
    uint8_t scale,
    int8_t octaveOffset,
    int8_t* outNotes
) {
    if (chordIndex >= progression->length) {
        chordIndex = 0;
    }

    const ProgressionStep& step = progression->steps[chordIndex];
    const ChordShape& shape = chordShapes[step.chordType];

    // Calculate chord root
    int8_t chordRoot;
    if (step.isDiatonic) {
        // Get scale degree note (using existing scale system)
        chordRoot = GetScaleNote((ScaleType)scale, rootNote, step.scaleDegree);
    } else {
        // Direct semitone offset from root
        chordRoot = rootNote + step.scaleDegree;
    }

    // Apply octave offset (base octave = 48 = C3)
    int8_t baseNote = 48 + chordRoot + (octaveOffset * 12) + (step.octaveOffset * 12);

    // Build chord notes
    uint8_t count = 0;
    for (int i = 0; i < shape.numNotes && i < 6; i++) {
        int16_t note = baseNote + shape.intervals[i];
        // Clamp to valid MIDI range, preserving pitch class (octave-shift instead of hard clamp)
        while (note < 0) note += 12;
        while (note > 127) note -= 12;
        outNotes[count++] = (int8_t)note;
    }

    return count;
}

// ============================================================================
// CHORD-AWARE MELODY MAPPING
// ============================================================================

uint8_t GetCompatibleNotes(const ChordContext& ctx, MelodyCompatMode mode,
                           int8_t* outNotes, uint8_t maxNotes) {
    uint8_t count = 0;

    switch (mode) {
        case COMPAT_CHORD_TONES:
            // Strict chord tones only
            {
                const ChordShape& shape = chordShapes[ctx.chordType];
                for (int i = 0; i < shape.numNotes && count < maxNotes; i++) {
                    if (shape.intervals[i] != -128) {
                        // Add chord root offset and normalize to 0-11
                        int8_t note = (ctx.chordRoot + shape.intervals[i]) % 12;
                        if (note < 0) note += 12;
                        outNotes[count++] = note;
                    }
                }
            }
            break;

        case COMPAT_CHORD_PENTATONIC:
            // Pentatonic scale built on chord root
            {
                // Choose pentatonic based on chord type
                static const int8_t majorPenta[] = {0, 2, 4, 7, 9};   // Major pentatonic
                static const int8_t minorPenta[] = {0, 3, 5, 7, 10};  // Minor pentatonic
                static const int8_t wholeTone[] = {0, 2, 4, 8, 10};   // For augmented
                static const int8_t dimPenta[] = {0, 3, 6, 9};        // Diminished subset

                const int8_t* penta;
                uint8_t pentaLen;

                switch (ctx.chordType) {
                    case CHORD_MINOR:
                    case CHORD_MIN7:
                    case CHORD_MIN7B5:
                    case CHORD_MADD9:
                        penta = minorPenta;
                        pentaLen = 5;
                        break;
                    case CHORD_AUG:
                        penta = wholeTone;
                        pentaLen = 5;
                        break;
                    case CHORD_DIM:
                    case CHORD_DIM7:
                        penta = dimPenta;
                        pentaLen = 4;
                        break;
                    default:
                        // Major, sus, dom7, add9
                        penta = majorPenta;
                        pentaLen = 5;
                        break;
                }

                for (int i = 0; i < pentaLen && count < maxNotes; i++) {
                    int8_t note = (ctx.chordRoot + penta[i]) % 12;
                    if (note < 0) note += 12;
                    outNotes[count++] = note;
                }
            }
            break;

        case COMPAT_CHORD_SCALE:
        default:
            // Full scale from chord root
            {
                // 7-note scales based on chord type
                static const int8_t majorScale[] = {0, 2, 4, 5, 7, 9, 11};  // Ionian
                static const int8_t minorScale[] = {0, 2, 3, 5, 7, 8, 10};  // Aeolian
                static const int8_t mixolydian[] = {0, 2, 4, 5, 7, 9, 10};  // Mixolydian (dom7)
                static const int8_t wholeToneScale[] = {0, 2, 4, 6, 8, 10}; // Whole-tone
                static const int8_t dimScale[] = {0, 2, 3, 5, 6, 8, 9, 11}; // Diminished (half-whole)

                const int8_t* scale;
                uint8_t scaleLen;

                switch (ctx.chordType) {
                    case CHORD_MINOR:
                    case CHORD_MIN7:
                    case CHORD_MADD9:
                        scale = minorScale;
                        scaleLen = 7;
                        break;
                    case CHORD_DOM7:
                        scale = mixolydian;
                        scaleLen = 7;
                        break;
                    case CHORD_AUG:
                        scale = wholeToneScale;
                        scaleLen = 6;
                        break;
                    case CHORD_DIM:
                    case CHORD_DIM7:
                    case CHORD_MIN7B5:
                        scale = dimScale;
                        scaleLen = 8;
                        break;
                    default:
                        // Major, sus, add9
                        scale = majorScale;
                        scaleLen = 7;
                        break;
                }

                for (int i = 0; i < scaleLen && count < maxNotes; i++) {
                    int8_t note = (ctx.chordRoot + scale[i]) % 12;
                    if (note < 0) note += 12;
                    outNotes[count++] = note;
                }
            }
            break;
    }

    return count;
}

int8_t QuantizeToNearestNote(int8_t note, const int8_t* notes, uint8_t count) {
    if (count == 0) return note;

    // Get the pitch class (0-11) and octave
    int8_t octave = note / 12;
    int8_t pitchClass = note % 12;
    if (pitchClass < 0) {
        pitchClass += 12;
        octave--;
    }

    // Find nearest note in the set
    int8_t bestNote = notes[0];
    int8_t bestDist = 127;

    for (int i = 0; i < count; i++) {
        int8_t target = notes[i];

        // Check distance in both directions (including octave wrap)
        int8_t dist1 = pitchClass >= target ? pitchClass - target : target - pitchClass;
        int8_t dist2 = 12 - dist1;
        int8_t dist = (dist1 < dist2) ? dist1 : dist2;

        if (dist < bestDist) {
            bestDist = dist;
            bestNote = target;
        }
    }

    // Calculate the quantized note, preserving the octave as much as possible
    int8_t result = octave * 12 + bestNote;

    // If the result is more than 6 semitones away, try adjacent octave
    int8_t diff = result - note;
    if (diff > 6) {
        result -= 12;
    } else if (diff < -6) {
        result += 12;
    }

    return result;
}

int8_t MapNoteToChord(int8_t originalNote, const ChordContext& ctx,
                      MelodyCompatMode mode) {
    // Get compatible notes for this chord and mode
    int8_t compatibleNotes[12];
    uint8_t numNotes = GetCompatibleNotes(ctx, mode, compatibleNotes, 12);

    if (numNotes == 0) {
        return originalNote;  // No mapping possible
    }

    // Quantize to nearest compatible note
    return QuantizeToNearestNote(originalNote, compatibleNotes, numNotes);
}

// ============================================================================
// VIBE SYSTEM FUNCTIONS
// ============================================================================

uint8_t GetProgressionsForVibe(VibeType vibe, uint8_t* indices, uint8_t maxCount) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_PROGRESSIONS && count < maxCount; i++) {
        if (progressions[i].vibe == vibe) {
            indices[count++] = i;
        }
    }
    return count;
}

uint8_t GetProgressionsForVibeCategory(VibeType vibe, ProgressionCategory cat,
                                        uint8_t* indices, uint8_t maxCount) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < NUM_PROGRESSIONS && count < maxCount; i++) {
        if (progressions[i].vibe == vibe && progressions[i].category == cat) {
            indices[count++] = i;
        }
    }
    return count;
}

uint8_t GetSteadyChordForVibe(VibeType vibe, uint32_t seed) {
    uint8_t steadyIndices[8];
    uint8_t count = GetProgressionsForVibeCategory(vibe, PROG_STEADY, steadyIndices, 8);

    if (count == 0) {
        // Fallback: return first progression for this vibe
        uint8_t allIndices[32];
        count = GetProgressionsForVibe(vibe, allIndices, 32);
        return count > 0 ? allIndices[seed % count] : 0;
    }

    return steadyIndices[seed % count];
}

int8_t CalculateVibeRootShift(VibeType from, VibeType to, uint32_t seed) {
    // Minor -> Major: relative major is up 3 or down 4 (down 9 = up 3)
    if (from == VIBE_MINOR && to == VIBE_MAJOR) {
        return ((seed & 0x01) == 0) ? 3 : -4;  // Up minor 3rd or down major 3rd
    }

    // Major -> Minor: relative minor is up 4 or down 3 (same interval, opposite direction)
    if (from == VIBE_MAJOR && to == VIBE_MINOR) {
        return ((seed & 0x01) == 0) ? -3 : 4;  // Down minor 3rd or up major 3rd
    }

    // Whole-tone: shift by whole tone (2) or tritone (6)
    if (from == VIBE_WHOLE_TONE || to == VIBE_WHOLE_TONE) {
        return ((seed & 0x01) == 0) ? 2 : 6;
    }

    // Within same vibe, changing root: up by fifth (7) or up by semitone (1)
    return ((seed & 0x01) == 0) ? 7 : 1;
}

} // namespace themis
