/**
 * @file config.h
 * @brief Configuration persistence for Themis desktop
 */

#ifndef THEMIS_CONFIG_H
#define THEMIS_CONFIG_H

#include <string>

namespace themis_config {

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
