/**
 * @file config.cpp
 * @brief Configuration and control implementation
 */

#include "config.h"
#include "groove.h"
#include "drums.h"
#include "melody.h"

// Timing flag - set in audio callback, processed in main loop
volatile bool trigger16thNote = false;

// ============================================================================
// PERSISTENT STORAGE
// ============================================================================

void SaveSettings()
{
    settings.magic = SETTINGS_MAGIC;
    settings.bpm = bpm;
    settings.out2Division = (uint8_t)currentOut2Division;
    settings.out3Division = (uint8_t)currentOut3Division;
    settings.freezeEnabled = freezeEnabled ? 1 : 0;
    settings.melodyScale = (uint8_t)melodyScale;
    settings.melodyRoot = melodyRoot;
    settings.cvMelodyStyle = (uint8_t)melodyVoice.style;
    settings.midiMelodyStyle = (uint8_t)melodyMidiVoice.style;
    settings.midiMelChannel = melodyMidiChannel;
    settings.melodyFreezeEnabled = melodyFreezeEnabled ? 1 : 0;

    // Write to QSPI flash
    size_t size = sizeof(PersistentSettings);
    uint32_t addr = 0x90000000; // QSPI memory-mapped base address
    hw.seed.qspi.Erase(addr, addr + size);
    hw.seed.qspi.Write(addr, size, (uint8_t*)&settings);
}

void LoadSettings()
{
    // Read from QSPI flash using memory-mapped access
    uint32_t addr = 0x90000000; // QSPI memory-mapped base address
    PersistentSettings* flash_settings = (PersistentSettings*)addr;

    // Copy from flash to RAM
    memcpy(&settings, flash_settings, sizeof(PersistentSettings));

    // Validate magic number
    if(settings.magic == SETTINGS_MAGIC)
    {
        // Valid settings found, apply them
        bpm = settings.bpm;

        // Validate BPM range
        if(bpm < 20.0f || bpm > 300.0f)
        {
            bpm = 120.0f; // Reset to default if out of range
        }

        // Validate OUT2 division
        if(settings.out2Division >= NUM_OUT_DIVISIONS)
        {
            currentOut2Division = DIV_1_8; // Reset to default
        }
        else
        {
            currentOut2Division = (OutDivision)settings.out2Division;
        }

        // Validate OUT3 division
        if(settings.out3Division >= NUM_OUT_DIVISIONS)
        {
            currentOut3Division = DIV_1_4; // Reset to default
        }
        else
        {
            currentOut3Division = (OutDivision)settings.out3Division;
        }

        // Load freeze setting
        freezeEnabled = (settings.freezeEnabled != 0);

        // Load shared melody scale and root
        if(settings.melodyScale < NUM_SCALE_TYPES)
        {
            melodyScale = (ScaleType)settings.melodyScale;
        }
        else
        {
            melodyScale = SCALE_MINOR;
        }

        if(settings.melodyRoot < 12)
        {
            melodyRoot = settings.melodyRoot;
        }
        else
        {
            melodyRoot = 0; // Default to C
        }

        // Load CV melody style
        if(settings.cvMelodyStyle < NUM_MELODY_STYLES)
        {
            melodyVoice.style = (MelodyStyle)settings.cvMelodyStyle;
        }
        else
        {
            melodyVoice.style = MELODY_SUPPORTING;
        }

        // Load MIDI melody style
        if(settings.midiMelodyStyle < NUM_MELODY_STYLES)
        {
            melodyMidiVoice.style = (MelodyStyle)settings.midiMelodyStyle;
        }
        else
        {
            melodyMidiVoice.style = MELODY_ARPEGGIATOR;
        }

        if(settings.midiMelChannel < 16)
        {
            melodyMidiChannel = settings.midiMelChannel;
        }
        else
        {
            melodyMidiChannel = 0; // Default to channel 1
        }

        melodyFreezeEnabled = (settings.melodyFreezeEnabled != 0);
    }
    else
    {
        // No valid settings found, use defaults
        bpm = 120.0f;
        currentOut2Division = DIV_1_8;
        currentOut3Division = DIV_1_4;
        freezeEnabled = false;
        melodyScale = SCALE_MINOR;
        melodyRoot = 0;
        melodyVoice.style = MELODY_SUPPORTING;
        melodyMidiVoice.style = MELODY_ARPEGGIATOR;
        melodyMidiChannel = 0;
        melodyFreezeEnabled = false;

        // Save defaults
        SaveSettings();
    }
}

// ============================================================================
// CLOCK MANAGEMENT
// ============================================================================

void UpdateClockFrequency()
{
    // Convert BPM to frequency for 16th notes
    // 16th notes per minute = BPM * 4
    // Frequency = (BPM * 4) / 60
    float sixteenthNotesPerSecond = (bpm * 4.0f) / 60.0f;
    clockMetro.SetFreq(sixteenthNotesPerSecond);

    // Update groove offsets when BPM changes
    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        voiceGroove[i].UpdateOffset(bpm, hw.AudioSampleRate());
    }
}

// ============================================================================
// GATE TRIGGERS
// ============================================================================

void TriggerGate24ppqn() { gate24ppqn = true; gate24ppqnCounter = 0; }
void TriggerGate16th() { gate16th = true; gate16thCounter = 0; }
void TriggerGate2() { gate2 = true; gate2Counter = 0; }
void TriggerGateQuarter() { gateQuarter = true; gateQuarterCounter = 0; }
void TriggerGateReset() { gateReset = true; gateResetCounter = 0; }

bool ShouldTriggerOut2(uint8_t step, uint8_t bar)
{
    uint16_t totalStep = (bar * 16) + step; // Total 16th notes since start

    switch(currentOut2Division)
    {
        case DIV_1_16: return true;                    // Every 16th note
        case DIV_1_8:  return (step % 2) == 0;         // Every 8th note
        case DIV_1_4:  return (step % 4) == 0;         // Every quarter note
        case DIV_1_2:  return (step % 8) == 0;         // Every half note
        case DIV_1:    return (step == 0);             // Every bar (16 steps)
        case DIV_2:    return (totalStep % 32) == 0;   // Every 2 bars
        case DIV_4:    return (totalStep % 64) == 0;   // Every 4 bars
        default: return false;
    }
}

bool ShouldTriggerOut3(uint8_t step, uint8_t bar)
{
    uint16_t totalStep = (bar * 16) + step; // Total 16th notes since start

    switch(currentOut3Division)
    {
        case DIV_1_16: return true;                    // Every 16th note
        case DIV_1_8:  return (step % 2) == 0;         // Every 8th note
        case DIV_1_4:  return (step % 4) == 0;         // Every quarter note
        case DIV_1_2:  return (step % 8) == 0;         // Every half note
        case DIV_1:    return step == 0;               // Every bar
        case DIV_2:    return (totalStep % 32) == 0;   // Every 2 bars
        case DIV_4:    return (totalStep % 64) == 0;   // Every 4 bars
        default:       return (step % 4) == 0;         // Default to quarter
    }
}

// ============================================================================
// MIDI TRANSPORT
// ============================================================================

void SendMidiClock()
{
    uint8_t clockMsg[1] = {0xF8}; // MIDI Clock message
    hw.midi.SendMessage(clockMsg, 1);
}

void SendMidiStart()
{
    uint8_t startMsg[1] = {0xFA}; // MIDI Start message
    hw.midi.SendMessage(startMsg, 1);
    midiClockCounter = 0;
}

void SendMidiStop()
{
    uint8_t stopMsg[1] = {0xFC}; // MIDI Stop message
    hw.midi.SendMessage(stopMsg, 1);
}

void SendMidiContinue()
{
    uint8_t continueMsg[1] = {0xFB}; // MIDI Continue message
    hw.midi.SendMessage(continueMsg, 1);
}

void ToggleRunState()
{
    if(!externalClockMode)
    {
        isRunning = !isRunning;
        if(isRunning)
        {
            SendMidiStart();
            clockMetro.Reset();
            TriggerGateReset(); // Trigger reset pulse on start
            currentStep = 0;    // Reset pattern position
            barCounter = 0;     // Reset bar counter
            cycleCounter = 0;   // Reset cycle counter
            midiClockCounter = 0; // Reset MIDI clock counter
            generationSeed = System::GetUs(); // Initialize seed
            RandomizeVoicePersonalities(); // Randomize voice personalities at start
            GenerateVoicePatterns(); // Generate initial patterns
            RandomizeMelodyPersonality(); // Randomize melody personality at start

            // Initialize groove configurations
            for(int i = 0; i < NUM_DRUM_VOICES; i++)
            {
                voiceGroove[i].Init();
                voiceGroove[i].UpdateOffset(bpm, hw.AudioSampleRate());
            }

            // Randomize groove at start
            RandomizeGroove();

            // Initialize trigger queue
            InitTriggerQueue();
            globalSampleCounter = 0;
            lastBeatSample = 0;

            // Trigger first step immediately to avoid missing first downbeat
            trigger16thNote = true;
        }
        else
        {
            SendMelodyNoteOff();  // Send note-off before stopping
            SendMidiStop();
        }
    }
}

void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case SystemRealTime:
        {
            SystemRealTimeType rt = m.srt_type;
            switch(rt)
            {
                case TimingClock:
                    // Receiving external MIDI clock
                    externalClockMode = true;
                    lastMidiClockTime = System::GetNow();

                    if(isRunning)
                    {
                        midiClockCounter++;

                        // Send our own MIDI clock through
                        SendMidiClock();

                        // Trigger 24 PPQN gate on every clock
                        TriggerGate24ppqn();

                        // Trigger 16th note gate on every 6th clock
                        if(midiClockCounter % CLOCKS_PER_16TH == 0)
                        {
                            gateHigh = true;
                            gateHighCounter = 0;
                            TriggerGate16th();

                            // Signal main loop to process patterns (same as internal clock)
                            trigger16thNote = true;
                            lastBeatSample = globalSampleCounter; // Record beat time for look-ahead

                            // OUT3 gate will be triggered in ProcessClock() based on division setting
                        }
                    }
                    break;

                case Start:
                    // External start
                    externalClockMode = true;
                    isRunning = true;
                    midiClockCounter = 0;
                    lastMidiClockTime = System::GetNow();
                    SendMidiStart();
                    TriggerGateReset(); // Trigger reset pulse on external start
                    currentStep = 0;    // Reset pattern position
                    barCounter = 0;     // Reset bar counter
                    cycleCounter = 0;   // Reset cycle counter
                    break;

                case Stop:
                    // External stop
                    externalClockMode = true;
                    isRunning = false;
                    lastMidiClockTime = System::GetNow();
                    SendMelodyNoteOff();  // Send note-off before stopping
                    SendMidiStop();
                    gateHigh = false;
                    break;

                case Continue:
                    // External continue
                    externalClockMode = true;
                    isRunning = true;
                    lastMidiClockTime = System::GetNow();
                    SendMidiContinue();
                    break;

                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// CONTROLS
// ============================================================================

void ProcessControls()
{
    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();

    int32_t inc = hw.encoder.Increment();
    bool buttonPressed = hw.encoder.RisingEdge();
    uint32_t now = System::GetNow();

    // Handle encoder based on display state
    switch(currentDisplayState)
    {
        case DISPLAY_DEFAULT:
            // In default mode, encoder rotation enters config menu
            if(inc != 0)
            {
                currentDisplayState = DISPLAY_CONFIG_MENU;
                currentConfigOption = CONFIG_BPM;
                lastEncoderActivity = now;
            }
            // Encoder button toggles run/stop
            else if(buttonPressed)
            {
                ToggleRunState();
            }
            break;

        case DISPLAY_CONFIG_MENU:
            // In config menu, encoder rotates through options
            if(inc != 0)
            {
                int option = (int)currentConfigOption + inc;
                if(option < 0) option = 0;
                if(option >= NUM_CONFIG_OPTIONS) option = NUM_CONFIG_OPTIONS - 1;
                currentConfigOption = (ConfigOption)option;
                lastEncoderActivity = now;
            }
            // Encoder button selects option
            else if(buttonPressed)
            {
                if(currentConfigOption == CONFIG_BACK)
                {
                    // Back to default display
                    currentDisplayState = DISPLAY_DEFAULT;
                }
                else if(currentConfigOption == CONFIG_PATTERN_INFO)
                {
                    // Enter pattern info display
                    currentDisplayState = DISPLAY_PATTERN_INFO;
                    patternInfoScroll = 0;
                }
                else if(currentConfigOption == CONFIG_FREEZE)
                {
                    // Toggle freeze directly (binary option)
                    freezeEnabled = !freezeEnabled;
                    SaveSettings();
                }
                else if(currentConfigOption == CONFIG_MELODY_FREEZE)
                {
                    // Toggle melody freeze directly (binary option)
                    melodyFreezeEnabled = !melodyFreezeEnabled;
                    SaveSettings();
                }
                else if(currentConfigOption == CONFIG_TUNE_MODE)
                {
                    // Toggle tune mode directly (binary option)
                    tuneModeEnabled = !tuneModeEnabled;
                    // Note: tune mode is not saved to flash - it's a temporary mode
                }
                else if(currentConfigOption == CONFIG_RANDOMIZE_ALL)
                {
                    // Randomize all parameters immediately
                    RandomizeAllParameters();
                }
                else
                {
                    // Enter edit mode for selected config
                    currentDisplayState = DISPLAY_CONFIG_EDIT;
                }
                lastEncoderActivity = now;
            }
            // Timeout check (only in config menu, not in edit mode)
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_CONFIG_EDIT:
            // In edit mode, encoder changes value
            if(inc != 0)
            {
                switch(currentConfigOption)
                {
                    case CONFIG_BPM:
                        if(!externalClockMode)
                        {
                            bpm += inc * 0.5f;
                            bpm = fclamp(bpm, 20.0f, 300.0f);
                            UpdateClockFrequency();
                        }
                        break;

                    case CONFIG_OUT2_DIVISION:
                        {
                            int div = (int)currentOut2Division + inc;
                            if(div < 0) div = 0;
                            if(div >= NUM_OUT_DIVISIONS) div = NUM_OUT_DIVISIONS - 1;
                            currentOut2Division = (OutDivision)div;
                        }
                        break;

                    case CONFIG_OUT3_DIVISION:
                        {
                            int div = (int)currentOut3Division + inc;
                            if(div < 0) div = 0;
                            if(div >= NUM_OUT_DIVISIONS) div = NUM_OUT_DIVISIONS - 1;
                            currentOut3Division = (OutDivision)div;
                        }
                        break;

                    // CONFIG_FREEZE is handled directly in menu, not in edit mode

                    case CONFIG_MELODY_SCALE:
                        {
                            int scale = (int)melodyScale + inc;
                            if(scale < 0) scale = 0;
                            if(scale >= NUM_SCALE_TYPES) scale = NUM_SCALE_TYPES - 1;
                            melodyScale = (ScaleType)scale;
                            // Regenerate both melody voices with new scale
                            GenerateMelodyPatternFor(&melodyVoice);
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_MELODY_ROOT:
                        {
                            int root = (int)melodyRoot + inc;
                            if(root < 0) root = 0;
                            if(root >= 12) root = 11;
                            melodyRoot = (uint8_t)root;
                            // Regenerate both melody voices with new root
                            GenerateMelodyPatternFor(&melodyVoice);
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_CV_STYLE:
                        {
                            int style = (int)melodyVoice.style + inc;
                            if(style < 0) style = 0;
                            if(style >= NUM_MELODY_STYLES) style = NUM_MELODY_STYLES - 1;
                            melodyVoice.style = (MelodyStyle)style;
                            // Also randomize sub-style when changing style
                            if(melodyVoice.style == MELODY_SUPPORTING)
                                melodyVoice.subStyle = System::GetUs() % NUM_SUPPORTING_SUBSTYLES;
                            else
                                melodyVoice.subStyle = System::GetUs() % NUM_ARP_SUBSTYLES;
                            // Regenerate CV melody with new style
                            GenerateMelodyPatternFor(&melodyVoice);
                        }
                        break;

                    case CONFIG_MIDI_STYLE:
                        {
                            int style = (int)melodyMidiVoice.style + inc;
                            if(style < 0) style = 0;
                            if(style >= NUM_MELODY_STYLES) style = NUM_MELODY_STYLES - 1;
                            melodyMidiVoice.style = (MelodyStyle)style;
                            // Also randomize sub-style when changing style
                            if(melodyMidiVoice.style == MELODY_SUPPORTING)
                                melodyMidiVoice.subStyle = System::GetUs() % NUM_SUPPORTING_SUBSTYLES;
                            else
                                melodyMidiVoice.subStyle = System::GetUs() % NUM_ARP_SUBSTYLES;
                            // Regenerate MIDI melody with new style
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_MIDI_MEL_CH:
                        {
                            int ch = (int)melodyMidiChannel + inc;
                            if(ch < 0) ch = 0;
                            if(ch >= 16) ch = 15;
                            melodyMidiChannel = (uint8_t)ch;
                        }
                        break;

                    // CONFIG_MELODY_FREEZE is handled directly in menu, not in edit mode

                    default:
                        break;
                }
            }
            // Encoder button confirms and returns to config menu
            else if(buttonPressed)
            {
                // Save settings to persistent storage
                SaveSettings();

                currentDisplayState = DISPLAY_CONFIG_MENU;
                lastEncoderActivity = now;
            }
            break;

        case DISPLAY_PATTERN_INFO:
            // In pattern info mode, encoder scrolls
            if(inc != 0)
            {
                patternInfoScroll += inc;
                if(patternInfoScroll < 0) patternInfoScroll = 0;
                int maxScroll = 3; // 7 total lines - 4 visible = 3 max scroll
                if(patternInfoScroll > maxScroll) patternInfoScroll = maxScroll;
            }
            // Encoder button returns to config menu
            else if(buttonPressed)
            {
                currentDisplayState = DISPLAY_CONFIG_MENU;
            }
            break;
    }

    // Check for external clock timeout
    if(externalClockMode)
    {
        if(now - lastMidiClockTime > midiClockTimeout)
        {
            // No external clock received recently, switch to internal
            externalClockMode = false;
        }
    }
}

void ProcessClock()
{
    // Check if we got a 16th note trigger from audio callback
    if(trigger16thNote)
    {
        trigger16thNote = false; // Clear flag

        // Trigger gate output on gate out port
        gateHigh = true;
        gateHighCounter = 0;

        // Trigger 16th note gate
        TriggerGate16th();

        // Trigger OUT2 and OUT3 gates based on division settings
        // Calculate actual bar number (0-7) and step within bar (0-15)
        uint8_t actualBar = (barCounter * 2) + (currentStep / 16);
        uint8_t stepInBar = currentStep % 16;
        if(ShouldTriggerOut2(stepInBar, actualBar))
        {
            TriggerGate2();
        }
        if(ShouldTriggerOut3(stepInBar, actualBar))
        {
            TriggerGateQuarter();
        }

        // Process drum patterns on each 16th note
        ProcessDrumPatterns();

        // Send MIDI clock (24 PPQN)
        // For each 16th note, send 6 MIDI clocks and trigger 24ppqn gate
        for(int i = 0; i < CLOCKS_PER_16TH; i++)
        {
            SendMidiClock();
            TriggerGate24ppqn();
            midiClockCounter++;
        }
    }

    // Handle gate pulse timing for 16th note gate (on Audio Out 1)
    if(gateHigh)
    {
        gateHighCounter++;
        // Lower gate after GATE_PULSE_MS milliseconds
        // Assuming we're called at ~1kHz, this gives us ~10ms pulses
        if(gateHighCounter > GATE_PULSE_MS)
        {
            gateHigh = false;
        }
    }

    // Set gate output to Analog voice gate
    // Note: analogGateHigh timing is managed in AudioCallback at 48kHz
    dsy_gpio_write(&hw.gate_output, analogGateHigh ? 1 : 0);
}
