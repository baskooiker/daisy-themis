/**
 * @file display.cpp
 * @brief OLED display rendering implementation
 */

#include "display.h"
#include <string>

void UpdateDisplay()
{
    hw.display.Fill(false);

    std::string str;
    char*       cstr;
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
                        melodyMidiChannel + 1);
                hw.display.WriteString(buffer, Font_6x8, true);
            }
            break;

        case DISPLAY_CONFIG_MENU:
        {
            // Show config menu with values (arrow on config name)
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            // Calculate scroll offset to keep selected item visible
            // Display can show 5 items (rows 10, 20, 30, 40, 50)
            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue; // Skip items outside visible area
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, ">%-11s%d",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            (int)bpm);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%d", configOptionNames[i], (int)bpm);
                    }
                }
                else if(i == CONFIG_OUT2_DIVISION)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            outDivisionNames[currentOut2Division]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], outDivisionNames[currentOut2Division]);
                    }
                }
                else if(i == CONFIG_OUT3_DIVISION)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            outDivisionNames[currentOut3Division]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], outDivisionNames[currentOut3Division]);
                    }
                }
                else if(i == CONFIG_FREEZE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            freezeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], freezeEnabled ? "On" : "Off");
                    }
                }
                else if(i == CONFIG_MELODY_SCALE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            scaleNames[melodyScale]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], scaleNames[melodyScale]);
                    }
                }
                else if(i == CONFIG_MELODY_ROOT)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            rootNoteNames[melodyRoot]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], rootNoteNames[melodyRoot]);
                    }
                }
                else if(i == CONFIG_CV_STYLE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyStyleNames[melodyVoice.style]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyStyleNames[melodyVoice.style]);
                    }
                }
                else if(i == CONFIG_MIDI_STYLE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyStyleNames[melodyMidiVoice.style]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyStyleNames[melodyMidiVoice.style]);
                    }
                }
                else if(i == CONFIG_MIDI_MEL_CH)
                {
                    sprintf(buffer, ">%-11s%d",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyMidiChannel + 1);  // Display 1-indexed
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%d", configOptionNames[i], melodyMidiChannel + 1);
                    }
                }
                else if(i == CONFIG_MELODY_FREEZE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyFreezeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyFreezeEnabled ? "On" : "Off");
                    }
                }
                else if(i == CONFIG_TUNE_MODE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            tuneModeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], tuneModeEnabled ? "On" : "Off");
                    }
                }
                else // CONFIG_PATTERN_INFO, CONFIG_BACK, or CONFIG_RANDOMIZE_ALL
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
            // Show config menu with values (arrow on value being edited)
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            // Calculate scroll offset to keep selected item visible
            // Display can show 5 items (rows 10, 20, 30, 40, 50)
            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue; // Skip items outside visible area
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, " %-10s%s%d",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            (int)bpm);
                }
                else if(i == CONFIG_OUT2_DIVISION)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            outDivisionNames[currentOut2Division]);
                }
                else if(i == CONFIG_OUT3_DIVISION)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            outDivisionNames[currentOut3Division]);
                }
                else if(i == CONFIG_FREEZE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            freezeEnabled ? "On" : "Off");
                }
                else if(i == CONFIG_MELODY_SCALE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            scaleNames[melodyScale]);
                }
                else if(i == CONFIG_MELODY_ROOT)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            rootNoteNames[melodyRoot]);
                }
                else if(i == CONFIG_CV_STYLE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyStyleNames[melodyVoice.style]);
                }
                else if(i == CONFIG_MIDI_STYLE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyStyleNames[melodyMidiVoice.style]);
                }
                else if(i == CONFIG_MIDI_MEL_CH)
                {
                    sprintf(buffer, " %-10s%s%d",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyMidiChannel + 1);  // Display 1-indexed
                }
                else if(i == CONFIG_MELODY_FREEZE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyFreezeEnabled ? "On" : "Off");
                }
                else if(i == CONFIG_TUNE_MODE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            tuneModeEnabled ? "On" : "Off");
                }
                else // CONFIG_PATTERN_INFO, CONFIG_BACK, or CONFIG_RANDOMIZE_ALL
                {
                    sprintf(buffer, " %s", configOptionNames[i]);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }

            // Show hint at bottom
            if(externalClockMode && currentConfigOption == CONFIG_BPM)
            {
                hw.display.SetCursor(0, 50);
                hw.display.WriteString("(External Clock)", Font_6x8, true);
            }
        }
        break;

        case DISPLAY_PATTERN_INFO:
            // Pattern info display
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=PATTERN INFO=", Font_6x8, true);

            // Voice labels (will be updated for index 3 based on which plays backbeat)
            const char* voiceLabels[] = {"D1", "D2", "ML", "??", "H2", "An"};
            voiceLabels[3] = (fundamentalBeatVoice == CLAP) ? "SN" : "CL";

            const char* styleNames[] = {"Syn", "Str", "Euc", "AEu", "FKi"};
            const char* densityNames[] = {"Lo", "Md", "Hi"};
            const char* interactionSymbols[] = {"", "Div", "AB", "AH", "A2"};
            const char* melStyleNames[] = {"Sup", "Arp"};

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

                    // Find interaction partner name
                    char partnerName[4] = "";
                    if(voice->interaction != INTERACTION_NONE)
                    {
                        // Find which voice index is the partner
                        for(int j = 0; j < 6; j++)
                        {
                            if(generativeVoices[j].voice == voice->interactionPartner)
                            {
                                strncpy(partnerName, voiceLabels[j], 3);
                                partnerName[2] = '\0';
                                break;
                            }
                        }
                    }

                    // Format: "D1 :Euc Hi L32 >D2" (aligned columns)
                    sprintf(buffer, "%-2s:%s %s L%-2d%s%s",
                            voiceLabels[voiceIdx],
                            styleNames[voice->rhythmStyle],
                            densityNames[voice->density],
                            voice->patternLength,
                            (voice->interaction != INTERACTION_NONE) ? " >" : "",
                            partnerName);
                }
                else
                {
                    // Melody voices (lineIdx 7 = CV, lineIdx 8 = MIDI)
                    MelodyConfig* mel = (lineIdx == 7) ? &melodyVoice : &melodyMidiVoice;
                    const char* melLabel = (lineIdx == 7) ? "CV" : "MD";

                    // Format: "CV:Sup Euc Lo L32"
                    sprintf(buffer, "%-2s:%s %s %s L%-2d",
                            melLabel,
                            melStyleNames[mel->style],
                            styleNames[mel->rhythmStyle],
                            densityNames[mel->density],
                            mel->patternLength);
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
            break;
    }

    hw.display.Update();
}
