/**
 * @file themis_acid.cpp
 * @brief Acid voice implementation - 303-style bass line generation
 */

#include "themis_acid.h"
#include "themis_chords.h"
#include <cstring>

namespace themis {

// ============================================================================
// STRING TABLES
// ============================================================================

const char* acidActivityNames[NUM_ACID_ACTIVITIES] = {
    "Sparse",
    "Moderate",
    "Busy"
};

const char* acidModeNames[NUM_ACID_MODES] = {
    "Manual",
    "Auto"
};

// ============================================================================
// RHYTHM PATTERN PRESETS
// ============================================================================
// Each pattern is 16 steps. Bit patterns: MSB = step 0, LSB = step 15
// triggers: which steps play a note
// accents: which of those are accented (velocity 127)
// slides: which notes slide into the next
// holds: which notes have extended gate length

const AcidRhythmPattern acidRhythmPatterns[NUM_ACID_RHYTHM_PATTERNS] = {
    // Pattern 0: Classic straight 16ths - driving techno bass
    {
        .triggers = 0b1111111111111111,  // All 16 steps
        .accents  = 0b1000100010001000,  // Accents on 1, 5, 9, 13
        .slides   = 0b0010001000100010,  // Slides on 3, 7, 11, 15
        .holds    = 0b0000000000000000,
        .name = "Straight"
    },

    // Pattern 1: Syncopated - off-beat emphasis
    {
        .triggers = 0b1011101110111011,  // Syncopated with rests
        .accents  = 0b0010001000100010,  // Off-beat accents
        .slides   = 0b0001000100010001,  // Slides before rests
        .holds    = 0b0000000000000000,
        .name = "Syncopated"
    },

    // Pattern 2: Sparse minimal - space and tension
    {
        .triggers = 0b1000100010001000,  // Quarter notes only
        .accents  = 0b1000000010000000,  // Accents on 1 and 9
        .slides   = 0b0000100000001000,  // Occasional slides
        .holds    = 0b1000100010001000,  // All held
        .name = "Minimal"
    },

    // Pattern 3: Busy 303 - classic acid feel
    {
        .triggers = 0b1110111011101110,  // Dense with gaps
        .accents  = 0b0100010001000100,  // Accent pattern
        .slides   = 0b0010001000100010,  // Regular slides
        .holds    = 0b0000001000000010,  // Occasional holds
        .name = "Classic"
    },

    // Pattern 4: Triplet feel - 3-against-4 tension
    {
        .triggers = 0b1001001001001001,  // Triplet-ish
        .accents  = 0b1000000001000000,  // Sparse accents
        .slides   = 0b0001001000001001,  // Slides on weak beats
        .holds    = 0b0000001000000010,
        .name = "Triplet"
    },

    // Pattern 5: Rolling - continuous movement
    {
        .triggers = 0b1111111011111110,  // Almost all, gaps on 8, 16
        .accents  = 0b1000010010000100,  // Shifted accents
        .slides   = 0b0111011101110111,  // Many slides
        .holds    = 0b0000000000000000,
        .name = "Rolling"
    },

    // Pattern 6: Stutter - repeated notes
    {
        .triggers = 0b1100110011001100,  // Pairs
        .accents  = 0b1000100010001000,  // First of pair
        .slides   = 0b0100010001000100,  // Second of pair slides
        .holds    = 0b0000000000000000,
        .name = "Stutter"
    },

    // Pattern 7: Breakbeat - broken rhythm
    {
        .triggers = 0b1010110010101100,  // Asymmetric
        .accents  = 0b0000100000001000,  // Sparse accents
        .slides   = 0b0010010010000100,  // Unexpected slides
        .holds    = 0b1000000010000000,  // Held downbeats
        .name = "Breakbeat"
    },

    // Pattern 8: Pumping - four-on-floor feel
    {
        .triggers = 0b1001100110011001,  // Pumping rhythm
        .accents  = 0b1000000010000000,  // Downbeat accents
        .slides   = 0b0001100000011000,  // Slides up to 8, down from 12
        .holds    = 0b0000000000000000,
        .name = "Pumping"
    },

    // Pattern 9: Shuffle - swung feel
    {
        .triggers = 0b1010101010101010,  // 8th notes
        .accents  = 0b1000100010001000,  // On-beat accents
        .slides   = 0b0010001000100010,  // Slides on up-beats
        .holds    = 0b1000100010001000,  // Hold downbeats
        .name = "Shuffle"
    },

    // Pattern 10: Ghost - lots of quiet ghost notes
    {
        .triggers = 0b1111111111111111,  // All 16
        .accents  = 0b1000000010000000,  // Very few accents (ghost notes)
        .slides   = 0b0101010101010101,  // Alternating slides
        .holds    = 0b0000000000000000,
        .name = "Ghost"
    },

    // Pattern 11: Accent heavy - punchy
    {
        .triggers = 0b1110111011101110,
        .accents  = 0b1110011001100110,  // Many accents
        .slides   = 0b0000100000001000,  // Few slides
        .holds    = 0b0000000000000000,
        .name = "Punchy"
    },

    // Pattern 12: Slide heavy - liquid
    {
        .triggers = 0b1111111111111111,
        .accents  = 0b1000000010000000,  // Sparse accents
        .slides   = 0b0111111101111111,  // Almost all slide
        .holds    = 0b0000000000000000,
        .name = "Liquid"
    },

    // Pattern 13: Hold heavy - sustained
    {
        .triggers = 0b1000100110001001,  // Sparse triggers
        .accents  = 0b1000000010000000,
        .slides   = 0b0000000100000001,
        .holds    = 0b1000100110001001,  // All held
        .name = "Sustained"
    },

    // Pattern 14: Random feel - unpredictable
    {
        .triggers = 0b1011001101100111,  // Irregular
        .accents  = 0b0001000100010001,  // Off-beat accents
        .slides   = 0b0100100010010010,  // Irregular slides
        .holds    = 0b0000001000000100,
        .name = "Chaotic"
    },

    // Pattern 15: Fill pattern - for transitions
    {
        .triggers = 0b1111111111111111,
        .accents  = 0b1010101010101010,  // Alternating accents
        .slides   = 0b0101010101010101,  // Alternating slides
        .holds    = 0b0000000000000000,
        .name = "Fill"
    }
};

// ============================================================================
// MELODY PATTERN PRESETS
// ============================================================================
// Scale degree offsets: 0 = root, 1 = 2nd, 2 = 3rd, etc.
// Negative = below root, positive = above
// These create different melodic contours when combined with rhythm patterns

const AcidMelodyPattern acidMelodyPatterns[NUM_ACID_MELODY_PATTERNS] = {
    // Pattern 0: Root focused - hypnotic
    {
        .notes = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
        .name = "Root"
    },

    // Pattern 1: Root-fifth bounce
    {
        .notes = {0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0, 0, 0, 4, 0},
        .name = "Root-5th"
    },

    // Pattern 2: Minor third pattern - dark
    {
        .notes = {0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0},
        .name = "Minor 3rd"
    },

    // Pattern 3: Ascending run
    {
        .notes = {0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 3, 2, 1},
        .name = "Ascending"
    },

    // Pattern 4: Descending run
    {
        .notes = {0, -1, -2, -3, 0, -1, -2, -3, 0, -1, -2, -3, -4, -3, -2, -1},
        .name = "Descending"
    },

    // Pattern 5: Octave jump - classic acid
    {
        .notes = {0, 0, 7, 0, 0, 0, 7, 0, 0, 0, 7, 0, 0, 7, 0, 0},
        .name = "Octave"
    },

    // Pattern 6: Chromatic approach
    {
        .notes = {0, 0, 0, -1, 0, 0, 0, -1, 0, 0, 0, 1, 0, 0, 0, 1},
        .name = "Chromatic"
    },

    // Pattern 7: Wide intervals
    {
        .notes = {0, 4, 0, -3, 0, 4, 0, -3, 0, 5, 0, -4, 0, 5, 0, -4},
        .name = "Wide"
    },

    // Pattern 8: Stepwise motion
    {
        .notes = {0, 1, 2, 1, 0, -1, -2, -1, 0, 1, 2, 1, 0, -1, -2, -1},
        .name = "Stepwise"
    },

    // Pattern 9: Arpeggio up
    {
        .notes = {0, 2, 4, 0, 2, 4, 7, 4, 0, 2, 4, 0, 2, 4, 7, 4},
        .name = "Arp Up"
    },

    // Pattern 10: Arpeggio down
    {
        .notes = {7, 4, 2, 0, 7, 4, 2, 0, 7, 4, 2, 0, -2, 0, 2, 4},
        .name = "Arp Down"
    },

    // Pattern 11: Pedal tone (alternating root)
    {
        .notes = {0, 2, 0, 4, 0, 2, 0, 4, 0, 2, 0, 5, 0, 4, 0, 2},
        .name = "Pedal"
    },

    // Pattern 12: Call and response
    {
        .notes = {0, 0, 2, 4, 0, 0, 0, 0, 4, 4, 2, 0, 0, 0, 0, 0},
        .name = "Call-Resp"
    },

    // Pattern 13: Tension build
    {
        .notes = {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 4, 5, 6},
        .name = "Build"
    },

    // Pattern 14: Random jumps (fixed but irregular)
    {
        .notes = {0, 3, -2, 5, 0, -1, 4, 2, 0, -3, 1, 6, 0, 2, -2, 4},
        .name = "Random"
    },

    // Pattern 15: Resolution pattern
    {
        .notes = {4, 3, 2, 1, 0, 0, 0, 0, 4, 3, 2, 1, 0, 0, 0, 0},
        .name = "Resolve"
    }
};

// ============================================================================
// FILL PATTERNS (pre-defined fills for end-of-bar variations)
// ============================================================================

// Fill note sequences - scale degree offsets for fills
static const int8_t fillPatternA[] = {0, 2, 4, 7};      // Arpeggio up
static const int8_t fillPatternB[] = {7, 4, 2, 0};      // Arpeggio down
static const int8_t fillPatternC[] = {0, 0, 7, 7};      // Octave bounce
static const int8_t fillPatternD[] = {0, 1, 2, 3};      // Chromatic run

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Simple pseudo-random number generator
 */
static uint32_t AcidRandom(uint32_t seed, uint32_t step)
{
    uint32_t x = seed ^ (step * 2654435761u);
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    return x;
}

/**
 * @brief Check probability (0-100)
 */
static bool CheckProbability(uint32_t seed, uint8_t prob)
{
    if (prob >= 100) return true;
    if (prob == 0) return false;
    return (seed % 100) < prob;
}

/**
 * @brief Get scale intervals for chord-aware mapping
 */
static void GetScaleIntervals(uint8_t chordType, const int8_t** intervals, uint8_t* length)
{
    // Use minor pentatonic as base scale for acid bass
    // This gives us: root, minor 3rd, 4th, 5th, minor 7th
    static const int8_t minorPenta[] = {0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24};
    static const int8_t majorPenta[] = {0, 2, 4, 7, 9, 12, 14, 16, 19, 21, 24};
    static const int8_t naturalMinor[] = {0, 2, 3, 5, 7, 8, 10, 12, 14, 15, 17, 19};

    // Suppress unused variable warning
    (void)naturalMinor;

    // Choose scale based on chord type
    if (chordType == CHORD_MAJOR || chordType == CHORD_MAJ7 || chordType == CHORD_DOM7) {
        *intervals = majorPenta;
        *length = 11;
    } else {
        // Minor, minor7, diminished - use minor pentatonic
        *intervals = minorPenta;
        *length = 11;
    }
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

int8_t MapAcidNoteToMidi(
    int8_t scaleDegree,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    int8_t octaveOffset,
    int8_t octaveShift)
{
    // Get scale intervals
    const int8_t* intervals;
    uint8_t length;
    GetScaleIntervals(chordCtx.chordType, &intervals, &length);

    // Handle negative scale degrees (wrap around)
    int8_t degree = scaleDegree;
    int8_t octaveAdj = 0;

    while (degree < 0) {
        degree += 7;  // Wrap up
        octaveAdj -= 1;
    }
    while (degree >= (int8_t)length) {
        degree -= 7;
        octaveAdj += 1;
    }

    // Clamp degree to valid range
    if (degree < 0) degree = 0;
    if (degree >= (int8_t)length) degree = length - 1;

    // Get semitone offset from scale
    int8_t semitones = intervals[degree];

    // Calculate final MIDI note
    // Base: melody root (global) + chord root offset + scale semitones
    int8_t baseNote = melodyRoot + chordCtx.chordRoot;
    int8_t note = baseNote + semitones;

    // Apply octave adjustments
    note += (octaveOffset + octaveShift + octaveAdj) * 12;

    // Clamp to valid MIDI range, preserving pitch class (octave-shift instead of hard clamp)
    while (note < 24) note += 12;   // Shift up octaves until >= C1
    while (note > 96) note -= 12;   // Shift down octaves until <= C7

    return note;
}

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
    uint8_t& outGateLength)
{
    // Work with 16-step pattern (step modulo 16)
    uint8_t patternStep = step & 0x0F;
    state.stepPosition = patternStep;

    // Update bar position (for fills)
    if (patternStep == 0) {
        state.barPosition++;
    }

    // Get current patterns
    uint8_t rhythmIdx = (config.mode == ACID_MODE_MANUAL)
        ? config.rhythmPattern : state.currentRhythmPattern;
    uint8_t melodyIdx = (config.mode == ACID_MODE_MANUAL)
        ? config.melodyPattern : state.currentMelodyPattern;

    // Clamp indices
    if (rhythmIdx >= NUM_ACID_RHYTHM_PATTERNS) rhythmIdx = 0;
    if (melodyIdx >= NUM_ACID_MELODY_PATTERNS) melodyIdx = 0;

    const AcidRhythmPattern& rhythm = acidRhythmPatterns[rhythmIdx];
    const AcidMelodyPattern& melody = acidMelodyPatterns[melodyIdx];

    // Check if in fill mode
    if (state.inFill && state.fillStepsRemaining > 0) {
        state.fillStepsRemaining--;
        return ProcessFillStep(state, chordCtx, melodyRoot, seed, outNote, outVelocity, outSlide);
    }

    // Check for fill at end of bar (steps 12-15)
    if (patternStep >= 12 && config.fillProb > 0) {
        uint32_t fillSeed = AcidRandom(seed, step + 1000);
        if (ShouldTriggerFill(state, patternStep, config.fillProb, fillSeed)) {
            state.inFill = true;
            state.fillStepsRemaining = 4;
            return ProcessFillStep(state, chordCtx, melodyRoot, seed, outNote, outVelocity, outSlide);
        }
    }

    // Check trigger from pattern
    bool shouldTrigger = IsStepActive16(rhythm.triggers, patternStep);

    // Apply trigger probability
    if (shouldTrigger && config.triggerProb < 100) {
        uint32_t trigSeed = AcidRandom(seed, step);
        if (!CheckProbability(trigSeed, config.triggerProb)) {
            shouldTrigger = false;
        }
    }

    // Activity level affects trigger probability
    if (shouldTrigger) {
        if (config.activity == ACID_ACTIVITY_SPARSE) {
            // Skip some triggers in sparse mode
            uint32_t sparseSeed = AcidRandom(seed, step + 500);
            if ((sparseSeed % 100) < 30) {  // 30% chance to skip
                shouldTrigger = false;
            }
        } else if (config.activity == ACID_ACTIVITY_BUSY) {
            // Add extra triggers in busy mode (on even steps without triggers)
            // Already triggering, so no change needed
        }
    } else if (config.activity == ACID_ACTIVITY_BUSY) {
        // In busy mode, add extra triggers
        uint32_t busySeed = AcidRandom(seed, step + 600);
        if ((busySeed % 100) < 20) {  // 20% chance to add trigger
            shouldTrigger = true;
        }
    }

    if (!shouldTrigger) {
        return false;
    }

    // Get base note from melody pattern
    int8_t scaleDegree = melody.notes[patternStep];

    // Determine octave shift from probability
    int8_t octaveShift = 0;
    uint32_t octSeed = AcidRandom(seed, step + 100);
    if (CheckProbability(octSeed, config.octaveUpProb)) {
        octaveShift = 1;
    } else if (CheckProbability(AcidRandom(seed, step + 200), config.octaveDownProb)) {
        octaveShift = -1;
    }

    // Map to MIDI note
    outNote = MapAcidNoteToMidi(scaleDegree, chordCtx, melodyRoot,
                                  config.octaveOffset, octaveShift);

    // Determine accent
    bool hasAccent = IsStepActive16(rhythm.accents, patternStep);
    // Apply accent probability for random accents
    if (!hasAccent && config.accentProb > 0) {
        uint32_t accSeed = AcidRandom(seed, step + 300);
        if (CheckProbability(accSeed, config.accentProb)) {
            hasAccent = true;
        }
    }
    outVelocity = hasAccent ? 127 : 64;

    // Determine slide
    outSlide = IsStepActive16(rhythm.slides, patternStep);
    // Apply slide probability for random slides
    if (!outSlide && config.slideProb > 0) {
        uint32_t slideSeed = AcidRandom(seed, step + 400);
        if (CheckProbability(slideSeed, config.slideProb)) {
            outSlide = true;
        }
    }

    // Determine gate length
    bool hasHold = IsStepActive16(rhythm.holds, patternStep);
    if (hasHold || outSlide) {
        outGateLength = 4;  // Long gate for held notes or slides
    } else {
        outGateLength = 1;  // Short gate
    }

    // Update state
    state.previousNote = state.currentNote;
    state.currentNote = outNote;
    state.slideActive = outSlide;

    return true;
}

bool ShouldTriggerFill(
    const AcidState& state,
    uint8_t step,
    uint8_t fillProb,
    uint32_t seed)
{
    // Only trigger fills at start of last 4 steps of bar
    if (step != 12) return false;

    // Every 4 bars, higher fill probability
    bool extendedBar = (state.barPosition % 4) == 3;
    uint8_t effectiveProb = extendedBar ? fillProb + 20 : fillProb;
    if (effectiveProb > 100) effectiveProb = 100;

    return CheckProbability(seed, effectiveProb);
}

bool ProcessFillStep(
    AcidState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint32_t seed,
    int8_t& outNote,
    uint8_t& outVelocity,
    bool& outSlide)
{
    // Choose fill pattern based on seed
    uint32_t fillSeed = AcidRandom(seed, state.barPosition);
    uint8_t fillType = fillSeed % 4;

    const int8_t* fillPattern;
    switch (fillType) {
        case 0: fillPattern = fillPatternA; break;
        case 1: fillPattern = fillPatternB; break;
        case 2: fillPattern = fillPatternC; break;
        default: fillPattern = fillPatternD; break;
    }

    // Get fill step (0-3)
    uint8_t fillStep = 4 - state.fillStepsRemaining;
    int8_t scaleDegree = fillPattern[fillStep];

    // Map to MIDI
    outNote = MapAcidNoteToMidi(scaleDegree, chordCtx, melodyRoot, -1, 0);

    // Fills are accented and often slide
    outVelocity = 127;
    outSlide = (fillStep < 3);  // Slide except on last fill step

    // End fill
    if (state.fillStepsRemaining == 0) {
        state.inFill = false;
    }

    return true;
}

void RandomizeAcidPatterns(
    AcidState& state,
    const AcidConfig& config,
    uint32_t seed)
{
    // Only randomize in auto mode
    if (config.mode != ACID_MODE_AUTO) return;

    // Decrement timer
    if (state.variationTimer > 0) {
        state.variationTimer--;
        return;
    }

    // Reset timer (64-128 steps = 2-4 bars)
    state.variationTimer = 64 + (seed % 64);

    // Select new patterns
    uint32_t rSeed = AcidRandom(seed, state.barPosition);

    // Higher chance to pick patterns matching activity level
    uint8_t rhythmBase, melodyBase;
    switch (config.activity) {
        case ACID_ACTIVITY_SPARSE:
            // Prefer patterns 2, 13 (minimal, sustained)
            rhythmBase = (rSeed % 2) ? 2 : 13;
            melodyBase = (rSeed % 3);  // Root-focused patterns
            break;
        case ACID_ACTIVITY_BUSY:
            // Prefer patterns 3, 5, 10, 11 (classic, rolling, ghost, punchy)
            rhythmBase = ((rSeed >> 4) % 4);
            if (rhythmBase == 0) rhythmBase = 3;
            else if (rhythmBase == 1) rhythmBase = 5;
            else if (rhythmBase == 2) rhythmBase = 10;
            else rhythmBase = 11;
            melodyBase = 3 + ((rSeed >> 8) % 6);  // More varied melodic patterns
            break;
        default:  // MODERATE
            rhythmBase = (rSeed >> 4) % NUM_ACID_RHYTHM_PATTERNS;
            melodyBase = (rSeed >> 8) % NUM_ACID_MELODY_PATTERNS;
            break;
    }

    // Some randomization even within activity constraints
    if ((rSeed >> 12) % 4 == 0) {
        rhythmBase = (rSeed >> 16) % NUM_ACID_RHYTHM_PATTERNS;
    }
    if ((rSeed >> 12) % 4 == 1) {
        melodyBase = (rSeed >> 20) % NUM_ACID_MELODY_PATTERNS;
    }

    state.currentRhythmPattern = rhythmBase;
    state.currentMelodyPattern = melodyBase;
    state.lastRandomValue = (uint8_t)rSeed;
}

void GetAcidPatternName(
    uint8_t rhythmIdx,
    uint8_t melodyIdx,
    char* buffer,
    uint8_t bufSize)
{
    if (rhythmIdx >= NUM_ACID_RHYTHM_PATTERNS) rhythmIdx = 0;
    if (melodyIdx >= NUM_ACID_MELODY_PATTERNS) melodyIdx = 0;

    const char* rName = acidRhythmPatterns[rhythmIdx].name;
    const char* mName = acidMelodyPatterns[melodyIdx].name;

    // Format: "Rhythm/Melody"
    int len = 0;
    while (*rName && len < bufSize - 2) {
        buffer[len++] = *rName++;
    }
    buffer[len++] = '/';
    while (*mName && len < bufSize - 1) {
        buffer[len++] = *mName++;
    }
    buffer[len] = '\0';
}

} // namespace themis
