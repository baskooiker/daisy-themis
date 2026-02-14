/**
 * @file types.h
 * @brief Core type definitions for Themis - Master Clock & Generative Drum Sequencer
 *
 * This file contains all enums, structs, and type definitions used throughout
 * the Themis firmware. Include this file in any module that needs access to
 * the shared type system.
 *
 * @note All pattern data is stored with bit 31 = step 0 (MSB-first)
 * @note MIDI channels are 0-indexed (channel 1 = 0)
 */

#ifndef THEMIS_TYPES_H
#define THEMIS_TYPES_H

#include <cstdint>

// ============================================================================
// CONSTANTS
// ============================================================================

/** @defgroup timing_constants Timing Constants */
/** @{ */
constexpr uint8_t MIDI_CLOCK_PPQN = 24;      ///< Pulses per quarter note
constexpr uint8_t CLOCKS_PER_16TH = 6;       ///< MIDI clocks per 16th note
constexpr uint8_t CLOCKS_PER_QUARTER = 24;   ///< MIDI clocks per quarter note
constexpr uint32_t GATE_PULSE_SAMPLES = 480; ///< Gate pulse width (10ms @ 48kHz)
constexpr uint32_t RESET_PULSE_SAMPLES = 960;///< Reset pulse width (20ms @ 48kHz)
constexpr uint32_t GATE_PULSE_MS = 10;       ///< Gate pulse width in milliseconds
/** @} */

/** @defgroup queue_sizes Queue Sizes */
/** @{ */
constexpr uint8_t TRIGGER_QUEUE_SIZE = 32;   ///< Max queued drum triggers
constexpr uint8_t MELODY_QUEUE_SIZE = 16;    ///< Max queued melody triggers
/** @} */

/** @defgroup midi_config MIDI Configuration */
/** @{ */
constexpr uint8_t DRM1_MIDI_CHANNEL = 9;     ///< Drums on channel 10 (0-indexed)
/** @} */

/** @defgroup ui_config UI Configuration */
/** @{ */
constexpr uint32_t MENU_TIMEOUT_MS = 10000;  ///< Menu auto-close timeout (10s)
/** @} */

/** @defgroup storage_config Storage Configuration */
/** @{ */
constexpr uint32_t SETTINGS_MAGIC = 0x54484D53; ///< "THMS" validation magic
/** @} */

// ============================================================================
// DRUM SYSTEM ENUMS
// ============================================================================

/**
 * @enum DrumVoice
 * @brief Identifiers for each drum voice
 *
 * Maps to MIDI notes via drumNotes[] array.
 * ANALOG is the CV/Gate output voice, not sent over MIDI.
 */
enum DrumVoice
{
    KICK = 0,           ///< Kick drum (MIDI note 36)
    DRUM1,              ///< Tom 1 (MIDI note 48)
    DRUM2,              ///< Tom 2 (MIDI note 41)
    MULTI,              ///< Multi/perc (MIDI note 58)
    SNARE,              ///< Snare (MIDI note 40)
    HIHAT1_CLOSED,      ///< Hi-hat 1 closed (MIDI note 49)
    HIHAT1_OPEN,        ///< Hi-hat 1 open (MIDI note 51)
    HIHAT2_CLOSED,      ///< Hi-hat 2 closed (MIDI note 42)
    HIHAT2_OPEN,        ///< Hi-hat 2 open (MIDI note 44)
    CLAP,               ///< Clap (MIDI note 39)
    ANALOG,             ///< Analog CV/Gate output (not MIDI)
    NUM_DRUM_VOICES
};

/**
 * @enum RhythmStyle
 * @brief Pattern generation algorithms for drum voices
 */
enum RhythmStyle
{
    RHYTHM_SYNCOPATED,      ///< Off-beat emphasis
    RHYTHM_STRAIGHT,        ///< On-beat emphasis
    RHYTHM_EUCLIDEAN,       ///< Evenly distributed hits
    RHYTHM_ANTI_EUCLIDEAN,  ///< Clustered/grouped hits
    RHYTHM_FOLLOW_KICK,     ///< Mirrors kick drum pattern
    NUM_RHYTHM_STYLES
};

/**
 * @enum DensityLevel
 * @brief Pattern density (number of hits per bar)
 */
enum DensityLevel
{
    DENSITY_LOW,            ///< Sparse (few hits)
    DENSITY_MEDIUM,         ///< Moderate density
    DENSITY_HIGH,           ///< Dense (many hits)
    NUM_DENSITY_LEVELS
};

/**
 * @enum InteractionStyle
 * @brief How two voices interact rhythmically
 */
enum InteractionStyle
{
    INTERACTION_NONE,           ///< Independent patterns
    INTERACTION_DIVIDED,        ///< Hits split between voices
    INTERACTION_ALTERNATE_BAR,  ///< Switch every bar
    INTERACTION_ALTERNATE_HALF, ///< Switch every half bar
    INTERACTION_ALTERNATE_TWO,  ///< Switch every 2 bars
    NUM_INTERACTION_STYLES
};

// ============================================================================
// MELODY SYSTEM ENUMS
// ============================================================================

/**
 * @enum ScaleType
 * @brief Available musical scales
 *
 * Scale intervals are defined in melody.cpp (scaleMinor, scaleMinorBlues, etc.)
 */
enum ScaleType
{
    SCALE_MINOR,            ///< Natural minor: 0,2,3,5,7,8,10
    SCALE_MINOR_BLUES,      ///< Minor blues: 0,3,5,6,7,10
    SCALE_MINOR_PENTATONIC, ///< Minor pentatonic: 0,3,5,7,10
    SCALE_GYPSY,            ///< Hungarian gypsy: 0,2,3,6,7,8,11
    NUM_SCALE_TYPES
};

/**
 * @enum MelodyStyle
 * @brief Main melody generation modes
 */
enum MelodyStyle
{
    MELODY_SUPPORTING,      ///< Sparse, rhythmically supportive
    MELODY_ARPEGGIATOR,     ///< Dense, cyclic patterns
    NUM_MELODY_STYLES
};

/**
 * @enum SupportingSubStyle
 * @brief Sub-modes for MELODY_SUPPORTING style
 */
enum SupportingSubStyle
{
    SUPPORT_FOLLOW_KICK,    ///< Trigger on kick drum hits
    SUPPORT_OWN_SPARSE,     ///< Independent sparse pattern
    SUPPORT_SUBSET_KICK,    ///< Subset of kick hits
    NUM_SUPPORTING_SUBSTYLES
};

/**
 * @enum ArpSubStyle
 * @brief Sub-modes for MELODY_ARPEGGIATOR style
 */
enum ArpSubStyle
{
    ARP_CHORD_TONES,        ///< Root, 3rd, 5th, 7th cycle
    ARP_SCALE_ASCENDING,    ///< Ascending scale degrees
    ARP_SCALE_RANDOM,       ///< Random scale notes
    NUM_ARP_SUBSTYLES
};

// ============================================================================
// VARIATION SYSTEM ENUMS
// ============================================================================

/**
 * @enum VariationMode
 * @brief Pattern variation modes (AB or ABC patterns)
 */
enum VariationMode
{
    VAR_MODE_OFF,           ///< No variation (single pattern)
    VAR_MODE_AB,            ///< Two variations (A and B)
    VAR_MODE_ABC,           ///< Three variations (A, B, and C)
    NUM_VARIATION_MODES
};

/**
 * @enum VariationGranularity
 * @brief How often variation switches occur
 */
enum VariationGranularity
{
    VAR_GRAN_BAR,           ///< Switch every bar (16 steps)
    VAR_GRAN_HALF_BAR,      ///< Switch every half bar (8 steps)
    VAR_GRAN_QUARTER,       ///< Switch every quarter bar (4 steps)
    NUM_VARIATION_GRANULARITIES
};

/**
 * @enum VariationSequence
 * @brief Preset sequences for variation playback order
 */
enum VariationSequence
{
    VAR_SEQ_AAAA,           ///< All A (effectively no variation)
    VAR_SEQ_AAAB,           ///< Three A, one B
    VAR_SEQ_AABB,           ///< Two A, two B
    VAR_SEQ_ABAB,           ///< Alternating A and B
    VAR_SEQ_ABAC,           ///< A, B, A, C (requires ABC mode)
    VAR_SEQ_AAABAAAC,       ///< Three A, B, three A, C (8-segment)
    NUM_VARIATION_SEQUENCES
};

/**
 * @enum MelodyVoiceType
 * @brief Output destination for melody triggers
 */
enum MelodyVoiceType
{
    MELODY_CV,              ///< Output to DAC (CV1) + Gate
    MELODY_MIDI             ///< Output to MIDI
};

// ============================================================================
// POLY VOICE (CHORDS) ENUMS
// ============================================================================

/**
 * @enum ChordType
 * @brief Available chord qualities
 */
enum ChordType
{
    CHORD_MAJOR,            ///< Major triad: 0, 4, 7
    CHORD_MINOR,            ///< Minor triad: 0, 3, 7
    CHORD_DIM,              ///< Diminished: 0, 3, 6
    CHORD_AUG,              ///< Augmented: 0, 4, 8
    CHORD_SUS2,             ///< Suspended 2nd: 0, 2, 7
    CHORD_SUS4,             ///< Suspended 4th: 0, 5, 7
    CHORD_MAJ7,             ///< Major 7th: 0, 4, 7, 11
    CHORD_MIN7,             ///< Minor 7th: 0, 3, 7, 10
    CHORD_DOM7,             ///< Dominant 7th: 0, 4, 7, 10
    CHORD_DIM7,             ///< Diminished 7th: 0, 3, 6, 9
    CHORD_MIN7B5,           ///< Half-diminished: 0, 3, 6, 10
    CHORD_ADD9,             ///< Major add 9: 0, 4, 7, 14
    CHORD_MADD9,            ///< Minor add 9: 0, 3, 7, 14
    NUM_CHORD_TYPES
};

/**
 * @enum ChordRate
 * @brief How often chord changes occur
 */
enum ChordRate
{
    CHORD_RATE_2_BARS,      ///< Change every 64 steps (2 bars)
    CHORD_RATE_1_BAR,       ///< Change every 32 steps (1 bar)
    CHORD_RATE_HALF_BAR,    ///< Change every 16 steps (half bar)
    CHORD_RATE_QUARTER,     ///< Change every 8 steps (quarter bar)
    NUM_CHORD_RATES
};

constexpr uint8_t NUM_PROGRESSIONS = 16;  ///< Number of built-in progressions

// ============================================================================
// UI SYSTEM ENUMS
// ============================================================================

/**
 * @enum DisplayState
 * @brief Current display/menu state
 */
enum DisplayState
{
    DISPLAY_DEFAULT,        ///< Main running display
    DISPLAY_CONFIG_MENU,    ///< Configuration menu
    DISPLAY_CONFIG_EDIT,    ///< Editing a config value
    DISPLAY_PATTERN_INFO,   ///< Pattern visualization
    DISPLAY_FREEZE_MENU,    ///< Freeze submenu
    DISPLAY_SYSTEM_MENU,    ///< System settings submenu
    DISPLAY_SYSTEM_EDIT,    ///< Editing a system setting value
    DISPLAY_HARMONY_MENU,   ///< Harmony submenu
    DISPLAY_HARMONY_EDIT,   ///< Editing a harmony setting value
    DISPLAY_VOICES_MENU,    ///< Voices submenu
    DISPLAY_VOICE_DETAIL,   ///< Individual voice detail view
    DISPLAY_VOICE_EDIT      ///< Editing a voice detail value
};

/**
 * @enum ConfigOption
 * @brief Available configuration menu items
 *
 * Order determines menu display order.
 */
enum ConfigOption
{
    CONFIG_BPM,             ///< Tempo (20-300 BPM)
    CONFIG_TUNE_MODE,       ///< VCO tuning mode
    CONFIG_HARMONY_MENU,    ///< Opens harmony submenu
    CONFIG_VOICES_MENU,     ///< Opens voices submenu
    CONFIG_RANDOMIZE_ALL,   ///< Randomize all parameters
    CONFIG_PATTERN_INFO,    ///< Show pattern info
    CONFIG_FREEZE_MENU,     ///< Opens freeze submenu
    CONFIG_SYSTEM_MENU,     ///< Opens system settings submenu
    CONFIG_BACK,            ///< Exit menu
    NUM_CONFIG_OPTIONS
};

/**
 * @enum HarmonyOption
 * @brief Harmony submenu items
 */
enum HarmonyOption
{
    HARMONY_SCALE,          ///< Shared melody scale
    HARMONY_ROOT,           ///< Shared melody root note
    HARMONY_PROGRESSION,    ///< Poly voice progression
    HARMONY_RATE,           ///< Poly voice chord rate
    HARMONY_BACK,
    NUM_HARMONY_OPTIONS
};

/**
 * @enum VoiceMenuItem
 * @brief Voices submenu items (each opens a detail view)
 */
enum VoiceMenuItem
{
    VOICE_MELODY,           ///< Melody voice settings (CV + MIDI)
    VOICE_BASS,             ///< Bass voice settings
    VOICE_RHYTHM,           ///< Rhythm player settings
    VOICE_BACK,
    NUM_VOICE_MENU_ITEMS
};

/**
 * @enum VoiceDetailItem
 * @brief Items within a voice detail view
 */
enum VoiceDetailItem
{
    VDETAIL_ACTIVE,         ///< Voice on/off toggle
    VDETAIL_STYLE,          ///< Style (melody) or Mode (rhythm)
    VDETAIL_OCTAVE,         ///< Octave offset (bass/rhythm)
    VDETAIL_BACK,
    NUM_VOICE_DETAIL_ITEMS
};

/**
 * @enum FreezeOption
 * @brief Freeze submenu items
 */
enum FreezeOption
{
    FREEZE_ALL,             ///< Toggle all freezes at once
    FREEZE_DRUMS,           ///< freezeEnabled
    FREEZE_MELODY,          ///< melodyFreezeEnabled
    FREEZE_BASS,            ///< bassVoiceConfig.freezePattern
    FREEZE_RHYTHM,          ///< rhythmPlayerConfig.freezeStyle
    FREEZE_CHORDS,          ///< chordRandomizerConfig.freezeEnabled
    FREEZE_BACK,
    NUM_FREEZE_OPTIONS
};

/**
 * @enum SystemOption
 * @brief System settings submenu items
 */
enum SystemOption
{
    SYSTEM_MEL_MIDI_CH,     ///< melodyMidiChannel
    SYSTEM_DRUM_MIDI_CH,    ///< drumMidiChannel
    SYSTEM_BASS_MIDI_CH,    ///< bassVoiceConfig.midiChannel
    SYSTEM_RHYTHM_MIDI_CH,  ///< rhythmPlayerConfig.midiChannel
    SYSTEM_BACK,
    NUM_SYSTEM_OPTIONS
};

/**
 * @enum OutDivision
 * @brief Clock division options for outputs
 */
enum OutDivision
{
    DIV_1_16,               ///< 1/16 note (16th)
    DIV_1_8,                ///< 1/8 note (8th)
    DIV_1_4,                ///< 1/4 note (quarter)
    DIV_1_2,                ///< 1/2 note (half)
    DIV_1,                  ///< 1 bar (whole)
    DIV_2,                  ///< 2 bars
    DIV_4,                  ///< 4 bars
    NUM_OUT_DIVISIONS
};

// ============================================================================
// STRUCTS
// ============================================================================

/**
 * @struct GrooveConfig
 * @brief Per-voice groove timing configuration
 */
struct GrooveConfig
{
    float groovePercent;        ///< Offset as % of 16th note (-100 to +100)
    int32_t offsetSamples;      ///< Calculated sample offset
    static const int32_t MAX_OFFSET_MS = 10; ///< Maximum offset in ms

    void Init()
    {
        groovePercent = 0.0f;
        offsetSamples = 0;
    }

    /**
     * @brief Calculate sample offset from percentage and tempo
     * @param currentBPM Current tempo in BPM
     * @param sampleRate Audio sample rate
     */
    void UpdateOffset(float currentBPM, float sampleRate);
};

/**
 * @struct MidiTrigger
 * @brief Queue entry for scheduled drum MIDI triggers
 */
struct MidiTrigger
{
    DrumVoice voice;            ///< Which drum voice
    uint8_t velocity;           ///< MIDI velocity (1-127)
    uint64_t fireSample;        ///< When to fire (global sample count)
    bool active;                ///< Is this entry valid

    void Init()
    {
        active = false;
        fireSample = 0;
        voice = KICK;
        velocity = 0;
    }
};

/**
 * @struct MelodyTrigger
 * @brief Queue entry for scheduled melody triggers
 */
struct MelodyTrigger
{
    MelodyVoiceType voiceType;  ///< CV or MIDI output
    int8_t note;                ///< Semitone from C2
    uint64_t fireSample;        ///< When to fire
    bool active;                ///< Is this entry valid

    void Init()
    {
        active = false;
        fireSample = 0;
        note = 0;
        voiceType = MELODY_CV;
    }
};

/**
 * @struct VariationConfig
 * @brief Configuration for AB/ABC pattern variations
 */
struct VariationConfig
{
    VariationMode mode;         ///< Off, AB, or ABC mode
    VariationSequence sequence; ///< Which sequence pattern to use
    VariationGranularity granularity; ///< How often to switch variations
    RhythmStyle styleB;         ///< Rhythm style for B variation
    RhythmStyle styleC;         ///< Rhythm style for C variation
    DensityLevel densityB;      ///< Density for B variation
    DensityLevel densityC;      ///< Density for C variation
};

/**
 * @struct VoiceConfig
 * @brief Configuration for a generative drum voice
 */
struct VoiceConfig
{
    DrumVoice voice;            ///< Which drum voice this configures
    RhythmStyle rhythmStyle;    ///< Pattern generation algorithm
    DensityLevel density;       ///< How dense the pattern is
    InteractionStyle interaction; ///< How it interacts with partner
    DrumVoice interactionPartner; ///< Partner voice for interaction
    uint32_t pattern;           ///< Generated bit pattern (A variation)
    uint32_t patternB;          ///< B variation pattern
    uint32_t patternC;          ///< C variation pattern
    uint8_t patternLength;      ///< Steps in pattern (8-32)
    bool active;                ///< Is voice enabled
    VariationConfig variation;  ///< AB/ABC variation settings
};

/**
 * @struct MelodyConfig
 * @brief Configuration for a melody voice (CV or MIDI)
 */
struct MelodyConfig
{
    MelodyStyle style;          ///< Supporting or Arpeggiator
    uint8_t subStyle;           ///< Sub-style within main style
    RhythmStyle rhythmStyle;    ///< Pattern generation algorithm (same as drums)
    DensityLevel density;       ///< Pattern density (same as drums)
    uint32_t rhythmPattern;     ///< When notes trigger (bit pattern, A variation)
    uint32_t rhythmPatternB;    ///< B variation rhythm pattern
    uint32_t rhythmPatternC;    ///< C variation rhythm pattern
    uint8_t patternLength;      ///< Pattern length (typically 32)
    int8_t noteSequence[32];    ///< Pre-generated notes (semitones from C2, A variation)
    int8_t noteSequenceB[32];   ///< B variation note sequence
    int8_t noteSequenceC[32];   ///< C variation note sequence
    uint8_t sequencePos;        ///< Current position in sequence
    uint8_t currentOctave;      ///< Current octave offset (0-2)
    bool active;                ///< Is voice enabled
    VariationConfig variation;  ///< AB/ABC variation settings
};

/**
 * @struct PersistentSettings
 * @brief Settings saved to QSPI flash
 *
 * @note Total size should be kept to 32 bytes for alignment
 * @note Magic number validates stored data integrity
 */
struct PersistentSettings
{
    uint32_t magic;             ///< SETTINGS_MAGIC for validation
    float bpm;                  ///< Tempo (20-300)
    uint8_t freezeEnabled;      ///< Drum freeze state
    uint8_t melodyScale;        ///< ScaleType enum value
    uint8_t melodyRoot;         ///< Root note (0-11)
    uint8_t cvMelodyStyle;      ///< MelodyStyle for CV voice
    uint8_t midiMelodyStyle;    ///< MelodyStyle for MIDI voice
    uint8_t midiMelChannel;     ///< MIDI channel (0-15)
    uint8_t melodyFreezeEnabled;///< Melody freeze state
    uint8_t polyActive;         ///< Poly voice enabled
    uint8_t polyProgression;    ///< Poly voice progression index
    uint8_t polyRate;           ///< Poly voice chord rate
    int8_t polyOctave;          ///< Poly voice octave offset
    uint8_t polyMidiChannel;    ///< Poly voice MIDI channel
    uint8_t drumMidiChannel;    ///< Drum MIDI channel (default 9)
    uint8_t bassMidiChannel;    ///< Bass MIDI channel (default 4)
    uint8_t rhythmMidiChannel;  ///< Rhythm MIDI channel (default 3)
    uint8_t bassFreezeEnabled;  ///< Bass freeze state
    uint8_t rhythmFreezeEnabled;///< Rhythm freeze state
    uint8_t chordFreezeEnabled; ///< Chord freeze state
    uint8_t bassOctave;         ///< Bass voice octave offset (stored as int8_t)
    uint8_t rhythmOctave;       ///< Rhythm player octave offset (stored as int8_t)
    uint8_t rhythmMode;         ///< Rhythm player mode (0=Manual, 1=Morph)
    uint8_t voiceActiveBits;    ///< Bitmask: bit0=cvMel, bit1=midiMel, bit2=poly, bit3=bass, bit4=rhythm
    uint8_t reserved[2];        ///< Padding to 32 bytes
};

/**
 * @struct TuringMachine
 * @brief Turing machine-style generative CV sequencer
 *
 * Uses a shift register with probability-based bit mutation
 * to create slowly-evolving melodic patterns.
 */
struct TuringMachine
{
    uint32_t shiftRegister;     ///< 32-bit pattern storage
    uint8_t currentLength;      ///< Active sequence length
    uint8_t targetLength;       ///< Target for morphing
    float lengthMorph;          ///< Morph progress (0-1)
    uint8_t currentPos;         ///< Position in sequence
    float randomProbability;    ///< Bit mutation chance (0-1)
    float targetProbability;    ///< Target for probability drift
    float cvOutput;             ///< Current CV value (0-1)
    uint32_t morphCounter;      ///< Steps since last morph
    uint32_t morphInterval;     ///< Steps between morphs

    void Init(uint32_t seed);
    void Process();
};

// ============================================================================
// POLY VOICE (CHORDS) STRUCTS
// ============================================================================

/**
 * @struct ChordShape
 * @brief Defines intervals in a chord
 */
struct ChordShape
{
    int8_t intervals[6];        ///< Intervals from root, -128 = unused
    uint8_t numNotes;           ///< Number of notes in chord
};

/**
 * @struct ProgressionStep
 * @brief Single chord in a progression
 */
struct ProgressionStep
{
    int8_t rootOffset;          ///< Root note offset (semitones or scale degree)
    ChordType chordType;        ///< Chord quality
};

/**
 * @struct ChordProgression
 * @brief A sequence of chords
 */
struct ChordProgression
{
    const char* name;           ///< Display name
    ProgressionStep steps[8];   ///< Up to 8 chords
    uint8_t numChords;          ///< Number of chords in progression
    bool diatonic;              ///< True = scale degrees, false = chromatic
};

/**
 * @struct PolyVoiceConfig
 * @brief Configuration for poly voice (chords/pads)
 */
struct PolyVoiceConfig
{
    bool active;                ///< Is poly voice enabled
    uint8_t progressionIndex;   ///< Current progression (0-15)
    uint8_t chordRate;          ///< ChordRate enum value
    uint8_t velocity;           ///< MIDI velocity (0-127)
    int8_t octaveOffset;        ///< Octave shift (-2 to +2)
    uint8_t midiChannel;        ///< MIDI channel (0-15)
};

/**
 * @struct PolyVoiceState
 * @brief Runtime state for poly voice
 */
struct PolyVoiceState
{
    uint8_t currentChordIndex;  ///< Current chord in progression (0-7)
    uint8_t stepsUntilChange;   ///< Steps until next chord
    int8_t activeNotes[6];      ///< Currently held MIDI notes
    uint8_t numActiveNotes;     ///< Number of active notes
    bool notesOn;               ///< Are notes currently held

    void Init()
    {
        currentChordIndex = 0;
        stepsUntilChange = 0;
        numActiveNotes = 0;
        notesOn = false;
        for(int i = 0; i < 6; i++) activeNotes[i] = 0;
    }
};

// ============================================================================
// HELPER MACROS
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

#endif // THEMIS_TYPES_H
