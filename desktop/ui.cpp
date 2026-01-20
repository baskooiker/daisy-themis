/**
 * @file ui.cpp
 * @brief ImGui user interface implementation
 */

#include "ui.h"
#include "platform_desktop.h"
#include "themis_data.h"
#include "themis_patterns.h"
#include "audio.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <SDL2/SDL.h>

#ifdef THEMIS_ENABLE_MIDI
#include "midi_out.h"
#endif

namespace themis_ui {

// Global instance
ThemisUI g_ui;

void ThemisUI::Init(themis::Sequencer* seq)
{
    sequencer = seq;

    // Initialize mixer state
    for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
        drumMute[i] = false;
        drumSolo[i] = false;
        drumActivity[i] = 0.0f;
    }
    melodyCVMute = false;
    melodyCVSolo = false;
    melodyMidiMute = false;
    melodyMidiSolo = false;
}

bool ThemisUI::IsAnySoloActive() const
{
    for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
        if (drumSolo[i]) return true;
    }
    return melodyCVSolo || melodyMidiSolo;
}

bool ThemisUI::ShouldPlayDrum(themis::DrumVoice voice) const
{
    if (drumMute[voice]) return false;

    if (IsAnySoloActive()) {
        return drumSolo[voice];
    }
    return true;
}

bool ThemisUI::ShouldPlayMelodyCV() const
{
    if (melodyCVMute) return false;

    if (IsAnySoloActive()) {
        return melodyCVSolo;
    }
    return true;
}

bool ThemisUI::ShouldPlayMelodyMidi() const
{
    if (melodyMidiMute) return false;

    if (IsAnySoloActive()) {
        return melodyMidiSolo;
    }
    return true;
}

const char* ThemisUI::GetRhythmStyleName(themis::RhythmStyle style)
{
    return themis::rhythmStyleNames[style];
}

const char* ThemisUI::GetDensityName(themis::DensityLevel density)
{
    return themis::densityNames[density];
}

const char* ThemisUI::GetInteractionStyleName(themis::InteractionStyle style)
{
    return themis::interactionStyleNames[style];
}

const char* ThemisUI::GetMelodyStyleName(themis::MelodyStyle style)
{
    return themis::melodyStyleNames[style];
}

const char* ThemisUI::GetScaleName(themis::ScaleType scale)
{
    return themis::scaleNames[scale];
}

const char* ThemisUI::GetVariationModeName(themis::VariationMode mode)
{
    return themis::variationModeNames[mode];
}

const char* ThemisUI::GetVariationSequenceName(themis::VariationSequence seq)
{
    return themis::variationSequenceNames[seq];
}

void ThemisUI::Render()
{
    if (!sequencer) return;

    // Get display size for full-window layout
    ImGuiIO& io = ImGui::GetIO();
    float mixerHeight = 160.0f;

    // Main window - fill screen except for mixer at bottom
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - mixerHeight));

    ImGui::Begin("Themis - Generative Drum Sequencer", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar);

    // Menu bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("ImGui Demo", nullptr, &showDemoWindow);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Sections
    RenderTransportControls();
    ImGui::Separator();

    if (ImGui::BeginTabBar("MainTabs")) {
        if (ImGui::BeginTabItem("Voices & Patterns")) {
            RenderVoicesAndPatterns();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Global")) {
            RenderGlobalControls();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Output")) {
            RenderOutputSection();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();

    // Mixer window at bottom
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - mixerHeight));
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, mixerHeight));

    ImGui::Begin("Mixer", nullptr,
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);
    RenderMixer();
    ImGui::End();

    // Demo window
    if (showDemoWindow) {
        ImGui::ShowDemoWindow(&showDemoWindow);
    }
}

void ThemisUI::RenderTransportControls()
{
    ImGui::Text("Transport");
    ImGui::SameLine();

    // Play/Stop button
    if (sequencer->isRunning) {
        if (ImGui::Button("Stop")) {
            sequencer->Stop();
        }
    } else {
        if (ImGui::Button("Play")) {
            sequencer->Start();
        }
    }

    ImGui::SameLine();

    // BPM control
    ImGui::SetNextItemWidth(80);
    ImGui::DragFloat("BPM", &sequencer->bpm, 1.0f, 20.0f, 300.0f, "%.0f");

    ImGui::SameLine();

    // Step display
    ImGui::Text("Step: %d/32  Bar: %d/4  Cycle: %d",
                sequencer->currentStep + 1,
                sequencer->barCounter + 1,
                sequencer->cycleCounter);

    ImGui::SameLine();

    // Current pattern index
    ImGui::Text("Kick:%d Clap:%d Hat:%d Groove:%d",
                sequencer->currentKickPattern,
                sequencer->currentClapPattern,
                sequencer->currentHatPattern,
                sequencer->currentGroovePattern);
}

void ThemisUI::RenderVoicesAndPatterns()
{
    // Create a two-column layout: voices on left, patterns on right
    float availWidth = ImGui::GetContentRegionAvail().x;
    float leftWidth = availWidth * 0.45f;
    float rightWidth = availWidth * 0.55f;

    // Left column - Voice parameters
    ImGui::BeginChild("VoiceParams", ImVec2(leftWidth, 0), true);

    // Drum section header with buttons
    ImGui::Text("DRUM VOICES");
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Patterns")) {
        sequencer->RandomizePatterns();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Personality")) {
        sequencer->RandomizeVoicePersonalities();
        sequencer->GenerateVoicePatterns();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Freeze##Drums", &sequencer->freezeEnabled);

    ImGui::Separator();

    // Compact voice parameters
    for (int i = 0; i < 6; i++) {
        RenderCompactVoiceRow(&sequencer->generativeVoices[i], i);
    }

    ImGui::Spacing();
    ImGui::Separator();

    // Melody section header
    ImGui::Text("MELODY VOICES");
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Melody")) {
        sequencer->RandomizeMelodyPersonality();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Freeze##Melody", &sequencer->melodyFreezeEnabled);

    // Scale and root on same line
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("Scale", GetScaleName(sequencer->melodyScale))) {
        for (int s = 0; s < themis::NUM_SCALE_TYPES; s++) {
            if (ImGui::Selectable(themis::scaleNames[s], sequencer->melodyScale == s)) {
                sequencer->melodyScale = (themis::ScaleType)s;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    if (ImGui::BeginCombo("Root", themis::rootNoteNames[sequencer->melodyRoot])) {
        for (int r = 0; r < 12; r++) {
            if (ImGui::Selectable(themis::rootNoteNames[r], sequencer->melodyRoot == r)) {
                sequencer->melodyRoot = r;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Compact melody parameters
    RenderCompactMelodyRow(&sequencer->melodyVoice, "CV");
    RenderCompactMelodyRow(&sequencer->melodyMidiVoice, "MIDI");

    ImGui::EndChild();

    ImGui::SameLine();

    // Right column - Pattern visualization
    ImGui::BeginChild("PatternViz", ImVec2(rightWidth, 0), true);
    RenderPatternVisualization();
    ImGui::EndChild();
}

void ThemisUI::RenderCompactVoiceRow(themis::VoiceConfig* voice, int index)
{
    ImGui::PushID(index);

    // Voice name and active toggle
    ImGui::Checkbox(themis::drumNames[voice->voice], &voice->active);

    // Rhythm style
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo("##rhythm", GetRhythmStyleName(voice->rhythmStyle))) {
        for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
            if (ImGui::Selectable(themis::rhythmStyleNames[s], voice->rhythmStyle == s)) {
                voice->rhythmStyle = (themis::RhythmStyle)s;
            }
        }
        ImGui::EndCombo();
    }

    // Density
    ImGui::SameLine();
    ImGui::SetNextItemWidth(55);
    if (ImGui::BeginCombo("##density", GetDensityName(voice->density))) {
        for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
            if (ImGui::Selectable(themis::densityNames[d], voice->density == d)) {
                voice->density = (themis::DensityLevel)d;
            }
        }
        ImGui::EndCombo();
    }

    // Variation sequence (combines mode and sequence selection)
    ImGui::SameLine();
    ImGui::SetNextItemWidth(75);
    const char* currentSeqName = (voice->variation.mode == themis::VAR_MODE_OFF)
        ? "Off" : themis::variationSequenceNames[voice->variation.sequence];
    if (ImGui::BeginCombo("##var", currentSeqName)) {
        // "Off" option
        if (ImGui::Selectable("Off", voice->variation.mode == themis::VAR_MODE_OFF)) {
            voice->variation.mode = themis::VAR_MODE_OFF;
        }
        // Sequence options (skip AAAA since that's effectively Off)
        for (int s = 1; s < themis::NUM_VARIATION_SEQUENCES; s++) {
            bool isSelected = (voice->variation.mode != themis::VAR_MODE_OFF &&
                              voice->variation.sequence == s);
            if (ImGui::Selectable(themis::variationSequenceNames[s], isSelected)) {
                voice->variation.sequence = (themis::VariationSequence)s;
                // Set mode based on whether sequence uses C (ABAC, AAABAAAC)
                if (s == themis::VAR_SEQ_ABAC || s == themis::VAR_SEQ_AAABAAAC) {
                    voice->variation.mode = themis::VAR_MODE_ABC;
                } else {
                    voice->variation.mode = themis::VAR_MODE_AB;
                }
            }
        }
        ImGui::EndCombo();
    }

    // Interaction (only show if not NONE)
    if (voice->interaction != themis::INTERACTION_NONE) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "[%s]",
                          GetInteractionStyleName(voice->interaction));
    }

    ImGui::PopID();
}

void ThemisUI::RenderCompactMelodyRow(themis::MelodyConfig* voice, const char* name)
{
    ImGui::PushID(name);

    // Name and active
    char label[16];
    snprintf(label, sizeof(label), "%s##active", name);
    ImGui::Checkbox(label, &voice->active);

    // Style
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65);
    if (ImGui::BeginCombo("##style", GetMelodyStyleName(voice->style))) {
        for (int s = 0; s < themis::NUM_MELODY_STYLES; s++) {
            if (ImGui::Selectable(themis::melodyStyleNames[s], voice->style == s)) {
                voice->style = (themis::MelodyStyle)s;
            }
        }
        ImGui::EndCombo();
    }

    // Rhythm
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70);
    if (ImGui::BeginCombo("##rhythm", GetRhythmStyleName(voice->rhythmStyle))) {
        for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
            if (ImGui::Selectable(themis::rhythmStyleNames[s], voice->rhythmStyle == s)) {
                voice->rhythmStyle = (themis::RhythmStyle)s;
            }
        }
        ImGui::EndCombo();
    }

    // Density
    ImGui::SameLine();
    ImGui::SetNextItemWidth(55);
    if (ImGui::BeginCombo("##density", GetDensityName(voice->density))) {
        for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
            if (ImGui::Selectable(themis::densityNames[d], voice->density == d)) {
                voice->density = (themis::DensityLevel)d;
            }
        }
        ImGui::EndCombo();
    }

    // Variation sequence (combines mode and sequence selection)
    ImGui::SameLine();
    ImGui::SetNextItemWidth(75);
    const char* currentSeqName = (voice->variation.mode == themis::VAR_MODE_OFF)
        ? "Off" : themis::variationSequenceNames[voice->variation.sequence];
    if (ImGui::BeginCombo("##var", currentSeqName)) {
        // "Off" option
        if (ImGui::Selectable("Off", voice->variation.mode == themis::VAR_MODE_OFF)) {
            voice->variation.mode = themis::VAR_MODE_OFF;
        }
        // Sequence options (skip AAAA since that's effectively Off)
        for (int s = 1; s < themis::NUM_VARIATION_SEQUENCES; s++) {
            bool isSelected = (voice->variation.mode != themis::VAR_MODE_OFF &&
                              voice->variation.sequence == s);
            if (ImGui::Selectable(themis::variationSequenceNames[s], isSelected)) {
                voice->variation.sequence = (themis::VariationSequence)s;
                // Set mode based on whether sequence uses C (ABAC, AAABAAAC)
                if (s == themis::VAR_SEQ_ABAC || s == themis::VAR_SEQ_AAABAAAC) {
                    voice->variation.mode = themis::VAR_MODE_ABC;
                } else {
                    voice->variation.mode = themis::VAR_MODE_AB;
                }
            }
        }
        ImGui::EndCombo();
    }

    ImGui::PopID();
}

void ThemisUI::RenderDrumVoices()
{
    ImGui::Text("Generative Drum Voices");
    ImGui::SameLine();
    if (ImGui::Button("Randomize Patterns")) {
        sequencer->RandomizePatterns();
    }
    ImGui::SameLine();
    if (ImGui::Button("Randomize Personalities")) {
        sequencer->RandomizeVoicePersonalities();
        sequencer->GenerateVoicePatterns();
    }

    ImGui::Spacing();

    // Freeze control
    ImGui::Checkbox("Freeze Drums", &sequencer->freezeEnabled);

    ImGui::Spacing();

    // Voice panels
    for (int i = 0; i < 6; i++) {
        RenderVoicePanel(&sequencer->generativeVoices[i], i);
    }
}

void ThemisUI::RenderVoicePanel(themis::VoiceConfig* voice, int index)
{
    char label[32];
    snprintf(label, sizeof(label), "Voice %d (%s)###Voice%d",
             index + 1, themis::drumNames[voice->voice], index);

    if (ImGui::CollapsingHeader(label)) {
        ImGui::PushID(index);
        ImGui::Indent();

        // Active toggle
        ImGui::Checkbox("Active", &voice->active);

        // Rhythm style
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        if (ImGui::BeginCombo("Style", GetRhythmStyleName(voice->rhythmStyle))) {
            for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
                if (ImGui::Selectable(themis::rhythmStyleNames[s],
                                       voice->rhythmStyle == s)) {
                    voice->rhythmStyle = (themis::RhythmStyle)s;
                }
            }
            ImGui::EndCombo();
        }

        // Density
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Density", GetDensityName(voice->density))) {
            for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
                if (ImGui::Selectable(themis::densityNames[d],
                                       voice->density == d)) {
                    voice->density = (themis::DensityLevel)d;
                }
            }
            ImGui::EndCombo();
        }

        // Pattern length
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        int len = voice->patternLength;
        if (ImGui::DragInt("Len", &len, 1, 4, 32)) {
            voice->patternLength = len;
        }

        // Interaction
        ImGui::SetNextItemWidth(100);
        if (ImGui::BeginCombo("Interaction", GetInteractionStyleName(voice->interaction))) {
            for (int i = 0; i < themis::NUM_INTERACTION_STYLES; i++) {
                if (ImGui::Selectable(themis::interactionStyleNames[i],
                                       voice->interaction == i)) {
                    voice->interaction = (themis::InteractionStyle)i;
                }
            }
            ImGui::EndCombo();
        }

        // Variation config
        RenderVariationConfig(&voice->variation, "DrumVar");

        ImGui::Unindent();
        ImGui::PopID();
    }
}

void ThemisUI::RenderVariationConfig(themis::VariationConfig* config, const char* id)
{
    ImGui::PushID(id);

    // Variation mode
    ImGui::SetNextItemWidth(60);
    if (ImGui::BeginCombo("Var", GetVariationModeName(config->mode))) {
        for (int m = 0; m < themis::NUM_VARIATION_MODES; m++) {
            if (ImGui::Selectable(themis::variationModeNames[m], config->mode == m)) {
                config->mode = (themis::VariationMode)m;
            }
        }
        ImGui::EndCombo();
    }

    if (config->mode != themis::VAR_MODE_OFF) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Seq", GetVariationSequenceName(config->sequence))) {
            for (int s = 0; s < themis::NUM_VARIATION_SEQUENCES; s++) {
                if (ImGui::Selectable(themis::variationSequenceNames[s],
                                       config->sequence == s)) {
                    config->sequence = (themis::VariationSequence)s;
                }
            }
            ImGui::EndCombo();
        }

        // B variation style/density
        ImGui::Text("  B:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("##StyleB", GetRhythmStyleName(config->styleB))) {
            for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
                if (ImGui::Selectable(themis::rhythmStyleNames[s], config->styleB == s)) {
                    config->styleB = (themis::RhythmStyle)s;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        if (ImGui::BeginCombo("##DensityB", GetDensityName(config->densityB))) {
            for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
                if (ImGui::Selectable(themis::densityNames[d], config->densityB == d)) {
                    config->densityB = (themis::DensityLevel)d;
                }
            }
            ImGui::EndCombo();
        }

        // C variation (only for ABC mode)
        if (config->mode == themis::VAR_MODE_ABC) {
            ImGui::Text("  C:");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::BeginCombo("##StyleC", GetRhythmStyleName(config->styleC))) {
                for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
                    if (ImGui::Selectable(themis::rhythmStyleNames[s], config->styleC == s)) {
                        config->styleC = (themis::RhythmStyle)s;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::BeginCombo("##DensityC", GetDensityName(config->densityC))) {
                for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
                    if (ImGui::Selectable(themis::densityNames[d], config->densityC == d)) {
                        config->densityC = (themis::DensityLevel)d;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    ImGui::PopID();
}

void ThemisUI::RenderMelodyVoices()
{
    ImGui::Text("Melody Voices");
    ImGui::SameLine();
    if (ImGui::Button("Randomize Melody")) {
        sequencer->RandomizeMelodyPersonality();
    }

    ImGui::Spacing();

    // Shared settings
    ImGui::Text("Shared Settings:");

    // Scale
    ImGui::SetNextItemWidth(100);
    if (ImGui::BeginCombo("Scale", GetScaleName(sequencer->melodyScale))) {
        for (int s = 0; s < themis::NUM_SCALE_TYPES; s++) {
            if (ImGui::Selectable(themis::scaleNames[s], sequencer->melodyScale == s)) {
                sequencer->melodyScale = (themis::ScaleType)s;
            }
        }
        ImGui::EndCombo();
    }

    // Root note
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::BeginCombo("Root", themis::rootNoteNames[sequencer->melodyRoot])) {
        for (int r = 0; r < 12; r++) {
            if (ImGui::Selectable(themis::rootNoteNames[r], sequencer->melodyRoot == r)) {
                sequencer->melodyRoot = r;
            }
        }
        ImGui::EndCombo();
    }

    // Freeze
    ImGui::SameLine();
    ImGui::Checkbox("Freeze Melody", &sequencer->melodyFreezeEnabled);

    ImGui::Separator();

    // CV Voice panel
    RenderMelodyPanel(&sequencer->melodyVoice, "CV Voice", false);

    ImGui::Spacing();

    // MIDI Voice panel
    RenderMelodyPanel(&sequencer->melodyMidiVoice, "MIDI Voice", true);
}

void ThemisUI::RenderMelodyPanel(themis::MelodyConfig* voice, const char* name, bool isMidi)
{
    if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID(name);
        ImGui::Indent();

        // Active toggle
        ImGui::Checkbox("Active", &voice->active);

        // Main style
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Style", GetMelodyStyleName(voice->style))) {
            for (int s = 0; s < themis::NUM_MELODY_STYLES; s++) {
                if (ImGui::Selectable(themis::melodyStyleNames[s], voice->style == s)) {
                    voice->style = (themis::MelodyStyle)s;
                }
            }
            ImGui::EndCombo();
        }

        // Rhythm style
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Rhythm", GetRhythmStyleName(voice->rhythmStyle))) {
            for (int s = 0; s < themis::NUM_RHYTHM_STYLES; s++) {
                if (ImGui::Selectable(themis::rhythmStyleNames[s], voice->rhythmStyle == s)) {
                    voice->rhythmStyle = (themis::RhythmStyle)s;
                }
            }
            ImGui::EndCombo();
        }

        // Density
        ImGui::SameLine();
        ImGui::SetNextItemWidth(70);
        if (ImGui::BeginCombo("Density", GetDensityName(voice->density))) {
            for (int d = 0; d < themis::NUM_DENSITY_LEVELS; d++) {
                if (ImGui::Selectable(themis::densityNames[d], voice->density == d)) {
                    voice->density = (themis::DensityLevel)d;
                }
            }
            ImGui::EndCombo();
        }

        // Variation config
        RenderVariationConfig(&voice->variation, isMidi ? "MidiMelVar" : "CVMelVar");

        ImGui::Unindent();
        ImGui::PopID();
    }
}

void ThemisUI::RenderGlobalControls()
{
    ImGui::Text("Global Controls");

    ImGui::Separator();

    // Groove settings
    ImGui::Text("Groove Pattern: %d", sequencer->currentGroovePattern);
    ImGui::SameLine();
    if (ImGui::Button("Randomize Groove")) {
        sequencer->RandomizeGroove();
    }

    // Groove amounts table
    if (ImGui::CollapsingHeader("Groove Amounts")) {
        ImGui::Columns(3, "GrooveColumns");
        ImGui::Text("Voice"); ImGui::NextColumn();
        ImGui::Text("Timing"); ImGui::NextColumn();
        ImGui::Text("Velocity"); ImGui::NextColumn();
        ImGui::Separator();

        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
            ImGui::PushID(i);
            ImGui::Text("%s", themis::drumNames[i]);
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##timing", &sequencer->grooveAmount[i], 0.0f, 1.0f);
            ImGui::NextColumn();

            ImGui::SetNextItemWidth(-1);
            ImGui::SliderFloat("##velocity", &sequencer->grooveVelocityAmount[i], 0.0f, 1.0f);
            ImGui::NextColumn();

            ImGui::PopID();
        }
        ImGui::Columns(1);
    }

    ImGui::Separator();

    // Randomize All button
    if (ImGui::Button("Randomize Everything", ImVec2(200, 40))) {
        sequencer->RandomizeAll();
    }

    ImGui::Separator();

    // Change intervals
    int patternInterval = sequencer->patternChangeInterval;
    if (ImGui::SliderInt("Pattern Change Interval (2-bar phrases)", &patternInterval, 1, 8)) {
        sequencer->patternChangeInterval = patternInterval;
    }

    int personalityInterval = sequencer->personalityChangeInterval;
    if (ImGui::SliderInt("Personality Change Interval (8-bar cycles)", &personalityInterval, 1, 8)) {
        sequencer->personalityChangeInterval = personalityInterval;
    }
}

void ThemisUI::RenderOutputSection()
{
    ImGui::Text("Audio Output");

    // Volume control
    float volume = themis_audio::g_audioEngine.GetVolume();
    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
        themis_audio::g_audioEngine.SetVolume(volume);
    }

    // Mute toggle
    bool muted = themis_audio::g_audioEngine.IsMuted();
    if (ImGui::Checkbox("Mute", &muted)) {
        themis_audio::g_audioEngine.SetMuted(muted);
    }

    // Level meter
    float level = themis_audio::g_audioEngine.GetPeakLevel();
    ImGui::ProgressBar(level, ImVec2(-1, 20), "");

    ImGui::Separator();

#ifdef THEMIS_ENABLE_MIDI
    ImGui::Text("MIDI Output");

    // Get available ports
    auto ports = themis_midi::g_midiOutput.GetAvailablePorts();

    // Port selection
    const char* currentPort = themis_midi::g_midiOutput.IsOpen()
        ? themis_midi::g_midiOutput.GetCurrentPortName().c_str()
        : "None";

    if (ImGui::BeginCombo("MIDI Port", currentPort)) {
        if (ImGui::Selectable("None", !themis_midi::g_midiOutput.IsOpen())) {
            themis_midi::g_midiOutput.Close();
            selectedMidiPort = -1;
        }
        for (size_t i = 0; i < ports.size(); i++) {
            bool isSelected = (selectedMidiPort == (int)i);
            if (ImGui::Selectable(ports[i].c_str(), isSelected)) {
                if (themis_midi::g_midiOutput.OpenPort((int)i)) {
                    selectedMidiPort = (int)i;
                }
            }
        }
        ImGui::EndCombo();
    }
#else
    ImGui::Text("MIDI Output: Disabled (RtMidi not found)");
#endif

    ImGui::Separator();

    // CV/Gate display
    ImGui::Text("CV/Gate Outputs (simulated)");
    if (g_desktopPlatform) {
        ImGui::Text("CV1: %.2fV  Gate1: %s",
                    g_desktopPlatform->cvOutputs[0],
                    g_desktopPlatform->gateOutputs[0] ? "HIGH" : "LOW");
    }
}

void ThemisUI::RenderPatternVisualization()
{
    ImGui::Text("Pattern Visualization (click pattern name to copy debug info)");

    // Helper to render pattern string
    auto RenderPatternString = [&](uint32_t pattern, int length) {
        for (int i = 0; i < length && i < 32; i++) {
            bool active = themis::IsStepActive(pattern, i);
            bool isCurrent = (i == (sequencer->currentStep % length) && sequencer->isRunning);

            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }

            ImGui::Text("%s", active ? "X" : ".");

            if (isCurrent) {
                ImGui::PopStyleColor();
            }

            if (i < length - 1) ImGui::SameLine(0, 2);
            if (i == 15 && length > 16) {
                ImGui::SameLine(0, 10);
                ImGui::Text("|");
                ImGui::SameLine(0, 10);
            }
        }
    };

    // Kick pattern - clickable
    char kickDebug[512];
    snprintf(kickDebug, sizeof(kickDebug),
        "=== KICK DEBUG ===\n"
        "Pattern Index: %d\n"
        "Pattern Value: 0x%08X\n"
        "Step: %d, Bar: %d\n",
        sequencer->currentKickPattern,
        themis::kickPatterns[sequencer->currentKickPattern],
        sequencer->currentStep, sequencer->barCounter);

    if (ImGui::Selectable("Kick", false, ImGuiSelectableFlags_None, ImVec2(40, 0))) {
        SDL_SetClipboardText(kickDebug);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
    ImGui::SameLine();
    ImGui::Text("(Pattern %d):", sequencer->currentKickPattern);
    ImGui::SameLine();
    RenderPatternString(themis::kickPatterns[sequencer->currentKickPattern], 32);

    // Clap pattern - clickable
    char clapDebug[512];
    snprintf(clapDebug, sizeof(clapDebug),
        "=== CLAP DEBUG ===\n"
        "Pattern Index: %d\n"
        "Pattern Value: 0x%08X\n"
        "Step: %d, Bar: %d\n",
        sequencer->currentClapPattern,
        themis::clapPatterns[sequencer->currentClapPattern],
        sequencer->currentStep, sequencer->barCounter);

    if (ImGui::Selectable("Clap", false, ImGuiSelectableFlags_None, ImVec2(40, 0))) {
        SDL_SetClipboardText(clapDebug);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
    ImGui::SameLine();
    ImGui::Text("(Pattern %d):", sequencer->currentClapPattern);
    ImGui::SameLine();
    RenderPatternString(themis::clapPatterns[sequencer->currentClapPattern], 32);

    ImGui::Separator();

    // Helper lambda to render a single pattern row
    auto RenderPatternRow = [&](uint32_t pattern, int length, char varLabel, bool isActive, bool isCurrent) {
        int activeSteps = 0;
        for (int i = 0; i < length && i < 32; i++) {
            if (themis::IsStepActive(pattern, i)) activeSteps++;
        }
        bool isEmpty = (activeSteps == 0);

        if (isEmpty) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        } else if (!isActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        } else if (!isCurrent) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        }

        for (int i = 0; i < length && i < 32; i++) {
            bool active = themis::IsStepActive(pattern, i);
            bool isCurrentStep = (i == (sequencer->currentStep % length) && sequencer->isRunning && isCurrent);

            if (isCurrentStep && !isEmpty) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }

            ImGui::Text("%s", active ? "X" : ".");

            if (isCurrentStep && !isEmpty) {
                ImGui::PopStyleColor();
            }

            if (i < length - 1) ImGui::SameLine(0, 2);
        }

        if (isEmpty || !isActive || !isCurrent) {
            ImGui::PopStyleColor();
        }

        if (isEmpty) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), " EMPTY!");
        }
    };

    // Generative voice patterns
    for (int v = 0; v < 6; v++) {
        themis::VoiceConfig* voice = &sequencer->generativeVoices[v];

        // Get current variation
        uint8_t var = themis::GetCurrentVariation(&voice->variation,
                                                   sequencer->currentStep,
                                                   sequencer->barCounter);
        uint32_t activePattern = (var == 1) ? voice->patternB
                               : (var == 2) ? voice->patternC
                               : voice->pattern;

        // Count active steps for debug
        int activeSteps = 0;
        for (int i = 0; i < voice->patternLength && i < 32; i++) {
            if (themis::IsStepActive(activePattern, i)) activeSteps++;
        }

        // Create debug string for this voice
        char voiceDebug[1024];
        snprintf(voiceDebug, sizeof(voiceDebug),
            "=== DRUM VOICE DEBUG ===\n"
            "Voice: %s (index %d)\n"
            "Active: %s\n"
            "Rhythm Style: %s\n"
            "Density: %s\n"
            "Interaction: %s\n"
            "Pattern Length: %d\n"
            "Current Variation: %c\n"
            "Variation Mode: %s\n"
            "Pattern A: 0x%08X\n"
            "Pattern B: 0x%08X\n"
            "Pattern C: 0x%08X\n"
            "Active Pattern: 0x%08X\n"
            "Active Steps: %d\n"
            "Step: %d, Bar: %d\n",
            themis::drumNames[voice->voice], v,
            voice->active ? "YES" : "NO",
            themis::rhythmStyleNames[voice->rhythmStyle],
            themis::densityNames[voice->density],
            themis::interactionStyleNames[voice->interaction],
            voice->patternLength,
            var == 0 ? 'A' : (var == 1 ? 'B' : 'C'),
            themis::variationModeNames[voice->variation.mode],
            voice->pattern, voice->patternB, voice->patternC,
            activePattern, activeSteps,
            sequencer->currentStep, sequencer->barCounter);

        // Clickable voice name
        ImGui::PushID(v);
        if (ImGui::Selectable(themis::drumNames[voice->voice], false, ImGuiSelectableFlags_None, ImVec2(50, 0))) {
            SDL_SetClipboardText(voiceDebug);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
        ImGui::SameLine();

        // Always show A, B, C patterns
        ImGui::Text("[A]:");
        ImGui::SameLine();
        RenderPatternRow(voice->pattern, voice->patternLength, 'A', voice->active, var == 0);
        ImGui::Text("      [B]:");
        ImGui::SameLine();
        RenderPatternRow(voice->patternB, voice->patternLength, 'B', voice->active, var == 1);
        ImGui::Text("      [C]:");
        ImGui::SameLine();
        RenderPatternRow(voice->patternC, voice->patternLength, 'C', voice->active, var == 2);
        ImGui::PopID();

        // Show personality info on next line
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
            "    %s | %s | L%d%s",
            themis::rhythmStyleNames[voice->rhythmStyle],
            themis::densityNames[voice->density],
            voice->patternLength,
            voice->interaction != themis::INTERACTION_NONE ?
                (std::string(" | ") + themis::interactionStyleNames[voice->interaction] +
                 " w/" + themis::drumNames[voice->interactionPartner]).c_str() : "");
    }

    ImGui::Separator();

    // Melody voice patterns
    themis::MelodyConfig* melodyVoices[2] = {
        &sequencer->melodyVoice,
        &sequencer->melodyMidiVoice
    };
    const char* melodyNames[2] = { "MelCV", "MelMIDI" };
    const char* melodyFullNames[2] = { "Melody CV", "Melody MIDI" };

    for (int m = 0; m < 2; m++) {
        themis::MelodyConfig* melody = melodyVoices[m];

        // Get current variation
        uint8_t var = themis::GetCurrentVariation(&melody->variation,
                                                   sequencer->currentStep,
                                                   sequencer->barCounter);
        uint32_t activePattern = (var == 1) ? melody->rhythmPatternB
                               : (var == 2) ? melody->rhythmPatternC
                               : melody->rhythmPattern;

        // Count active steps for debug
        int activeSteps = 0;
        for (int i = 0; i < melody->patternLength && i < 32; i++) {
            if (themis::IsStepActive(activePattern, i)) activeSteps++;
        }
        bool isEmpty = (activeSteps == 0);

        // Create debug string for melody
        char melodyDebug[1536];
        snprintf(melodyDebug, sizeof(melodyDebug),
            "=== MELODY VOICE DEBUG ===\n"
            "Voice: %s\n"
            "Active: %s\n"
            "Style: %s\n"
            "SubStyle: %d\n"
            "Rhythm Style: %s\n"
            "Density: %s\n"
            "Pattern Length: %d\n"
            "Current Variation: %c\n"
            "Variation Mode: %s\n"
            "Rhythm Pattern A: 0x%08X\n"
            "Rhythm Pattern B: 0x%08X\n"
            "Rhythm Pattern C: 0x%08X\n"
            "Active Pattern: 0x%08X\n"
            "Active Steps: %d\n"
            "Scale: %s\n"
            "Root: %s\n"
            "Step: %d, Bar: %d\n"
            "Note Sequence A: [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]\n"
            "isEmpty: %s\n",
            melodyFullNames[m],
            melody->active ? "YES" : "NO",
            themis::melodyStyleNames[melody->style],
            melody->subStyle,
            themis::rhythmStyleNames[melody->rhythmStyle],
            themis::densityNames[melody->density],
            melody->patternLength,
            var == 0 ? 'A' : (var == 1 ? 'B' : 'C'),
            themis::variationModeNames[melody->variation.mode],
            melody->rhythmPattern, melody->rhythmPatternB, melody->rhythmPatternC,
            activePattern, activeSteps,
            themis::scaleNames[sequencer->melodyScale],
            themis::rootNoteNames[sequencer->melodyRoot],
            sequencer->currentStep, sequencer->barCounter,
            melody->noteSequence[0], melody->noteSequence[1], melody->noteSequence[2], melody->noteSequence[3],
            melody->noteSequence[4], melody->noteSequence[5], melody->noteSequence[6], melody->noteSequence[7],
            melody->noteSequence[8], melody->noteSequence[9], melody->noteSequence[10], melody->noteSequence[11],
            melody->noteSequence[12], melody->noteSequence[13], melody->noteSequence[14], melody->noteSequence[15],
            isEmpty ? "YES" : "NO");

        // Clickable melody name
        ImGui::PushID(m + 100);
        if (ImGui::Selectable(melodyNames[m], false, ImGuiSelectableFlags_None, ImVec2(55, 0))) {
            SDL_SetClipboardText(melodyDebug);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
        ImGui::SameLine();

        // Always show A, B, C patterns
        ImGui::Text("[A]:");
        ImGui::SameLine();
        RenderPatternRow(melody->rhythmPattern, melody->patternLength, 'A', melody->active, var == 0);
        ImGui::Text("        [B]:");
        ImGui::SameLine();
        RenderPatternRow(melody->rhythmPatternB, melody->patternLength, 'B', melody->active, var == 1);
        ImGui::Text("        [C]:");
        ImGui::SameLine();
        RenderPatternRow(melody->rhythmPatternC, melody->patternLength, 'C', melody->active, var == 2);
        ImGui::PopID();

        // Show personality info on next line
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
            "    %s | %s | %s | L%d",
            themis::melodyStyleNames[melody->style],
            themis::rhythmStyleNames[melody->rhythmStyle],
            themis::densityNames[melody->density],
            melody->patternLength);
    }
}

void ThemisUI::RenderMixer()
{
    // Decay activity indicators
    float activityDecay = 0.05f;
    for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
        if (drumActivity[i] > 0.0f) drumActivity[i] -= activityDecay;
        if (drumActivity[i] < 0.0f) drumActivity[i] = 0.0f;
    }
    if (melodyCVActivity > 0.0f) melodyCVActivity -= activityDecay;
    if (melodyCVActivity < 0.0f) melodyCVActivity = 0.0f;
    if (melodyMidiActivity > 0.0f) melodyMidiActivity -= activityDecay;
    if (melodyMidiActivity < 0.0f) melodyMidiActivity = 0.0f;

    // Compact channel strip helper lambda
    auto RenderChannel = [](const char* name, bool* mute, bool* solo,
                            float activity, bool shouldPlay, ImVec4 activeColor) {
        ImGui::BeginGroup();

        // Activity indicator (small square)
        ImVec4 indicatorColor = shouldPlay
            ? ImVec4(activeColor.x + activity * 0.5f, activeColor.y, activeColor.z, 1.0f)
            : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, indicatorColor);
        ImGui::ProgressBar(activity, ImVec2(50, 30), "");
        ImGui::PopStyleColor();

        // Channel name
        ImGui::Text("%-6s", name);

        // M/S buttons - capture state BEFORE button click to avoid push/pop mismatch
        bool isMuted = *mute;
        bool isSoloed = *solo;

        if (isMuted) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
        }
        if (ImGui::SmallButton("M")) {
            *mute = !(*mute);
        }
        if (isMuted) {
            ImGui::PopStyleColor();
        }

        ImGui::SameLine();

        if (isSoloed) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.2f, 1.0f));
        }
        if (ImGui::SmallButton("S")) {
            *solo = !(*solo);
        }
        if (isSoloed) {
            ImGui::PopStyleColor();
        }

        ImGui::EndGroup();
        ImGui::SameLine(0, 8);
    };

    // Row 1: All channels
    ImGui::Text("Channels:");
    ImGui::SameLine(80);

    for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
        ImGui::PushID(i);
        RenderChannel(themis::drumNames[i], &drumMute[i], &drumSolo[i],
                     drumActivity[i], ShouldPlayDrum((themis::DrumVoice)i),
                     ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
        ImGui::PopID();
    }

    ImGui::PushID("MelCV");
    RenderChannel("MelCV", &melodyCVMute, &melodyCVSolo,
                 melodyCVActivity, ShouldPlayMelodyCV(),
                 ImVec4(0.2f, 0.4f, 0.8f, 1.0f));
    ImGui::PopID();

    ImGui::PushID("MelMID");
    RenderChannel("MelMID", &melodyMidiMute, &melodyMidiSolo,
                 melodyMidiActivity, ShouldPlayMelodyMidi(),
                 ImVec4(0.6f, 0.2f, 0.8f, 1.0f));
    ImGui::PopID();

    ImGui::NewLine();

    // Quick actions row
    if (ImGui::SmallButton("Clear Mutes")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumMute[i] = false;
        melodyCVMute = false;
        melodyMidiMute = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear Solos")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = false;
        melodyCVSolo = false;
        melodyMidiSolo = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Solo Drums")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = true;
        melodyCVSolo = false;
        melodyMidiSolo = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Solo Melody")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = false;
        melodyCVSolo = true;
        melodyMidiSolo = true;
    }

    ImGui::Separator();

    // Sound shaping section
    ImGui::Text("Sound Shaping");

    // Filter cutoff
    float filterCutoff = themis_audio::g_audioEngine.GetFilterCutoff();
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("Filter", &filterCutoff, 0.0f, 1.0f, "%.2f")) {
        themis_audio::g_audioEngine.SetFilterCutoff(filterCutoff);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Low-pass filter cutoff: 0=dark, 1=bright");
    }

    ImGui::SameLine(0, 30);

    // Decay amount
    float decayAmount = themis_audio::g_audioEngine.GetDecayAmount();
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("Decay", &decayAmount, 0.0f, 1.0f, "%.2f")) {
        themis_audio::g_audioEngine.SetDecayAmount(decayAmount);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Envelope decay: 0=short/tight, 1=long/loose");
    }

    // Volume (moved here from Output tab for convenience)
    ImGui::SameLine(0, 30);
    float volume = themis_audio::g_audioEngine.GetVolume();
    ImGui::SetNextItemWidth(150);
    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f, "%.2f")) {
        themis_audio::g_audioEngine.SetVolume(volume);
    }

    // Level meter
    ImGui::SameLine();
    float level = themis_audio::g_audioEngine.GetPeakLevel();
    ImGui::ProgressBar(level, ImVec2(80, 0), "");
}

void ThemisUI::TriggerDrumActivity(themis::DrumVoice voice)
{
    if (voice < themis::NUM_DRUM_VOICES) {
        drumActivity[voice] = 1.0f;
    }
}

void ThemisUI::TriggerMelodyCVActivity()
{
    melodyCVActivity = 1.0f;
}

void ThemisUI::TriggerMelodyMidiActivity()
{
    melodyMidiActivity = 1.0f;
}

} // namespace themis_ui
