/**
 * @file ui_pattern.cpp
 * @brief Pattern visualization UI implementation
 */

#include "ui_internal.h"

namespace themis_ui {

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
        (void)varLabel;  // Suppress unused warning
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

    // Melody voice pattern
    {
        themis::MelodyConfig* melody = &sequencer->melodyVoice;
        const char* melodyName = "Melody";
        const char* melodyFullName = "Melody";
        int m = 0;

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
            melodyFullName,
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
        if (ImGui::Selectable(melodyName, false, ImGuiSelectableFlags_None, ImVec2(55, 0))) {
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

    ImGui::Separator();

    // Chord Voice info
    ImGui::PushID("ChordViz");
    if (sequencer->chordVoice.active) {
        const auto& prog = themis::progressions[sequencer->chordVoice.progressionIndex];

        // Create debug string for chord voice
        char chordDebug[512];
        snprintf(chordDebug, sizeof(chordDebug),
            "=== CHORD VOICE DEBUG ===\n"
            "Active: YES\n"
            "Progression: %s\n"
            "Chord Rate: %s\n"
            "Current Chord: %d/%d\n"
            "Steps Until Change: %d\n"
            "Octave Offset: %d\n"
            "Velocity: %d\n"
            "Notes On: %s\n"
            "Num Active Notes: %d\n",
            prog.name,
            themis::chordRateNames[sequencer->chordVoice.chordRate],
            sequencer->chordState.currentChordIndex + 1, prog.length,
            sequencer->chordState.stepsUntilChange,
            sequencer->chordVoice.octaveOffset,
            sequencer->chordVoice.velocity,
            sequencer->chordState.notesOn ? "YES" : "NO",
            sequencer->chordState.numActiveNotes);

        // Clickable chord name
        if (ImGui::Selectable("Chords", false, ImGuiSelectableFlags_None, ImVec2(50, 0))) {
            SDL_SetClipboardText(chordDebug);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
        ImGui::SameLine();

        // Show progression sequence
        ImGui::Text("Prog: %s | ", prog.name);
        ImGui::SameLine();

        // Display chord sequence with current position highlighted
        for (int c = 0; c < prog.length; c++) {
            bool isCurrent = (c == sequencer->chordState.currentChordIndex);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }

            // Show chord type abbreviation
            const char* chordAbbrev[] = {"M", "m", "dim", "aug", "s2", "s4", "M7", "m7", "7", "d7", "m7b5", "+9", "m+9"};
            int chordType = prog.steps[c].chordType;
            ImGui::Text("%d%s", prog.steps[c].scaleDegree, chordAbbrev[chordType]);

            if (isCurrent) {
                ImGui::PopStyleColor();
            }

            if (c < prog.length - 1) {
                ImGui::SameLine(0, 5);
                ImGui::Text("-");
                ImGui::SameLine(0, 5);
            }
        }

        // Show timing info
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
            "    Rate: %s | Oct: %+d | Steps to change: %d",
            themis::chordRateNames[sequencer->chordVoice.chordRate],
            sequencer->chordVoice.octaveOffset,
            sequencer->chordState.stepsUntilChange);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Chords (inactive)");
    }
    ImGui::PopID();

    ImGui::Separator();

    // Rhythm Player info
    ImGui::PushID("RhythmViz");
    if (sequencer->rhythmVoice.active) {
        const char* modeNames[] = {"Manual", "Morph"};
        const char* styleNames[] = {"Chords", "Polyrhythm"};
        const char* activityNames[] = {"Sparse", "Moderate", "Busy"};

        // Create debug string for rhythm player
        char rhythmDebug[512];
        snprintf(rhythmDebug, sizeof(rhythmDebug),
            "=== RHYTHM PLAYER DEBUG ===\n"
            "Active: YES\n"
            "Mode: %s\n"
            "Current Style: %s\n"
            "Target Style: %s\n"
            "Morph Progress: %.1f%%\n"
            "Intensity: %.1f%%\n"
            "Activity: %s\n"
            "Bar Position: %d\n"
            "Num Active Notes: %d\n"
            "Note Duration: %d\n"
            "Octave Offset: %d\n"
            "MIDI Channel: %d\n",
            modeNames[sequencer->rhythmVoice.mode],
            styleNames[sequencer->rhythmState.currentStyle],
            styleNames[sequencer->rhythmState.targetStyle],
            sequencer->rhythmState.styleMorphProgress * 100.0f,
            sequencer->rhythmState.intensity * 100.0f,
            activityNames[sequencer->rhythmVoice.activity],
            sequencer->rhythmState.barPosition,
            sequencer->rhythmState.numActiveNotes,
            sequencer->rhythmState.noteDuration,
            sequencer->rhythmVoice.octaveOffset,
            sequencer->rhythmVoice.midiChannel + 1);

        // Clickable rhythm name
        if (ImGui::Selectable("Rhythm", false, ImGuiSelectableFlags_None, ImVec2(50, 0))) {
            SDL_SetClipboardText(rhythmDebug);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
        ImGui::SameLine();

        // Show mode and style
        ImGui::Text("Mode: %s | Style: ", modeNames[sequencer->rhythmVoice.mode]);
        ImGui::SameLine();

        // Show current/target style with morph progress
        if (sequencer->rhythmState.styleMorphProgress < 1.0f) {
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "%s -> %s (%.0f%%)",
                              styleNames[sequencer->rhythmState.currentStyle],
                              styleNames[sequencer->rhythmState.targetStyle],
                              sequencer->rhythmState.styleMorphProgress * 100.0f);
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.4f, 0.6f, 1.0f), "%s",
                              styleNames[sequencer->rhythmState.currentStyle]);
        }

        ImGui::SameLine(0, 15);

        // Intensity bar visualization
        float intensity = sequencer->rhythmState.intensity;
        ImGui::ProgressBar(intensity, ImVec2(80, 14), "");
        ImGui::SameLine();
        ImGui::Text("I:%.0f%%", intensity * 100.0f);

        // Show activity and notes info
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
            "    Activity: %s | Notes: %d | Bar: %d/16",
            activityNames[sequencer->rhythmVoice.activity],
            sequencer->rhythmState.numActiveNotes,
            sequencer->rhythmState.barPosition + 1);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Rhythm (inactive)");
    }
    ImGui::PopID();

    ImGui::Separator();

    // Bass Voice pattern visualization
    ImGui::PushID("BassViz");
    if (sequencer->bassVoice.active) {
        uint8_t bassPatIdx = sequencer->bassState.currentPattern;
        if (bassPatIdx >= themis::NUM_BASS_PATTERNS) bassPatIdx = 0;
        const themis::BassPattern& bassPat = themis::bassPatterns[bassPatIdx];

        // Get pitch pattern info for debug
        uint8_t dbgPitchIdx = sequencer->bassState.currentPitchPattern;
        if (dbgPitchIdx >= themis::NUM_BASS_PITCH_PATTERNS) dbgPitchIdx = 0;
        const char* dbgPitchName = themis::bassPitchPatterns[dbgPitchIdx].name;

        // Create debug string for bass voice
        char bassDebug[512];
        snprintf(bassDebug, sizeof(bassDebug),
            "=== BASS VOICE DEBUG ===\n"
            "Active: YES\n"
            "Pattern: %s (#%d, len=%d)\n"
            "Pitch Pattern: %s (#%d, len=%d)\n"
            "Triggers: 0x%08X\n"
            "Accents:  0x%08X\n"
            "Holds:    0x%08X\n"
            "Freeze: %s\n"
            "Octave Offset: %d\n"
            "MIDI Channel: %d\n"
            "Current Note: %d\n"
            "Gate Remaining: %d\n",
            bassPat.name, bassPatIdx, bassPat.length,
            dbgPitchName, dbgPitchIdx, themis::bassPitchPatterns[dbgPitchIdx].length,
            bassPat.triggers, bassPat.accents, bassPat.holds,
            sequencer->bassVoice.freezePattern ? "YES" : "NO",
            sequencer->bassVoice.octaveOffset,
            sequencer->bassVoice.midiChannel + 1,
            sequencer->bassState.currentNote,
            sequencer->bassState.gateStepsRemaining);

        // Clickable bass name
        if (ImGui::Selectable("Bass", false, ImGuiSelectableFlags_None, ImVec2(40, 0))) {
            SDL_SetClipboardText(bassDebug);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to copy debug info");
        ImGui::SameLine();

        // Determine rhythm A/B state independently
        bool rhythmBIsActive = false;
        if (sequencer->bassVoice.rhythmVariation.mode != themis::VAR_MODE_OFF) {
            uint8_t var = themis::GetCurrentVariation(&sequencer->bassVoice.rhythmVariation,
                sequencer->currentStep, sequencer->barCounter);
            rhythmBIsActive = (var >= 1);
        }
        bool rhythmAIsActive = !rhythmBIsActive;

        // Determine pitch A/B state independently
        bool pitchBIsActive = false;
        if (sequencer->bassVoice.pitchVariation.mode != themis::VAR_MODE_OFF) {
            uint8_t var = themis::GetCurrentVariation(&sequencer->bassVoice.pitchVariation,
                sequencer->currentStep, sequencer->barCounter);
            pitchBIsActive = (var >= 1);
        }
        bool pitchAIsActive = !pitchBIsActive;

        // Rhythm pattern label with length
        char rhythmLabel[32];
        if (bassPat.length != 16) {
            snprintf(rhythmLabel, sizeof(rhythmLabel), "A: %s(%d) |", bassPat.name, bassPat.length);
        } else {
            snprintf(rhythmLabel, sizeof(rhythmLabel), "A: %s |", bassPat.name);
        }
        ImGui::Text("%s", rhythmLabel);
        ImGui::SameLine();

        // Render A rhythm pattern (variable length)
        uint8_t rhythmStep = sequencer->currentStep % bassPat.length;
        for (int i = 0; i < bassPat.length; i++) {
            bool isTrigger = themis::IsStepActiveVar(bassPat.triggers, i, bassPat.length);
            bool isAccent = themis::IsStepActiveVar(bassPat.accents, i, bassPat.length);
            bool isHold = themis::IsStepActiveVar(bassPat.holds, i, bassPat.length);
            bool isCurrent = (i == (int)rhythmStep && sequencer->isRunning && rhythmAIsActive);

            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            } else if (isAccent && isTrigger) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.1f, 1.0f));
            }

            const char* sym;
            if (isTrigger && isHold) sym = "O";
            else if (isTrigger && isAccent) sym = "X";
            else if (isTrigger) sym = "x";
            else sym = ".";

            ImGui::Text("%s", sym);

            if (isCurrent || (isAccent && isTrigger)) {
                ImGui::PopStyleColor();
            }

            if (i < bassPat.length - 1) ImGui::SameLine(0, 2);
            // Add bar separator for 32-step patterns
            if (i == 15 && bassPat.length > 16) {
                ImGui::SameLine(0, 10);
                ImGui::Text("|");
                ImGui::SameLine(0, 10);
            }
        }

        // Pitch pattern row (A) - variable length
        uint8_t pitchIdx = sequencer->bassState.currentPitchPattern;
        if (pitchIdx >= themis::NUM_BASS_PITCH_PATTERNS) pitchIdx = 0;
        const themis::BassPitchPattern& pitchPat = themis::bassPitchPatterns[pitchIdx];

        static const char* pitchSymbols[] = {
            "R", "U", "D", "3", "5", "7", "2", "4", "6", "+", "-"
        };

        char pitchLabel[32];
        if (pitchPat.length != 16) {
            snprintf(pitchLabel, sizeof(pitchLabel), "  P: %s(%d) |", pitchPat.name, pitchPat.length);
        } else {
            snprintf(pitchLabel, sizeof(pitchLabel), "  P: %s |", pitchPat.name);
        }
        ImGui::Text("%s", pitchLabel);
        ImGui::SameLine();

        uint8_t pitchStep = sequencer->currentStep % pitchPat.length;
        for (int i = 0; i < pitchPat.length; i++) {
            uint8_t pt = pitchPat.steps[i];
            bool isCurrent = (i == (int)pitchStep && sequencer->isRunning && pitchAIsActive);
            bool isRoot = (pt == themis::BASS_PITCH_ROOT);

            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            } else if (!isRoot) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            }

            const char* sym = (pt < themis::NUM_BASS_PITCH_TYPES) ? pitchSymbols[pt] : "?";
            ImGui::Text("%s", sym);

            if (isCurrent || !isRoot) {
                ImGui::PopStyleColor();
            }

            if (i < pitchPat.length - 1) ImGui::SameLine(0, 2);
            if (i == 15 && pitchPat.length > 16) {
                ImGui::SameLine(0, 10);
                ImGui::Text("|");
                ImGui::SameLine(0, 10);
            }
        }

        // Show B rhythm pattern row when rhythm variation is active
        if (sequencer->bassVoice.rhythmVariation.mode != themis::VAR_MODE_OFF) {
            uint8_t bPatIdx = sequencer->bassState.currentPatternB;
            if (bPatIdx >= themis::NUM_BASS_PATTERNS) bPatIdx = 0;
            const themis::BassPattern& bPat = themis::bassPatterns[bPatIdx];

            char bLabel[32];
            if (bPat.length != 16) {
                snprintf(bLabel, sizeof(bLabel), "  B: %s(%d) |", bPat.name, bPat.length);
            } else {
                snprintf(bLabel, sizeof(bLabel), "  B: %s |", bPat.name);
            }
            ImGui::Text("%s", bLabel);
            ImGui::SameLine();

            uint8_t bRhythmStep = sequencer->currentStep % bPat.length;
            for (int i = 0; i < bPat.length; i++) {
                bool isTrigger = themis::IsStepActiveVar(bPat.triggers, i, bPat.length);
                bool isAccent = themis::IsStepActiveVar(bPat.accents, i, bPat.length);
                bool isHold = themis::IsStepActiveVar(bPat.holds, i, bPat.length);
                bool isCurrent = (i == (int)bRhythmStep && sequencer->isRunning && rhythmBIsActive);

                if (isCurrent) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                } else if (isAccent && isTrigger) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.5f, 0.1f, 0.7f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.8f, 0.7f));
                }

                const char* sym;
                if (isTrigger && isHold) sym = "O";
                else if (isTrigger && isAccent) sym = "X";
                else if (isTrigger) sym = "x";
                else sym = ".";

                ImGui::Text("%s", sym);
                ImGui::PopStyleColor();

                if (i < bPat.length - 1) ImGui::SameLine(0, 2);
                if (i == 15 && bPat.length > 16) {
                    ImGui::SameLine(0, 10);
                    ImGui::Text("|");
                    ImGui::SameLine(0, 10);
                }
            }
        }

        // Show B pitch pattern row when pitch variation is active
        if (sequencer->bassVoice.pitchVariation.mode != themis::VAR_MODE_OFF) {
            uint8_t bPitchIdx = sequencer->bassState.currentPitchPatternB;
            if (bPitchIdx >= themis::NUM_BASS_PITCH_PATTERNS) bPitchIdx = 0;
            const themis::BassPitchPattern& bPitchPat = themis::bassPitchPatterns[bPitchIdx];

            char bpLabel[32];
            if (bPitchPat.length != 16) {
                snprintf(bpLabel, sizeof(bpLabel), "  P: %s(%d) |", bPitchPat.name, bPitchPat.length);
            } else {
                snprintf(bpLabel, sizeof(bpLabel), "  P: %s |", bPitchPat.name);
            }
            ImGui::Text("%s", bpLabel);
            ImGui::SameLine();

            uint8_t bPitchStep = sequencer->currentStep % bPitchPat.length;
            for (int i = 0; i < bPitchPat.length; i++) {
                uint8_t pt = bPitchPat.steps[i];
                bool isCurrent = (i == (int)bPitchStep && sequencer->isRunning && pitchBIsActive);
                bool isRoot = (pt == themis::BASS_PITCH_ROOT);

                if (isCurrent) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
                } else if (!isRoot) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.5f, 0.7f, 0.7f));
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.8f, 0.7f));
                }

                const char* sym = (pt < themis::NUM_BASS_PITCH_TYPES) ? pitchSymbols[pt] : "?";
                ImGui::Text("%s", sym);
                ImGui::PopStyleColor();

                if (i < bPitchPat.length - 1) ImGui::SameLine(0, 2);
                if (i == 15 && bPitchPat.length > 16) {
                    ImGui::SameLine(0, 10);
                    ImGui::Text("|");
                    ImGui::SameLine(0, 10);
                }
            }
        }

        // Legend and info
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.8f, 1.0f),
            "    Oct: %+d | Note: %d | x=hit X=accent O=hold | R=root U=8va 3=3rd 5=5th 7=7th",
            sequencer->bassVoice.octaveOffset,
            sequencer->bassState.currentNote);
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Bass   (inactive)");
    }
    ImGui::PopID();
}

} // namespace themis_ui
