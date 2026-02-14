/**
 * @file ui_chords.cpp
 * @brief Chord vibe randomization UI tab
 *
 * Provides controls for the vibe-based chord progression system:
 * - Freeze control to pause all chord changes
 * - Vibe category toggles (Minor, Whole-tone, Major)
 * - Per-vibe progression selection with enable/disable toggles
 * - Manual progression selection with "Use" buttons
 * - Chord sequence display
 */

#include "ui_internal.h"

namespace themis_ui {

// Helper to get scale degree name
static const char* GetScaleDegreeName(int8_t degree, bool isDiatonic) {
    if (!isDiatonic) {
        // Chromatic offset - show as semitones
        static char buf[8];
        if (degree >= 0) {
            snprintf(buf, sizeof(buf), "+%d", degree);
        } else {
            snprintf(buf, sizeof(buf), "%d", degree);
        }
        return buf;
    }

    // Diatonic scale degrees (0-6 = I-VII)
    static const char* degreeNames[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    if (degree >= 0 && degree < 7) {
        return degreeNames[degree];
    }
    return "?";
}

// Helper to get chord duration string
static const char* GetChordDurationString(uint8_t progLength, uint8_t chordRate) {
    using namespace themis;
    // Calculate total bars for the progression
    uint16_t totalSteps = progLength * chordRateSteps[chordRate];
    float totalBars = totalSteps / 32.0f;

    static char buf[32];
    if (totalBars >= 1.0f) {
        if (totalBars == (int)totalBars) {
            snprintf(buf, sizeof(buf), "%d bar%s", (int)totalBars, totalBars > 1 ? "s" : "");
        } else {
            snprintf(buf, sizeof(buf), "%.1f bars", totalBars);
        }
    } else {
        snprintf(buf, sizeof(buf), "1/%d bar", (int)(1.0f / totalBars));
    }
    return buf;
}

void ThemisUI::RenderChordsTab()
{
    using namespace themis;

    // === CURRENT STATE DISPLAY ===
    ImGui::Text("Current State:");
    ImGui::SameLine();

    // Vibe indicator
    ImVec4 vibeColor;
    switch (sequencer->chordRandomizerState.currentVibe) {
        case VIBE_MINOR: vibeColor = ImVec4(0.4f, 0.6f, 0.8f, 1.0f); break;
        case VIBE_WHOLE_TONE: vibeColor = ImVec4(0.7f, 0.5f, 0.7f, 1.0f); break;
        case VIBE_MAJOR: vibeColor = ImVec4(0.9f, 0.7f, 0.3f, 1.0f); break;
        case VIBE_HALF_WHOLE: vibeColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f); break;
        case VIBE_WHOLE_HALF: vibeColor = ImVec4(0.3f, 0.7f, 0.5f, 1.0f); break;
        default: vibeColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
    }
    ImGui::TextColored(vibeColor, "Vibe: %s", vibeNames[sequencer->chordRandomizerState.currentVibe]);

    ImGui::SameLine();
    ImGui::Text("| Root: %s", rootNoteNames[sequencer->melodyRoot]);

    ImGui::SameLine();
    const ChordProgression& currentProg = progressions[sequencer->chordVoice.progressionIndex];
    ImGui::Text("| Prog: %s [%s]", currentProg.name, progCategoryNames[currentProg.category]);

    // Chord rate and duration
    ImGui::SameLine();
    ImGui::TextDisabled("| Rate: %s", chordRateNames[sequencer->chordVoice.chordRate]);
    ImGui::SameLine();
    ImGui::TextDisabled("| Duration: %s",
        GetChordDurationString(currentProg.length, sequencer->chordVoice.chordRate));

    // Transition indicator
    if (sequencer->chordRandomizerState.inTransition) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "| TRANSITIONING (%d cycles)",
                          sequencer->chordRandomizerState.transitionBarsRemaining);
    }

    // Change timer (in progression cycles)
    ImGui::SameLine();
    ImGui::TextDisabled("| Timer: %d cycles", sequencer->chordRandomizerState.changeTimer);

    // Current chord sequence display
    ImGui::Separator();
    ImGui::Text("Current Progression Sequence:");
    ImGui::SameLine();

    for (int i = 0; i < currentProg.length; i++) {
        const ProgressionStep& step = currentProg.steps[i];

        // Highlight current chord
        if (i == sequencer->chordState.currentChordIndex && sequencer->chordVoice.active) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "[%s%s]",
                GetScaleDegreeName(step.scaleDegree, step.isDiatonic),
                chordTypeNames[step.chordType]);
        } else {
            ImGui::TextDisabled("%s%s",
                GetScaleDegreeName(step.scaleDegree, step.isDiatonic),
                chordTypeNames[step.chordType]);
        }

        if (i < currentProg.length - 1) {
            ImGui::SameLine();
            ImGui::TextDisabled("-");
            ImGui::SameLine();
        }
    }

    ImGui::Separator();

    // === FREEZE CONTROL ===
    ImGui::Checkbox("Freeze Chord Randomization", &sequencer->chordRandomizer.freezeEnabled);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Stop all automatic chord/vibe changes");
    }

    ImGui::SameLine();
    if (ImGui::Button("Randomize Chords")) {
        sequencer->RandomizeChordVoice();
    }

    // Chord rate selector
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    if (ImGui::BeginCombo("Rate", chordRateNames[sequencer->chordVoice.chordRate])) {
        for (int r = 0; r < NUM_CHORD_RATES; r++) {
            if (ImGui::Selectable(chordRateNames[r], sequencer->chordVoice.chordRate == r)) {
                sequencer->chordVoice.chordRate = r;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("How fast chords change (affects total progression duration)");
    }

    ImGui::Separator();

    // === VIBE TOGGLES ===
    ImGui::Text("Enabled Vibes:");

    // Count enabled vibes to prevent disabling the last one
    int enabledVibeCount = 0;
    for (int v = 0; v < NUM_VIBE_TYPES; v++) {
        if (sequencer->chordRandomizer.enabledVibes & (1 << v)) {
            enabledVibeCount++;
        }
    }

    for (int v = 0; v < NUM_VIBE_TYPES; v++) {
        bool enabled = sequencer->chordRandomizer.enabledVibes & (1 << v);
        ImGui::SameLine();

        // Color the checkbox based on vibe
        ImVec4 vColor;
        switch (v) {
            case VIBE_MINOR: vColor = ImVec4(0.4f, 0.6f, 0.8f, 1.0f); break;
            case VIBE_WHOLE_TONE: vColor = ImVec4(0.7f, 0.5f, 0.7f, 1.0f); break;
            case VIBE_MAJOR: vColor = ImVec4(0.9f, 0.7f, 0.3f, 1.0f); break;
            case VIBE_HALF_WHOLE: vColor = ImVec4(0.8f, 0.3f, 0.3f, 1.0f); break;
            case VIBE_WHOLE_HALF: vColor = ImVec4(0.3f, 0.7f, 0.5f, 1.0f); break;
            default: vColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); break;
        }

        // Prevent unchecking if this is the last enabled vibe
        bool isLastEnabled = enabled && (enabledVibeCount == 1);

        ImGui::PushStyleColor(ImGuiCol_CheckMark, vColor);
        if (isLastEnabled) {
            // Show as disabled - can't uncheck the last one
            ImGui::BeginDisabled();
            ImGui::Checkbox(vibeNames[v], &enabled);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("At least one vibe must be enabled");
            }
        } else {
            if (ImGui::Checkbox(vibeNames[v], &enabled)) {
                if (enabled) {
                    sequencer->chordRandomizer.enabledVibes |= (1 << v);
                } else {
                    sequencer->chordRandomizer.enabledVibes &= ~(1 << v);
                }
            }
        }
        ImGui::PopStyleColor();
    }

    ImGui::Separator();

    // === PROGRESSION SELECTION PER VIBE ===
    ImGui::Text("Progressions by Vibe:");
    ImGui::Spacing();

    for (int v = 0; v < NUM_VIBE_TYPES; v++) {
        char headerLabel[64];
        snprintf(headerLabel, sizeof(headerLabel), "%s###Vibe%d", vibeNames[v], v);

        // Color the header based on vibe
        ImVec4 vColor;
        switch (v) {
            case VIBE_MINOR: vColor = ImVec4(0.3f, 0.5f, 0.7f, 1.0f); break;
            case VIBE_WHOLE_TONE: vColor = ImVec4(0.6f, 0.4f, 0.6f, 1.0f); break;
            case VIBE_MAJOR: vColor = ImVec4(0.8f, 0.6f, 0.2f, 1.0f); break;
            case VIBE_HALF_WHOLE: vColor = ImVec4(0.7f, 0.2f, 0.2f, 1.0f); break;
            case VIBE_WHOLE_HALF: vColor = ImVec4(0.2f, 0.6f, 0.4f, 1.0f); break;
            default: vColor = ImVec4(0.4f, 0.4f, 0.4f, 1.0f); break;
        }

        ImGui::PushStyleColor(ImGuiCol_Header, vColor);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(vColor.x + 0.1f, vColor.y + 0.1f, vColor.z + 0.1f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(vColor.x + 0.2f, vColor.y + 0.2f, vColor.z + 0.2f, 1.0f));

        bool headerOpen = ImGui::CollapsingHeader(headerLabel, v == sequencer->chordRandomizerState.currentVibe ?
                                                   ImGuiTreeNodeFlags_DefaultOpen : 0);
        ImGui::PopStyleColor(3);

        if (headerOpen) {
            ImGui::Indent();

            // Get all progressions for this vibe
            uint8_t indices[32];
            uint8_t count = GetProgressionsForVibe((VibeType)v, indices, 32);

            // Group by category
            for (int cat = 0; cat < NUM_PROG_CATEGORIES; cat++) {
                // Count progressions in this category
                int catCount = 0;
                for (int p = 0; p < count; p++) {
                    if (progressions[indices[p]].category == cat) {
                        catCount++;
                    }
                }

                if (catCount == 0) continue;

                ImGui::TextDisabled("%s:", progCategoryNames[cat]);

                for (int p = 0; p < count; p++) {
                    uint8_t progIdx = indices[p];
                    if (progressions[progIdx].category != cat) continue;

                    const ChordProgression& prog = progressions[progIdx];

                    // Find position within this vibe for the bitmask
                    uint8_t vibePos = 0;
                    for (uint8_t j = 0; j < progIdx; j++) {
                        if (progressions[j].vibe == (VibeType)v) {
                            vibePos++;
                        }
                    }

                    ImGui::PushID(progIdx);

                    // Enable checkbox
                    bool enabled = sequencer->chordRandomizer.enabledProgressions[v] & (1 << vibePos);
                    if (ImGui::Checkbox("##enable", &enabled)) {
                        if (enabled) {
                            sequencer->chordRandomizer.enabledProgressions[v] |= (1 << vibePos);
                        } else {
                            sequencer->chordRandomizer.enabledProgressions[v] &= ~(1 << vibePos);
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Enable/disable for randomization");
                    }

                    ImGui::SameLine();

                    // Highlight if currently playing
                    if (progIdx == sequencer->chordVoice.progressionIndex) {
                        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.3f, 1.0f), "%s", prog.name);
                    } else {
                        ImGui::Text("%s", prog.name);
                    }

                    // Show chord count and duration
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%d chords, %s)", prog.length,
                        GetChordDurationString(prog.length, sequencer->chordVoice.chordRate));

                    // Use button - select this progression
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Use")) {
                        if (progIdx != sequencer->chordVoice.progressionIndex) {
                            sequencer->chordState.pendingProgressionIndex = progIdx;
                            // Also update vibe if needed
                            if (prog.vibe != sequencer->chordRandomizerState.currentVibe) {
                                sequencer->chordRandomizerState.currentVibe = prog.vibe;
                            }
                        }
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Switch to this progression at end of current");
                    }

                    // Show pending indicator
                    if (sequencer->chordState.pendingProgressionIndex == progIdx) {
                        ImGui::SameLine();
                        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "(pending)");
                    }

                    // Show chord sequence on next line
                    ImGui::Indent();
                    ImGui::TextDisabled("  ");
                    ImGui::SameLine();
                    for (int i = 0; i < prog.length; i++) {
                        const ProgressionStep& step = prog.steps[i];
                        ImGui::TextDisabled("%s%s",
                            GetScaleDegreeName(step.scaleDegree, step.isDiatonic),
                            chordTypeNames[step.chordType]);
                        if (i < prog.length - 1) {
                            ImGui::SameLine();
                            ImGui::TextDisabled("-");
                            ImGui::SameLine();
                        }
                    }
                    ImGui::Unindent();

                    ImGui::PopID();
                }

                ImGui::Spacing();
            }

            ImGui::Unindent();
        }
    }

    ImGui::Separator();

    // === RANDOMIZATION INFO ===
    if (ImGui::CollapsingHeader("Randomization Rules (Info)")) {
        ImGui::Indent();

        ImGui::TextDisabled("How Auto-Randomization Works:");
        ImGui::BulletText("Every 2-4 progression cycles, system considers a change");
        ImGui::BulletText("20%% chance to change vibe (if multiple vibes enabled)");
        ImGui::BulletText("10%% chance to change root note (within same vibe)");
        ImGui::BulletText("Otherwise, picks new progression from current vibe");

        ImGui::Spacing();
        ImGui::TextDisabled("Vibe Transitions:");
        ImGui::BulletText("When changing vibes, plays 'steady' chord for ~8 bars");
        ImGui::BulletText("Minor -> Major: Root shifts +3 or -4 semitones");
        ImGui::BulletText("Major -> Minor: Root shifts -3 or +4 semitones");
        ImGui::BulletText("To/From Whole-tone: Root shifts +2 or +6 semitones");
        ImGui::BulletText("To/From Half-Whole: Root shifts +3 or +6 semitones");
        ImGui::BulletText("To/From Whole-Half: Root shifts +3 or +1 semitones");

        ImGui::Spacing();
        ImGui::TextDisabled("Root Changes (within same vibe):");
        ImGui::BulletText("Root shifts up by 5th (+7) or up by semitone (+1)");
        ImGui::BulletText("Plays steady chord during transition");

        ImGui::Spacing();
        ImGui::TextDisabled("Progression Categories:");
        ImGui::BulletText("Steady: 1 chord - used for transitions");
        ImGui::BulletText("Cadence: 2 chords - short movements");
        ImGui::BulletText("Full: 4+ chords - complete progressions");

        ImGui::Unindent();
    }
}

} // namespace themis_ui
