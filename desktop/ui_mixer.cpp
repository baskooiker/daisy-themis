/**
 * @file ui_mixer.cpp
 * @brief Mixer UI implementation
 */

#include "ui_internal.h"

namespace themis_ui {

void ThemisUI::RenderMixer()
{
    // Track effective play states BEFORE any UI changes
    bool wasPolyPlaying = ShouldPlayPoly();
    bool wasRhythmPlaying = ShouldPlayRhythm();
    bool wasAcidPlaying = ShouldPlayAcid();

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
    if (polyActivity > 0.0f) polyActivity -= activityDecay;
    if (polyActivity < 0.0f) polyActivity = 0.0f;
    if (rhythmActivity > 0.0f) rhythmActivity -= activityDecay;
    if (rhythmActivity < 0.0f) rhythmActivity = 0.0f;
    if (acidActivity > 0.0f) acidActivity -= activityDecay;
    if (acidActivity < 0.0f) acidActivity = 0.0f;

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

    ImGui::PushID("Pads");
    RenderChannel("Pads", &polyMute, &polySolo,
                 polyActivity, ShouldPlayPoly(),
                 ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
    ImGui::PopID();

    ImGui::PushID("Rhythm");
    RenderChannel("Rhythm", &rhythmMute, &rhythmSolo,
                 rhythmActivity, ShouldPlayRhythm(),
                 ImVec4(0.8f, 0.4f, 0.6f, 1.0f));
    ImGui::PopID();

    ImGui::PushID("Acid");
    RenderChannel("Acid", &acidMute, &acidSolo,
                 acidActivity, ShouldPlayAcid(),
                 ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
    ImGui::PopID();

    // Stop notes when effective playing state changes from true to false
    if (wasPolyPlaying && !ShouldPlayPoly()) {
        themis_audio::g_audioEngine.StopAllPolyNotes();
    }
    if (wasRhythmPlaying && !ShouldPlayRhythm()) {
        themis_audio::g_audioEngine.StopAllRhythmNotes();
    }
    if (wasAcidPlaying && !ShouldPlayAcid()) {
        themis_audio::g_audioEngine.StopAllAcidNotes();
    }

    ImGui::NewLine();

    // Quick actions row
    if (ImGui::SmallButton("Clear Mutes")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumMute[i] = false;
        melodyCVMute = false;
        melodyMidiMute = false;
        polyMute = false;
        rhythmMute = false;
        acidMute = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear Solos")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = false;
        melodyCVSolo = false;
        melodyMidiSolo = false;
        polySolo = false;
        rhythmSolo = false;
        acidSolo = false;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Solo Drums")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = true;
        melodyCVSolo = false;
        melodyMidiSolo = false;
        polySolo = false;
        rhythmSolo = false;
        acidSolo = false;
        // Stop melodic voices immediately
        themis_audio::g_audioEngine.StopAllPolyNotes();
        themis_audio::g_audioEngine.StopAllRhythmNotes();
        themis_audio::g_audioEngine.StopAllAcidNotes();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Solo Melody")) {
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) drumSolo[i] = false;
        melodyCVSolo = true;
        melodyMidiSolo = true;
        polySolo = false;
        rhythmSolo = false;
        acidSolo = false;
        // Stop polyphonic melodic voices immediately
        themis_audio::g_audioEngine.StopAllPolyNotes();
        themis_audio::g_audioEngine.StopAllRhythmNotes();
        themis_audio::g_audioEngine.StopAllAcidNotes();
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

void ThemisUI::TriggerPolyActivity()
{
    polyActivity = 1.0f;
}

void ThemisUI::TriggerRhythmActivity()
{
    rhythmActivity = 1.0f;
}

void ThemisUI::TriggerAcidActivity()
{
    acidActivity = 1.0f;
}

} // namespace themis_ui
