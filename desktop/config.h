/**
 * @file config.h
 * @brief Configuration persistence for Themis desktop
 */

#ifndef THEMIS_CONFIG_H
#define THEMIS_CONFIG_H

#include <string>

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

    // Mixer mute/solo settings (11 drum voices + melody CV + melody MIDI + poly)
    bool drumMute[11] = {false};
    bool drumSolo[11] = {false};
    bool melodyCVMute = false;
    bool melodyCVSolo = false;
    bool melodyMidiMute = false;
    bool melodyMidiSolo = false;
    bool polyMute = false;
    bool polySolo = false;

    // Voice activation states
    bool melodyCVActive = true;
    bool melodyMidiActive = true;
    bool polyActive = true;

    // Rhythm player settings
    bool rhythmActive = true;
    bool rhythmMute = false;
    bool rhythmSolo = false;
    int rhythmMode = 1;        // 0=Manual, 1=Morph
    int rhythmPlayStyle = 0;   // 0=Chords, 1=Arps, 2=Poly
    int rhythmMidiChannel = 3; // 0-indexed
    int rhythmOctaveOffset = 0;

    // Acid voice settings
    bool acidActive = true;
    bool acidMute = false;
    bool acidSolo = false;
    int acidMode = 1;          // 0=Manual, 1=Auto
    int acidRhythmPattern = 0;
    int acidMelodyPattern = 0;
    int acidActivity = 1;      // 0=Sparse, 1=Moderate, 2=Busy
    int acidMidiChannel = 4;   // 0-indexed
    int acidOctaveOffset = -1;

    // Per-voice synth parameters
    VoiceSynthParams kickSynth;
    VoiceSynthParams snareSynth;
    VoiceSynthParams hihatSynth;
    VoiceSynthParams clapSynth;
    VoiceSynthParams tomSynth;
    VoiceSynthParams rhythmSynth;
    VoiceSynthParams acidSynth;
    VoiceSynthParams padSynth;
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
