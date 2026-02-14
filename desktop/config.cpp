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

// Helper to load synth params from a key prefix
static void LoadSynthParam(const std::string& key, const std::string& value,
                           const std::string& prefix, VoiceSynthParams& params)
{
    if (key == prefix + "_filter_cutoff") {
        params.filterCutoff = std::stof(value);
    } else if (key == prefix + "_filter_decay") {
        params.filterDecay = std::stof(value);
    } else if (key == prefix + "_vca_decay") {
        params.vcaDecay = std::stof(value);
    } else if (key == prefix + "_vco_type") {
        params.vcoType = std::stoi(value);
    } else if (key == prefix + "_filter_env_amount") {
        params.filterEnvAmount = std::stof(value);
    }
}

// Helper to save synth params with a key prefix
static void SaveSynthParams(std::ofstream& file, const std::string& prefix,
                            const VoiceSynthParams& params)
{
    file << prefix << "_filter_cutoff=" << params.filterCutoff << "\n";
    file << prefix << "_filter_decay=" << params.filterDecay << "\n";
    file << prefix << "_vca_decay=" << params.vcaDecay << "\n";
    file << prefix << "_vco_type=" << params.vcoType << "\n";
    file << prefix << "_filter_env_amount=" << params.filterEnvAmount << "\n";
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
        } else if (key == "melody_channel") {
            settings.melodyChannel = std::stoi(value);
        } else if (key == "melody_mute") {
            settings.melodyMute = (value == "true" || value == "1");
        } else if (key == "melody_solo") {
            settings.melodySolo = (value == "true" || value == "1");
        } else if (key == "chord_mute") {
            settings.chordMute = (value == "true" || value == "1");
        } else if (key == "chord_solo") {
            settings.chordSolo = (value == "true" || value == "1");
        } else if (key == "melody_active") {
            settings.melodyActive = (value == "true" || value == "1");
        } else if (key == "chord_active") {
            settings.chordActive = (value == "true" || value == "1");
        } else if (key == "rhythm_active") {
            settings.rhythmActive = (value == "true" || value == "1");
        } else if (key == "rhythm_mute") {
            settings.rhythmMute = (value == "true" || value == "1");
        } else if (key == "rhythm_solo") {
            settings.rhythmSolo = (value == "true" || value == "1");
        } else if (key == "rhythm_mode") {
            settings.rhythmMode = std::stoi(value);
        } else if (key == "rhythm_play_style") {
            settings.rhythmPlayStyle = std::stoi(value);
        } else if (key == "rhythm_midi_channel") {
            settings.rhythmMidiChannel = std::stoi(value);
        } else if (key == "rhythm_octave_offset") {
            settings.rhythmOctaveOffset = std::stoi(value);
        } else if (key == "bass_active") {
            settings.bassActive = (value == "true" || value == "1");
        } else if (key == "bass_mute") {
            settings.bassMute = (value == "true" || value == "1");
        } else if (key == "bass_solo") {
            settings.bassSolo = (value == "true" || value == "1");
        } else if (key == "bass_freeze_pattern") {
            settings.bassFreezePattern = (value == "true" || value == "1");
        } else if (key == "bass_fills_enabled") {
            settings.bassFillsEnabled = (value == "true" || value == "1");
        } else if (key == "bass_octave_random") {
            settings.bassOctaveRandom = std::stoi(value);
        } else if (key == "bass_midi_channel") {
            settings.bassMidiChannel = std::stoi(value);
        } else if (key == "bass_octave_offset") {
            settings.bassOctaveOffset = std::stoi(value);
        } else if (key == "bass_variation_mode" || key == "bass_rhythm_variation_mode") {
            settings.bassRhythmVariationMode = std::stoi(value);
        } else if (key == "bass_variation_sequence" || key == "bass_rhythm_variation_sequence") {
            settings.bassRhythmVariationSequence = std::stoi(value);
        } else if (key == "bass_pitch_variation_mode") {
            settings.bassPitchVariationMode = std::stoi(value);
        } else if (key == "bass_pitch_variation_sequence") {
            settings.bassPitchVariationSequence = std::stoi(value);
        } else if (key.substr(0, 10) == "drum_mute_") {
            int idx = std::stoi(key.substr(10));
            if (idx >= 0 && idx < 11) {
                settings.drumMute[idx] = (value == "true" || value == "1");
            }
        } else if (key.substr(0, 10) == "drum_solo_") {
            int idx = std::stoi(key.substr(10));
            if (idx >= 0 && idx < 11) {
                settings.drumSolo[idx] = (value == "true" || value == "1");
            }
        } else if (key == "chord_freeze_enabled") {
            settings.chordFreezeEnabled = (value == "true" || value == "1");
        } else if (key == "chord_enabled_vibes") {
            settings.chordEnabledVibes = (uint8_t)std::stoi(value);
        } else if (key.substr(0, 26) == "chord_enabled_progressions") {
            int idx = std::stoi(key.substr(27));
            if (idx >= 0 && idx < 5) {
                settings.chordEnabledProgressions[idx] = (uint32_t)std::stoul(value);
            }
        } else if (key == "chord_progression_index") {
            settings.chordProgressionIndex = std::stoi(value);
        } else if (key == "chord_rate") {
            settings.chordRate = std::stoi(value);
        } else if (key == "chord_octave_offset") {
            settings.chordOctaveOffset = std::stoi(value);
        } else if (key == "chord_midi_channel") {
            settings.chordMidiChannel = std::stoi(value);
        }

        // Synth parameters for each voice
        LoadSynthParam(key, value, "kick_synth", settings.kickSynth);
        LoadSynthParam(key, value, "snare_synth", settings.snareSynth);
        LoadSynthParam(key, value, "hihat_synth", settings.hihatSynth);
        LoadSynthParam(key, value, "clap_synth", settings.clapSynth);
        LoadSynthParam(key, value, "tom_synth", settings.tomSynth);
        LoadSynthParam(key, value, "rhythm_synth", settings.rhythmSynth);
        LoadSynthParam(key, value, "bass_synth", settings.bassSynth);
        LoadSynthParam(key, value, "pad_synth", settings.padSynth);
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
    file << "melody_channel=" << settings.melodyChannel << "\n\n";

    file << "[mixer]\n";
    for (int i = 0; i < 11; i++) {
        file << "drum_mute_" << i << "=" << (settings.drumMute[i] ? "true" : "false") << "\n";
        file << "drum_solo_" << i << "=" << (settings.drumSolo[i] ? "true" : "false") << "\n";
    }
    file << "melody_mute=" << (settings.melodyMute ? "true" : "false") << "\n";
    file << "melody_solo=" << (settings.melodySolo ? "true" : "false") << "\n";
    file << "chord_mute=" << (settings.chordMute ? "true" : "false") << "\n";
    file << "chord_solo=" << (settings.chordSolo ? "true" : "false") << "\n\n";

    file << "[voices]\n";
    file << "melody_active=" << (settings.melodyActive ? "true" : "false") << "\n";
    file << "chord_active=" << (settings.chordActive ? "true" : "false") << "\n\n";

    file << "[rhythm]\n";
    file << "rhythm_active=" << (settings.rhythmActive ? "true" : "false") << "\n";
    file << "rhythm_mute=" << (settings.rhythmMute ? "true" : "false") << "\n";
    file << "rhythm_solo=" << (settings.rhythmSolo ? "true" : "false") << "\n";
    file << "rhythm_mode=" << settings.rhythmMode << "\n";
    file << "rhythm_play_style=" << settings.rhythmPlayStyle << "\n";
    file << "rhythm_midi_channel=" << settings.rhythmMidiChannel << "\n";
    file << "rhythm_octave_offset=" << settings.rhythmOctaveOffset << "\n\n";

    file << "[bass]\n";
    file << "bass_active=" << (settings.bassActive ? "true" : "false") << "\n";
    file << "bass_mute=" << (settings.bassMute ? "true" : "false") << "\n";
    file << "bass_solo=" << (settings.bassSolo ? "true" : "false") << "\n";
    file << "bass_freeze_pattern=" << (settings.bassFreezePattern ? "true" : "false") << "\n";
    file << "bass_fills_enabled=" << (settings.bassFillsEnabled ? "true" : "false") << "\n";
    file << "bass_octave_random=" << settings.bassOctaveRandom << "\n";
    file << "bass_midi_channel=" << settings.bassMidiChannel << "\n";
    file << "bass_octave_offset=" << settings.bassOctaveOffset << "\n";
    file << "bass_rhythm_variation_mode=" << settings.bassRhythmVariationMode << "\n";
    file << "bass_rhythm_variation_sequence=" << settings.bassRhythmVariationSequence << "\n";
    file << "bass_pitch_variation_mode=" << settings.bassPitchVariationMode << "\n";
    file << "bass_pitch_variation_sequence=" << settings.bassPitchVariationSequence << "\n\n";

    file << "[chords]\n";
    file << "chord_freeze_enabled=" << (settings.chordFreezeEnabled ? "true" : "false") << "\n";
    file << "chord_enabled_vibes=" << (int)settings.chordEnabledVibes << "\n";
    for (int i = 0; i < 5; i++) {
        file << "chord_enabled_progressions_" << i << "=" << settings.chordEnabledProgressions[i] << "\n";
    }
    file << "chord_progression_index=" << settings.chordProgressionIndex << "\n";
    file << "chord_rate=" << settings.chordRate << "\n";
    file << "chord_octave_offset=" << settings.chordOctaveOffset << "\n";
    file << "chord_midi_channel=" << settings.chordMidiChannel << "\n\n";

    file << "[synth_params]\n";
    SaveSynthParams(file, "kick_synth", settings.kickSynth);
    SaveSynthParams(file, "snare_synth", settings.snareSynth);
    SaveSynthParams(file, "hihat_synth", settings.hihatSynth);
    SaveSynthParams(file, "clap_synth", settings.clapSynth);
    SaveSynthParams(file, "tom_synth", settings.tomSynth);
    SaveSynthParams(file, "rhythm_synth", settings.rhythmSynth);
    SaveSynthParams(file, "bass_synth", settings.bassSynth);
    SaveSynthParams(file, "pad_synth", settings.padSynth);

    file.close();
    return true;
}

} // namespace themis_config
