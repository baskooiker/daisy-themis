/**
 * @file ui.cpp
 * @brief ImGui user interface implementation - core functions
 *
 * Split into multiple files for maintainability:
 *   ui.cpp        - Core UI, transport, voice/pattern controls
 *   ui_synth.cpp  - Synth parameters tab
 *   ui_mixer.cpp  - Mixer and activity triggers
 *   ui_pattern.cpp - Pattern visualization
 */

#include "ui_internal.h"

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
    return melodyCVSolo || melodyMidiSolo || polySolo || rhythmSolo || acidSolo;
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

bool ThemisUI::ShouldPlayPoly() const
{
    if (polyMute) return false;

    if (IsAnySoloActive()) {
        return polySolo;
    }
    return true;
}

bool ThemisUI::ShouldPlayRhythm() const
{
    if (rhythmMute) return false;

    if (IsAnySoloActive()) {
        return rhythmSolo;
    }
    return true;
}

bool ThemisUI::ShouldPlayAcid() const
{
    if (acidMute) return false;

    if (IsAnySoloActive()) {
        return acidSolo;
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
        if (ImGui::BeginTabItem("Synth")) {
            RenderSynthParams();
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

    ImGui::Spacing();
    ImGui::Separator();

    // Poly Voice section header
    ImGui::Text("POLY VOICE (CHORDS)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Poly")) {
        sequencer->RandomizePolyVoice();
    }

    ImGui::Separator();

    // Compact poly voice controls
    ImGui::PushID("PolyCompact");

    // Active toggle
    ImGui::Checkbox("Poly", &sequencer->polyVoice.active);

    // Progression - changes are pending until end of current progression
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80);
    // Show current progression, or pending if one is set
    uint8_t displayProgIndex = sequencer->polyVoice.progressionIndex;
    bool hasPending = (sequencer->polyState.pendingProgressionIndex >= 0);
    if (hasPending) {
        displayProgIndex = (uint8_t)sequencer->polyState.pendingProgressionIndex;
    }
    if (ImGui::BeginCombo("##prog", themis::progressions[displayProgIndex].name)) {
        for (int p = 0; p < themis::NUM_PROGRESSIONS; p++) {
            if (ImGui::Selectable(themis::progressions[p].name, displayProgIndex == p)) {
                // Set as pending - will switch at end of current progression
                if (p != sequencer->polyVoice.progressionIndex) {
                    sequencer->polyState.pendingProgressionIndex = p;
                } else {
                    // If selecting current, cancel pending
                    sequencer->polyState.pendingProgressionIndex = -1;
                }
            }
        }
        ImGui::EndCombo();
    }
    if (hasPending && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Will change at end of current progression");
    }

    // Rate
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65);
    if (ImGui::BeginCombo("##rate", themis::chordRateNames[sequencer->polyVoice.chordRate])) {
        for (int r = 0; r < themis::NUM_CHORD_RATES; r++) {
            if (ImGui::Selectable(themis::chordRateNames[r],
                                   sequencer->polyVoice.chordRate == r)) {
                sequencer->polyVoice.chordRate = r;
            }
        }
        ImGui::EndCombo();
    }

    // Octave
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    int oct = sequencer->polyVoice.octaveOffset;
    if (ImGui::DragInt("##oct", &oct, 0.1f, -2, 2, "Oct%+d")) {
        sequencer->polyVoice.octaveOffset = oct;
    }

    // Show current chord info (and pending indicator)
    if (sequencer->polyVoice.active) {
        const auto& prog = themis::progressions[sequencer->polyVoice.progressionIndex];
        ImGui::SameLine();
        if (hasPending) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "[%d/%d->]",
                              sequencer->polyState.currentChordIndex + 1, prog.length);
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.6f, 0.2f, 1.0f), "[%d/%d]",
                              sequencer->polyState.currentChordIndex + 1, prog.length);
        }
    }

    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();

    // Rhythm Player section
    ImGui::Text("RHYTHM PLAYER");
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Rhythm")) {
        sequencer->RandomizeRhythmVoice();
    }

    ImGui::Separator();

    ImGui::PushID("RhythmCompact");

    // Active toggle and mode
    ImGui::Checkbox("Rhythm", &sequencer->rhythmVoice.active);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    const char* rhythmModeNames[] = {"Manual", "Morph"};
    if (ImGui::BeginCombo("##rmode", rhythmModeNames[sequencer->rhythmVoice.mode])) {
        for (int m = 0; m < themis::NUM_RHYTHM_PLAYER_MODES; m++) {
            if (ImGui::Selectable(rhythmModeNames[m], sequencer->rhythmVoice.mode == m)) {
                sequencer->rhythmVoice.mode = (themis::RhythmPlayerMode)m;
            }
        }
        ImGui::EndCombo();
    }

    // Style
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65);
    const char* styleNames[] = {"Chords", "Arps", "Poly"};
    if (ImGui::BeginCombo("##rstyle", styleNames[sequencer->rhythmVoice.playStyle])) {
        for (int s = 0; s < themis::NUM_RHYTHM_PLAY_STYLES; s++) {
            if (ImGui::Selectable(styleNames[s], sequencer->rhythmVoice.playStyle == s)) {
                sequencer->rhythmVoice.playStyle = (themis::RhythmPlayStyle)s;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Play style: Chords, Arpeggios, Polyrhythm");

    // Octave
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    int rhythmOct = sequencer->rhythmVoice.octaveOffset;
    if (ImGui::DragInt("##rOct", &rhythmOct, 0.1f, -2, 2, "Oct%+d")) {
        sequencer->rhythmVoice.octaveOffset = rhythmOct;
    }

    // MIDI Channel
    ImGui::SameLine();
    ImGui::SetNextItemWidth(45);
    int rhythmMidiCh = sequencer->rhythmVoice.midiChannel + 1;
    if (ImGui::DragInt("##rCh", &rhythmMidiCh, 0.1f, 1, 16, "Ch%d")) {
        sequencer->rhythmVoice.midiChannel = rhythmMidiCh - 1;
    }

    // Manual mode parameters (only if in manual mode)
    if (sequencer->rhythmVoice.mode == themis::RHYTHM_MODE_MANUAL) {
        // Activity level
        const char* activityNames[] = {"Sparse", "Mod", "Busy"};
        ImGui::SetNextItemWidth(55);
        if (ImGui::BeginCombo("##ract", activityNames[sequencer->rhythmVoice.activity])) {
            for (int a = 0; a < themis::NUM_RHYTHM_ACTIVITY_LEVELS; a++) {
                if (ImGui::Selectable(activityNames[a], sequencer->rhythmVoice.activity == a)) {
                    sequencer->rhythmVoice.activity = (themis::RhythmActivity)a;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Activity/density level");

        // Articulation
        ImGui::SameLine();
        const char* articNames[] = {"Stac", "Norm", "Leg"};
        ImGui::SetNextItemWidth(50);
        if (ImGui::BeginCombo("##rartic", articNames[sequencer->rhythmVoice.articulation])) {
            for (int a = 0; a < themis::NUM_RHYTHM_ARTICULATIONS; a++) {
                if (ImGui::Selectable(articNames[a], sequencer->rhythmVoice.articulation == a)) {
                    sequencer->rhythmVoice.articulation = (themis::RhythmArticulation)a;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Articulation: Staccato, Normal, Legato");

        // Arp direction (only relevant for arp mode)
        if (sequencer->rhythmVoice.playStyle == themis::RHYTHM_PLAY_ARPEGGIOS) {
            ImGui::SameLine();
            const char* arpDirNames[] = {"Up", "Dn", "U/D", "Rnd"};
            ImGui::SetNextItemWidth(45);
            if (ImGui::BeginCombo("##rarp", arpDirNames[sequencer->rhythmVoice.arpDirection])) {
                for (int d = 0; d < themis::NUM_ARP_DIRECTIONS; d++) {
                    if (ImGui::Selectable(arpDirNames[d], sequencer->rhythmVoice.arpDirection == d)) {
                        sequencer->rhythmVoice.arpDirection = (themis::ArpDirection)d;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Arpeggio direction");
        }

        // Follow kick
        ImGui::SameLine();
        ImGui::Checkbox("Kick##rfollow", &sequencer->rhythmVoice.followKick);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sync rhythm to kick pattern");
    }

    // Freeze checkbox - prevent automatic style changes
    ImGui::SameLine();
    ImGui::Checkbox("Freeze##rstyle", &sequencer->rhythmVoice.freezeStyle);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Prevent automatic style changes");

    // Show current state if active
    if (sequencer->rhythmVoice.active) {
        ImGui::SameLine();
        const char* styleDisplayNames[] = {"Chords", "Arps", "Poly"};
        ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.6f, 1.0f), "[%s I:%.0f%%]",
                          styleDisplayNames[sequencer->rhythmState.currentStyle],
                          sequencer->rhythmState.intensity * 100.0f);
    }

    ImGui::PopID();

    ImGui::Spacing();
    ImGui::Separator();

    // Acid Voice section
    ImGui::Text("ACID VOICE");
    ImGui::SameLine();
    if (ImGui::SmallButton("Rnd Acid")) {
        sequencer->RandomizeAcidVoice();
    }

    ImGui::Separator();

    ImGui::PushID("AcidCompact");

    // Active toggle and mode
    ImGui::Checkbox("Acid", &sequencer->acidVoice.active);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    const char* acidModeNames[] = {"Manual", "Auto"};
    if (ImGui::BeginCombo("##amode", acidModeNames[sequencer->acidVoice.mode])) {
        for (int m = 0; m < themis::NUM_ACID_MODES; m++) {
            if (ImGui::Selectable(acidModeNames[m], sequencer->acidVoice.mode == m)) {
                sequencer->acidVoice.mode = (themis::AcidMode)m;
            }
        }
        ImGui::EndCombo();
    }

    // Activity level
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65);
    const char* acidActivityNames[] = {"Sparse", "Mod", "Busy"};
    if (ImGui::BeginCombo("##aact", acidActivityNames[sequencer->acidVoice.activity])) {
        for (int a = 0; a < themis::NUM_ACID_ACTIVITIES; a++) {
            if (ImGui::Selectable(acidActivityNames[a], sequencer->acidVoice.activity == a)) {
                sequencer->acidVoice.activity = (themis::AcidActivity)a;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Note density/activity");

    // Octave
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    int acidOct = sequencer->acidVoice.octaveOffset;
    if (ImGui::DragInt("##aOct", &acidOct, 0.1f, -2, 2, "Oct%+d")) {
        sequencer->acidVoice.octaveOffset = acidOct;
    }

    // MIDI Channel
    ImGui::SameLine();
    ImGui::SetNextItemWidth(45);
    int acidMidiCh = sequencer->acidVoice.midiChannel + 1;
    if (ImGui::DragInt("##aCh", &acidMidiCh, 0.1f, 1, 16, "Ch%d")) {
        sequencer->acidVoice.midiChannel = acidMidiCh - 1;
    }

    // Manual mode parameters (pattern selection)
    if (sequencer->acidVoice.mode == themis::ACID_MODE_MANUAL) {
        // Rhythm pattern
        ImGui::SetNextItemWidth(80);
        char rhythmPatLabel[32];
        themis::GetAcidPatternName(sequencer->acidVoice.rhythmPattern, 0, rhythmPatLabel, 16);
        if (ImGui::BeginCombo("R##rpat", rhythmPatLabel)) {
            for (int p = 0; p < themis::NUM_ACID_RHYTHM_PATTERNS; p++) {
                char patName[32];
                themis::GetAcidPatternName(p, 0, patName, 16);
                if (ImGui::Selectable(patName, sequencer->acidVoice.rhythmPattern == p)) {
                    sequencer->acidVoice.rhythmPattern = p;
                    sequencer->acidState.currentRhythmPattern = p;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rhythm pattern preset");

        // Melody pattern
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        char melodyPatLabel[32];
        themis::GetAcidPatternName(0, sequencer->acidVoice.melodyPattern, melodyPatLabel, 16);
        // Extract just melody name (after '/')
        const char* melName = melodyPatLabel;
        const char* slash = strchr(melodyPatLabel, '/');
        if (slash) melName = slash + 1;
        if (ImGui::BeginCombo("M##mpat", melName)) {
            for (int p = 0; p < themis::NUM_ACID_MELODY_PATTERNS; p++) {
                char patName[32];
                themis::GetAcidPatternName(0, p, patName, 16);
                const char* mName = patName;
                const char* sl = strchr(patName, '/');
                if (sl) mName = sl + 1;
                if (ImGui::Selectable(mName, sequencer->acidVoice.melodyPattern == p)) {
                    sequencer->acidVoice.melodyPattern = p;
                    sequencer->acidState.currentMelodyPattern = p;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Melody pattern preset");
    }

    // Show current pattern info if active
    if (sequencer->acidVoice.active) {
        ImGui::SameLine();
        char patternName[32];
        themis::GetAcidPatternName(
            sequencer->acidState.currentRhythmPattern,
            sequencer->acidState.currentMelodyPattern,
            patternName, 32);
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.1f, 1.0f), "[%s]", patternName);
    }

    ImGui::PopID();

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

    // Compat mode (chord-aware melody mapping)
    ImGui::SameLine();
    ImGui::SetNextItemWidth(55);
    if (ImGui::BeginCombo("##compat", themis::compatModeNames[voice->compatMode])) {
        for (int c = 0; c < themis::NUM_COMPAT_MODES; c++) {
            if (ImGui::Selectable(themis::compatModeNames[c], voice->compatMode == c)) {
                voice->compatMode = (themis::MelodyCompatMode)c;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Chord mapping: Chord=strict chord tones, Penta=pentatonic, Scale=full scale");
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Poly Voice (Pads/Chords) panel
    if (ImGui::CollapsingHeader("Poly Voice (Pads)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("PolyVoice");
        ImGui::Indent();

        // Active toggle
        ImGui::Checkbox("Active", &sequencer->polyVoice.active);

        ImGui::SameLine();
        if (ImGui::Button("Randomize##Poly")) {
            sequencer->RandomizePolyVoice();
        }

        // Progression selector
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("Progression", themis::progressions[sequencer->polyVoice.progressionIndex].name)) {
            for (int p = 0; p < themis::NUM_PROGRESSIONS; p++) {
                if (ImGui::Selectable(themis::progressions[p].name,
                                       sequencer->polyVoice.progressionIndex == p)) {
                    sequencer->polyVoice.progressionIndex = p;
                    sequencer->polyState.Init();
                }
            }
            ImGui::EndCombo();
        }

        // Chord rate
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (ImGui::BeginCombo("Rate", themis::chordRateNames[sequencer->polyVoice.chordRate])) {
            for (int r = 0; r < themis::NUM_CHORD_RATES; r++) {
                if (ImGui::Selectable(themis::chordRateNames[r],
                                       sequencer->polyVoice.chordRate == r)) {
                    sequencer->polyVoice.chordRate = r;
                }
            }
            ImGui::EndCombo();
        }

        // Octave offset
        ImGui::SetNextItemWidth(60);
        int octave = sequencer->polyVoice.octaveOffset;
        if (ImGui::InputInt("Octave", &octave)) {
            if (octave < -2) octave = -2;
            if (octave > 2) octave = 2;
            sequencer->polyVoice.octaveOffset = octave;
        }

        // Velocity
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        int vel = sequencer->polyVoice.velocity;
        if (ImGui::SliderInt("Velocity", &vel, 1, 127)) {
            sequencer->polyVoice.velocity = vel;
        }

        // Variation mode
        ImGui::SetNextItemWidth(60);
        const char* varModes[] = {"Off", "A/B"};
        int varMode = (sequencer->polyVoice.variationMode == themis::VAR_MODE_OFF) ? 0 : 1;
        if (ImGui::Combo("Variation", &varMode, varModes, 2)) {
            sequencer->polyVoice.variationMode = (varMode == 0)
                ? themis::VAR_MODE_OFF : themis::VAR_MODE_AB;
        }

        // B Progression (if variation is on)
        if (sequencer->polyVoice.variationMode != themis::VAR_MODE_OFF) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo("B Prog", themis::progressions[sequencer->polyVoice.progressionB].name)) {
                for (int p = 0; p < themis::NUM_PROGRESSIONS; p++) {
                    if (ImGui::Selectable(themis::progressions[p].name,
                                           sequencer->polyVoice.progressionB == p)) {
                        sequencer->polyVoice.progressionB = p;
                    }
                }
                ImGui::EndCombo();
            }
        }

        // Show current chord info
        if (sequencer->polyVoice.active) {
            const auto& prog = themis::progressions[sequencer->polyVoice.progressionIndex];
            ImGui::Text("Current: Chord %d/%d", sequencer->polyState.currentChordIndex + 1, prog.length);
        }

        ImGui::Unindent();
        ImGui::PopID();
    }
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

// RenderSynthParams() is implemented in ui_synth.cpp

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

// RenderPatternVisualization() is implemented in ui_pattern.cpp
// RenderMixer() and TriggerActivity*() are implemented in ui_mixer.cpp

} // namespace themis_ui
