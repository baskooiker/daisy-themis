/**
 * @file config.h
 * @brief Configuration persistence for Themis desktop
 */

#ifndef THEMIS_CONFIG_H
#define THEMIS_CONFIG_H

#include <string>
#include <cstdint>

namespace themis_config {

/**
 * @enum VcoType
 * @brief Oscillator waveform type
 */
enum VcoType {
    VCO_SAW = 0,
    VCO_SQUARE,
    VCO_TRIANGLE,
    VCO_SINE,
    NUM_VCO_TYPES
};

/**
 * @struct VoiceSynthParams
 * @brief Per-voice synthesizer parameters
 */
struct VoiceSynthParams {
    float filterCutoff = 1.0f;      ///< Filter cutoff (0.0-1.0)
    float filterDecay = 0.5f;       ///< Filter envelope decay (0.0-1.0)
    float vcaDecay = 0.5f;          ///< VCA envelope decay (0.0-1.0)
    int vcoType = VCO_SAW;          ///< Oscillator waveform type
    float filterEnvAmount = 0.5f;   ///< Filter envelope modulation depth (0.0-1.0)
};

/**
 * @struct Settings
 * @brief All persistable application settings
 */
struct Settings {
    // Audio settings
    float volume = 0.7f;
    float filterCutoff = 1.0f;
    float decayAmount = 0.5f;
    bool muted = false;

    // Transport settings
    float bpm = 120.0f;

    // Global settings
    int patternChangeInterval = 4;
    int personalityChangeInterval = 4;

    // MIDI settings
    int midiPort = -1;
    std::string midiPortName;
    int melodyChannel = 0;     // 0-indexed (displayed as 1-16)

    // Mixer mute/solo settings (11 drum voices + melody + chord)
    bool drumMute[11] = {false};
    bool drumSolo[11] = {false};
    bool melodyMute = false;
    bool melodySolo = false;
    bool chordMute = false;
    bool chordSolo = false;

    // Voice activation states
    bool melodyActive = true;
    bool chordActive = true;

    // Rhythm player settings
    bool rhythmActive = true;
    bool rhythmMute = false;
    bool rhythmSolo = false;
    int rhythmMode = 1;        // 0=Manual, 1=Morph
    int rhythmPlayStyle = 0;   // 0=Chords, 1=Poly
    int rhythmMidiChannel = 3; // 0-indexed
    int rhythmOctaveOffset = 0;

    // Bass voice settings
    bool bassActive = true;
    bool bassMute = false;
    bool bassSolo = false;
    bool bassFreezePattern = false;
    bool bassFillsEnabled = true;
    int bassOctaveRandom = 0;
    int bassMidiChannel = 4;   // 0-indexed
    int bassOctaveOffset = -1;
    int bassRhythmVariationMode = 0;     // 0=Off, 1=AB
    int bassRhythmVariationSequence = 1; // VAR_SEQ_AAAB
    int bassPitchVariationMode = 0;      // 0=Off, 1=AB
    int bassPitchVariationSequence = 1;  // VAR_SEQ_AAAB

    // Per-voice synth parameters
    VoiceSynthParams kickSynth;
    VoiceSynthParams snareSynth;
    VoiceSynthParams hihatSynth;
    VoiceSynthParams clapSynth;
    VoiceSynthParams tomSynth;
    VoiceSynthParams rhythmSynth;
    VoiceSynthParams bassSynth;
    VoiceSynthParams padSynth;

    // Chord randomizer settings
    bool chordFreezeEnabled = false;
    uint8_t chordEnabledVibes = 0x1F;  // Bitmask: all 5 vibes enabled
    uint32_t chordEnabledProgressions[5] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};  // Per-vibe
    int chordProgressionIndex = 0;
    int chordRate = 1;  // CHORD_RATE_1_BAR
    int chordOctaveOffset = 0;
    int chordMidiChannel = 1;  // 0-indexed (displayed as 1-16)
};

/**
 * @brief Get the config directory path (~/.config/themis/)
 */
std::string GetConfigDir();

/**
 * @brief Get the config file path (~/.config/themis/config.ini)
 */
std::string GetConfigPath();

/**
 * @brief Ensure config directory exists
 * @return true if directory exists or was created successfully
 */
bool EnsureConfigDir();

/**
 * @brief Load settings from config file
 * @param settings Settings struct to populate
 * @return true if loaded successfully, false if file doesn't exist or error
 */
bool LoadSettings(Settings& settings);

/**
 * @brief Save settings to config file
 * @param settings Settings to save
 * @return true if saved successfully
 */
bool SaveSettings(const Settings& settings);

} // namespace themis_config

#endif // THEMIS_CONFIG_H
