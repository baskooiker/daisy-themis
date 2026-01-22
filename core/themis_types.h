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

    void Init()
    {
        currentChordIndex = 0;
        stepsUntilChange = 0;
        numActiveNotes = 0;
        notesOn = false;
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

} // namespace themis

#endif // THEMIS_CORE_TYPES_H
