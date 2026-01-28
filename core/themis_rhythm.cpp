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

// Arpeggio patterns - note density patterns
static const uint16_t arpPatterns[] = {
    0x8888,  // Quarter notes
    0xAAAA,  // 8th notes
    0xCCCC,  // Triplet feel
    0xEEEE   // Dense
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

        case RHYTHM_PLAY_ARPEGGIOS:
        case RHYTHM_PLAY_POLYRHYTHM:
            return arpPatterns[patternIndex];

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
    uint8_t nextInversion = state.currentInversion;

    switch (state.inversionPattern) {
        case INVERSION_CYCLE:
            // Cycle: root -> 1st -> 2nd -> root
            nextInversion = (state.currentInversion + 1) % 3;
            break;

        case INVERSION_RANDOM:
            // Random inversion (0-2)
            nextInversion = seed % 3;
            break;

        case INVERSION_ROOT_ONLY:
        default:
            // Always root position
            nextInversion = 0;
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
// ARPEGGIO
// ============================================================================

int8_t GetArpeggioNote(
    const ChordContext& chordCtx,
    RhythmPlayerState& state,
    int8_t octaveOffset)
{
    const ChordShape& shape = chordShapes[chordCtx.chordType];
    int8_t baseNote = 48 + (octaveOffset * 12) + chordCtx.chordRoot;

    // Get note at current arp index
    uint8_t noteIndex = state.arpIndex % shape.numNotes;
    int8_t note = baseNote + shape.intervals[noteIndex];

    // Update arp index based on direction
    state.arpIndex += state.arpDirection;

    // Handle direction changes at boundaries
    if (state.arpIndex >= shape.numNotes) {
        state.arpIndex = shape.numNotes - 2;
        state.arpDirection = -1;
    } else if (state.arpIndex == 255) {  // Wrapped around (unsigned)
        state.arpIndex = 1;
        state.arpDirection = 1;
    }

    return note;
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

    // Intensity affects probability
    uint8_t prob = (uint8_t)(state.intensity * 30);
    bool randomBoost = ((seed & 0xFF) < prob);

    return patternActive || (randomBoost && state.currentStyle == RHYTHM_PLAY_ARPEGGIOS);
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
        case RHYTHM_PLAY_ARPEGGIOS:
            baseDuration = 2;  // Arps are usually staccato
            break;
        case RHYTHM_PLAY_POLYRHYTHM:
            baseDuration = 3;  // Poly varies
            break;
        default:
            baseDuration = 3;
    }

    // Articulation setting
    switch (config.articulation) {
        case ARTICULATION_STACCATO:
            baseDuration = (baseDuration + 1) / 2;
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

    // Clamp
    if (baseDuration < 1) baseDuration = 1;
    if (baseDuration > 8) baseDuration = 8;

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

    // Randomize arp direction
    config.arpDirection = (ArpDirection)((seed >> 8) % NUM_ARP_DIRECTIONS);

    // Update arp state based on direction
    switch (config.arpDirection) {
        case ARP_DOWN:
            state.arpDirection = -1;
            break;
        case ARP_UP_DOWN:
            state.arpDirection = (seed & 0x100) ? 1 : -1;
            break;
        case ARP_RANDOM:
            // Random is handled per-note
            break;
        default:
            state.arpDirection = 1;
            break;
    }

    // Randomize polyrhythm lengths
    uint8_t polyOptions1[] = {3, 5, 7};
    uint8_t polyOptions2[] = {4, 6, 8};
    state.polyLength1 = polyOptions1[(seed >> 12) % 3];
    state.polyLength2 = polyOptions2[(seed >> 14) % 3];

    // Randomize follow kick
    config.followKick = ((seed >> 16) & 0x03) == 0;

    // Randomize inversion pattern (60% cycle, 30% random, 10% root-only)
    uint8_t invRand = (seed >> 18) % 10;
    if (invRand < 6) {
        state.inversionPattern = INVERSION_CYCLE;
    } else if (invRand < 9) {
        state.inversionPattern = INVERSION_RANDOM;
    } else {
        state.inversionPattern = INVERSION_ROOT_ONLY;
    }

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

        case RHYTHM_PLAY_ARPEGGIOS: {
            // Handle random arp direction
            if (config.arpDirection == ARP_RANDOM) {
                state.arpDirection = ((seed & 0x01) == 0) ? 1 : -1;
            }
            outNotes[0] = GetArpeggioNote(chordCtx, state, config.octaveOffset);
            outNumNotes = 1;
            break;
        }

        case RHYTHM_PLAY_POLYRHYTHM:
            if (!ProcessPolyrhythm(chordCtx, state, config.octaveOffset, step, outNotes, outNumNotes)) {
                return false;  // Poly pattern didn't trigger
            }
            break;

        default:
            return false;
    }

    // If morphing between styles, sometimes blend (play from target style too)
    if (state.styleMorphProgress < 1.0f && state.styleMorphProgress > 0.3f) {
        // 30% chance to also play something from target style
        if ((seed & 0x0F) < 5) {
            int8_t extraNote = -1;

            switch (state.targetStyle) {
                case RHYTHM_PLAY_ARPEGGIOS:
                    extraNote = GetArpeggioNote(chordCtx, state, config.octaveOffset);
                    break;
                default:
                    break;
            }

            if (extraNote != -1 && outNumNotes < 6) {
                // Avoid duplicates
                bool duplicate = false;
                for (uint8_t i = 0; i < outNumNotes; i++) {
                    if (outNotes[i] == extraNote) {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate) {
                    outNotes[outNumNotes++] = extraNote;
                }
            }
        }
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
