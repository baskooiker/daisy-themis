/**
 * @file audio.h
 * @brief Simple drum synthesizers for desktop testing
 */

#ifndef THEMIS_AUDIO_H
#define THEMIS_AUDIO_H

#include "themis_types.h"
#include <SDL2/SDL.h>
#include <cstdint>
#include <mutex>
#include <atomic>

namespace themis_audio {

// ============================================================================
// DRUM VOICE SYNTHESIZERS
// ============================================================================

/**
 * @class KickSynth
 * @brief Simple kick drum synthesizer
 *
 * Sine wave with pitch envelope + click transient
 */
class KickSynth {
public:
    void Trigger(uint8_t velocity);
    float Process(float sampleRate);
    bool IsActive() const { return active; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects tone brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects decay time
    float paramFilterEnvAmt = 0.5f;   // 0-1, affects pitch envelope depth

private:
    bool active = false;
    float phase = 0.0f;
    float envPhase = 0.0f;
    float velocity = 0.0f;
    float clickPhase = 0.0f;
};

/**
 * @class SnareSynth
 * @brief Simple snare drum synthesizer
 *
 * Filtered noise + tone body
 */
class SnareSynth {
public:
    void Trigger(uint8_t velocity);
    float Process(float sampleRate);
    bool IsActive() const { return active; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects noise brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects decay time
    float paramFilterEnvAmt = 0.5f;   // 0-1, affects filter sweep

private:
    bool active = false;
    float envPhase = 0.0f;
    float velocity = 0.0f;
    float tonePhase = 0.0f;
    float filterState = 0.0f;
};

/**
 * @class HiHatSynth
 * @brief Simple hi-hat synthesizer
 *
 * Filtered noise with variable decay
 */
class HiHatSynth {
public:
    void Trigger(uint8_t velocity, bool open);
    float Process(float sampleRate);
    bool IsActive() const { return active; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects decay time

private:
    bool active = false;
    float envPhase = 0.0f;
    float velocity = 0.0f;
    bool isOpen = false;
    float filterState1 = 0.0f;
    float filterState2 = 0.0f;
};

/**
 * @class ClapSynth
 * @brief Simple clap synthesizer
 *
 * Layered noise bursts with reverb tail
 */
class ClapSynth {
public:
    void Trigger(uint8_t velocity);
    float Process(float sampleRate);
    bool IsActive() const { return active; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects tail length

private:
    bool active = false;
    float envPhase = 0.0f;
    float velocity = 0.0f;
    int burstCount = 0;
    float filterState = 0.0f;
};

/**
 * @class TomSynth
 * @brief Simple tom drum synthesizer
 *
 * Sine wave with pitch decay
 */
class TomSynth {
public:
    void Trigger(uint8_t velocity, float basePitch);
    float Process(float sampleRate);
    bool IsActive() const { return active; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects tone color
    float paramVcaDecay = 0.5f;       // 0-1, affects decay time

private:
    bool active = false;
    float phase = 0.0f;
    float envPhase = 0.0f;
    float velocity = 0.0f;
    float basePitch = 100.0f;
};

// ============================================================================
// PAD SYNTH (POLYPHONIC)
// ============================================================================

/**
 * @struct PadVoice
 * @brief Single voice in polyphonic pad synth
 */
struct PadVoice {
    bool active = false;
    int8_t note = 0;        // MIDI note number for identification
    float phase = 0.0f;
    float freq = 0.0f;
    float env = 0.0f;       // Current envelope value
    float targetEnv = 0.0f; // Target envelope (1 when on, 0 when off)
    float filterState = 0.0f;
};

/**
 * @class PadSynth
 * @brief Polyphonic pad synthesizer for chords
 *
 * Warm pad sound with slow attack/release, suitable for chord progressions
 */
class PadSynth {
public:
    static constexpr int MAX_VOICES = 6;

    void NoteOn(int8_t note, uint8_t velocity);
    void NoteOff(int8_t note);
    void AllNotesOff();
    float Process(float sampleRate);

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects release time
    int paramVcoType = 0;             // 0=saw, 1=square, 2=triangle, 3=sine
    float paramFilterEnvAmt = 0.5f;   // 0-1, affects filter sweep

private:
    PadVoice voices[MAX_VOICES];
    float attackRate = 0.002f;   // Slow attack for pad
    float releaseRate = 0.0008f; // Slow release for pad
};

// ============================================================================
// RHYTHM SYNTH
// ============================================================================

/**
 * @struct RhythmVoice
 * @brief Single voice in polyphonic rhythm synth
 */
struct RhythmVoice {
    bool active = false;
    int8_t note = 0;
    float phase = 0.0f;
    float freq = 0.0f;
    float env = 0.0f;
    float targetEnv = 0.0f;
    float filterState = 0.0f;
};

/**
 * @class RhythmSynth
 * @brief Polyphonic rhythm synth (electric piano / organ character)
 *
 * Punchy attack, suitable for chord stabs and arpeggios
 */
class RhythmSynth {
public:
    static constexpr int MAX_VOICES = 6;

    void NoteOn(int8_t note, uint8_t velocity);
    void NoteOff(int8_t note);
    void AllNotesOff();
    float Process(float sampleRate);

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects brightness
    float paramVcaDecay = 0.5f;       // 0-1, affects release time
    int paramVcoType = 0;             // 0=saw, 1=square, 2=triangle, 3=sine
    float paramFilterEnvAmt = 0.5f;   // 0-1, affects filter sweep

private:
    RhythmVoice voices[MAX_VOICES];
    float velocity = 0.8f;
    float attackRate = 0.02f;    // Punchy attack
    float releaseRate = 0.005f;  // Medium release
};

// ============================================================================
// BASS SYNTH
// ============================================================================

/**
 * @class BassSynth
 * @brief Monophonic bass synth
 *
 * Sawtooth wave with resonant filter for root-note bass lines
 */
class BassSynth {
public:
    void NoteOn(int8_t note, uint8_t velocity);
    void NoteOff(int8_t note);
    void AllNotesOff();
    float Process(float sampleRate);
    bool IsActive() const { return active || env > 0.001f; }

    // Configurable parameters
    float paramFilterCutoff = 1.0f;   // 0-1, affects base filter cutoff
    float paramVcaDecay = 0.5f;       // 0-1, affects decay/release time
    float paramFilterEnvAmt = 0.5f;   // 0-1, affects filter envelope depth

private:
    bool active = false;
    int8_t currentNote = -1;
    float phase = 0.0f;
    float freq = 0.0f;
    float env = 0.0f;
    float targetEnv = 0.0f;
    float filterState = 0.0f;
    float filterState2 = 0.0f;  // For resonance
    float velocity = 0.0f;
    bool isAccent = false;

    // Envelope rates
    float attackRate = 0.05f;    // Fast attack for punchy bass
    float decayRate = 0.008f;    // Medium decay
    float releaseRate = 0.02f;   // Quick release

    // Filter settings
    float baseCutoff = 0.15f;    // Base filter cutoff
    float accentCutoff = 0.6f;   // Cutoff when accented
    float resonance = 0.7f;      // Fixed resonance
};

// ============================================================================
// AUDIO ENGINE
// ============================================================================

/**
 * @class AudioEngine
 * @brief Manages SDL2 audio output and drum synths
 */
class AudioEngine {
public:
    static constexpr int NUM_KICK = 1;
    static constexpr int NUM_SNARE = 2;
    static constexpr int NUM_HIHAT = 2;
    static constexpr int NUM_CLAP = 1;
    static constexpr int NUM_TOM = 4;

    bool Init(int sampleRate = 48000);
    void Pause();
    void Shutdown();

    void TriggerDrum(themis::DrumVoice voice, uint8_t velocity);
    void TriggerMelodyMidi(int8_t note, uint8_t velocity);
    void StopMelodyMidi();

    // Poly voice (pads/chords)
    void TriggerPolyChord(const int8_t* notes, uint8_t count, uint8_t velocity);
    void ReleasePolyChord(const int8_t* notes, uint8_t count);
    void StopAllPolyNotes();

    // Rhythm player voice
    void TriggerRhythmNotes(const int8_t* notes, uint8_t count, uint8_t velocity);
    void ReleaseRhythmNotes(const int8_t* notes, uint8_t count);
    void StopAllRhythmNotes();

    // Bass voice
    void TriggerBass(int8_t note, uint8_t velocity);
    void StopBass(int8_t note);
    void StopAllBassNotes();

    void SetVolume(float vol) { volume = vol; }
    float GetVolume() const { return volume; }

    void SetMuted(bool mute) { muted = mute; }
    bool IsMuted() const { return muted; }

    float GetSampleRate() const { return sampleRate; }

    // Peak level for visualization
    float GetPeakLevel() const { return peakLevel; }

    // Global sound shaping
    void SetFilterCutoff(float cutoff) { filterCutoff = cutoff; }
    float GetFilterCutoff() const { return filterCutoff; }

    void SetDecayAmount(float decay) { decayAmount = decay; }
    float GetDecayAmount() const { return decayAmount; }

    // Per-voice synth parameters (drums)
    void SetKickFilterCutoff(float v) { kickFilterCutoff = v; }
    void SetKickVcaDecay(float v) { kickVcaDecay = v; }
    void SetKickFilterEnvAmount(float v) { kickFilterEnvAmount = v; }

    void SetSnareFilterCutoff(float v) { snareFilterCutoff = v; }
    void SetSnareVcaDecay(float v) { snareVcaDecay = v; }
    void SetSnareFilterEnvAmount(float v) { snareFilterEnvAmount = v; }

    void SetHihatFilterCutoff(float v) { hihatFilterCutoff = v; }
    void SetHihatVcaDecay(float v) { hihatVcaDecay = v; }

    void SetClapFilterCutoff(float v) { clapFilterCutoff = v; }
    void SetClapVcaDecay(float v) { clapVcaDecay = v; }

    void SetTomFilterCutoff(float v) { tomFilterCutoff = v; }
    void SetTomVcaDecay(float v) { tomVcaDecay = v; }

    // Per-voice synth parameters (melodic)
    void SetRhythmFilterCutoff(float v) { rhythmFilterCutoff = v; }
    void SetRhythmVcaDecay(float v) { rhythmVcaDecay = v; }
    void SetRhythmVcoType(int v) { rhythmVcoType = v; }
    void SetRhythmFilterEnvAmount(float v) { rhythmFilterEnvAmount = v; }

    void SetBassFilterCutoff(float v) { bassFilterCutoff = v; }
    void SetBassVcaDecay(float v) { bassVcaDecay = v; }
    void SetBassFilterEnvAmount(float v) { bassFilterEnvAmount = v; }

    void SetPadFilterCutoff(float v) { padFilterCutoff = v; }
    void SetPadVcaDecay(float v) { padVcaDecay = v; }
    void SetPadVcoType(int v) { padVcoType = v; }
    void SetPadFilterEnvAmount(float v) { padFilterEnvAmount = v; }

private:
    static void AudioCallback(void* userdata, Uint8* stream, int len);
    void ProcessAudio(float* buffer, int frames);

    SDL_AudioDeviceID audioDevice = 0;
    float sampleRate = 48000.0f;
    std::atomic<float> volume{0.7f};
    std::atomic<bool> muted{false};
    std::atomic<float> peakLevel{0.0f};
    std::atomic<float> filterCutoff{1.0f};  // 0.0 = closed, 1.0 = open
    std::atomic<float> decayAmount{0.5f};   // 0.0 = short, 1.0 = long

    // Per-voice synth parameters (drums)
    std::atomic<float> kickFilterCutoff{1.0f};
    std::atomic<float> kickVcaDecay{0.5f};
    std::atomic<float> kickFilterEnvAmount{0.5f};

    std::atomic<float> snareFilterCutoff{1.0f};
    std::atomic<float> snareVcaDecay{0.5f};
    std::atomic<float> snareFilterEnvAmount{0.5f};

    std::atomic<float> hihatFilterCutoff{1.0f};
    std::atomic<float> hihatVcaDecay{0.5f};

    std::atomic<float> clapFilterCutoff{1.0f};
    std::atomic<float> clapVcaDecay{0.5f};

    std::atomic<float> tomFilterCutoff{1.0f};
    std::atomic<float> tomVcaDecay{0.5f};

    // Per-voice synth parameters (melodic)
    std::atomic<float> rhythmFilterCutoff{1.0f};
    std::atomic<float> rhythmVcaDecay{0.5f};
    std::atomic<int> rhythmVcoType{0};
    std::atomic<float> rhythmFilterEnvAmount{0.5f};

    std::atomic<float> bassFilterCutoff{1.0f};
    std::atomic<float> bassVcaDecay{0.5f};
    std::atomic<float> bassFilterEnvAmount{0.5f};

    std::atomic<float> padFilterCutoff{1.0f};
    std::atomic<float> padVcaDecay{0.5f};
    std::atomic<int> padVcoType{0};
    std::atomic<float> padFilterEnvAmount{0.5f};

    // Master filter state
    float masterFilterState = 0.0f;

    std::mutex synthMutex;

    // Synth instances
    KickSynth kick;
    SnareSynth snare[NUM_SNARE];
    HiHatSynth hihat[NUM_HIHAT];
    ClapSynth clap;
    TomSynth tom[NUM_TOM];

    // Melody synth (square wave with filter)
    float melodyMidiPhase = 0.0f;
    float melodyMidiFreq = 0.0f;
    float melodyMidiEnv = 0.0f;
    bool melodyMidiActive = false;
    float melodyMidiFilterState = 0.0f;

    // Poly voice (pads)
    PadSynth padSynth;

    // Rhythm player voice
    RhythmSynth rhythmSynth;

    // Bass voice
    BassSynth bassSynth;
};

// Global audio engine instance
extern AudioEngine g_audioEngine;

} // namespace themis_audio

#endif // THEMIS_AUDIO_H
