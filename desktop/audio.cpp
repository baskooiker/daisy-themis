/**
 * @file audio.cpp
 * @brief Audio engine and drum synthesizers implementation
 */

#include "audio.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <iostream>

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

    // Decay rate affected by paramVcaDecay (0.5 = default 8.0, 0 = 16.0 fast, 1 = 4.0 slow)
    float decayRate = 16.0f - paramVcaDecay * 12.0f;

    // Amplitude envelope - fast attack, configurable decay
    float ampEnv = expf(-envPhase * decayRate);

    // Pitch envelope depth affected by paramFilterEnvAmt
    float pitchDepth = 100.0f + paramFilterEnvAmt * 200.0f;
    float pitchEnv = 150.0f + pitchDepth * expf(-envPhase * 30.0f);

    // Main body - sine wave
    phase += pitchEnv / sampleRate;
    float body = sinf(phase * 2.0f * M_PI) * ampEnv;

    // Click transient - amount affected by filter cutoff
    float click = 0.0f;
    float clickAmount = 0.1f + paramFilterCutoff * 0.3f;
    if (envPhase < 0.005f) {
        clickPhase += 3000.0f / sampleRate;
        click = sinf(clickPhase * 2.0f * M_PI) * (1.0f - envPhase / 0.005f) * clickAmount;
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

    // Decay rate affected by paramVcaDecay
    float decayRate = 25.0f - paramVcaDecay * 15.0f;

    // Noise envelope
    float noiseEnv = expf(-envPhase * decayRate);

    // Tone envelope (faster decay)
    float toneEnv = expf(-envPhase * (decayRate + 10.0f));

    // Noise component with high-pass filter - cutoff affected by paramFilterCutoff
    float noise = RandomNoise();
    float cutoff = 0.1f + paramFilterCutoff * 0.4f;
    filterState += cutoff * (noise - filterState);
    float filteredNoise = noise - filterState;

    // Tone component (around 200Hz) - mix affected by paramFilterEnvAmt
    tonePhase += 200.0f / sampleRate;
    float tone = sinf(tonePhase * 2.0f * M_PI);

    float noiseMix = 0.4f + paramFilterEnvAmt * 0.3f;
    float toneMix = 1.0f - noiseMix;
    float output = (filteredNoise * noiseEnv * noiseMix + tone * toneEnv * toneMix) * velocity;

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

    // Decay rate depends on open/closed, affected by paramVcaDecay
    float baseDecay = isOpen ? 3.0f : 30.0f;
    float decayMod = 1.5f - paramVcaDecay;  // 0.5 to 1.5
    float decayRate = baseDecay * decayMod;
    float env = expf(-envPhase * decayRate);

    // Generate metallic noise using band-pass filtered noise
    float noise = RandomNoise();

    // High-pass filter - affected by paramFilterCutoff
    float hp_cutoff = 0.5f + paramFilterCutoff * 0.4f;
    filterState1 += hp_cutoff * (noise - filterState1);
    float hp_out = noise - filterState1;

    // Low-pass filter - affected by paramFilterCutoff
    float lp_cutoff = 0.2f + paramFilterCutoff * 0.5f;
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

    // Tail duration affected by paramVcaDecay
    float tailDuration = 0.1f + paramVcaDecay * 0.2f;
    float burstDurations[] = {0.01f, 0.008f, 0.008f, tailDuration};

    // Decay rates affected by paramVcaDecay
    float tailDecay = 15.0f - paramVcaDecay * 10.0f;

    for (int i = 0; i < 4; i++) {
        if (envPhase >= burstTimes[i]) {
            float localTime = envPhase - burstTimes[i];
            if (localTime < burstDurations[i]) {
                float decay = (i < 3) ? 100.0f : tailDecay;
                burstEnv += expf(-localTime * decay) * (i < 3 ? 0.7f : 1.0f);
            }
        }
    }

    // Filtered noise - cutoff affected by paramFilterCutoff
    float noise = RandomNoise();
    float cutoff = 0.2f + paramFilterCutoff * 0.5f;
    filterState += cutoff * (noise - filterState);
    float filteredNoise = noise - filterState;

    float output = filteredNoise * burstEnv * velocity * 0.4f;

    // Deactivate after all bursts complete
    float totalDuration = 0.1f + tailDuration;
    if (envPhase > totalDuration) {
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

    // Decay rate affected by paramVcaDecay
    float decayRate = 10.0f - paramVcaDecay * 6.0f;

    // Amplitude envelope
    float ampEnv = expf(-envPhase * decayRate);

    // Pitch envelope - depth affected by paramFilterCutoff
    float pitchDepth = 30.0f + paramFilterCutoff * 40.0f;
    float pitch = basePitch + pitchDepth * expf(-envPhase * 20.0f);

    // Sine wave body
    phase += pitch / sampleRate;
    float output = sinf(phase * 2.0f * M_PI) * ampEnv * velocity;

    if (ampEnv < 0.001f) {
        active = false;
    }

    return output;
}

// ============================================================================
// PAD SYNTH
// ============================================================================

void PadSynth::NoteOn(int8_t note, uint8_t velocity)
{
    // Find existing voice with same note or free voice
    int freeVoice = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].note == note && voices[i].active) {
            // Retrigger existing voice
            voices[i].targetEnv = (float)velocity / 127.0f;
            return;
        }
        if (!voices[i].active && freeVoice == -1) {
            freeVoice = i;
        }
    }

    // Use free voice or steal quietest
    if (freeVoice == -1) {
        float quietest = 1.0f;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voices[i].env < quietest) {
                quietest = voices[i].env;
                freeVoice = i;
            }
        }
    }

    if (freeVoice >= 0) {
        PadVoice& v = voices[freeVoice];
        v.active = true;
        v.note = note;
        v.freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
        v.targetEnv = (float)velocity / 127.0f;
        v.phase = 0.0f;
        v.filterState = 0.0f;
    }
}

void PadSynth::NoteOff(int8_t note)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].note == note && voices[i].active) {
            voices[i].targetEnv = 0.0f;
            // Don't set active = false yet, let the release finish
        }
    }
}

void PadSynth::AllNotesOff()
{
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].targetEnv = 0.0f;
    }
}

float PadSynth::Process(float sampleRate)
{
    (void)sampleRate;  // Rate-dependent values are pre-calculated

    float output = 0.0f;

    // Release rate affected by paramVcaDecay
    float actualReleaseRate = releaseRate * (1.5f - paramVcaDecay);

    // Filter cutoff affected by paramFilterCutoff
    float filterCoeff = 0.05f + paramFilterCutoff * 0.25f;

    for (int i = 0; i < MAX_VOICES; i++) {
        PadVoice& v = voices[i];
        if (!v.active && v.env < 0.001f) continue;

        // Smooth envelope
        float envRate = (v.targetEnv > v.env) ? attackRate : actualReleaseRate;
        v.env += (v.targetEnv - v.env) * envRate;

        // Deactivate when envelope is done releasing
        if (v.targetEnv == 0.0f && v.env < 0.001f) {
            v.active = false;
            continue;
        }

        // Oscillator - type affected by paramVcoType
        v.phase += v.freq / sampleRate;
        if (v.phase > 1.0f) v.phase -= 1.0f;

        float osc;
        switch (paramVcoType) {
            case 1: // Square
                osc = (v.phase < 0.5f) ? 1.0f : -1.0f;
                break;
            case 2: // Triangle
                osc = (v.phase < 0.5f) ? (4.0f * v.phase - 1.0f) : (3.0f - 4.0f * v.phase);
                break;
            case 3: // Sine
                osc = sinf(v.phase * 2.0f * M_PI);
                break;
            default: // Saw (with subtle sine for warmth)
                {
                    float sine = sinf(v.phase * 2.0f * M_PI);
                    float saw = v.phase * 2.0f - 1.0f;
                    osc = sine * 0.5f + saw * 0.5f;
                }
                break;
        }

        // Low-pass filter
        v.filterState += filterCoeff * (osc - v.filterState);

        output += v.filterState * v.env * 0.12f;
    }

    return output;
}

// ============================================================================
// RHYTHM SYNTH
// ============================================================================

void RhythmSynth::NoteOn(int8_t note, uint8_t vel)
{
    velocity = (float)vel / 127.0f;

    // Find existing voice with same note or free voice
    int freeVoice = -1;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].note == note && voices[i].active) {
            // Retrigger existing voice
            voices[i].targetEnv = velocity;
            voices[i].env = 0.0f;  // Hard retrigger for punchy sound
            return;
        }
        if (!voices[i].active && freeVoice == -1) {
            freeVoice = i;
        }
    }

    // Use free voice or steal quietest
    if (freeVoice == -1) {
        float quietest = 1.0f;
        for (int i = 0; i < MAX_VOICES; i++) {
            if (voices[i].env < quietest) {
                quietest = voices[i].env;
                freeVoice = i;
            }
        }
    }

    if (freeVoice >= 0) {
        RhythmVoice& v = voices[freeVoice];
        v.active = true;
        v.note = note;
        v.freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
        v.targetEnv = velocity;
        v.env = 0.0f;  // Start from zero for punchy attack
        v.phase = 0.0f;
        v.filterState = 0.0f;
    }
}

void RhythmSynth::NoteOff(int8_t note)
{
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].note == note && voices[i].active) {
            voices[i].targetEnv = 0.0f;
        }
    }
}

void RhythmSynth::AllNotesOff()
{
    for (int i = 0; i < MAX_VOICES; i++) {
        voices[i].targetEnv = 0.0f;
    }
}

float RhythmSynth::Process(float sampleRate)
{
    float output = 0.0f;

    // Release rate affected by paramVcaDecay
    float actualReleaseRate = releaseRate * (1.5f - paramVcaDecay);

    // Filter cutoff affected by paramFilterCutoff
    float filterCoeff = 0.1f + paramFilterCutoff * 0.3f;

    for (int i = 0; i < MAX_VOICES; i++) {
        RhythmVoice& v = voices[i];
        if (!v.active && v.env < 0.001f) continue;

        // Smooth envelope with punchy attack
        float envRate = (v.targetEnv > v.env) ? attackRate : actualReleaseRate;
        v.env += (v.targetEnv - v.env) * envRate;

        // Deactivate when envelope is done releasing
        if (v.targetEnv == 0.0f && v.env < 0.001f) {
            v.active = false;
            continue;
        }

        v.phase += v.freq / sampleRate;
        if (v.phase > 1.0f) v.phase -= 1.0f;

        float osc;
        switch (paramVcoType) {
            case 0: // Saw - bright and cutting
                osc = v.phase * 2.0f - 1.0f;
                break;
            case 1: // Square - hollow organ-like
                osc = (v.phase < 0.5f) ? 1.0f : -1.0f;
                break;
            case 2: // Triangle - soft and warm
                osc = (v.phase < 0.5f) ? (4.0f * v.phase - 1.0f) : (3.0f - 4.0f * v.phase);
                break;
            case 3: // Sine - very pure
                osc = sinf(v.phase * 2.0f * M_PI);
                break;
            default: // FM-like electric piano (default)
                {
                    float fundamental = sinf(v.phase * 2.0f * M_PI);
                    float second = sinf(v.phase * 4.0f * M_PI) * 0.3f;
                    float third = sinf(v.phase * 6.0f * M_PI) * 0.15f;
                    float bell = sinf(v.phase * 10.0f * M_PI) * 0.1f * expf(-v.env * 5.0f + 5.0f);
                    osc = fundamental + second + third + bell;
                }
                break;
        }

        // Low-pass filter
        v.filterState += filterCoeff * (osc - v.filterState);

        output += v.filterState * v.env * 0.15f;
    }

    return output;
}

// ============================================================================
// BASS SYNTH
// ============================================================================

void BassSynth::NoteOn(int8_t note, uint8_t vel)
{
    // Store whether this is an accent (velocity >= 100)
    isAccent = (vel >= 100);

    // Calculate frequency
    freq = 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);

    // Retrigger envelope
    env = 0.0f;
    targetEnv = 1.0f;

    velocity = (float)vel / 127.0f;
    currentNote = note;
    active = true;
}

void BassSynth::NoteOff(int8_t note)
{
    if (currentNote == note) {
        targetEnv = 0.0f;
    }
}

void BassSynth::AllNotesOff()
{
    targetEnv = 0.0f;
    active = false;
    currentNote = -1;
}

float BassSynth::Process(float sampleRate)
{
    if (!active && env < 0.001f) {
        return 0.0f;
    }

    // Safety check for uninitialized frequency
    if (freq <= 0.0f) {
        freq = 110.0f;
    }

    // Decay/release rates affected by paramVcaDecay
    float actualDecayRate = decayRate * (1.5f - paramVcaDecay);
    float actualReleaseRate = releaseRate * (1.5f - paramVcaDecay);

    // Envelope with attack-decay-release
    if (targetEnv > env) {
        // Attack
        env += (targetEnv - env) * attackRate;
    } else if (targetEnv < 0.001f) {
        // Release
        env -= env * actualReleaseRate;
    } else {
        // Decay to sustain
        env += (0.7f - env) * actualDecayRate;  // Sustain at 0.7
    }

    // Deactivate when envelope is done
    if (targetEnv == 0.0f && env < 0.001f) {
        active = false;
        currentNote = -1;
        return 0.0f;
    }

    // Sawtooth oscillator
    phase += freq / sampleRate;
    if (phase > 1.0f) phase -= 1.0f;

    float saw = (phase * 2.0f) - 1.0f;

    // Base cutoff affected by paramFilterCutoff
    float actualBaseCutoff = baseCutoff * (0.5f + paramFilterCutoff);
    float actualAccentCutoff = accentCutoff * (0.5f + paramFilterCutoff);

    // Envelope modulation depth affected by paramFilterEnvAmt
    float envMod = env * (0.2f + paramFilterEnvAmt * 0.4f);

    // Calculate filter cutoff based on accent and envelope
    float cutoff = isAccent
        ? actualAccentCutoff + envMod
        : actualBaseCutoff + envMod;

    // Clamp cutoff to safe range for filter stability
    if (cutoff > 0.7f) cutoff = 0.7f;
    if (cutoff < 0.05f) cutoff = 0.05f;

    // Resonant low-pass filter (2-pole)
    float maxFeedback = 3.5f;
    float feedback = resonance * (1.0f + cutoff * 2.0f);
    if (feedback > maxFeedback) feedback = maxFeedback;

    float input = saw - filterState2 * feedback;

    // Clamp input to prevent runaway
    if (input > 2.0f) input = 2.0f;
    if (input < -2.0f) input = -2.0f;

    filterState += cutoff * (input - filterState);
    filterState2 += cutoff * (filterState - filterState2);

    // Clamp filter states
    if (filterState > 10.0f) filterState = 10.0f;
    if (filterState < -10.0f) filterState = -10.0f;
    if (filterState2 > 10.0f) filterState2 = 10.0f;
    if (filterState2 < -10.0f) filterState2 = -10.0f;

    // Soft clip
    float output = filterState2;
    if (output > 0.8f) output = 0.8f + (output - 0.8f) * 0.3f;
    if (output < -0.8f) output = -0.8f + (output + 0.8f) * 0.3f;

    // Apply velocity and envelope
    return output * env * velocity * 0.5f;
}

// ============================================================================
// AUDIO ENGINE
// ============================================================================

bool AudioEngine::Init(int rate)
{
    sampleRate = (float)rate;

    // Log available audio drivers
    int numDrivers = SDL_GetNumAudioDrivers();
    std::cout << "Available audio drivers: ";
    for (int i = 0; i < numDrivers; i++) {
        std::cout << SDL_GetAudioDriver(i);
        if (i < numDrivers - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << "Current audio driver: " << (SDL_GetCurrentAudioDriver() ? SDL_GetCurrentAudioDriver() : "none") << std::endl;

    SDL_AudioSpec desired, obtained;
    SDL_zero(desired);

    desired.freq = rate;
    desired.format = AUDIO_F32SYS;
    desired.channels = 2;
    desired.samples = 512;  // Slightly larger buffer for stability
    desired.callback = AudioCallback;
    desired.userdata = this;

    // Try to open with some flexibility in format
    audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained,
                                       SDL_AUDIO_ALLOW_FREQUENCY_CHANGE |
                                       SDL_AUDIO_ALLOW_SAMPLES_CHANGE);
    if (audioDevice == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;

        // List available audio devices for debugging
        int numDevices = SDL_GetNumAudioDevices(0);
        std::cout << "Available audio output devices (" << numDevices << "):" << std::endl;
        for (int i = 0; i < numDevices; i++) {
            std::cout << "  " << i << ": " << SDL_GetAudioDeviceName(i, 0) << std::endl;
        }

        return false;
    }

    std::cout << "Audio device opened successfully" << std::endl;
    std::cout << "  Requested: " << rate << " Hz, " << desired.samples << " samples" << std::endl;
    std::cout << "  Obtained:  " << obtained.freq << " Hz, " << obtained.samples << " samples" << std::endl;

    sampleRate = (float)obtained.freq;

    // Start audio playback
    SDL_PauseAudioDevice(audioDevice, 0);

    return true;
}

void AudioEngine::Pause()
{
    if (audioDevice != 0) {
        SDL_PauseAudioDevice(audioDevice, 1);
        // Wait for any in-flight audio callback to finish and release synthMutex
        SDL_Delay(100);
        // Acquire and release the mutex to guarantee the callback has exited
        synthMutex.lock();
        synthMutex.unlock();
    }
}

void AudioEngine::Shutdown()
{
    if (audioDevice != 0) {
        // Pause and wait for callbacks to finish
        Pause();

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

void AudioEngine::TriggerChordNotes(const int8_t* notes, uint8_t count, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    for (uint8_t i = 0; i < count && i < 6; i++) {
        padSynth.NoteOn(notes[i], velocity);
    }
}

void AudioEngine::ReleaseChordNotes(const int8_t* notes, uint8_t count)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    for (uint8_t i = 0; i < count && i < 6; i++) {
        padSynth.NoteOff(notes[i]);
    }
}

void AudioEngine::StopAllChordNotes()
{
    std::lock_guard<std::mutex> lock(synthMutex);
    padSynth.AllNotesOff();
}

void AudioEngine::TriggerRhythmNotes(const int8_t* notes, uint8_t count, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    for (uint8_t i = 0; i < count && i < 6; i++) {
        rhythmSynth.NoteOn(notes[i], velocity);
    }
}

void AudioEngine::ReleaseRhythmNotes(const int8_t* notes, uint8_t count)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    for (uint8_t i = 0; i < count && i < 6; i++) {
        rhythmSynth.NoteOff(notes[i]);
    }
}

void AudioEngine::StopAllRhythmNotes()
{
    std::lock_guard<std::mutex> lock(synthMutex);
    rhythmSynth.AllNotesOff();
}

void AudioEngine::TriggerBass(int8_t note, uint8_t velocity)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    bassSynth.NoteOn(note, velocity);
}

void AudioEngine::StopBass(int8_t note)
{
    std::lock_guard<std::mutex> lock(synthMutex);
    bassSynth.NoteOff(note);
}

void AudioEngine::StopAllBassNotes()
{
    std::lock_guard<std::mutex> lock(synthMutex);
    bassSynth.AllNotesOff();
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

    // Update per-voice synth parameters from atomics
    // Drums
    kick.paramFilterCutoff = kickFilterCutoff.load();
    kick.paramVcaDecay = kickVcaDecay.load();
    kick.paramFilterEnvAmt = kickFilterEnvAmount.load();

    for (int j = 0; j < NUM_SNARE; j++) {
        snare[j].paramFilterCutoff = snareFilterCutoff.load();
        snare[j].paramVcaDecay = snareVcaDecay.load();
        snare[j].paramFilterEnvAmt = snareFilterEnvAmount.load();
    }

    for (int j = 0; j < NUM_HIHAT; j++) {
        hihat[j].paramFilterCutoff = hihatFilterCutoff.load();
        hihat[j].paramVcaDecay = hihatVcaDecay.load();
    }

    clap.paramFilterCutoff = clapFilterCutoff.load();
    clap.paramVcaDecay = clapVcaDecay.load();

    for (int j = 0; j < NUM_TOM; j++) {
        tom[j].paramFilterCutoff = tomFilterCutoff.load();
        tom[j].paramVcaDecay = tomVcaDecay.load();
    }

    // Melodic voices
    rhythmSynth.paramFilterCutoff = rhythmFilterCutoff.load();
    rhythmSynth.paramVcaDecay = rhythmVcaDecay.load();
    rhythmSynth.paramVcoType = rhythmVcoType.load();
    rhythmSynth.paramFilterEnvAmt = rhythmFilterEnvAmount.load();

    bassSynth.paramFilterCutoff = bassFilterCutoff.load();
    bassSynth.paramVcaDecay = bassVcaDecay.load();
    bassSynth.paramFilterEnvAmt = bassFilterEnvAmount.load();

    padSynth.paramFilterCutoff = padFilterCutoff.load();
    padSynth.paramVcaDecay = padVcaDecay.load();
    padSynth.paramVcoType = padVcoType.load();
    padSynth.paramFilterEnvAmt = padFilterEnvAmount.load();

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

        // Melody synth (square wave with filter)
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

        // Pad synth (polyphonic chords)
        sample += padSynth.Process(sampleRate);

        // Rhythm player synth
        sample += rhythmSynth.Process(sampleRate);

        // Bass synth
        sample += bassSynth.Process(sampleRate);

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
