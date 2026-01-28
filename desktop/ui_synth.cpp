/**
 * @file ui_synth.cpp
 * @brief Synth parameters tab UI implementation
 */

#include "ui_internal.h"

namespace themis_ui {

void ThemisUI::RenderSynthParams()
{
    ImGui::Text("Per-Voice Synthesizer Parameters");
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
        "Adjust filter cutoff, decay, and waveform for each voice");
    ImGui::Separator();

    // VCO type names
    static const char* vcoTypeNames[] = { "Saw", "Square", "Triangle", "Sine" };

    // Helper lambda to render voice synth params
    auto RenderVoiceParams = [&](const char* name, const char* id,
                                  std::function<void(float)> setCutoff,
                                  std::function<void(float)> setDecay,
                                  std::function<void(float)> setEnvAmt,
                                  std::function<void(int)> setVco,
                                  float& cutoff, float& decay, float& envAmt, int* vco) {
        ImGui::PushID(id);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%-8s", name);
        ImGui::SameLine(80);

        // Filter Cutoff
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("Cut##cut", &cutoff, 0.0f, 1.0f, "%.2f")) {
            setCutoff(cutoff);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter Cutoff");

        ImGui::SameLine();

        // VCA Decay
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("Dcy##dcy", &decay, 0.0f, 1.0f, "%.2f")) {
            setDecay(decay);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("VCA Decay (note length)");

        ImGui::SameLine();

        // Filter Envelope Amount
        ImGui::SetNextItemWidth(80);
        if (ImGui::SliderFloat("Env##env", &envAmt, 0.0f, 1.0f, "%.2f")) {
            setEnvAmt(envAmt);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Filter Envelope Amount");

        // VCO type (only for melodic voices)
        if (vco != nullptr && setVco) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            if (ImGui::BeginCombo("VCO##vco", vcoTypeNames[*vco])) {
                for (int t = 0; t < 4; t++) {
                    if (ImGui::Selectable(vcoTypeNames[t], *vco == t)) {
                        *vco = t;
                        setVco(t);
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Oscillator Waveform");
        }

        ImGui::PopID();
    };

    // Drum voices section
    if (ImGui::CollapsingHeader("Drum Voices", ImGuiTreeNodeFlags_DefaultOpen)) {
        // Static local storage for UI values
        static float kickCut = 1.0f, kickDcy = 0.5f, kickEnv = 0.5f;
        static float snareCut = 1.0f, snareDcy = 0.5f, snareEnv = 0.5f;
        static float hihatCut = 1.0f, hihatDcy = 0.5f;
        static float clapCut = 1.0f, clapDcy = 0.5f;
        static float tomCut = 1.0f, tomDcy = 0.5f;

        RenderVoiceParams("Kick", "kick",
            [](float v) { themis_audio::g_audioEngine.SetKickFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetKickVcaDecay(v); },
            [](float v) { themis_audio::g_audioEngine.SetKickFilterEnvAmount(v); },
            nullptr,
            kickCut, kickDcy, kickEnv, nullptr);

        RenderVoiceParams("Snare", "snare",
            [](float v) { themis_audio::g_audioEngine.SetSnareFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetSnareVcaDecay(v); },
            [](float v) { themis_audio::g_audioEngine.SetSnareFilterEnvAmount(v); },
            nullptr,
            snareCut, snareDcy, snareEnv, nullptr);

        static float hihatEnvDummy = 0.5f;
        RenderVoiceParams("HiHat", "hihat",
            [](float v) { themis_audio::g_audioEngine.SetHihatFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetHihatVcaDecay(v); },
            [](float) {},  // No env amount for hihat
            nullptr,
            hihatCut, hihatDcy, hihatEnvDummy, nullptr);

        static float clapEnvDummy = 0.5f;
        RenderVoiceParams("Clap", "clap",
            [](float v) { themis_audio::g_audioEngine.SetClapFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetClapVcaDecay(v); },
            [](float) {},  // No env amount for clap
            nullptr,
            clapCut, clapDcy, clapEnvDummy, nullptr);

        static float tomEnvDummy = 0.5f;
        RenderVoiceParams("Tom", "tom",
            [](float v) { themis_audio::g_audioEngine.SetTomFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetTomVcaDecay(v); },
            [](float) {},  // No env amount for tom
            nullptr,
            tomCut, tomDcy, tomEnvDummy, nullptr);
    }

    // Melodic voices section
    if (ImGui::CollapsingHeader("Melodic Voices", ImGuiTreeNodeFlags_DefaultOpen)) {
        static float rhythmCut = 1.0f, rhythmDcy = 0.5f, rhythmEnv = 0.5f;
        static int rhythmVco = 0;
        static float acidCut = 1.0f, acidDcy = 0.5f, acidEnv = 0.5f;
        static float padCut = 1.0f, padDcy = 0.5f, padEnv = 0.5f;
        static int padVco = 0;

        RenderVoiceParams("Rhythm", "rhythm",
            [](float v) { themis_audio::g_audioEngine.SetRhythmFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetRhythmVcaDecay(v); },
            [](float v) { themis_audio::g_audioEngine.SetRhythmFilterEnvAmount(v); },
            [](int v) { themis_audio::g_audioEngine.SetRhythmVcoType(v); },
            rhythmCut, rhythmDcy, rhythmEnv, &rhythmVco);

        RenderVoiceParams("Acid", "acid",
            [](float v) { themis_audio::g_audioEngine.SetAcidFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetAcidVcaDecay(v); },
            [](float v) { themis_audio::g_audioEngine.SetAcidFilterEnvAmount(v); },
            nullptr,  // Acid always uses saw
            acidCut, acidDcy, acidEnv, nullptr);

        RenderVoiceParams("Pad", "pad",
            [](float v) { themis_audio::g_audioEngine.SetPadFilterCutoff(v); },
            [](float v) { themis_audio::g_audioEngine.SetPadVcaDecay(v); },
            [](float v) { themis_audio::g_audioEngine.SetPadFilterEnvAmount(v); },
            [](int v) { themis_audio::g_audioEngine.SetPadVcoType(v); },
            padCut, padDcy, padEnv, &padVco);
    }
}

} // namespace themis_ui
