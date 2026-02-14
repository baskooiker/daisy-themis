/**
 * @file display.cpp
 * @brief OLED display rendering implementation
 */

#include "display.h"
#include "drums.h"

void UpdateDisplay()
{
    hw.display.Fill(false);

    char        buffer[30];
    int         configScrollOffset = 0;
    int         displayRow = 0;

    switch(currentDisplayState)
    {
        case DISPLAY_DEFAULT:
            // BPM, Mode, Status, and Bar:Beat counter on one line
            hw.display.SetCursor(0, 0);
            if(isRunning)
            {
                // Calculate bar (1-8) using barCounter (counts 2-bar phrases)
                int bar = (barCounter * 2) + (currentStep / 16) + 1;  // 1-8
                int beat = (currentStep % 16) / 4 + 1; // 1-4
                sprintf(buffer, "BPM:%d %s %s %d:%d",
                        (int)bpm,
                        externalClockMode ? "EXT" : "INT",
                        isRunning ? "RUN" : "STOP",
                        bar, beat);
            }
            else
            {
                sprintf(buffer, "BPM:%d %s %s",
                        (int)bpm,
                        externalClockMode ? "EXT" : "INT",
                        isRunning ? "RUN" : "STOP");
            }
            hw.display.WriteString(buffer, Font_6x8, true);

            // Show tune mode indicator or normal melody info
            if(tuneModeEnabled)
            {
                // Tune mode active - show clear indicator
                hw.display.SetCursor(0, 12);
                hw.display.WriteString(">>> TUNE MODE <<<", Font_6x8, true);
                hw.display.SetCursor(0, 22);
                hw.display.WriteString("Middle C (1V/60)", Font_6x8, true);
            }
            else
            {
                // Shared scale/root info (Line 2)
                hw.display.SetCursor(0, 12);
                sprintf(buffer, "Key: %s %s",
                        rootNoteNames[melodyRoot],
                        scaleNames[melodyScale]);
                hw.display.WriteString(buffer, Font_6x8, true);

                // CV voice info (Line 3)
                hw.display.SetCursor(0, 22);
                sprintf(buffer, "CV: %s", melodyStyleNames[melodyVoice.style]);
                hw.display.WriteString(buffer, Font_6x8, true);

                // MIDI voice info (Line 4)
                hw.display.SetCursor(0, 32);
                sprintf(buffer, "MIDI: %s Ch:%d",
                        melodyStyleNames[melodyMidiVoice.style],
                        melodyChannel + 1);
                hw.display.WriteString(buffer, Font_6x8, true);
            }
            break;

        case DISPLAY_CONFIG_MENU:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue;
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, "%s%-11s%d",
                            (i == currentConfigOption) ? ">" : " ",
                            configOptionNames[i], (int)bpm);
                }
                else if(i == CONFIG_TUNE_MODE)
                {
                    sprintf(buffer, "%s%-11s%s",
                            (i == currentConfigOption) ? ">" : " ",
                            configOptionNames[i], tuneModeEnabled ? "On" : "Off");
                }
                else
                {
                    sprintf(buffer, "%s%s",
                            (i == currentConfigOption) ? ">" : " ",
                            configOptionNames[i]);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_CONFIG_EDIT:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue;
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, " %-10s%s%d",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            (int)bpm);
                }
                else
                {
                    sprintf(buffer, " %s", configOptionNames[i]);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }

            if(externalClockMode && currentConfigOption == CONFIG_BPM)
            {
                hw.display.SetCursor(0, 50);
                hw.display.WriteString("(External Clock)", Font_6x8, true);
            }
        }
        break;

        case DISPLAY_PATTERN_INFO:
        {
            // Pattern info display
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=PATTERN INFO=", Font_6x8, true);

            // Voice labels (will be updated for index 3 based on which plays backbeat)
            const char* voiceLabels[] = {"D1", "D2", "ML", "??", "H2", "An"};
            voiceLabels[3] = (fundamentalBeatVoice == CLAP) ? "SN" : "CL";

            const char* styleNames[] = {"Syn", "Str", "Euc", "AEu", "FKi"};
            const char* densityNames[] = {"Lo", "Md", "Hi"};
            const char* melStyleNames[] = {"Sup", "Arp"};
            const char varChars[] = {'A', 'B', 'C'};

            // We have 9 total lines: 1 fundamental beat + 6 generative voices + 2 melody voices
            // Display can show 4 lines at once (rows 10, 24, 38, 52) - tighter spacing for 64px display
            int totalLines = 9;
            int maxScroll = totalLines - 4;

            // Display 4 lines starting from scroll offset
            for(int i = 0; i < 4; i++)
            {
                int lineIdx = i + patternInfoScroll;
                if(lineIdx >= totalLines) break;

                hw.display.SetCursor(0, 10 + i * 14);

                if(lineIdx == 0)
                {
                    // First line: fundamental beat
                    char beatVoiceName = (fundamentalBeatVoice == CLAP) ? 'C' : 'S';
                    sprintf(buffer, "%-2c:Fund Beat", beatVoiceName);
                }
                else if(lineIdx <= 6)
                {
                    // Generative voices (lineIdx 1-6 = voiceIdx 0-5)
                    int voiceIdx = lineIdx - 1;
                    VoiceConfig* voice = &generativeVoices[voiceIdx];

                    // Get current variation indicator
                    char varIndicator[4] = "";
                    if(voice->variation.mode != VAR_MODE_OFF)
                    {
                        uint8_t var = GetCurrentVariation(&voice->variation, currentStep, barCounter);
                        sprintf(varIndicator, "[%c]", varChars[var]);
                    }

                    // Format: "D1:Euc Hi L32 [A]" (with variation indicator when enabled)
                    sprintf(buffer, "%-2s:%s %s L%-2d%s",
                            voiceLabels[voiceIdx],
                            styleNames[voice->rhythmStyle],
                            densityNames[voice->density],
                            voice->patternLength,
                            varIndicator);
                }
                else
                {
                    // Melody voices (lineIdx 7 = CV, lineIdx 8 = MIDI)
                    MelodyConfig* mel = (lineIdx == 7) ? &melodyVoice : &melodyMidiVoice;
                    const char* melLabel = (lineIdx == 7) ? "CV" : "MD";

                    // Get current variation indicator
                    char varIndicator[4] = "";
                    if(mel->variation.mode != VAR_MODE_OFF)
                    {
                        uint8_t var = GetCurrentVariation(&mel->variation, currentStep, barCounter);
                        sprintf(varIndicator, "[%c]", varChars[var]);
                    }

                    // Format: "CV:Sup Euc Lo L32 [A]"
                    sprintf(buffer, "%-2s:%s %s %s L%-2d%s",
                            melLabel,
                            melStyleNames[mel->style],
                            styleNames[mel->rhythmStyle],
                            densityNames[mel->density],
                            mel->patternLength,
                            varIndicator);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }

            // Show scroll indicators
            if(patternInfoScroll > 0)
            {
                hw.display.SetCursor(120, 10);
                hw.display.WriteString("^", Font_6x8, true);
            }
            if(patternInfoScroll < maxScroll)
            {
                hw.display.SetCursor(120, 50);
                hw.display.WriteString("v", Font_6x8, true);
            }
        }
        break;

        case DISPLAY_FREEZE_MENU:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== FREEZE ===", Font_6x8, true);

            // Calculate scroll offset
            freezeScrollOffset = 0;
            if(currentFreezeOption > 4) freezeScrollOffset = currentFreezeOption - 4;

            // Check if all individual freezes are on
            bool allFrozen = freezeEnabled && melodyFreezeEnabled
                && bassVoiceConfig.freezePattern && rhythmPlayerConfig.freezeStyle
                && chordRandomizerConfig.freezeEnabled;

            for(int i = 0; i < NUM_FREEZE_OPTIONS; i++)
            {
                int row = i - freezeScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                const char* val = "";
                if(i == FREEZE_ALL)
                    val = allFrozen ? "On" : "Off";
                else if(i == FREEZE_DRUMS)
                    val = freezeEnabled ? "On" : "Off";
                else if(i == FREEZE_MELODY)
                    val = melodyFreezeEnabled ? "On" : "Off";
                else if(i == FREEZE_BASS)
                    val = bassVoiceConfig.freezePattern ? "On" : "Off";
                else if(i == FREEZE_RHYTHM)
                    val = rhythmPlayerConfig.freezeStyle ? "On" : "Off";
                else if(i == FREEZE_CHORDS)
                    val = chordRandomizerConfig.freezeEnabled ? "On" : "Off";

                if(i == FREEZE_BACK)
                    sprintf(buffer, "%s%s", (i == currentFreezeOption) ? ">" : " ", freezeOptionNames[i]);
                else
                    sprintf(buffer, "%s%-11s%s", (i == currentFreezeOption) ? ">" : " ", freezeOptionNames[i], val);

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_SYSTEM_MENU:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== SYSTEM ===", Font_6x8, true);

            systemScrollOffset = 0;
            if(currentSystemOption > 4) systemScrollOffset = currentSystemOption - 4;

            for(int i = 0; i < NUM_SYSTEM_OPTIONS; i++)
            {
                int row = i - systemScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                if(i == SYSTEM_BACK)
                {
                    sprintf(buffer, "%s%s", (i == currentSystemOption) ? ">" : " ", systemOptionNames[i]);
                }
                else
                {
                    uint8_t ch = 0;
                    if(i == SYSTEM_MELODY_CH) ch = melodyChannel;
                    else if(i == SYSTEM_DRUM_MIDI_CH) ch = drumMidiChannel;
                    else if(i == SYSTEM_BASS_MIDI_CH) ch = bassMidiChannel;
                    else if(i == SYSTEM_RHYTHM_MIDI_CH) ch = rhythmMidiChannel;

                    sprintf(buffer, "%s%-11sCh%d", (i == currentSystemOption) ? ">" : " ",
                            systemOptionNames[i], ch + 1);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_SYSTEM_EDIT:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== SYSTEM ===", Font_6x8, true);

            systemScrollOffset = 0;
            if(currentSystemOption > 4) systemScrollOffset = currentSystemOption - 4;

            for(int i = 0; i < NUM_SYSTEM_OPTIONS; i++)
            {
                int row = i - systemScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                if(i == SYSTEM_BACK)
                {
                    sprintf(buffer, " %s", systemOptionNames[i]);
                }
                else
                {
                    uint8_t ch = 0;
                    if(i == SYSTEM_MELODY_CH) ch = melodyChannel;
                    else if(i == SYSTEM_DRUM_MIDI_CH) ch = drumMidiChannel;
                    else if(i == SYSTEM_BASS_MIDI_CH) ch = bassMidiChannel;
                    else if(i == SYSTEM_RHYTHM_MIDI_CH) ch = rhythmMidiChannel;

                    sprintf(buffer, " %-10s%sCh%d", systemOptionNames[i],
                            (i == currentSystemOption) ? ">" : " ", ch + 1);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_HARMONY_MENU:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== HARMONY ===", Font_6x8, true);

            harmonyScrollOffset = 0;
            if(currentHarmonyOption > 4) harmonyScrollOffset = currentHarmonyOption - 4;

            for(int i = 0; i < NUM_HARMONY_OPTIONS; i++)
            {
                int row = i - harmonyScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                const char* val = "";
                if(i == HARMONY_SCALE) val = scaleNames[melodyScale];
                else if(i == HARMONY_ROOT) val = rootNoteNames[melodyRoot];
                else if(i == HARMONY_PROGRESSION) val = progressions[chordVoice.progressionIndex].name;
                else if(i == HARMONY_RATE) val = chordRateNames[chordVoice.chordRate];

                if(i == HARMONY_BACK)
                    sprintf(buffer, "%s%s", (i == currentHarmonyOption) ? ">" : " ", harmonyOptionNames[i]);
                else
                    sprintf(buffer, "%s%-11s%s", (i == currentHarmonyOption) ? ">" : " ",
                            harmonyOptionNames[i], val);

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_HARMONY_EDIT:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== HARMONY ===", Font_6x8, true);

            harmonyScrollOffset = 0;
            if(currentHarmonyOption > 4) harmonyScrollOffset = currentHarmonyOption - 4;

            for(int i = 0; i < NUM_HARMONY_OPTIONS; i++)
            {
                int row = i - harmonyScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                const char* val = "";
                if(i == HARMONY_SCALE) val = scaleNames[melodyScale];
                else if(i == HARMONY_ROOT) val = rootNoteNames[melodyRoot];
                else if(i == HARMONY_PROGRESSION) val = progressions[chordVoice.progressionIndex].name;
                else if(i == HARMONY_RATE) val = chordRateNames[chordVoice.chordRate];

                if(i == HARMONY_BACK)
                    sprintf(buffer, " %s", harmonyOptionNames[i]);
                else
                    sprintf(buffer, " %-10s%s%s", harmonyOptionNames[i],
                            (i == currentHarmonyOption) ? ">" : " ", val);

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_VOICES_MENU:
        {
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== VOICES ===", Font_6x8, true);

            voiceScrollOffset = 0;
            if(currentVoiceMenuItem > 4) voiceScrollOffset = currentVoiceMenuItem - 4;

            for(int i = 0; i < NUM_VOICE_MENU_ITEMS; i++)
            {
                int row = i - voiceScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                if(i == VOICE_BACK)
                {
                    sprintf(buffer, "%s%s", (i == currentVoiceMenuItem) ? ">" : " ", voiceMenuNames[i]);
                }
                else
                {
                    bool active = false;
                    if(i == VOICE_MELODY) active = melodyMidiVoice.active;
                    else if(i == VOICE_BASS) active = bassVoiceConfig.active;
                    else if(i == VOICE_RHYTHM) active = rhythmPlayerConfig.active;

                    sprintf(buffer, "%s%-13s%s",
                            (i == currentVoiceMenuItem) ? ">" : " ",
                            voiceMenuNames[i],
                            active ? "On" : "Off");
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_VOICE_DETAIL:
        {
            hw.display.SetCursor(0, 0);
            const char* voiceHeaders[] = {"== MELODY ==", "== BASS ==", "== RHYTHM =="};
            hw.display.WriteString(voiceHeaders[currentVoiceMenuItem], Font_6x8, true);

            // Melody: Active, Style, Back (3)
            // Bass: Active, Octave, Back (3)
            // Rhythm: Active, Mode, Octave, Back (4)
            uint8_t itemCount = (currentVoiceMenuItem == VOICE_RHYTHM) ? 4 : 3;

            voiceDetailScrollOffset = 0;
            if(currentVoiceDetail > 4) voiceDetailScrollOffset = currentVoiceDetail - 4;

            for(int i = 0; i < itemCount; i++)
            {
                int row = i - voiceDetailScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                bool isBack = (i == itemCount - 1);
                bool isSelected = (i == (int)currentVoiceDetail);

                if(isBack)
                {
                    sprintf(buffer, "%sBack", isSelected ? ">" : " ");
                }
                else if(i == 0)
                {
                    bool active = false;
                    if(currentVoiceMenuItem == VOICE_MELODY) active = melodyMidiVoice.active;
                    else if(currentVoiceMenuItem == VOICE_BASS) active = bassVoiceConfig.active;
                    else if(currentVoiceMenuItem == VOICE_RHYTHM) active = rhythmPlayerConfig.active;
                    sprintf(buffer, "%s%-11s%s", isSelected ? ">" : " ", "Active", active ? "On" : "Off");
                }
                else if(currentVoiceMenuItem == VOICE_MELODY)
                {
                    sprintf(buffer, "%s%-11s%s", isSelected ? ">" : " ", "Style",
                            melodyStyleNames[melodyMidiVoice.style]);
                }
                else if(currentVoiceMenuItem == VOICE_RHYTHM)
                {
                    if(i == 1)
                    {
                        sprintf(buffer, "%s%-11s%s", isSelected ? ">" : " ", "Mode",
                                rhythmModeNames[rhythmPlayerConfig.mode]);
                    }
                    else
                    {
                        sprintf(buffer, "%s%-11s%d", isSelected ? ">" : " ", "Octave",
                                rhythmPlayerConfig.octaveOffset);
                    }
                }
                else
                {
                    // Bass: octave (pos 1)
                    sprintf(buffer, "%s%-11s%d", isSelected ? ">" : " ", "Octave",
                            bassVoiceConfig.octaveOffset);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_VOICE_EDIT:
        {
            hw.display.SetCursor(0, 0);
            const char* voiceHeaders[] = {"== MELODY ==", "== BASS ==", "== RHYTHM =="};
            hw.display.WriteString(voiceHeaders[currentVoiceMenuItem], Font_6x8, true);

            uint8_t itemCount = (currentVoiceMenuItem == VOICE_RHYTHM) ? 4 : 3;

            voiceDetailScrollOffset = 0;
            if(currentVoiceDetail > 4) voiceDetailScrollOffset = currentVoiceDetail - 4;

            for(int i = 0; i < itemCount; i++)
            {
                int row = i - voiceDetailScrollOffset;
                if(row < 0 || row > 4) continue;
                hw.display.SetCursor(0, 10 + row * 10);

                bool isBack = (i == itemCount - 1);
                bool isSelected = (i == (int)currentVoiceDetail);

                if(isBack)
                {
                    sprintf(buffer, " Back");
                }
                else if(i == 0)
                {
                    bool active = false;
                    if(currentVoiceMenuItem == VOICE_MELODY) active = melodyMidiVoice.active;
                    else if(currentVoiceMenuItem == VOICE_BASS) active = bassVoiceConfig.active;
                    else if(currentVoiceMenuItem == VOICE_RHYTHM) active = rhythmPlayerConfig.active;
                    sprintf(buffer, " %-10s%s%s", "Active", isSelected ? ">" : " ", active ? "On" : "Off");
                }
                else if(currentVoiceMenuItem == VOICE_BASS)
                {
                    sprintf(buffer, " %-10s%s%d", "Octave", isSelected ? ">" : " ",
                            bassVoiceConfig.octaveOffset);
                }
                else if(currentVoiceMenuItem == VOICE_RHYTHM && i == 2)
                {
                    sprintf(buffer, " %-10s%s%d", "Octave", isSelected ? ">" : " ",
                            rhythmPlayerConfig.octaveOffset);
                }
                else
                {
                    sprintf(buffer, " %s", "");
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;
    }

    hw.display.Update();
}
