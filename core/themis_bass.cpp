/**
 * @file themis_bass.cpp
 * @brief Bass voice implementation - root-note bass line generation
 */

#include "themis_bass.h"
#include "themis_chords.h"

namespace themis {

// ============================================================================
// BASS PATTERN PRESETS
// ============================================================================
// Bit patterns: bit [length-1] = step 0
// triggers: which steps play a note
// accents: which of those are accented (velocity 120 vs 70)
// holds: which notes hold longer (3 steps vs 1 step)
//
// 16-step layout (16th notes in one bar):
// Beat:  1  e  &  a  2  e  &  a  3  e  &  a  4  e  &  a
// Step:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15

const BassPattern bassPatterns[NUM_BASS_PATTERNS] = {
    // === 16-step patterns (0-15) ===

    // 0: Root - Solid quarter notes with pickup into next bar
    {0b1000100010001001, 0b1000000010000000, 0b1000100010000000, 16, "Root"},
    // 1: Funk - Classic James Brown syncopation
    {0b1001001010100010, 0b1000000010000000, 0b1000000000000000, 16, "Funk"},
    // 2: Push - Anticipated beats
    {0b1001000100100011, 0b0001000100100001, 0b1000000000000000, 16, "Push"},
    // 3: Dub - Reggae one-drop
    {0b1000000010010010, 0b1000000010000000, 0b1000000010010000, 16, "Dub"},
    // 4: Bounce - Hip hop lazy groove
    {0b1000001100010010, 0b1000000000000000, 0b1000001000000000, 16, "Bounce"},
    // 5: House - Pumping offbeat energy
    {0b1001010010010010, 0b1000000010000000, 0b0001010000010000, 16, "House"},
    // 6: Stab - Offbeat stabs
    {0b0010001000100110, 0b0010000000100000, 0b0000000000000000, 16, "Stab"},
    // 7: Latin - Clave-influenced
    {0b1010011010001010, 0b1000001010000000, 0b1010000000000000, 16, "Latin"},
    // 8: Broken - UK garage / broken beat
    {0b1100100100101000, 0b1000000000001000, 0b0000100100000000, 16, "Broken"},
    // 9: Tresillo - 3+3+2 Afro-Cuban
    {0b1001001010110010, 0b1000000010000000, 0b1001000010000000, 16, "Tresillo"},
    // 10: Sparse - Ultra minimal
    {0b1000000000110000, 0b1000000000000000, 0b1000000000110000, 16, "Sparse"},
    // 11: Drive - Driving eighths
    {0b1010101010100110, 0b1000100010000010, 0b0000000000000000, 16, "Drive"},
    // 12: Swing - Shuffle feel
    {0b1001000110010010, 0b1000000010000000, 0b1000000010000000, 16, "Swing"},
    // 13: Afro - Afrobeat cross-rhythm
    {0b1001010011010010, 0b1000000010000000, 0b1000000010000000, 16, "Afro"},
    // 14: DnB - Drum & bass syncopated pairs
    {0b1000001100110010, 0b1000000000000000, 0b0000001100000000, 16, "DnB"},
    // 15: Wonky - Complex cross-rhythm
    {0b1001010001010011, 0b1000000000010001, 0b0000000000000000, 16, "Wonky"},

    // === 8-step patterns (16-18) ===

    // 16: Pulse8 - Simple pulse: beats 1 and 3
    // Steps: X . . . X . . .
    {0b10001000, 0b10000000, 0b10001000, 8, "Pulse8"},
    // 17: Offbeat8 - Offbeat hits: steps 1,3,5,7
    // Steps: . X . X . X . X
    {0b01010101, 0b01000001, 0b00000000, 8, "Offbt8"},
    // 18: Gallop8 - Galloping: 1-e-a pattern
    // Steps: X X . X . . X .
    {0b11010010, 0b10000000, 0b10000000, 8, "Gallop8"},

    // === 32-step patterns (19-21) ===

    // 19: Evolve - Sparse bar 1, busier bar 2
    // Bar1: X . . . . . . . X . . . . . . .
    // Bar2: X . . X . . X . X . X . . . X .
    {0b10000000100000001001001010100010, 0b10000000100000001000000010000000, 0b10000000100000000001000000000000, 32, "Evolve"},
    // 20: TwoBar - 2-bar phrase with unique second bar
    // Bar1: X . . . X . . . X . . . X . . .
    // Bar2: X . X . . . X . . . X . X . . X
    {0b10001000100010001010001000101001, 0b10000000100000001000000000000001, 0b10001000100000000000000000000000, 32, "TwoBar"},
    // 21: Build - Builds density across 32 steps
    // Bar1: X . . . . . . . . . . . . . . .
    // Bar2: X . X . X . X . X X X . X . X .
    {0b10000000000000001010101011101010, 0b10000000000000001000100010001000, 0b10000000000000000000000000000000, 32, "Build"},
};

// ============================================================================
// BASS PITCH PATTERN PRESETS
// ============================================================================
// R = ROOT, U = OCT_UP, D = OCT_DOWN, 3 = THIRD, 5 = FIFTH, 7 = SEVENTH
// 2 = SCALE_2, 4 = SCALE_4, 6 = SCALE_6, + = APPROACH_UP, - = APPROACH_DN

#define R BASS_PITCH_ROOT
#define U BASS_PITCH_OCT_UP
#define D BASS_PITCH_OCT_DOWN
#define T BASS_PITCH_THIRD
#define F BASS_PITCH_FIFTH
#define S BASS_PITCH_SEVENTH
#define P2 BASS_PITCH_SCALE_2
#define P4 BASS_PITCH_SCALE_4
#define P6 BASS_PITCH_SCALE_6
#define AU BASS_PITCH_APPROACH_UP
#define AD BASS_PITCH_APPROACH_DN

const BassPitchPattern bassPitchPatterns[NUM_BASS_PITCH_PATTERNS] = {
    // === 16-step pitch patterns (0-9) ===

    // 0: Root - All root notes
    {{R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Root"},
    // 1: Octave - Root with occasional octave jumps
    {{R, R, R, R, U, R, R, R, R, R, U, R, R, R, R, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Octave"},
    // 2: Fifth - Root with fifths on offbeats
    {{R, R, F, R, R, R, F, R, R, R, F, R, R, R, F, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Fifth"},
    // 3: Chord - Root on beats, third+fifth on offbeats
    {{R, R, T, R, R, R, F, R, R, R, T, R, R, R, F, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Chord"},
    // 4: Walk - Root->third->fifth->octave per beat
    {{R, R, R, R, T, T, T, T, F, F, F, F, U, U, U, U,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Walk"},
    // 5: Run - 12 steps root, then scale run at bar end
    {{R, R, R, R, R, R, R, R, R, R, R, R, P2, P4, P6, F,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Run"},
    // 6: Arp - Ascending/descending arpeggio
    {{R, R, T, T, F, F, U, U, U, U, F, F, T, T, R, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Arp"},
    // 7: Pendulum - Alternating root and octave up
    {{R, R, U, U, R, R, U, U, R, R, U, U, R, R, U, U,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Pendulum"},
    // 8: Approach - Root with chromatic approach tones before downbeats
    {{R, R, R, AU, R, R, R, AD, R, R, R, AU, R, R, R, AD,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "Approach"},
    // 9: Seventh - Root with seventh and fifth on upbeats
    {{R, R, S, R, R, R, F, R, R, R, S, R, R, R, F, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 16, "7th"},

    // === 8-step pitch patterns (10-12) ===

    // 10: Arp8 - Quick R->3->5->U->U->5->3->R
    {{R, T, F, U, U, F, T, R,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 8, "Arp8"},
    // 11: OctPump8 - R-U-R-U-R-U-R-U
    {{R, U, R, U, R, U, R, U,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 8, "OctPmp8"},
    // 12: Climb8 - R-R-3-3-5-5-U-U
    {{R, R, T, T, F, F, U, U,  R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R,R}, 8, "Climb8"},

    // === 32-step pitch patterns (13-15) ===

    // 13: LongWalk - Slow ascent R->3->5->U over 2 bars
    {{R, R, R, R, R, R, R, R, T, T, T, T, T, T, T, T, F, F, F, F, F, F, F, F, U, U, U, U, U, U, U, U}, 32, "LngWalk"},
    // 14: LongRun - 28 steps root, then 4-step scale run
    {{R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, P2, P4, P6, F}, 32, "LngRun"},
    // 15: TwoFace - Bar 1 all root, bar 2 chord tones
    {{R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, R, T, R, F, R, T, R, U, R, F, R, T, R, U, R, R}, 32, "TwoFace"},
};

#undef R
#undef U
#undef D
#undef T
#undef F
#undef S
#undef P2
#undef P4
#undef P6
#undef AU
#undef AD

// ============================================================================
// PITCH RESOLUTION
// ============================================================================

int8_t ResolveBassPitchType(BassPitchType type, int8_t baseNote, const ChordContext& chordCtx)
{
    if (type == BASS_PITCH_ROOT) return baseNote;

    const ChordShape& shape = chordShapes[chordCtx.chordType];
    int8_t note = baseNote;

    switch (type) {
        case BASS_PITCH_OCT_UP:
            note = baseNote + 12;
            break;
        case BASS_PITCH_OCT_DOWN:
            note = baseNote - 12;
            break;
        case BASS_PITCH_THIRD:
            note = baseNote + shape.intervals[1];  // 3rd interval
            break;
        case BASS_PITCH_FIFTH:
            note = baseNote + shape.intervals[2];  // 5th interval
            break;
        case BASS_PITCH_SEVENTH:
            // Use 7th if available (numNotes > 3), otherwise fall back to 5th
            if (shape.numNotes > 3 && shape.intervals[3] != -128 && shape.intervals[3] != -1) {
                note = baseNote + shape.intervals[3];
            } else {
                note = baseNote + shape.intervals[2];  // Fallback to 5th
            }
            break;
        case BASS_PITCH_SCALE_2:
            note = baseNote + 2;
            break;
        case BASS_PITCH_SCALE_4:
            note = baseNote + 5;
            break;
        case BASS_PITCH_SCALE_6:
            note = baseNote + 8;
            break;
        case BASS_PITCH_APPROACH_UP:
            note = baseNote + 1;
            break;
        case BASS_PITCH_APPROACH_DN:
            note = baseNote - 1;
            break;
        default:
            break;
    }

    // Clamp to MIDI 24-96
    while (note < 24) note += 12;
    while (note > 96) note -= 12;

    return note;
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

bool ProcessBassStep(
    const BassConfig& config,
    BassState& state,
    const ChordContext& chordCtx,
    int8_t melodyRoot,
    uint8_t step,
    int8_t& outNote,
    uint8_t& outVelocity,
    uint8_t& outGateLength)
{
    // Get current rhythm pattern
    uint8_t patIdx = state.currentPattern;
    if (patIdx >= NUM_BASS_PATTERNS) patIdx = 0;

    const BassPattern& pattern = bassPatterns[patIdx];

    // Wrap step to rhythm pattern length (independent of pitch length)
    uint8_t rhythmStep = step % pattern.length;

    // Check if this step triggers
    if (!IsStepActiveVar(pattern.triggers, rhythmStep, pattern.length)) {
        return false;
    }

    // Calculate root note: melodyRoot + chordRoot, with octave offset
    int8_t rootSemitone = melodyRoot + chordCtx.chordRoot;
    // Normalize to 0-11
    while (rootSemitone < 0) rootSemitone += 12;
    rootSemitone = rootSemitone % 12;

    // Base MIDI note: C2 (36) + root + octave offset
    int8_t note = 36 + rootSemitone + (config.octaveOffset * 12);

    // Clamp to valid MIDI range
    while (note < 24) note += 12;
    while (note > 96) note -= 12;

    // Apply pitch pattern (independent length wrapping)
    uint8_t pitchIdx = state.currentPitchPattern;
    if (pitchIdx < NUM_BASS_PITCH_PATTERNS) {
        const BassPitchPattern& pitchPat = bassPitchPatterns[pitchIdx];
        uint8_t pitchStep = step % pitchPat.length;
        BassPitchType pitchType = pitchPat.steps[pitchStep];
        if (pitchType != BASS_PITCH_ROOT) {
            note = ResolveBassPitchType(pitchType, note, chordCtx);
        }
    }

    outNote = note;

    // Determine accent
    bool hasAccent = IsStepActiveVar(pattern.accents, rhythmStep, pattern.length);
    outVelocity = hasAccent ? 120 : 70;

    // Determine gate length
    bool hasHold = IsStepActiveVar(pattern.holds, rhythmStep, pattern.length);
    outGateLength = hasHold ? 3 : 1;

    return true;
}

void RandomizeBassPattern(
    BassState& state,
    const BassConfig& config,
    uint32_t seed)
{
    // Note: freeze check is the caller's responsibility
    state.currentPattern = seed % NUM_BASS_PATTERNS;

    // Pick B pattern with >= density than A
    const BassPattern& patA = bassPatterns[state.currentPattern];
    uint8_t densityA = CountTriggersVar(patA.triggers, patA.length);
    uint8_t candidates[NUM_BASS_PATTERNS];
    uint8_t numCandidates = 0;

    for (uint8_t i = 0; i < NUM_BASS_PATTERNS; i++) {
        if (i != state.currentPattern &&
            CountTriggersVar(bassPatterns[i].triggers, bassPatterns[i].length) >= densityA) {
            candidates[numCandidates++] = i;
        }
    }

    if (numCandidates > 0) {
        state.currentPatternB = candidates[(seed >> 8) % numCandidates];
    } else {
        state.currentPatternB = state.currentPattern;
    }

    // Randomize pitch patterns (A and B)
    state.currentPitchPattern = (seed >> 12) % NUM_BASS_PITCH_PATTERNS;
    uint8_t pitchB = (seed >> 16) % NUM_BASS_PITCH_PATTERNS;
    if (pitchB == state.currentPitchPattern) {
        pitchB = (pitchB + 1) % NUM_BASS_PITCH_PATTERNS;
    }
    state.currentPitchPatternB = pitchB;
}

} // namespace themis
