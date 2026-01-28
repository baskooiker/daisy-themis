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
 * Applied at trigger time when poly voice is active.
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
    VariationMode mode;
    VariationSequence sequence;
    VariationGranularity granularity;
    RhythmStyle styleB;
    RhythmStyle styleC;
    DensityLevel densityB;
    DensityLevel densityC;
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

struct PolyVoiceConfig
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

struct PolyVoiceState
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
    RHYTHM_PLAY_ARPEGGIOS,      ///< Arpeggiated chord tones
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
 * @enum ArpDirection
 * @brief Arpeggio direction for arpeggio mode
 */
enum ArpDirection
{
    ARP_UP,                     ///< Ascending
    ARP_DOWN,                   ///< Descending
    ARP_UP_DOWN,                ///< Ping-pong
    ARP_RANDOM,                 ///< Random order
    NUM_ARP_DIRECTIONS
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
    ArpDirection arpDirection;      ///< Arpeggio direction (for arp mode)
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
        arpDirection = ARP_UP;
        followKick = false;
        freezeStyle = false;
    }
};

/**
 * @enum InversionPattern
 * @brief How inversions are selected
 */
enum InversionPattern
{
    INVERSION_CYCLE,            ///< Cycle through root -> 1st -> 2nd -> root
    INVERSION_RANDOM,           ///< Random inversion each chord
    INVERSION_ROOT_ONLY,        ///< Always use root position
    NUM_INVERSION_PATTERNS
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

    // Arpeggio state
    uint8_t arpIndex;               ///< Current arpeggio note index
    int8_t arpDirection;            ///< +1 ascending, -1 descending

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
    InversionPattern inversionPattern; ///< How inversions are selected

    void Init()
    {
        currentStyle = RHYTHM_PLAY_CHORDS;
        targetStyle = RHYTHM_PLAY_CHORDS;
        styleMorphProgress = 1.0f;
        morphTimer = 64;            // ~2 bars

        patternPosition = 0;
        barPosition = 0;

        arpIndex = 0;
        arpDirection = 1;

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
        inversionPattern = INVERSION_CYCLE;
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
        enabledVibes = 0x07;  // All vibes enabled
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
// ACID VOICE ENUMS
// ============================================================================

/**
 * @enum AcidMode
 * @brief Operating mode for acid voice
 */
enum AcidMode
{
    ACID_MODE_MANUAL,           ///< Pattern selection via config
    ACID_MODE_AUTO,             ///< Auto-vary patterns with probability
    NUM_ACID_MODES
};

/**
 * @enum AcidGateLength
 * @brief Gate/note length options
 */
enum AcidGateLength
{
    ACID_GATE_SHORT,            ///< Very short (1/32)
    ACID_GATE_MEDIUM,           ///< Medium (1/16)
    ACID_GATE_LONG,             ///< Long (1/8)
    ACID_GATE_TIE,              ///< Tied to next note (slide)
    NUM_ACID_GATE_LENGTHS
};

/**
 * @enum AcidActivity
 * @brief Overall activity/density level
 */
enum AcidActivity
{
    ACID_ACTIVITY_SPARSE,       ///< Few notes, lots of space
    ACID_ACTIVITY_MODERATE,     ///< Balanced
    ACID_ACTIVITY_BUSY,         ///< Dense, driving pattern
    NUM_ACID_ACTIVITIES
};

// ============================================================================
// ACID VOICE STRUCTS
// ============================================================================

/**
 * @struct AcidStep
 * @brief Single step in an acid pattern
 *
 * Packs trigger, accent, slide, octave shift, and note data.
 */
struct AcidStep
{
    uint8_t trigger : 1;        ///< Note triggers on this step
    uint8_t accent : 1;         ///< Accent (velocity 127 vs 64)
    uint8_t slide : 1;          ///< Slide to next note (overlap MIDI)
    uint8_t hold : 1;           ///< Extended gate length
    int8_t noteOffset : 4;      ///< Scale degree offset (-8 to +7)
};

/**
 * @struct AcidConfig
 * @brief Configuration for acid voice
 */
struct AcidConfig
{
    bool active;                    ///< Is acid voice enabled
    AcidMode mode;                  ///< Manual or Auto mode
    uint8_t midiChannel;            ///< MIDI output channel (0-15)
    int8_t octaveOffset;            ///< Base octave offset (-2 to +2)

    // Pattern selection (manual mode)
    uint8_t rhythmPattern;          ///< Which rhythm pattern preset (0-15)
    uint8_t melodyPattern;          ///< Which melody pattern preset (0-15)

    // Activity level
    AcidActivity activity;          ///< Overall density/activity

    // Probability settings (0-100)
    uint8_t triggerProb;            ///< Probability a step triggers (100 = always)
    uint8_t accentProb;             ///< Probability of random accent
    uint8_t slideProb;              ///< Probability of random slide
    uint8_t octaveUpProb;           ///< Probability of octave up shift
    uint8_t octaveDownProb;         ///< Probability of octave down shift
    uint8_t fillProb;               ///< Probability of end-of-bar fill

    void Init()
    {
        active = true;              // Active by default
        mode = ACID_MODE_AUTO;
        midiChannel = 4;            // Default to channel 5 (0-indexed)
        octaveOffset = 0;           // Middle register by default
        rhythmPattern = 0;
        melodyPattern = 0;
        activity = ACID_ACTIVITY_MODERATE;
        triggerProb = 90;           // 90% trigger probability
        accentProb = 20;            // 20% random accent
        slideProb = 30;             // 30% random slide
        octaveUpProb = 15;          // 15% octave up
        octaveDownProb = 10;        // 10% octave down
        fillProb = 40;              // 40% end-of-bar fill
    }
};

/**
 * @struct AcidState
 * @brief Runtime state for acid voice
 */
struct AcidState
{
    // Pattern position
    uint8_t stepPosition;           ///< Current step (0-15)
    uint8_t barPosition;            ///< Current bar for fill timing

    // Current pattern data (runtime, may be modified by probability)
    uint8_t currentRhythmPattern;   ///< Active rhythm pattern index
    uint8_t currentMelodyPattern;   ///< Active melody pattern index

    // Note tracking for slides
    int8_t currentNote;             ///< Currently playing note (-1 if none)
    int8_t previousNote;            ///< Previous note (for slide reference)
    bool slideActive;               ///< Is a slide in progress
    uint8_t gateStepsRemaining;     ///< Steps until note-off

    // Fill state
    bool inFill;                    ///< Currently playing a fill
    uint8_t fillStepsRemaining;     ///< Steps remaining in fill

    // Auto mode variation
    uint8_t variationTimer;         ///< Steps until pattern variation
    uint8_t lastRandomValue;        ///< For seeded variation

    void Init()
    {
        stepPosition = 0;
        barPosition = 0;
        currentRhythmPattern = 0;
        currentMelodyPattern = 0;
        currentNote = -1;
        previousNote = -1;
        slideActive = false;
        gateStepsRemaining = 0;
        inFill = false;
        fillStepsRemaining = 0;
        variationTimer = 64;        // ~2 bars
        lastRandomValue = 0;
    }
};

/**
 * @struct AcidTrigger
 * @brief Queue entry for scheduled acid triggers
 */
struct AcidTrigger
{
    int8_t note;                    ///< MIDI note number
    uint8_t velocity;               ///< MIDI velocity (64 or 127)
    uint64_t fireSample;            ///< When to fire
    bool active;                    ///< Is this entry valid
    bool isNoteOff;                 ///< True for note-off events
    bool isSlideNote;               ///< True if this overlaps for slide

    void Init()
    {
        note = 0;
        velocity = 64;
        fireSample = 0;
        active = false;
        isNoteOff = false;
        isSlideNote = false;
    }
};

} // namespace themis

#endif // THEMIS_CORE_TYPES_H
