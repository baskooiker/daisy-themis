/**
 * @file themis_sequencer.h
 * @brief Sequencer state machine for Themis
 */

#ifndef THEMIS_SEQUENCER_H
#define THEMIS_SEQUENCER_H

#include "themis_types.h"
#include <functional>

namespace themis {

/**
 * @class Sequencer
 * @brief Main sequencer state machine
 *
 * Manages all drum voices, melody voices, and timing.
 * Uses callbacks for trigger output (platform-independent).
 */
class Sequencer {
public:
    // ========================================================================
    // State
    // ========================================================================

    // Drum voices
    VoiceConfig generativeVoices[6];
    DrumVoice fundamentalBeatVoice = SNARE;

    // Melody voice
    MelodyConfig melodyVoice;
    ScaleType melodyScale = SCALE_MINOR;
    uint8_t melodyRoot = 0;
    uint8_t melodyChannel = 0;  ///< MIDI channel for melody voice (0-15)

    // Chord voice (chords/pads)
    ChordVoiceConfig chordVoice;
    ChordVoiceState chordState;

    // Chord randomizer (vibe-based chord progression system)
    ChordRandomizerConfig chordRandomizer;
    ChordRandomizerState chordRandomizerState;

    // Rhythm player voice (supporting accompaniment)
    RhythmPlayerConfig rhythmVoice;
    RhythmPlayerState rhythmState;

    // Bass voice (root-note bass lines)
    BassConfig bassVoice;
    BassState bassState;

    // Pattern indices
    uint8_t currentKickPattern = 0;
    uint8_t currentClapPattern = 0;
    uint8_t currentHatPattern = 0;
    uint8_t currentGroovePattern = 0;

    // Groove amounts per voice
    float grooveAmount[NUM_DRUM_VOICES];
    float grooveVelocityAmount[NUM_DRUM_VOICES];
    float melodyGrooveAmount = 0.5f;

    // Timing state
    uint8_t currentStep = 0;
    uint8_t barCounter = 0;
    uint8_t cycleCounter = 0;
    uint32_t generationSeed = 0;

    // Transport
    float bpm = 120.0f;
    bool isRunning = false;

    // Freeze controls
    bool freezeEnabled = false;
    bool melodyFreezeEnabled = false;

    // Fill state
    bool fillActive = false;
    bool fillIsHalfBar = false;
    uint8_t fillStartStep = 0;
    uint8_t currentFillSnareIndex = 0;
    uint8_t currentFillHatClosedIndex = 0;
    uint8_t currentFillHatOpenIndex = 0;

    // Configuration
    uint32_t patternChangeInterval = 4;
    uint32_t personalityChangeInterval = 4;

    // ========================================================================
    // Callbacks (set by platform)
    // ========================================================================

    /**
     * @brief Called when a drum should trigger
     * @param voice Which drum voice
     * @param velocity MIDI velocity (1-127)
     */
    std::function<void(DrumVoice voice, uint8_t velocity)> onDrumTrigger;

    /**
     * @brief Called when a melody note should trigger
     * @param note Semitone offset from C2
     */
    std::function<void(int8_t note)> onMelodyTrigger;

    /**
     * @brief Called when melody notes should stop
     */
    std::function<void()> onMelodyNoteOff;

    /**
     * @brief Called when chord voice notes should trigger
     * @param notes Array of MIDI note numbers
     * @param count Number of notes in chord
     * @param noteOn True for note-on, false for note-off
     */
    std::function<void(const int8_t* notes, uint8_t count, bool noteOn)> onChordTrigger;

    /**
     * @brief Called when rhythm player should trigger
     * @param notes Array of MIDI note numbers
     * @param count Number of notes
     * @param velocity MIDI velocity (1-127)
     * @param noteOn True for note-on, false for note-off
     */
    std::function<void(const int8_t* notes, uint8_t count, uint8_t velocity, bool noteOn)> onRhythmTrigger;

    /**
     * @brief Called when bass voice should trigger
     * @param note MIDI note number
     * @param velocity MIDI velocity (70 normal, 120 accent)
     * @param noteOn True for note-on, false for note-off
     */
    std::function<void(int8_t note, uint8_t velocity, bool noteOn)> onBassTrigger;

    // ========================================================================
    // Methods
    // ========================================================================

    /**
     * @brief Initialize sequencer with default values
     */
    void Init();

    /**
     * @brief Process one step of the sequencer
     * @param sampleRate Current audio sample rate
     *
     * Call this on every 16th note. Handles pattern generation,
     * variation switching, and triggering via callbacks.
     */
    void ProcessStep(float sampleRate);

    /**
     * @brief Start the sequencer
     */
    void Start();

    /**
     * @brief Stop the sequencer
     */
    void Stop();

    /**
     * @brief Randomize all drum patterns
     */
    void RandomizePatterns();

    /**
     * @brief Randomize voice personalities (rhythm styles, densities, interactions)
     */
    void RandomizeVoicePersonalities();

    /**
     * @brief Generate patterns for all voices
     */
    void GenerateVoicePatterns();

    /**
     * @brief Generate melody patterns for both voices
     */
    void GenerateMelodyPatterns();

    /**
     * @brief Randomize melody personalities
     */
    void RandomizeMelodyPersonality();

    /**
     * @brief Randomize groove pattern and amounts
     */
    void RandomizeGroove();

    /**
     * @brief Randomize everything (patterns, personalities, groove)
     */
    void RandomizeAll();

    /**
     * @brief Schedule a fill at the end of the current cycle
     */
    void ScheduleFill();

    /**
     * @brief Randomize chord voice settings
     */
    void RandomizeChordVoice();

    /**
     * @brief Randomize rhythm player voice settings
     */
    void RandomizeRhythmVoice();

    /**
     * @brief Randomize bass voice settings
     */
    void RandomizeBassVoice();

    /**
     * @brief Get context about current chord for melody mapping
     * @return ChordContext with root, type, and diatonic status
     */
    ChordContext GetCurrentChordContext() const;

    /**
     * @brief Process chord randomization at end of progression
     */
    void ProcessChordRandomization();

    /**
     * @brief Notify rhythm voice of chord progression cycle
     * Called when chord progression loops back to start
     */
    void NotifyRhythmOfChordCycle();

private:
    /**
     * @brief Transition to a new vibe
     */
    void TransitionToVibe(VibeType newVibe, uint32_t seed);

    /**
     * @brief Select a random enabled vibe
     */
    VibeType SelectRandomEnabledVibe(uint32_t seed);

    /**
     * @brief Select a random progression from a vibe
     */
    void SelectProgressionFromVibe(VibeType vibe, uint32_t seed);

    /**
     * @brief Select a steady chord from a vibe (for transitions)
     */
    void SelectSteadyChordFromVibe(VibeType vibe, uint32_t seed);
    /**
     * @brief Process chord voice changes
     */
    void ProcessChordVoice();

    /**
     * @brief Process rhythm player voice
     */
    void ProcessRhythmVoice();

    /**
     * @brief Process bass voice
     */
    void ProcessBassVoice();

    /**
     * @brief Trigger new chord
     */
    void TriggerChord();

    /**
     * @brief Release currently held chord voice notes
     */
    void ReleaseChord();
    /**
     * @brief Calculate groove offset in samples for a voice/step
     */
    int32_t CalculateGrooveOffset(DrumVoice voice, uint8_t step, float sampleRate);

    /**
     * @brief Calculate groove-adjusted velocity for a voice/step
     */
    uint8_t CalculateGrooveVelocity(DrumVoice voice, uint8_t baseVelocity, uint8_t step);

    /**
     * @brief Calculate melody groove offset in samples
     */
    int32_t CalculateMelodyGrooveOffset(uint8_t step, float sampleRate);

    /**
     * @brief Process drum patterns for current step
     */
    void ProcessDrumPatterns(float sampleRate);

    /**
     * @brief Process melody patterns for current step
     */
    void ProcessMelodyPatterns();

    /**
     * @brief Process fill patterns if active
     */
    void ProcessFillPatterns(uint8_t totalStep);

    /**
     * @brief Get default variation config
     */
    static VariationConfig GetDefaultVariationConfig();

    /**
     * @brief Get default melody variation config
     */
    static VariationConfig GetDefaultMelodyVariationConfig();
};

} // namespace themis

#endif // THEMIS_SEQUENCER_H
