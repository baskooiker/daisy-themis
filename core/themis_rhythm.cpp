/**
 * @file themis_rhythm.cpp
 * @brief Generative rhythm player implementation
 */

#include "themis_rhythm.h"
#include "themis_chords.h"
#include <cstdlib>

namespace themis {

// ============================================================================
// RHYTHM PATTERN TABLES
// ============================================================================

// Pad patterns - sparse, long-held chord sustains (16 steps each, MSB = step 0)
const PadPattern padPatterns[NUM_PAD_PATTERNS] = {
    {0x8000, "Whole"},      //  0: Hit on 1 only
    {0x8080, "Half 1-3"},   //  1: Hits on 1 and 3
    {0x8800, "Half 1-2"},   //  2: Hits on 1 and 2
    {0x0880, "Half 2-3"},   //  3: Hits on 2 and 3
    {0x4000, "Push 1"},     //  4: Just after beat 1
    {0x4040, "Push 1-3"},   //  5: After beats 1 and 3
    {0x0440, "Push 2-3"},   //  6: After beats 2 and 3
    {0x0040, "Late 3"},     //  7: Only after beat 3
    {0x8008, "Tie"},        //  8: Beat 1 + anticipated next bar
    {0x0808, "Backbeat"},   //  9: After beats 2 and 4
    {0x0088, "Late Half"},  // 10: Beat 3 + anticipated
    {0x0008, "Antic"},      // 11: Anticipated only (before next bar)
    {0x8040, "Open"},       // 12: Beat 1, then after 3
    {0x4080, "Close"},      // 13: After 1, then on 3
    {0x8808, "Drive"},      // 14: Beats 1, 2 + anticipated
    {0x4048, "Float"},      // 15: After 1, after 3, anticipated
};

// Chord patterns - sparse, medium, busy (16 steps each, MSB = step 0)
static const uint16_t chordPatternsSparse[] = {
    0x8000,  // On the 1
    0x8080,  // 1 and 3
    0x8008,  // 1 and 4&
    0x8880   // 1, 2, 3
};

static const uint16_t chordPatternsMedium[] = {
    0x8888,  // Steady quarters
    0x8A8A,  // With upbeats
    0x88A0,  // Syncopated
    0xA8A8   // Pushing
};

static const uint16_t chordPatternsBusy[] = {
    0xAAAA,  // All 8ths
    0xAEAE,  // Busier
    0xEAEA,  // Choppy
    0xEEEE   // Very dense
};

// ============================================================================
// INTENSITY SYSTEM
// ============================================================================

void UpdateRhythmIntensity(RhythmPlayerState& state, uint32_t seed)
{
    // Smooth transition toward target
    float diff = state.intensityTarget - state.intensity;
    float rate = 0.015f;  // Slower than soloist for stability

    if (diff > rate) {
        state.intensity += rate;
    } else if (diff < -rate) {
        state.intensity -= rate;
    } else {
        state.intensity = state.intensityTarget;
    }

    // Occasionally set new target (less often than soloist)
    if ((seed & 0x1F) == 0) {
        // Rhythm player stays more stable - narrower range
        float range = 0.5f;
        float newTarget = 0.25f + range * (float)((seed >> 8) & 0xFF) / 255.0f;

        // Clamp
        if (newTarget < 0.2f) newTarget = 0.2f;
        if (newTarget > 0.8f) newTarget = 0.8f;

        state.intensityTarget = newTarget;
    }
}

// ============================================================================
// STYLE MORPHING
// ============================================================================

void UpdateRhythmMorph(
    const RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    uint8_t step,
    uint32_t seed)
{
    (void)step;  // No longer used for automatic style changes
    (void)seed;  // Style selection now happens via NotifyRhythmOfChordCycle

    if (config.mode == RHYTHM_MODE_MANUAL) {
        // In manual mode, just snap to configured style
        state.currentStyle = config.playStyle;
        state.styleMorphProgress = 1.0f;
        return;
    }

    // If frozen, don't change styles
    if (config.freezeStyle) {
        return;
    }

    // Morph mode - smooth transition between styles
    // Style changes are now triggered by NotifyRhythmOfChordCycle() at chord cycle boundaries
    if (state.styleMorphProgress < 1.0f) {
        state.styleMorphProgress += 0.01f;  // ~100 steps to complete morph
        if (state.styleMorphProgress > 1.0f) {
            state.styleMorphProgress = 1.0f;
            state.currentStyle = state.targetStyle;
        }
    }

    // Decrement morph timer (used by NotifyRhythmOfChordCycle to decide when to change)
    if (state.morphTimer > 0) {
        state.morphTimer--;
    }
}

// ============================================================================
// PATTERN SELECTION
// ============================================================================

uint16_t GetRhythmPattern(
    RhythmPlayStyle style,
    RhythmActivity activity,
    uint8_t patternIndex)
{
    patternIndex = patternIndex % 4;

    switch (style) {
        case RHYTHM_PLAY_CHORDS:
            switch (activity) {
                case ACTIVITY_SPARSE:
                    return chordPatternsSparse[patternIndex];
                case ACTIVITY_BUSY:
                    return chordPatternsBusy[patternIndex];
                default:
                    return chordPatternsMedium[patternIndex];
            }

        case RHYTHM_PLAY_POLYRHYTHM:
            return chordPatternsMedium[patternIndex];

        case RHYTHM_PLAY_PAD:
            // Pad patterns handled separately via padPatterns table
            return chordPatternsSparse[patternIndex];

        default:
            return chordPatternsMedium[0];
    }
}

// ============================================================================
// CHORD VOICING & INVERSIONS
// ============================================================================

void ApplyInversion(int8_t* notes, uint8_t numNotes, uint8_t inversion)
{
    if (numNotes < 2 || inversion == 0) {
        return;  // No inversion needed
    }

    // Sort notes to find lowest ones (simple bubble sort for small arrays)
    for (uint8_t i = 0; i < numNotes - 1; i++) {
        for (uint8_t j = 0; j < numNotes - i - 1; j++) {
            if (notes[j] > notes[j + 1]) {
                int8_t temp = notes[j];
                notes[j] = notes[j + 1];
                notes[j + 1] = temp;
            }
        }
    }

    // Move the lowest note(s) up an octave based on inversion
    uint8_t notesToMove = (inversion > numNotes - 1) ? numNotes - 1 : inversion;
    for (uint8_t i = 0; i < notesToMove; i++) {
        notes[i] += 12;  // Move up one octave
    }
}

uint8_t GetNextInversion(RhythmPlayerState& state, uint32_t seed)
{
    uint8_t nextInversion;
    uint8_t roll = seed % 100;

    switch (state.inversionVariation) {
        case INV_VAR_HIGH:
            // Mostly inverted: 20% root, 40% 1st, 40% 2nd
            if (roll < 20) {
                nextInversion = 0;
            } else if (roll < 60) {
                nextInversion = 1;
            } else {
                nextInversion = 2;
            }
            break;

        case INV_VAR_LOW:
        default:
            // Mostly root: 70% root, 20% 1st, 10% 2nd
            if (roll < 70) {
                nextInversion = 0;
            } else if (roll < 90) {
                nextInversion = 1;
            } else {
                nextInversion = 2;
            }
            break;
    }

    state.currentInversion = nextInversion;
    return nextInversion;
}

void GenerateChordVoicing(
    const ChordContext& chordCtx,
    int8_t octaveOffset,
    uint8_t inversion,
    int8_t* outNotes,
    uint8_t& outNumNotes)
{
    outNumNotes = 0;

    // Get chord shape
    const ChordShape& shape = chordShapes[chordCtx.chordType];

    // Base octave (C3 = MIDI 48)
    int8_t baseNote = 48 + (octaveOffset * 12) + chordCtx.chordRoot;

    // Build chord in root position
    for (uint8_t i = 0; i < shape.numNotes && outNumNotes < 6; i++) {
        if (shape.intervals[i] != -128) {
            outNotes[outNumNotes++] = baseNote + shape.intervals[i];
        }
    }

    // Apply inversion
    ApplyInversion(outNotes, outNumNotes, inversion);
}

// ============================================================================
// POLYRHYTHM
// ============================================================================

bool ProcessPolyrhythm(
    const ChordContext& chordCtx,
    RhythmPlayerState& state,
    int8_t octaveOffset,
    uint8_t step,
    int8_t* outNotes,
    uint8_t& outNumNotes)
{
    outNumNotes = 0;
    bool trigger = false;

    const ChordShape& shape = chordShapes[chordCtx.chordType];
    int8_t baseNote = 48 + (octaveOffset * 12) + chordCtx.chordRoot;

    // Layer 1: 3-beat pattern
    if (step % state.polyLength1 == 0) {
        state.polyCounter1++;
        if (outNumNotes < 6) {
            uint8_t idx = state.polyCounter1 % shape.numNotes;
            outNotes[outNumNotes++] = baseNote + shape.intervals[idx];
            trigger = true;
        }
    }

    // Layer 2: 4-beat pattern (or 5, 7, etc. for variety)
    if (step % state.polyLength2 == 0) {
        state.polyCounter2++;
        if (outNumNotes < 6) {
            // Different note from the chord
            uint8_t idx = (state.polyCounter2 + 1) % shape.numNotes;
            outNotes[outNumNotes++] = baseNote + shape.intervals[idx] + 12;  // Octave up
            trigger = true;
        }
    }

    return trigger;
}

// ============================================================================
// PLAY DECISION
// ============================================================================

bool ShouldRhythmPlay(
    const RhythmPlayerConfig& config,
    const RhythmPlayerState& state,
    bool kickActive,
    uint8_t step,
    uint32_t seed)
{
    // Already playing notes that haven't ended?
    if (state.noteDuration > 0) {
        return false;
    }

    // Get pattern for current style
    uint8_t patternIndex = (seed >> 4) % 4;
    uint16_t pattern = GetRhythmPattern(state.currentStyle, config.activity, patternIndex);

    // Check pattern step (use lower 4 bits for 16-step pattern)
    uint8_t patternStep = step % 16;
    bool patternActive = IsStepActive16(pattern, patternStep);

    // Follow kick option
    if (config.followKick) {
        if (kickActive) {
            // Boost probability on kicks
            patternActive = patternActive || ((seed & 0x03) == 0);
        }
    }

    // Style-specific adjustments
    if (state.currentStyle == RHYTHM_PLAY_POLYRHYTHM) {
        // Polyrhythm has its own trigger logic
        return true;  // Always process, let ProcessPolyrhythm decide
    }

    if (state.currentStyle == RHYTHM_PLAY_PAD) {
        // Pad uses its own stored pattern, not the chord patterns
        uint8_t padIdx = state.padPatternA;
        if (padIdx >= NUM_PAD_PATTERNS) padIdx = 0;
        uint8_t patternStep = step % 16;
        return IsStepActive16(padPatterns[padIdx].triggers, patternStep);
    }

    return patternActive;
}

// ============================================================================
// VELOCITY
// ============================================================================

uint8_t CalculateRhythmVelocity(
    const RhythmPlayerState& state,
    uint8_t step,
    uint32_t seed)
{
    // Base velocity from intensity
    uint8_t baseVel = 50 + (uint8_t)(state.intensity * 40);

    // Beat emphasis (louder on downbeats)
    uint8_t beatPos = step % 4;
    if (beatPos == 0) {
        baseVel += 15;  // Strong beat
    } else if (beatPos == 2) {
        baseVel += 8;   // Medium beat
    }

    // Bar position emphasis
    if ((step % 16) == 0) {
        baseVel += 10;  // First beat of bar
    }

    // Style-specific adjustments
    if (state.currentStyle == RHYTHM_PLAY_POLYRHYTHM) {
        baseVel -= 10;  // Poly a bit softer
    } else if (state.currentStyle == RHYTHM_PLAY_PAD) {
        baseVel -= 5;   // Pad slightly softer
    }

    // Random variation
    int8_t variation = (int8_t)((seed & 0x0F) - 8);
    int16_t result = (int16_t)baseVel + variation;

    // Clamp
    if (result < 40) result = 40;
    if (result > 110) result = 110;

    return (uint8_t)result;
}

// ============================================================================
// DURATION
// ============================================================================

uint8_t CalculateRhythmDuration(
    const RhythmPlayerConfig& config,
    const RhythmPlayerState& state,
    uint32_t seed)
{
    uint8_t baseDuration;

    // Style affects base duration
    switch (state.currentStyle) {
        case RHYTHM_PLAY_CHORDS:
            baseDuration = 4;  // Chords sustain longer
            break;
        case RHYTHM_PLAY_POLYRHYTHM:
            baseDuration = 3;  // Poly varies
            break;
        case RHYTHM_PLAY_PAD:
            baseDuration = 8;  // Pads sustain longest (half a bar)
            break;
        default:
            baseDuration = 3;
    }

    // Articulation setting
    switch (config.articulation) {
        case ARTICULATION_STACCATO:
            if (state.currentStyle == RHYTHM_PLAY_PAD) {
                baseDuration = 4;  // Pad staccato still longer than chord staccato
            } else {
                baseDuration = (baseDuration + 1) / 2;
            }
            break;
        case ARTICULATION_LEGATO:
            baseDuration += 2;
            break;
        default:
            break;
    }

    // Variation
    if ((seed & 0x03) == 0) {
        baseDuration++;
    } else if ((seed & 0x03) == 1 && baseDuration > 1) {
        baseDuration--;
    }

    // Clamp - pad allows longer durations
    uint8_t maxDuration = (state.currentStyle == RHYTHM_PLAY_PAD) ? 14 : 8;
    if (baseDuration < 1) baseDuration = 1;
    if (baseDuration > maxDuration) baseDuration = maxDuration;

    return baseDuration;
}

// ============================================================================
// RANDOMIZATION
// ============================================================================

void RandomizeRhythmParams(
    RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    uint32_t seed)
{
    if (config.mode != RHYTHM_MODE_MORPH) {
        return;
    }

    // Randomize activity level
    config.activity = (RhythmActivity)(seed % NUM_RHYTHM_ACTIVITY_LEVELS);

    // Randomize articulation
    config.articulation = (RhythmArticulation)((seed >> 4) % NUM_RHYTHM_ARTICULATIONS);

    // Randomize inversion variation
    config.inversionVariation = (InversionVariation)((seed >> 8) % NUM_INV_VARIATIONS);

    // Randomize polyrhythm lengths
    uint8_t polyOptions1[] = {3, 5, 7};
    uint8_t polyOptions2[] = {4, 6, 8};
    state.polyLength1 = polyOptions1[(seed >> 12) % 3];
    state.polyLength2 = polyOptions2[(seed >> 14) % 3];

    // Randomize follow kick
    config.followKick = ((seed >> 16) & 0x03) == 0;

    // Set inversion variation on state to match config
    state.inversionVariation = config.inversionVariation;

    // Randomize pad patterns (ensure A and B are different)
    state.padPatternA = (seed >> 18) % NUM_PAD_PATTERNS;
    state.padPatternB = ((seed >> 22) % (NUM_PAD_PATTERNS - 1));
    if (state.padPatternB >= state.padPatternA) state.padPatternB++;

    // Set intensity target based on activity
    switch (config.activity) {
        case ACTIVITY_SPARSE:
            state.intensityTarget = 0.25f + 0.15f * (float)((seed >> 20) & 0x0F) / 15.0f;
            break;
        case ACTIVITY_BUSY:
            state.intensityTarget = 0.55f + 0.25f * (float)((seed >> 20) & 0x0F) / 15.0f;
            break;
        default:
            state.intensityTarget = 0.35f + 0.25f * (float)((seed >> 20) & 0x0F) / 15.0f;
            break;
    }

    // Reset timer (8-32 bars)
    state.randomizeTimer = 256 + ((seed >> 24) % 768);
}

// ============================================================================
// MAIN STEP PROCESSING
// ============================================================================

bool ProcessRhythmStep(
    const RhythmPlayerConfig& config,
    RhythmPlayerState& state,
    const ChordContext& chordCtx,
    bool kickActive,
    uint8_t step,
    uint32_t seed,
    int8_t* outNotes,
    uint8_t& outNumNotes)
{
    outNumNotes = 0;

    // Update systems
    UpdateRhythmIntensity(state, seed);
    UpdateRhythmMorph(config, state, step, seed ^ 0x12345678);

    // Track bar position
    state.barPosition = step % 16;
    state.patternPosition = step;

    // Handle note-off timing
    if (state.noteDuration > 0) {
        state.noteDuration--;
        if (state.noteDuration == 0) {
            // Clear active notes
            state.numActiveNotes = 0;
            for (int i = 0; i < 6; i++) {
                state.activeNotes[i] = -1;
            }
        }
    }

    // Check randomization timer
    if (config.mode == RHYTHM_MODE_MORPH) {
        state.randomizeTimer--;
        // Note: actual randomization happens in sequencer when timer hits 0
    }

    // Decide if we should play
    if (!ShouldRhythmPlay(config, state, kickActive, step, seed ^ 0xABCDEF00)) {
        return false;
    }

    // Don't retrigger if notes still playing
    if (state.numActiveNotes > 0) {
        return false;
    }

    // Generate notes based on current style
    switch (state.currentStyle) {
        case RHYTHM_PLAY_CHORDS: {
            // Get next inversion and apply it
            uint8_t inversion = GetNextInversion(state, seed ^ 0x789ABC00);
            GenerateChordVoicing(chordCtx, config.octaveOffset, inversion, outNotes, outNumNotes);
            break;
        }

        case RHYTHM_PLAY_POLYRHYTHM:
            if (!ProcessPolyrhythm(chordCtx, state, config.octaveOffset, step, outNotes, outNumNotes)) {
                return false;  // Poly pattern didn't trigger
            }
            break;

        case RHYTHM_PLAY_PAD: {
            // Pad uses chord voicing like chords, but with its own trigger pattern
            uint8_t inversion = GetNextInversion(state, seed ^ 0x789ABC00);
            GenerateChordVoicing(chordCtx, config.octaveOffset, inversion, outNotes, outNumNotes);
            break;
        }

        default:
            return false;
    }

    // Calculate duration and store active notes
    state.noteDuration = CalculateRhythmDuration(config, state, seed ^ 0xDEADFACE);
    state.numActiveNotes = outNumNotes;
    for (uint8_t i = 0; i < outNumNotes && i < 6; i++) {
        state.activeNotes[i] = outNotes[i];
    }

    return outNumNotes > 0;
}

} // namespace themis
