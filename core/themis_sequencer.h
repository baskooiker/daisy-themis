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

    // Melody voices
    MelodyConfig melodyVoice;
    MelodyConfig melodyMidiVoice;
    ScaleType melodyScale = SCALE_MINOR;
    uint8_t melodyRoot = 0;

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
     * @param isMidi True for MIDI output, false for CV output
     */
    std::function<void(int8_t note, bool isMidi)> onMelodyTrigger;

    /**
     * @brief Called when melody notes should stop
     * @param isMidi True for MIDI, false for CV
     */
    std::function<void(bool isMidi)> onMelodyNoteOff;

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

private:
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
