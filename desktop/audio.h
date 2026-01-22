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

private:
    PadVoice voices[MAX_VOICES];
    float attackRate = 0.002f;   // Slow attack for pad
    float releaseRate = 0.0008f; // Slow release for pad
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
    void Shutdown();

    void TriggerDrum(themis::DrumVoice voice, uint8_t velocity);
    void TriggerMelodyCV(int8_t note, uint8_t velocity);
    void TriggerMelodyMidi(int8_t note, uint8_t velocity);
    void StopMelodyCV();
    void StopMelodyMidi();

    // Poly voice (pads/chords)
    void TriggerPolyChord(const int8_t* notes, uint8_t count, uint8_t velocity);
    void ReleasePolyChord(const int8_t* notes, uint8_t count);
    void StopAllPolyNotes();

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

    // Master filter state
    float masterFilterState = 0.0f;

    std::mutex synthMutex;

    // Synth instances
    KickSynth kick;
    SnareSynth snare[NUM_SNARE];
    HiHatSynth hihat[NUM_HIHAT];
    ClapSynth clap;
    TomSynth tom[NUM_TOM];

    // Melody CV synth (saw wave)
    float melodyCVPhase = 0.0f;
    float melodyCVFreq = 0.0f;
    float melodyCVEnv = 0.0f;
    bool melodyCVActive = false;

    // Melody MIDI synth (square wave with different character)
    float melodyMidiPhase = 0.0f;
    float melodyMidiFreq = 0.0f;
    float melodyMidiEnv = 0.0f;
    bool melodyMidiActive = false;
    float melodyMidiFilterState = 0.0f;

    // Poly voice (pads)
    PadSynth padSynth;
};

// Global audio engine instance
extern AudioEngine g_audioEngine;

} // namespace themis_audio

#endif // THEMIS_AUDIO_H
