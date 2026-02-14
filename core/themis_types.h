/**
 * @file themis_types.h
 * @brief Core type definitions for Themis shared library
 *
 * This file contains platform-independent type definitions that are shared
 * between the firmware and desktop application.
 */

#ifndef THEMIS_CORE_TYPES_H
#define THEMIS_CORE_TYPES_H

#include <cstdint>

namespace themis {

// ============================================================================
// CONSTANTS
// ============================================================================

constexpr uint8_t MIDI_CLOCK_PPQN = 24;
constexpr uint8_t CLOCKS_PER_16TH = 6;
constexpr uint8_t CLOCKS_PER_QUARTER = 24;
constexpr uint32_t GATE_PULSE_MS = 10;

constexpr uint8_t TRIGGER_QUEUE_SIZE = 32;
constexpr uint8_t MELODY_QUEUE_SIZE = 16;

constexpr uint8_t DRM1_MIDI_CHANNEL = 9;  // Drums on channel 10 (0-indexed)

constexpr uint32_t SETTINGS_MAGIC = 0x54484D53;  // "THMS"

// ============================================================================
// DRUM SYSTEM ENUMS
// ============================================================================

enum DrumVoice
{
    KICK = 0,
    DRUM1,
    DRUM2,
    MULTI,
    SNARE,
    HIHAT1_CLOSED,
    HIHAT1_OPEN,
    HIHAT2_CLOSED,
    HIHAT2_OPEN,
    CLAP,
    ANALOG,
    NUM_DRUM_VOICES
};

enum RhythmStyle
{
    RHYTHM_SYNCOPATED,
    RHYTHM_STRAIGHT,
    RHYTHM_EUCLIDEAN,
    RHYTHM_ANTI_EUCLIDEAN,
    RHYTHM_FOLLOW_KICK,
    NUM_RHYTHM_STYLES
};

enum DensityLevel
{
    DENSITY_LOW,
    DENSITY_MEDIUM,
    DENSITY_HIGH,
    NUM_DENSITY_LEVELS
};

enum InteractionStyle
{
    INTERACTION_NONE,
    INTERACTION_DIVIDED,
    INTERACTION_ALTERNATE_BAR,
    INTERACTION_ALTERNATE_HALF,
    INTERACTION_ALTERNATE_TWO,
    NUM_INTERACTION_STYLES
};

// ============================================================================
// MELODY SYSTEM ENUMS
// ============================================================================

enum ScaleType
{
    SCALE_MINOR,
    SCALE_MINOR_BLUES,
    SCALE_MINOR_PENTATONIC,
    SCALE_GYPSY,
    SCALE_WHOLE_HALF,       // Whole-half diminished (octatonic)
    SCALE_HALF_WHOLE,       // Half-whole diminished (octatonic)
    SCALE_WHOLE_TONE,       // Whole tone scale
    NUM_SCALE_TYPES
};

enum MelodyStyle
{
    MELODY_SUPPORTING,
    MELODY_ARPEGGIATOR,
    NUM_MELODY_STYLES
};

enum SupportingSubStyle
{
    SUPPORT_FOLLOW_KICK,
    SUPPORT_OWN_SPARSE,
    SUPPORT_SUBSET_KICK,
    NUM_SUPPORTING_SUBSTYLES
};

enum ArpSubStyle
{
    ARP_CHORD_TONES,
    ARP_SCALE_ASCENDING,
    ARP_SCALE_RANDOM,
    NUM_ARP_SUBSTYLES
};

/**
 * @brief Melody compatibility mode for chord-aware mapping
 *
 * Controls how melody notes are quantized to harmonize with current chord.
 * Applied at trigger time when chord voice is active.
 */
enum MelodyCompatMode
{
    COMPAT_CHORD_TONES,      ///< Strict: root, 3rd, 5th, 7th of current chord (safest)
    COMPAT_CHORD_PENTATONIC, ///< Pentatonic built on chord root (balanced)
    COMPAT_CHORD_SCALE,      ///< Full scale from chord root (most freedom)
    NUM_COMPAT_MODES
};

// ============================================================================
// VARIATION SYSTEM ENUMS
// ============================================================================

enum VariationMode
{
    VAR_MODE_OFF,
    VAR_MODE_AB,
    VAR_MODE_ABC,
    NUM_VARIATION_MODES
};

enum VariationGranularity
{
    VAR_GRAN_BAR,
    VAR_GRAN_HALF_BAR,
    VAR_GRAN_QUARTER,
    NUM_VARIATION_GRANULARITIES
};

enum VariationSequence
{
    VAR_SEQ_AAAA,
    VAR_SEQ_AAAB,
    VAR_SEQ_AABB,
    VAR_SEQ_ABAB,
    VAR_SEQ_ABAC,
    VAR_SEQ_AAABAAAC,
    NUM_VARIATION_SEQUENCES
};

enum MelodyVoiceType
{
    MELODY_CV,
    MELODY_MIDI
};

// ============================================================================
// OUTPUT DIVISION ENUM
// ============================================================================

enum OutDivision
{
    DIV_1_16,
    DIV_1_8,
    DIV_1_4,
    DIV_1_2,
    DIV_1,
    DIV_2,
    DIV_4,
    NUM_OUT_DIVISIONS
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * @brief Context about the current chord for melody mapping
 *
 * Provides information about the currently playing chord so melody
 * notes can be quantized to harmonically compatible notes.
 */
struct ChordContext
{
    int8_t chordRoot;        ///< Absolute semitone from C (0=C, 7=G, etc.)
    uint8_t chordType;       ///< ChordType enum value (CHORD_MAJOR, CHORD_MINOR, etc.)
    bool isDiatonic;         ///< True if chord is diatonic to global scale
};

struct VariationConfig
{
    VariationMode mode = VAR_MODE_OFF;
    VariationSequence sequence = VAR_SEQ_AAAB;
    VariationGranularity granularity = VAR_GRAN_BAR;
    RhythmStyle styleB = RHYTHM_EUCLIDEAN;
    RhythmStyle styleC = RHYTHM_EUCLIDEAN;
    DensityLevel densityB = DENSITY_LOW;
    DensityLevel densityC = DENSITY_MEDIUM;
};

struct VoiceConfig
{
    DrumVoice voice;
    RhythmStyle rhythmStyle;
    DensityLevel density;
    InteractionStyle interaction;
    DrumVoice interactionPartner;
    uint32_t pattern;
    uint32_t patternB;
    uint32_t patternC;
    uint8_t patternLength;
    bool active;
    VariationConfig variation;
};

struct MelodyConfig
{
    MelodyStyle style;
    uint8_t subStyle;
    RhythmStyle rhythmStyle;
    DensityLevel density;
    uint32_t rhythmPattern;
    uint32_t rhythmPatternB;
    uint32_t rhythmPatternC;
    uint8_t patternLength;
    int8_t noteSequence[32];
    int8_t noteSequenceB[32];
    int8_t noteSequenceC[32];
    uint8_t sequencePos;
    uint8_t currentOctave;
    bool active;
    VariationConfig variation;
    MelodyCompatMode compatMode;  ///< How melody notes map to current chord
};

struct ChordVoiceConfig
{
    bool active;

    // Progression selection
    uint8_t progressionIndex;     // Which progression to use (0-15)
    uint8_t chordRate;            // ChordRate enum - how fast chords change

    // Sound shaping
    uint8_t velocity;             // 0-127
    int8_t octaveOffset;          // -2 to +2, shifts all chord notes

    // Variation
    uint8_t progressionB;         // Alternative progression for B variation
    VariationMode variationMode;  // Off, AB, ABC
};

struct ChordVoiceState
{
    uint8_t currentChordIndex;    // Which chord in progression (0-7)
    uint8_t stepsUntilChange;     // Countdown to next chord
    int8_t activeNotes[6];        // Currently sounding MIDI notes (for note-off)
    uint8_t numActiveNotes;       // How many notes are active
    bool notesOn;                 // Are notes currently sounding?
    int8_t pendingProgressionIndex; // Progression to switch to at end of current (-1 = none)

    void Init()
    {
        currentChordIndex = 0;
        stepsUntilChange = 0;
        numActiveNotes = 0;
        notesOn = false;
        pendingProgressionIndex = -1;
        for (int i = 0; i < 6; i++) {
            activeNotes[i] = 0;
        }
    }
};

struct GrooveConfig
{
    float groovePercent;
    int32_t offsetSamples;
    static const int32_t MAX_OFFSET_MS = 10;

    void Init()
    {
        groovePercent = 0.0f;
        offsetSamples = 0;
    }

    void UpdateOffset(float currentBPM, float sampleRate)
    {
        float samplesPerSixteenth = sampleRate * 15.0f / currentBPM;
        float offsetFloat = (groovePercent / 100.0f) * samplesPerSixteenth;
        int32_t maxOffsetSamples = (int32_t)(MAX_OFFSET_MS * sampleRate / 1000.0f);

        // Clamp
        if (offsetFloat < -maxOffsetSamples) offsetFloat = -maxOffsetSamples;
        if (offsetFloat > maxOffsetSamples) offsetFloat = maxOffsetSamples;
        offsetSamples = (int32_t)offsetFloat;
    }
};

struct MidiTrigger
{
    DrumVoice voice;
    uint8_t velocity;
    uint64_t fireSample;
    bool active;

    void Init()
    {
        active = false;
        fireSample = 0;
        voice = KICK;
        velocity = 0;
    }
};

struct MelodyTrigger
{
    MelodyVoiceType voiceType;
    int8_t note;
    uint64_t fireSample;
    bool active;

    void Init()
    {
        active = false;
        fireSample = 0;
        note = 0;
        voiceType = MELODY_CV;
    }
};

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * @brief Check if a step is active in a pattern
 * @param pattern 32-bit pattern (MSB = step 0)
 * @param step Step number (0-31)
 * @return true if step should trigger
 */
inline bool IsStepActive(uint32_t pattern, uint8_t step)
{
    return (pattern >> (31 - step)) & 0x01;
}

/**
 * @brief Check if a step is active in an 8-bit pattern
 * @param pattern 8-bit pattern (MSB = step 0)
 * @param step Step number (0-7)
 * @return true if step should trigger
 */
inline bool IsStepActive8(uint8_t pattern, uint8_t step)
{
    return (pattern >> (7 - step)) & 0x01;
}

/**
 * @brief Check if a step is active in a 16-bit pattern
 * @param pattern 16-bit pattern (MSB = step 0)
 * @param step Step number (0-15)
 * @return true if step should trigger
 */
inline bool IsStepActive16(uint16_t pattern, uint8_t step)
{
    return (pattern >> (15 - step)) & 0x01;
}

// ============================================================================
// RHYTHM PLAYER ENUMS
// ============================================================================

/**
 * @enum RhythmPlayStyle
 * @brief Current playing style for rhythm player
 */
enum RhythmPlayStyle
{
    RHYTHM_PLAY_CHORDS,         ///< Full chord stabs/pads
    RHYTHM_PLAY_POLYRHYTHM,     ///< Polyrhythmic moving patterns
    NUM_RHYTHM_PLAY_STYLES
};

/**
 * @enum RhythmPlayerMode
 * @brief Operating mode for rhythm player
 */
enum RhythmPlayerMode
{
    RHYTHM_MODE_MANUAL,         ///< Style controlled via config
    RHYTHM_MODE_MORPH,          ///< Auto-morph between styles
    NUM_RHYTHM_PLAYER_MODES
};

/**
 * @enum RhythmActivity
 * @brief Activity level / density
 */
enum RhythmActivity
{
    ACTIVITY_SPARSE,            ///< Few hits, lots of space
    ACTIVITY_MODERATE,          ///< Balanced activity
    ACTIVITY_BUSY,              ///< Dense, active playing
    NUM_RHYTHM_ACTIVITY_LEVELS
};

/**
 * @enum RhythmArticulation
 * @brief Note length / attack style
 */
enum RhythmArticulation
{
    ARTICULATION_STACCATO,      ///< Short, punchy
    ARTICULATION_NORMAL,        ///< Medium length
    ARTICULATION_LEGATO,        ///< Long, sustained
    NUM_RHYTHM_ARTICULATIONS
};

/**
 * @enum InversionVariation
 * @brief Controls how chord inversions are weighted
 */
enum InversionVariation
{
    INV_VAR_LOW,                ///< Mostly root position (70% root, 20% 1st, 10% 2nd)
    INV_VAR_HIGH,               ///< Mostly inverted (20% root, 40% 1st, 40% 2nd)
    NUM_INV_VARIATIONS
};

// ============================================================================
// RHYTHM PLAYER STRUCTS
// ============================================================================

/**
 * @struct RhythmPlayerConfig
 * @brief Configuration for rhythm player voice
 */
struct RhythmPlayerConfig
{
    bool active;                    ///< Is rhythm player enabled
    RhythmPlayerMode mode;          ///< Manual or Morph mode
    uint8_t midiChannel;            ///< MIDI output channel (0-15)
    int8_t octaveOffset;            ///< Base octave offset (-2 to +2)

    // Manual mode parameters (auto-change in morph mode)
    RhythmPlayStyle playStyle;      ///< Current playing style
    RhythmActivity activity;        ///< Note density
    RhythmArticulation articulation; ///< Note length tendency
    InversionVariation inversionVariation; ///< Chord inversion weighting
    bool followKick;                ///< Lock rhythm to kick pattern
    bool freezeStyle;               ///< Prevent automatic style changes

    void Init()
    {
        active = true;              // Active by default
        mode = RHYTHM_MODE_MORPH;
        midiChannel = 3;            // Default to channel 4 (0-indexed)
        octaveOffset = 0;
        playStyle = RHYTHM_PLAY_CHORDS;
        activity = ACTIVITY_MODERATE;
        articulation = ARTICULATION_NORMAL;
        inversionVariation = INV_VAR_LOW;
        followKick = false;
        freezeStyle = false;
    }
};

/**
 * @struct RhythmPlayerState
 * @brief Runtime state for rhythm player
 */
struct RhythmPlayerState
{
    // Style morphing
    RhythmPlayStyle currentStyle;   ///< Active playing style
    RhythmPlayStyle targetStyle;    ///< Style morphing towards
    float styleMorphProgress;       ///< 0.0-1.0 transition progress
    uint8_t morphTimer;             ///< Steps until next style change

    // Pattern position
    uint8_t patternPosition;        ///< Position in current rhythm pattern
    uint8_t barPosition;            ///< Position within bar (0-15)

    // Polyrhythm state
    uint8_t polyCounter1;           ///< Counter for polyrhythm layer 1
    uint8_t polyCounter2;           ///< Counter for polyrhythm layer 2
    uint8_t polyLength1;            ///< Length of poly layer 1 (3, 5, 7)
    uint8_t polyLength2;            ///< Length of poly layer 2 (4, 6, 8)

    // Active notes tracking
    int8_t activeNotes[6];          ///< Currently sounding MIDI notes
    uint8_t numActiveNotes;         ///< How many notes are active
    uint8_t noteDuration;           ///< Steps until note-off

    // Energy/intensity (affects velocity, density)
    float intensity;                ///< Current intensity (0.0-1.0)
    float intensityTarget;          ///< Target intensity

    // Randomization
    uint8_t randomizeTimer;         ///< Steps until parameter randomization

    // Chord inversion state
    uint8_t currentInversion;       ///< 0=root, 1=first, 2=second
    InversionVariation inversionVariation; ///< How inversions are weighted

    void Init()
    {
        currentStyle = RHYTHM_PLAY_CHORDS;
        targetStyle = RHYTHM_PLAY_CHORDS;
        styleMorphProgress = 1.0f;
        morphTimer = 64;            // ~2 bars

        patternPosition = 0;
        barPosition = 0;

        polyCounter1 = 0;
        polyCounter2 = 0;
        polyLength1 = 3;
        polyLength2 = 4;

        numActiveNotes = 0;
        noteDuration = 0;
        for (int i = 0; i < 6; i++) {
            activeNotes[i] = -1;
        }

        intensity = 0.5f;
        intensityTarget = 0.5f;

        randomizeTimer = 128;       // ~4 bars

        currentInversion = 0;
        inversionVariation = INV_VAR_LOW;
    }
};

/**
 * @struct RhythmTrigger
 * @brief Queue entry for scheduled rhythm player triggers
 */
struct RhythmTrigger
{
    int8_t notes[6];                ///< MIDI notes (up to 6 for chords)
    uint8_t numNotes;               ///< Number of notes in this trigger
    uint8_t velocity;               ///< MIDI velocity
    uint64_t fireSample;            ///< When to fire
    bool active;                    ///< Is this entry valid
    bool isNoteOff;                 ///< True for note-off events

    void Init()
    {
        for (int i = 0; i < 6; i++) {
            notes[i] = 0;
        }
        numNotes = 0;
        velocity = 0;
        fireSample = 0;
        active = false;
        isNoteOff = false;
    }
};

// ============================================================================
// CHORD VIBE SYSTEM ENUMS
// ============================================================================

/**
 * @enum VibeType
 * @brief Harmonic vibe categories for chord progressions
 */
enum VibeType
{
    VIBE_MINOR,       ///< Natural minor, harmonic minor, aeolian progressions
    VIBE_WHOLE_TONE,  ///< Whole tone scale, augmented chords, ambiguous tonality
    VIBE_MAJOR,       ///< Major scale, ionian, bright progressions
    VIBE_HALF_WHOLE,  ///< Half-whole diminished (dominant diminished)
    VIBE_WHOLE_HALF,  ///< Whole-half diminished
    NUM_VIBE_TYPES
};

/**
 * @enum ProgressionCategory
 * @brief Progression length/purpose category
 */
enum ProgressionCategory
{
    PROG_STEADY,    ///< 1 chord - for transitions
    PROG_CADENCE,   ///< 2 chords - short movements
    PROG_FULL,      ///< 4+ chords - complete progressions
    NUM_PROG_CATEGORIES
};

// ============================================================================
// CHORD VIBE SYSTEM STRUCTS
// ============================================================================

/**
 * @struct ChordRandomizerConfig
 * @brief Configuration for vibe-based chord randomization
 */
struct ChordRandomizerConfig
{
    bool freezeEnabled;              ///< Completely freeze chord changes
    uint8_t enabledVibes;            ///< Bitmask: which vibes can be selected
    uint32_t enabledProgressions[NUM_VIBE_TYPES];  ///< Per-vibe progression enable masks

    void Init()
    {
        freezeEnabled = false;
        enabledVibes = 0x1F;  // All 5 vibes enabled
        for (int i = 0; i < NUM_VIBE_TYPES; i++) {
            enabledProgressions[i] = 0xFFFFFFFF;  // All enabled
        }
    }
};

/**
 * @struct ChordRandomizerState
 * @brief Runtime state for chord randomization
 */
struct ChordRandomizerState
{
    VibeType currentVibe;           ///< Current harmonic vibe
    uint16_t changeTimer;           ///< Progression cycles until considering change
    bool inTransition;              ///< Currently playing transition chord
    int8_t transitionBarsRemaining; ///< Cycles left in transition

    void Init()
    {
        currentVibe = VIBE_MINOR;
        changeTimer = 3;  // 3 progression cycles before first auto-change
        inTransition = false;
        transitionBarsRemaining = 0;
    }
};

// ============================================================================
// BASS PITCH TYPES
// ============================================================================

enum BassPitchType : uint8_t {
    BASS_PITCH_ROOT = 0,      // Chord root
    BASS_PITCH_OCT_UP,        // Root +12
    BASS_PITCH_OCT_DOWN,      // Root -12
    BASS_PITCH_THIRD,         // Chord 3rd (from chordShapes)
    BASS_PITCH_FIFTH,         // Chord 5th
    BASS_PITCH_SEVENTH,       // Chord 7th (fallback to 5th if triad)
    BASS_PITCH_SCALE_2,       // +2 semitones (minor 2nd degree)
    BASS_PITCH_SCALE_4,       // +5 semitones (perfect 4th)
    BASS_PITCH_SCALE_6,       // +8 semitones (minor 6th)
    BASS_PITCH_APPROACH_UP,   // +1 chromatic
    BASS_PITCH_APPROACH_DN,   // -1 chromatic
    NUM_BASS_PITCH_TYPES
};

// ============================================================================
// BASS VOICE STRUCTS
// ============================================================================

/**
 * @struct BassConfig
 * @brief Configuration for bass voice
 */
struct BassConfig
{
    bool active;                    ///< Is bass voice enabled
    uint8_t midiChannel;            ///< MIDI output channel (0-15)
    int8_t octaveOffset;            ///< Base octave offset (-2 to +2)
    uint8_t patternIndex;           ///< Current pattern index (0-15)
    uint8_t pitchPatternIndex;      ///< Current pitch pattern index
    bool freezePattern;             ///< Prevent random pattern selection
    VariationConfig rhythmVariation; ///< AB variation for rhythm patterns
    VariationConfig pitchVariation;  ///< AB variation for pitch patterns (independent)

    void Init()
    {
        active = true;
        midiChannel = 4;            // Default: channel 5
        octaveOffset = -1;          // Bass register
        patternIndex = 0;
        pitchPatternIndex = 0;
        freezePattern = false;
        rhythmVariation.mode = VAR_MODE_OFF;
        rhythmVariation.sequence = VAR_SEQ_AAAB;
        rhythmVariation.granularity = VAR_GRAN_BAR;
        pitchVariation.mode = VAR_MODE_OFF;
        pitchVariation.sequence = VAR_SEQ_AAAB;
        pitchVariation.granularity = VAR_GRAN_BAR;
    }
};

/**
 * @struct BassState
 * @brief Runtime state for bass voice
 */
struct BassState
{
    uint8_t currentPattern;         ///< Active A pattern index
    uint8_t currentPatternB;        ///< Active B pattern index
    uint8_t currentPitchPattern;    ///< Active A pitch pattern index
    uint8_t currentPitchPatternB;   ///< Active B pitch pattern index
    int8_t currentNote;             ///< Currently playing MIDI note (-1 if none)
    uint8_t gateStepsRemaining;     ///< Steps until note-off
    uint8_t chordCyclesUntilChange; ///< Chord cycles remaining before next pattern change

    void Init()
    {
        currentPattern = 0;
        currentPatternB = 0;
        currentPitchPattern = 0;
        currentPitchPatternB = 0;
        currentNote = -1;
        gateStepsRemaining = 0;
        chordCyclesUntilChange = 0;
    }
};

} // namespace themis

#endif // THEMIS_CORE_TYPES_H
