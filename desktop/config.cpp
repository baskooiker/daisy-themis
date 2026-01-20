/**
 * @file config.cpp
 * @brief Configuration persistence implementation
 */

#include "config.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <sys/stat.h>
#include <iostream>

namespace themis_config {

std::string GetConfigDir()
{
    const char* home = std::getenv("HOME");
    if (!home) {
        home = "/tmp";
    }
    return std::string(home) + "/.config/themis";
}

std::string GetConfigPath()
{
    return GetConfigDir() + "/config.ini";
}

bool EnsureConfigDir()
{
    std::string configDir = GetConfigDir();

    // Check if directory exists
    struct stat st;
    if (stat(configDir.c_str(), &st) == 0) {
        return S_ISDIR(st.st_mode);
    }

    // Create ~/.config if it doesn't exist
    std::string parentDir = std::string(std::getenv("HOME") ? std::getenv("HOME") : "/tmp") + "/.config";
    mkdir(parentDir.c_str(), 0755);

    // Create ~/.config/themis
    return mkdir(configDir.c_str(), 0755) == 0;
}

static std::string Trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool LoadSettings(Settings& settings)
{
    std::string path = GetConfigPath();
    std::ifstream file(path);

    if (!file.is_open()) {
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Parse key=value
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, eq));
        std::string value = Trim(line.substr(eq + 1));

        // Match keys to settings
        if (key == "volume") {
            settings.volume = std::stof(value);
        } else if (key == "filter_cutoff") {
            settings.filterCutoff = std::stof(value);
        } else if (key == "decay_amount") {
            settings.decayAmount = std::stof(value);
        } else if (key == "muted") {
            settings.muted = (value == "true" || value == "1");
        } else if (key == "bpm") {
            settings.bpm = std::stof(value);
        } else if (key == "pattern_change_interval") {
            settings.patternChangeInterval = std::stoi(value);
        } else if (key == "personality_change_interval") {
            settings.personalityChangeInterval = std::stoi(value);
        } else if (key == "midi_port") {
            settings.midiPort = std::stoi(value);
        } else if (key == "midi_port_name") {
            settings.midiPortName = value;
        }
    }

    file.close();
    return true;
}

bool SaveSettings(const Settings& settings)
{
    if (!EnsureConfigDir()) {
        std::cerr << "Failed to create config directory" << std::endl;
        return false;
    }

    std::string path = GetConfigPath();
    std::ofstream file(path);

    if (!file.is_open()) {
        std::cerr << "Failed to open config file for writing: " << path << std::endl;
        return false;
    }

    file << "# Themis Desktop Configuration\n";
    file << "# This file is auto-generated. Edit at your own risk.\n\n";

    file << "[audio]\n";
    file << "volume=" << settings.volume << "\n";
    file << "filter_cutoff=" << settings.filterCutoff << "\n";
    file << "decay_amount=" << settings.decayAmount << "\n";
    file << "muted=" << (settings.muted ? "true" : "false") << "\n\n";

    file << "[transport]\n";
    file << "bpm=" << settings.bpm << "\n\n";

    file << "[global]\n";
    file << "pattern_change_interval=" << settings.patternChangeInterval << "\n";
    file << "personality_change_interval=" << settings.personalityChangeInterval << "\n\n";

    file << "[midi]\n";
    file << "midi_port=" << settings.midiPort << "\n";
    file << "midi_port_name=" << settings.midiPortName << "\n";

    file.close();
    return true;
}

} // namespace themis_config
