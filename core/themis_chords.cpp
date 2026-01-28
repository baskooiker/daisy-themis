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
    // TECHNO / DARK / MINIMAL - Static and hypnotic progressions
    // ========================================================================

    // 0: Static Minor - No changes, pure hypnotic drone
    {"Static Minor",
     {{0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     1, FEEL_DARK},

    // 1: Static Min7 - Minor 7th drone, rich but dark
    {"Static Min7",
     {{0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0},
      {0, true, CHORD_MIN7, 0}, {0, true, CHORD_MIN7, 0}},
     1, FEEL_DARK},

    // 2: Static Sus4 - Tension without resolution
    {"Static Sus4",
     {{0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0},
      {0, true, CHORD_SUS4, 0}, {0, true, CHORD_SUS4, 0}},
     1, FEEL_TENSE},

    // 3: Static Dim7 - Eerie diminished drone
    {"Static Dim7",
     {{0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0},
      {0, true, CHORD_DIM7, 0}, {0, true, CHORD_DIM7, 0}},
     1, FEEL_TENSE},

    // 4: Techno Pulse - Minimal movement i-iv
    {"Techno Pulse",
     {{0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK},

    // 5: Dark Phrygian - i to bII, classic dark techno
    {"Dark Phrygian",
     {{0, true, CHORD_MINOR, 0}, {1, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK},

    // 6: Hypnotic bVII - i with occasional bVII
    {"Hypnotic bVII",
     {{0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {-2, false, CHORD_MAJOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK},

    // 7: Industrial - i-bII-i-bVII, gritty movement
    {"Industrial",
     {{0, true, CHORD_MINOR, 0}, {1, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_DARK},

    // 8: Tritone - Dissonant tritone movement
    {"Tritone",
     {{0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {6, false, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_TENSE},

    // 9: Half-Dim Drone - Dark half-diminished tension
    {"Half-Dim",
     {{0, true, CHORD_MIN7B5, 0}, {0, true, CHORD_MIN7B5, 0},
      {0, true, CHORD_MIN7B5, 0}, {0, true, CHORD_MIN7B5, 0},
      {0, true, CHORD_MIN7B5, 0}, {0, true, CHORD_MIN7B5, 0},
      {0, true, CHORD_MIN7B5, 0}, {0, true, CHORD_MIN7B5, 0}},
     1, FEEL_TENSE},

    // ========================================================================
    // CLASSIC PROGRESSIONS - Moved from original positions
    // ========================================================================

    // === HAPPY/BRIGHT ===

    // 10: Pop (I - V - vi - IV) - Classic pop progression
    {"Pop I-V-vi-IV",
     {{0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_HAPPY},

    // 11: Rock (I - IV - V - IV)
    {"Rock I-IV-V",
     {{0, true, CHORD_MAJOR, 0}, {3, true, CHORD_MAJOR, 0},
      {4, true, CHORD_MAJOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_BRIGHT},

    // 12: Fifties (I - vi - IV - V)
    {"Fifties",
     {{0, true, CHORD_MAJOR, 0}, {5, true, CHORD_MINOR, 0},
      {3, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_NOSTALGIC},

    // === SAD/MELANCHOLIC ===

    // 13: Axis (vi - IV - I - V) - Emotional pop
    {"Axis vi-IV-I-V",
     {{5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_EMOTIONAL},

    // 14: Minor Epic (i - VI - III - VII)
    {"Epic Minor",
     {{0, true, CHORD_MINOR, 0}, {5, true, CHORD_MAJOR, 0},
      {2, true, CHORD_MAJOR, 0}, {6, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_EPIC},

    // 15: Natural Minor (i - iv - v - i)
    {"Natural Minor",
     {{0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {4, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_SAD},

    // === TENSE/DRAMATIC ===

    // 16: Andalusian (i - VII - VI - V) using semitones for bVII and bVI
    {"Andalusian",
     {{0, true, CHORD_MINOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {-4, false, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_TENSE},

    // 17: Mixolydian Rock (I - bVII - IV - I)
    {"Mixolydian",
     {{0, true, CHORD_MAJOR, 0}, {-2, false, CHORD_MAJOR, 0},
      {3, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_BRIGHT},

    // 18: Minor Plagal (i - iv - bVII - i)
    {"Minor Plagal",
     {{0, true, CHORD_MINOR, 0}, {3, true, CHORD_MINOR, 0},
      {-2, false, CHORD_MAJOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0},
      {0, true, CHORD_MINOR, 0}, {0, true, CHORD_MINOR, 0}},
     4, FEEL_MELANCHOLIC},

    // === JAZZ ===

    // 19: Jazz 2-5-1 (ii7 - V7 - Imaj7 - Imaj7)
    {"Jazz ii-V-I",
     {{1, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_JAZZY},

    // 20: Jazz Standard (I - vi - ii - V)
    {"Jazz Standard",
     {{0, true, CHORD_MAJ7, 0}, {5, true, CHORD_MIN7, 0},
      {1, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_JAZZY},

    // === DREAMY ===

    // 21: Dreamy Major 7ths (Imaj7 - IVmaj7 - vi7 - V7)
    {"Dreamy 7ths",
     {{0, true, CHORD_MAJ7, 0}, {3, true, CHORD_MAJ7, 0},
      {5, true, CHORD_MIN7, 0}, {4, true, CHORD_DOM7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0},
      {0, true, CHORD_MAJ7, 0}, {0, true, CHORD_MAJ7, 0}},
     4, FEEL_DREAMY},

    // 22: Major to Minor IV (I - iii - IV - iv)
    {"Maj-Min IV",
     {{0, true, CHORD_MAJOR, 0}, {2, true, CHORD_MINOR, 0},
      {3, true, CHORD_MAJOR, 0}, {3, true, CHORD_MINOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0}},
     4, FEEL_DREAMY},

    // === ADD9 / MODERN ===

    // 23: Modern Pop Add9 (Iadd9 - Vadd9 - vi - IV)
    {"Modern Add9",
     {{0, true, CHORD_ADD9, 0}, {4, true, CHORD_ADD9, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_ADD9, 0}, {0, true, CHORD_ADD9, 0},
      {0, true, CHORD_ADD9, 0}, {0, true, CHORD_ADD9, 0}},
     4, FEEL_DREAMY},

    // === 8-CHORD PROGRESSIONS ===

    // 24: Extended Pop (I - V - vi - IV - I - V - iii - IV)
    {"Extended Pop",
     {{0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {5, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0},
      {0, true, CHORD_MAJOR, 0}, {4, true, CHORD_MAJOR, 0},
      {2, true, CHORD_MINOR, 0}, {3, true, CHORD_MAJOR, 0}},
     8, FEEL_HAPPY},

    // 25: Circle of Fifths (vi - ii - V - I - IV - vii° - iii - vi)
    {"Circle of 5ths",
     {{5, true, CHORD_MINOR, 0}, {1, true, CHORD_MINOR, 0},
      {4, true, CHORD_MAJOR, 0}, {0, true, CHORD_MAJOR, 0},
      {3, true, CHORD_MAJOR, 0}, {6, true, CHORD_DIM, 0},
      {2, true, CHORD_MINOR, 0}, {5, true, CHORD_MINOR, 0}},
     8, FEEL_JAZZY},
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

} // namespace themis
