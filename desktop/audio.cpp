/**
 * @file audio.cpp
 * @brief Audio engine and drum synthesizers implementation
 */

#include "audio.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

namespace themis_audio {

// Global instance
AudioEngine g_audioEngine;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static inline float RandomNoise()
{
    return (float)rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

static inline float Clamp(float val, float min, float max)
{
    return std::min(std::max(val, min), max);
}

// ============================================================================
// KICK SYNTH
// ============================================================================

void KickSynth::Trigger(uint8_t vel)
{
    active = true;
    phase = 0.0f;
    envPhase = 0.0f;
    clickPhase = 0.0f;
    velocity = (float)vel / 127.0f;
}

float KickSynth::Process(float sampleRate)
{
    if (!active) return 0.0f;

    envPhase += 1.0f / sampleRate;

    // Amplitude envelope - fast attack, medium decay
    float ampEnv = expf(-envPhase * 8.0f);

    // Pitch envelope - starts high, drops to base frequency
    float pitchEnv = 150.0f + 200.0f * expf(-envPhase * 30.0f);

    // Main body - sine wave
    phase += pitchEnv / sampleRate;
    float body = sinf(phase * 2.0f * M_PI) * ampEnv;

    // Click transient
    float click = 0.0f;
    if (envPhase < 0.005f) {
        clickPhase += 3000.0f / sampleRate;
        click = sinf(clickPhase * 2.0f * M_PI) * (1.0f - envPhase / 0.005f) * 0.3f;
    }

    float output = (body + click) * velocity;

    // Deactivate when quiet
    if (ampEnv < 0.001f) {
        active = false;
    }

    return output;
}

// ============================================================================
// SNARE SYNTH
// ============================================================================

void SnareSynth::Trigger(uint8_t vel)
{
    active = true;
    envPhase = 0.0f;
    tonePhase = 0.0f;
    velocity = (float)vel / 127.0f;
}

float SnareSynth::Process(float sampleRate)
{
    if (!active) return 0.0f;

    envPhase += 1.0f / sampleRate;

    // Noise envelope
    float noiseEnv = expf(-envPhase * 15.0f);

    // Tone envelope (faster decay)
    float toneEnv = expf(-envPhase * 25.0f);

    // Noise component with high-pass filter
    float noise = RandomNoise();
    float cutoff = 0.3f;
    filterState += cutoff * (noise - filterState);
    float filteredNoise = noise - filterState;

    // Tone component (around 200Hz)
    tonePhase += 200.0f / sampleRate;
    float tone = sinf(tonePhase * 2.0f * M_PI);

    float output = (filteredNoise * noiseEnv * 0.6f + tone * toneEnv * 0.4f) * velocity;

    if (noiseEnv < 0.001f) {
        active = false;
    }

    return output;
}

// ============================================================================
// HI-HAT SYNTH
// ============================================================================

void HiHatSynth::Trigger(uint8_t vel, bool open)
{
    active = true;
    envPhase = 0.0f;
    velocity = (float)vel / 127.0f;
    isOpen = open;
}

float HiHatSynth::Process(float sampleRate)
{
    if (!active) return 0.0f;

    envPhase += 1.0f / sampleRate;

    // Decay rate depends on open/closed
    float decayRate = isOpen ? 3.0f : 30.0f;
    float env = expf(-envPhase * decayRate);

    // Generate metallic noise using band-pass filtered noise
    float noise = RandomNoise();

    // High-pass filter
    float hp_cutoff = 0.8f;
    filterState1 += hp_cutoff * (noise - filterState1);
    float hp_out = noise - filterState1;

    // Low-pass filter
    float lp_cutoff = 0.4f;
    filterState2 += lp_cutoff * (hp_out - filterState2);

    float output = filterState2 * env * velocity * 0.5f;

    if (env < 0.001f) {
        active = false;
    }

    return output;
}

// ============================================================================
// CLAP SYNTH
// ============================================================================

void ClapSynth::Trigger(uint8_t vel)
{
    active = true;
    envPhase = 0.0f;
    burstCount = 0;
    velocity = (float)vel / 127.0f;
}

float ClapSynth::Process(float sampleRate)
{
    if (!active) return 0.0f;

    envPhase += 1.0f / sampleRate;

    // Multiple bursts for clap character
    float burstEnv = 0.0f;
    float burstTimes[] = {0.0f, 0.015f, 0.030f, 0.045f};
    float burstDurations[] = {0.01f, 0.008f, 0.008f, 0.2f};

    for (int i = 0; i < 4; i++) {
        if (envPhase >= burstTimes[i]) {
            float localTime = envPhase - burstTimes[i];
            if (localTime < burstDurations[i]) {
                float decay = (i < 3) ? 100.0f : 10.0f;
                burstEnv += expf(-localTime * decay) * (i < 3 ? 0.7f : 1.0f);
            }
        }
    }

    // Filtered noise
    float noise = RandomNoise();
    float cutoff = 0.5f;
    filterState += cutoff * (noise - filterState);
    float filteredNoise = noise - filterState;

    float output = filteredNoise * burstEnv * velocity * 0.4f;

    // Deactivate after all bursts complete
    if (envPhase > 0.3f) {
        active = false;
    }

    return output;
}

// ============================================================================
// TOM SYNTH
// ============================================================================

void TomSynth::Trigger(uint8_t vel, float pitch)
{
    active = true;
    phase = 0.0f;
    envPhase = 0.0f;
    velocity = (float)vel / 127.0f;
    basePitch = pitch;
}

float TomSynth::Process(float sampleRate)
{
    if (!active) return 0.0f;

    envPhase += 1.0f / sampleRate;

    // Amplitude envelope
    float ampEnv = expf(-envPhase * 6.0f);

    // Pitch envelope - starts slightly high
    float pitch = basePitch + 50.0f * expf(-envPhase * 20.0f);

    // Sine wave body
    phase += pitch / sampleRate;
    float output = sinf(phase * 2.0f * M_PI) * ampEnv * velocity;

    if (ampEnv < 0.001f) {
        active = false;
    }

    return output;
}

// ============================================================================
// AUDIO ENGINE
// ============================================================================

bool AudioEngine::Init(int rate)
{
    sampleRate = (float)rate;

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);

    desired.freq = rate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 256;
    desired.callback = AudioCallback;
    desired.userdata = this;

    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audioDevice == 0) {
        SDL_Log("Failed to open audio: %s", SDL_GetError());
        return false;
    }

    sampleRate = (float)obtained.freq;

    // Start audio playback
    SDL_PauseAudioDevice(audioDevice, 0);

    return true;
}

void AudioEngine::Shutdown()
{
    if (audioDevice != 0) {
        // Pause audio first to prevent callbacks during cleanup
        SDL_PauseAudioDevice(audioDevice, 1);

        // Small delay to ensure any pending audio callbacks complete
        SDL_Delay(50);

        // Close the device
        SDL_CloseAudioDevice(audioDevice);
        audioDevice = 0;
    }
}

void AudioEngine::TriggerDrum(themis::DrumVoice voice, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);

    switch (voice) {
        case themis::KICK:
            kick.Trigger(velocity);
            break;

        case themis::SNARE:
            snare[0].Trigger(velocity);
            break;

        case themis::CLAP:
            clap.Trigger(velocity);
            break;

        case themis::HIHAT1_CLOSED:
        case themis::HIHAT2_CLOSED:
            hihat[0].Trigger(velocity, false);
            break;

        case themis::HIHAT1_OPEN:
        case themis::HIHAT2_OPEN:
            hihat[1].Trigger(velocity, true);
            break;

        case themis::DRUM1:
            tom[0].Trigger(velocity, 150.0f);
            break;

        case themis::DRUM2:
            tom[1].Trigger(velocity, 120.0f);
            break;

        case themis::MULTI:
            tom[2].Trigger(velocity, 200.0f);
            break;

        case themis::ANALOG:
            tom[3].Trigger(velocity, 80.0f);
            break;

        default:
            break;
    }
}

void AudioEngine::TriggerMelodyCV(int8_t note, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);

    // Convert semitone to frequency (C2 = MIDI 36 = ~65.4 Hz)
    float freq = 65.406f * powf(2.0f, (float)note / 12.0f);
    melodyCVFreq = freq;
    melodyCVEnv = (float)velocity / 127.0f;
    melodyCVActive = true;
}

void AudioEngine::StopMelodyCV()
{
    std::lock_guard<std::mutex> lock(synthMutex);
    melodyCVActive = false;
}

void AudioEngine::TriggerMelodyMidi(int8_t note, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);

    // Convert semitone to frequency (C2 = MIDI 36 = ~65.4 Hz)
    float freq = 65.406f * powf(2.0f, (float)note / 12.0f);
    melodyMidiFreq = freq;
    melodyMidiEnv = (float)velocity / 127.0f;
    melodyMidiActive = true;
}

void AudioEngine::StopMelodyMidi()
{
    std::lock_guard<std::mutex> lock(synthMutex);
    melodyMidiActive = false;
}

void AudioEngine::AudioCallback(void* userdata, Uint8* stream, int len)
{
    AudioEngine* engine = static_cast<AudioEngine*>(userdata);
    float* buffer = reinterpret_cast<float*>(stream);
    int frames = len / (sizeof(float) * 2);

    engine->ProcessAudio(buffer, frames);
}

void AudioEngine::ProcessAudio(float* buffer, int frames)
{
    std::lock_guard<std::mutex> lock(synthMutex);

    float peak = 0.0f;
    float vol = muted ? 0.0f : volume.load();
    float cutoff = filterCutoff.load();
    float decay = decayAmount.load();

    // Map cutoff to filter coefficient (0.01 to 1.0)
    float filterCoeff = 0.01f + cutoff * 0.99f;

    // Map decay to envelope multiplier - affects how fast envelopes decay
    // decay=0 -> faster decay (multiply envelope rates by 2.0)
    // decay=1 -> slower decay (multiply envelope rates by 0.3)
    float decayMult = 2.0f - decay * 1.7f;

    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;

        // Process all synths
        // Kick uses original sample rate so decay doesn't affect pitch
        sample += kick.Process(sampleRate);

        for (int j = 0; j < NUM_SNARE; j++) {
            sample += snare[j].Process(sampleRate * decayMult);
        }

        for (int j = 0; j < NUM_HIHAT; j++) {
            sample += hihat[j].Process(sampleRate * decayMult);
        }

        sample += clap.Process(sampleRate * decayMult);

        for (int j = 0; j < NUM_TOM; j++) {
            sample += tom[j].Process(sampleRate * decayMult);
        }

        // Melody CV synth (saw wave - brighter, analog-style)
        if (melodyCVActive && melodyCVEnv > 0.001f) {
            melodyCVPhase += melodyCVFreq / sampleRate;
            if (melodyCVPhase > 1.0f) melodyCVPhase -= 1.0f;

            // Saw wave
            float saw = melodyCVPhase * 2.0f - 1.0f;
            sample += saw * melodyCVEnv * 0.25f;

            // Decay envelope - affected by decay parameter
            float cvDecayRate = 0.9995f + decay * 0.0004f;
            melodyCVEnv *= cvDecayRate;
        }

        // Melody MIDI synth (square wave with filter - warmer, digital-style)
        if (melodyMidiActive && melodyMidiEnv > 0.001f) {
            melodyMidiPhase += melodyMidiFreq / sampleRate;
            if (melodyMidiPhase > 1.0f) melodyMidiPhase -= 1.0f;

            // Square wave with slight pulse width modulation feel
            float square = (melodyMidiPhase < 0.45f) ? 1.0f : -1.0f;

            // Simple low-pass filter for warmer sound
            float filterCoeffMidi = 0.15f;
            melodyMidiFilterState += filterCoeffMidi * (square - melodyMidiFilterState);

            sample += melodyMidiFilterState * melodyMidiEnv * 0.2f;

            // Decay envelope - affected by decay parameter
            float midiDecayRate = 0.9996f + decay * 0.0003f;
            melodyMidiEnv *= midiDecayRate;
        }

        // Apply master low-pass filter
        masterFilterState += filterCoeff * (sample - masterFilterState);
        sample = masterFilterState;

        // Apply volume
        sample *= vol;

        // Soft clipping
        sample = tanhf(sample);

        // Track peak
        float absSample = fabsf(sample);
        if (absSample > peak) peak = absSample;

        // Stereo output
        buffer[i * 2] = sample;
        buffer[i * 2 + 1] = sample;
    }

    peakLevel = peak;
}

} // namespace themis_audio
