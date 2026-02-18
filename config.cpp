/**
 * @file config.cpp
 * @brief Configuration and control implementation
 */

#include "config.h"
#include "groove.h"
#include "drums.h"
#include "melody.h"
#include "core/themis_chords.h"

// Timing flag - set in audio callback, processed in main loop
volatile bool trigger16thNote = false;

// ============================================================================
// PERSISTENT STORAGE
// ============================================================================

void SaveSettings()
{
    settings.magic = SETTINGS_MAGIC;
    settings.bpm = bpm;
    settings.freezeEnabled = freezeEnabled ? 1 : 0;
    settings.melodyScale = (uint8_t)melodyScale;
    settings.melodyRoot = melodyRoot;
    settings.cvMelodyStyle = (uint8_t)melodyMidiVoice.style;  // Both voices share style
    settings.midiMelodyStyle = (uint8_t)melodyMidiVoice.style;
    settings.melodyChannelStore = melodyChannel;
    settings.melodyFreezeEnabled = melodyFreezeEnabled ? 1 : 0;

    // Chord voice settings
    settings.chordActive = chordVoice.active ? 1 : 0;
    settings.chordProgression = chordVoice.progressionIndex;
    settings.chordRateIdx = chordVoice.chordRate;
    settings.chordOctave = chordVoice.octaveOffset;
    settings.chordMidiChannel = chordVoice.midiChannel;

    // MIDI channel settings
    settings.drumMidiChannel = drumMidiChannel;
    settings.bassMidiChannel = bassMidiChannel;
    settings.rhythmMidiChannel = rhythmMidiChannel;

    // Freeze settings
    settings.bassFreezeEnabled = bassVoiceConfig.freezePattern ? 1 : 0;
    settings.rhythmFreezeEnabled = rhythmPlayerConfig.freezeStyle ? 1 : 0;
    settings.chordFreezeEnabled = chordRandomizerConfig.freezeEnabled ? 1 : 0;

    // Voice settings
    settings.bassOctave = (uint8_t)bassVoiceConfig.octaveOffset;
    settings.rhythmOctave = (uint8_t)rhythmPlayerConfig.octaveOffset;
    settings.rhythmMode = (uint8_t)rhythmPlayerConfig.mode;
    settings.voiceActiveBits = 0;
    // Melody Voice controls both CV and MIDI — save as both bits
    if(melodyMidiVoice.active) settings.voiceActiveBits |= 0x03;  // bits 0+1
    if(bassVoiceConfig.active) settings.voiceActiveBits |= 0x08;
    if(rhythmPlayerConfig.active) settings.voiceActiveBits |= 0x10;
    if(tr8VoiceConfig.active) settings.voiceActiveBits |= 0x20;

    // TR-8 settings
    settings.tr8MidiChannel = tr8MidiChannel;
    settings.tr8FreezeKit = tr8VoiceConfig.freezeKit ? 1 : 0;

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

        // Load melody style — both voices share the same style
        if(settings.midiMelodyStyle < NUM_MELODY_STYLES)
        {
            melodyVoice.style = (MelodyStyle)settings.midiMelodyStyle;
            melodyMidiVoice.style = (MelodyStyle)settings.midiMelodyStyle;
        }
        else
        {
            melodyVoice.style = MELODY_SUPPORTING;
            melodyMidiVoice.style = MELODY_SUPPORTING;
        }

        if(settings.melodyChannelStore < 16)
        {
            melodyChannel = settings.melodyChannelStore;
        }
        else
        {
            melodyChannel = 0; // Default to channel 1
        }

        melodyFreezeEnabled = (settings.melodyFreezeEnabled != 0);

        // Load chord voice settings
        chordVoice.active = (settings.chordActive != 0);

        if(settings.chordProgression < themis::NUM_PROGRESSIONS)
        {
            chordVoice.progressionIndex = settings.chordProgression;
        }
        else
        {
            chordVoice.progressionIndex = 0;
        }

        if(settings.chordRateIdx < NUM_CHORD_RATES)
        {
            chordVoice.chordRate = settings.chordRateIdx;
        }
        else
        {
            chordVoice.chordRate = CHORD_RATE_1_BAR;
        }

        // Clamp octave offset to valid range
        if(settings.chordOctave >= -2 && settings.chordOctave <= 2)
        {
            chordVoice.octaveOffset = settings.chordOctave;
        }
        else
        {
            chordVoice.octaveOffset = 0;
        }

        if(settings.chordMidiChannel < 16)
        {
            chordVoice.midiChannel = settings.chordMidiChannel;
        }
        else
        {
            chordVoice.midiChannel = 1;  // Default to channel 2
        }

        // Load MIDI channel settings (validate: > 15 means uninitialized flash)
        drumMidiChannel = (settings.drumMidiChannel <= 15) ? settings.drumMidiChannel : 9;
        bassMidiChannel = (settings.bassMidiChannel <= 15) ? settings.bassMidiChannel : 4;
        rhythmMidiChannel = (settings.rhythmMidiChannel <= 15) ? settings.rhythmMidiChannel : 3;

        // Load freeze settings
        bassVoiceConfig.freezePattern = (settings.bassFreezeEnabled == 1);
        rhythmPlayerConfig.freezeStyle = (settings.rhythmFreezeEnabled == 1);
        chordRandomizerConfig.freezeEnabled = (settings.chordFreezeEnabled == 1);

        // Load bass octave (stored as uint8_t, interpret as int8_t)
        int8_t bOct = (int8_t)settings.bassOctave;
        bassVoiceConfig.octaveOffset = (bOct >= -2 && bOct <= 2) ? bOct : -1;

        // Load rhythm octave
        int8_t rOct = (int8_t)settings.rhythmOctave;
        rhythmPlayerConfig.octaveOffset = (rOct >= -2 && rOct <= 2) ? rOct : 0;

        // Load rhythm mode
        rhythmPlayerConfig.mode = (settings.rhythmMode < 2) ?
            (themis::RhythmPlayerMode)settings.rhythmMode : themis::RHYTHM_MODE_MORPH;

        // Load voice active bits (0xFF = uninitialized flash)
        if(settings.voiceActiveBits != 0xFF)
        {
            // Melody Voice controls both CV and MIDI — either bit activates both
            bool melActive = (settings.voiceActiveBits & 0x03) != 0;
            melodyVoice.active = melActive;
            melodyMidiVoice.active = melActive;
            bassVoiceConfig.active = (settings.voiceActiveBits & 0x08) != 0;
            rhythmPlayerConfig.active = (settings.voiceActiveBits & 0x10) != 0;
            tr8VoiceConfig.active = (settings.voiceActiveBits & 0x20) != 0;
        }

        // Load TR-8 settings
        tr8MidiChannel = (settings.tr8MidiChannel <= 15) ? settings.tr8MidiChannel : 9;
        tr8VoiceConfig.midiChannel = tr8MidiChannel;
        tr8VoiceConfig.freezeKit = (settings.tr8FreezeKit == 1);
    }
    else
    {
        // No valid settings found, use defaults
        bpm = 120.0f;
        freezeEnabled = false;
        melodyScale = SCALE_MINOR;
        melodyRoot = 0;
        melodyVoice.style = MELODY_SUPPORTING;
        melodyMidiVoice.style = MELODY_SUPPORTING;
        melodyChannel = 0;
        melodyFreezeEnabled = false;

        // Chord voice defaults
        chordVoice.active = false;
        chordVoice.progressionIndex = 0;
        chordVoice.chordRate = CHORD_RATE_1_BAR;
        chordVoice.octaveOffset = 0;
        chordVoice.midiChannel = 1;  // Default to channel 2

        // MIDI channel defaults
        drumMidiChannel = 9;
        bassMidiChannel = 4;
        rhythmMidiChannel = 3;

        // Freeze defaults
        bassVoiceConfig.freezePattern = false;
        rhythmPlayerConfig.freezeStyle = false;
        chordRandomizerConfig.freezeEnabled = false;

        // TR-8 defaults
        tr8VoiceConfig.Init();
        tr8VoiceState.Init();
        tr8MidiChannel = 9;

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
void TriggerGateReset() { gateReset = true; gateResetCounter = 0; }
void TriggerMelodyGate() { melodyGate = true; melodyGateCounter = 0; }
void TriggerBassGate() { bassGate = true; bassGateCounter = 0; }
void TriggerAnalogDrumGate() { analogDrumGate = true; analogDrumGateCounter = 0; }

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

            // Initialize chord voice
            InitChordVoice();

            // Initialize chord randomizer (Minor vibe only on firmware)
            chordRandomizerConfig.enabledVibes = 0x01; // Only VIBE_MINOR
            for(int i = 0; i < themis::NUM_VIBE_TYPES; i++)
                chordRandomizerConfig.enabledProgressions[i] = 0xFFFFFFFF;
            chordRandomizerState.Init();
            if(chordVoice.active)
                RandomizeChordVoice();

            // Initialize TR-8 voice
            {
                bool savedTr8Active = tr8VoiceConfig.active;
                bool savedTr8Freeze = tr8VoiceConfig.freezeKit;
                tr8VoiceConfig.Init();
                tr8VoiceConfig.active = savedTr8Active;
                tr8VoiceConfig.freezeKit = savedTr8Freeze;
                tr8VoiceConfig.midiChannel = tr8MidiChannel;
            }
            tr8VoiceState.Init();

            // Initialize bass voice
            bassVoiceConfig.Init();
            bassVoiceState.Init();
            bassNotePlaying = false;
            lastBassMidiNote = -1;

            // Initialize rhythm player
            rhythmPlayerConfig.Init();
            rhythmPlayerState.Init();
            rhythmNumActiveNotes = 0;
            rhythmNotesPlaying = false;

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
            SendChordNoteOff();    // Send chord note-off before stopping
            // Send bass note-off before stopping
            if(bassNotePlaying)
            {
                uint8_t noteOff[3] = {
                    static_cast<uint8_t>(0x80 | bassMidiChannel),
                    static_cast<uint8_t>(lastBassMidiNote),
                    0
                };
                hw.midi.SendMessage(noteOff, 3);
                bassNotePlaying = false;
            }
            // Send rhythm note-off before stopping
            if(rhythmNotesPlaying && rhythmNumActiveNotes > 0)
            {
                for(uint8_t n = 0; n < rhythmNumActiveNotes; n++)
                {
                    if(rhythmActiveNotes[n] >= 0)
                    {
                        uint8_t noteOff[3] = {
                            static_cast<uint8_t>(0x80 | rhythmMidiChannel),
                            static_cast<uint8_t>(rhythmActiveNotes[n]),
                            0
                        };
                        hw.midi.SendMessage(noteOff, 3);
                    }
                }
                rhythmNumActiveNotes = 0;
                rhythmNotesPlaying = false;
            }
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
                    SendChordNoteOff();    // Send chord note-off before stopping
                    // Send bass note-off before stopping
                    if(bassNotePlaying)
                    {
                        uint8_t noteOff[3] = {
                            static_cast<uint8_t>(0x80 | bassMidiChannel),
                            static_cast<uint8_t>(lastBassMidiNote),
                            0
                        };
                        hw.midi.SendMessage(noteOff, 3);
                        bassNotePlaying = false;
                    }
                    // Send rhythm note-off before stopping
                    if(rhythmNotesPlaying && rhythmNumActiveNotes > 0)
                    {
                        for(uint8_t n = 0; n < rhythmNumActiveNotes; n++)
                        {
                            if(rhythmActiveNotes[n] >= 0)
                            {
                                uint8_t noteOff[3] = {
                                    static_cast<uint8_t>(0x80 | rhythmMidiChannel),
                                    static_cast<uint8_t>(rhythmActiveNotes[n]),
                                    0
                                };
                                hw.midi.SendMessage(noteOff, 3);
                            }
                        }
                        rhythmNumActiveNotes = 0;
                        rhythmNotesPlaying = false;
                    }
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
            if(inc != 0)
            {
                int option = (int)currentConfigOption + inc;
                if(option < 0) option = 0;
                if(option >= NUM_CONFIG_OPTIONS) option = NUM_CONFIG_OPTIONS - 1;
                currentConfigOption = (ConfigOption)option;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                if(currentConfigOption == CONFIG_BACK)
                {
                    currentDisplayState = DISPLAY_DEFAULT;
                }
                else if(currentConfigOption == CONFIG_PATTERN_INFO)
                {
                    currentDisplayState = DISPLAY_PATTERN_INFO;
                    patternInfoScroll = 0;
                }
                else if(currentConfigOption == CONFIG_FREEZE_MENU)
                {
                    currentDisplayState = DISPLAY_FREEZE_MENU;
                    currentFreezeOption = FREEZE_ALL;
                    freezeScrollOffset = 0;
                }
                else if(currentConfigOption == CONFIG_SYSTEM_MENU)
                {
                    currentDisplayState = DISPLAY_SYSTEM_MENU;
                    currentSystemOption = SYSTEM_MELODY_CH;
                    systemScrollOffset = 0;
                }
                else if(currentConfigOption == CONFIG_HARMONY_MENU)
                {
                    currentDisplayState = DISPLAY_HARMONY_MENU;
                    currentHarmonyOption = HARMONY_SCALE;
                    harmonyScrollOffset = 0;
                }
                else if(currentConfigOption == CONFIG_VOICES_MENU)
                {
                    currentDisplayState = DISPLAY_VOICES_MENU;
                    currentVoiceMenuItem = VOICE_MELODY;
                    voiceScrollOffset = 0;
                }
                else if(currentConfigOption == CONFIG_TUNE_MODE)
                {
                    tuneModeEnabled = !tuneModeEnabled;
                }
                else if(currentConfigOption == CONFIG_RANDOMIZE_ALL)
                {
                    RandomizeAllParameters();
                }
                else
                {
                    currentDisplayState = DISPLAY_CONFIG_EDIT;
                }
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_CONFIG_EDIT:
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
                    default:
                        break;
                }
            }
            else if(buttonPressed)
            {
                SaveSettings();
                currentDisplayState = DISPLAY_CONFIG_MENU;
                lastEncoderActivity = now;
            }
            break;

        case DISPLAY_FREEZE_MENU:
            if(inc != 0)
            {
                int opt = (int)currentFreezeOption + inc;
                if(opt < 0) opt = 0;
                if(opt >= NUM_FREEZE_OPTIONS) opt = NUM_FREEZE_OPTIONS - 1;
                currentFreezeOption = (FreezeOption)opt;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                switch(currentFreezeOption)
                {
                    case FREEZE_ALL:
                    {
                        // If any freeze is off, turn all on; if all on, turn all off
                        bool allOn = freezeEnabled && melodyFreezeEnabled
                            && bassVoiceConfig.freezePattern && rhythmPlayerConfig.freezeStyle
                            && chordRandomizerConfig.freezeEnabled && tr8VoiceConfig.freezeKit;
                        bool newState = !allOn;
                        freezeEnabled = newState;
                        melodyFreezeEnabled = newState;
                        bassVoiceConfig.freezePattern = newState;
                        rhythmPlayerConfig.freezeStyle = newState;
                        chordRandomizerConfig.freezeEnabled = newState;
                        tr8VoiceConfig.freezeKit = newState;
                        break;
                    }
                    case FREEZE_DRUMS:
                        freezeEnabled = !freezeEnabled;
                        break;
                    case FREEZE_MELODY:
                        melodyFreezeEnabled = !melodyFreezeEnabled;
                        break;
                    case FREEZE_BASS:
                        bassVoiceConfig.freezePattern = !bassVoiceConfig.freezePattern;
                        break;
                    case FREEZE_RHYTHM:
                        rhythmPlayerConfig.freezeStyle = !rhythmPlayerConfig.freezeStyle;
                        break;
                    case FREEZE_CHORDS:
                        chordRandomizerConfig.freezeEnabled = !chordRandomizerConfig.freezeEnabled;
                        break;
                    case FREEZE_TR8:
                        tr8VoiceConfig.freezeKit = !tr8VoiceConfig.freezeKit;
                        break;
                    case FREEZE_BACK:
                        currentDisplayState = DISPLAY_CONFIG_MENU;
                        break;
                    default:
                        break;
                }
                SaveSettings();
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_SYSTEM_MENU:
            if(inc != 0)
            {
                int opt = (int)currentSystemOption + inc;
                if(opt < 0) opt = 0;
                if(opt >= NUM_SYSTEM_OPTIONS) opt = NUM_SYSTEM_OPTIONS - 1;
                currentSystemOption = (SystemOption)opt;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                if(currentSystemOption == SYSTEM_BACK)
                {
                    currentDisplayState = DISPLAY_CONFIG_MENU;
                }
                else
                {
                    currentDisplayState = DISPLAY_SYSTEM_EDIT;
                }
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_SYSTEM_EDIT:
            if(inc != 0)
            {
                int ch;
                switch(currentSystemOption)
                {
                    case SYSTEM_MELODY_CH:
                        ch = (int)melodyChannel + inc;
                        if(ch < 0) ch = 0;
                        if(ch > 15) ch = 15;
                        melodyChannel = (uint8_t)ch;
                        break;
                    case SYSTEM_DRUM_MIDI_CH:
                        ch = (int)drumMidiChannel + inc;
                        if(ch < 0) ch = 0;
                        if(ch > 15) ch = 15;
                        drumMidiChannel = (uint8_t)ch;
                        break;
                    case SYSTEM_BASS_MIDI_CH:
                        ch = (int)bassMidiChannel + inc;
                        if(ch < 0) ch = 0;
                        if(ch > 15) ch = 15;
                        bassMidiChannel = (uint8_t)ch;
                        break;
                    case SYSTEM_RHYTHM_MIDI_CH:
                        ch = (int)rhythmMidiChannel + inc;
                        if(ch < 0) ch = 0;
                        if(ch > 15) ch = 15;
                        rhythmMidiChannel = (uint8_t)ch;
                        break;
                    case SYSTEM_TR8_MIDI_CH:
                        ch = (int)tr8MidiChannel + inc;
                        if(ch < 0) ch = 0;
                        if(ch > 15) ch = 15;
                        tr8MidiChannel = (uint8_t)ch;
                        tr8VoiceConfig.midiChannel = tr8MidiChannel;
                        break;
                    default:
                        break;
                }
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                SaveSettings();
                currentDisplayState = DISPLAY_SYSTEM_MENU;
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_PATTERN_INFO:
            if(inc != 0)
            {
                patternInfoScroll += inc;
                if(patternInfoScroll < 0) patternInfoScroll = 0;
                int maxScroll = 3;
                if(patternInfoScroll > maxScroll) patternInfoScroll = maxScroll;
            }
            else if(buttonPressed)
            {
                currentDisplayState = DISPLAY_CONFIG_MENU;
            }
            break;

        case DISPLAY_HARMONY_MENU:
            if(inc != 0)
            {
                int opt = (int)currentHarmonyOption + inc;
                if(opt < 0) opt = 0;
                if(opt >= NUM_HARMONY_OPTIONS) opt = NUM_HARMONY_OPTIONS - 1;
                currentHarmonyOption = (HarmonyOption)opt;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                if(currentHarmonyOption == HARMONY_BACK)
                {
                    currentDisplayState = DISPLAY_CONFIG_MENU;
                }
                else
                {
                    currentDisplayState = DISPLAY_HARMONY_EDIT;
                }
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_HARMONY_EDIT:
            if(inc != 0)
            {
                switch(currentHarmonyOption)
                {
                    case HARMONY_SCALE:
                    {
                        int scale = (int)melodyScale + inc;
                        if(scale < 0) scale = 0;
                        if(scale >= NUM_SCALE_TYPES) scale = NUM_SCALE_TYPES - 1;
                        melodyScale = (ScaleType)scale;
                        GenerateMelodyPatternFor(&melodyVoice);
                        GenerateMelodyPatternFor(&melodyMidiVoice);
                        break;
                    }
                    case HARMONY_ROOT:
                    {
                        int root = (int)melodyRoot + inc;
                        if(root < 0) root = 0;
                        if(root >= 12) root = 11;
                        melodyRoot = (uint8_t)root;
                        GenerateMelodyPatternFor(&melodyVoice);
                        GenerateMelodyPatternFor(&melodyMidiVoice);
                        break;
                    }
                    case HARMONY_PROGRESSION:
                    {
                        SendChordNoteOff();
                        int prog = (int)chordVoice.progressionIndex + inc;
                        if(prog < 0) prog = 0;
                        if(prog >= themis::NUM_PROGRESSIONS) prog = themis::NUM_PROGRESSIONS - 1;
                        chordVoice.progressionIndex = (uint8_t)prog;
                        chordState.currentChordIndex = 0;
                        break;
                    }
                    case HARMONY_RATE:
                    {
                        int rate = (int)chordVoice.chordRate + inc;
                        if(rate < 0) rate = 0;
                        if(rate >= NUM_CHORD_RATES) rate = NUM_CHORD_RATES - 1;
                        chordVoice.chordRate = (uint8_t)rate;
                        break;
                    }
                    default:
                        break;
                }
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                SaveSettings();
                currentDisplayState = DISPLAY_HARMONY_MENU;
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_VOICES_MENU:
            if(inc != 0)
            {
                int opt = (int)currentVoiceMenuItem + inc;
                if(opt < 0) opt = 0;
                if(opt >= NUM_VOICE_MENU_ITEMS) opt = NUM_VOICE_MENU_ITEMS - 1;
                currentVoiceMenuItem = (VoiceMenuItem)opt;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                if(currentVoiceMenuItem == VOICE_BACK)
                {
                    currentDisplayState = DISPLAY_CONFIG_MENU;
                }
                else
                {
                    currentDisplayState = DISPLAY_VOICE_DETAIL;
                    currentVoiceDetail = VDETAIL_ACTIVE;
                    voiceDetailScrollOffset = 0;
                }
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_VOICE_DETAIL:
        {
            uint8_t detailCount;
            if(currentVoiceMenuItem == VOICE_RHYTHM) detailCount = 4;
            else if(currentVoiceMenuItem == VOICE_TR8) detailCount = 2;  // Active, Back
            else detailCount = 3;

            if(inc != 0)
            {
                int opt = (int)currentVoiceDetail + inc;
                if(opt < 0) opt = 0;
                if(opt >= detailCount) opt = detailCount - 1;
                currentVoiceDetail = (VoiceDetailItem)opt;
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                bool isBack = ((int)currentVoiceDetail == detailCount - 1);

                if(isBack)
                {
                    currentDisplayState = DISPLAY_VOICES_MENU;
                }
                else if(currentVoiceDetail == VDETAIL_ACTIVE)
                {
                    if(currentVoiceMenuItem == VOICE_MELODY)
                    {
                        // Toggle both CV and MIDI melody together
                        bool newState = !melodyMidiVoice.active;
                        melodyVoice.active = newState;
                        melodyMidiVoice.active = newState;
                    }
                    else if(currentVoiceMenuItem == VOICE_BASS)
                    {
                        bassVoiceConfig.active = !bassVoiceConfig.active;
                        if(!bassVoiceConfig.active && bassNotePlaying)
                        {
                            uint8_t noteOff[3] = {
                                static_cast<uint8_t>(0x80 | bassMidiChannel),
                                static_cast<uint8_t>(lastBassMidiNote),
                                0
                            };
                            hw.midi.SendMessage(noteOff, 3);
                            bassNotePlaying = false;
                        }
                    }
                    else if(currentVoiceMenuItem == VOICE_TR8)
                    {
                        tr8VoiceConfig.active = !tr8VoiceConfig.active;
                    }
                    else if(currentVoiceMenuItem == VOICE_RHYTHM)
                    {
                        rhythmPlayerConfig.active = !rhythmPlayerConfig.active;
                        if(!rhythmPlayerConfig.active && rhythmNotesPlaying && rhythmNumActiveNotes > 0)
                        {
                            for(uint8_t n = 0; n < rhythmNumActiveNotes; n++)
                            {
                                if(rhythmActiveNotes[n] >= 0)
                                {
                                    uint8_t noteOff[3] = {
                                        static_cast<uint8_t>(0x80 | rhythmMidiChannel),
                                        static_cast<uint8_t>(rhythmActiveNotes[n]),
                                        0
                                    };
                                    hw.midi.SendMessage(noteOff, 3);
                                }
                            }
                            rhythmNumActiveNotes = 0;
                            rhythmNotesPlaying = false;
                        }
                    }
                    SaveSettings();
                }
                else if((int)currentVoiceDetail == 1 && currentVoiceMenuItem == VOICE_MELODY)
                {
                    // Toggle melody style — applies to both CV and MIDI voice
                    MelodyStyle newStyle = (melodyMidiVoice.style == MELODY_SUPPORTING) ?
                        MELODY_ARPEGGIATOR : MELODY_SUPPORTING;
                    uint8_t newSubStyle;
                    if(newStyle == MELODY_SUPPORTING)
                        newSubStyle = System::GetUs() % NUM_SUPPORTING_SUBSTYLES;
                    else
                        newSubStyle = System::GetUs() % NUM_ARP_SUBSTYLES;

                    melodyVoice.style = newStyle;
                    melodyVoice.subStyle = newSubStyle;
                    melodyMidiVoice.style = newStyle;
                    melodyMidiVoice.subStyle = newSubStyle;
                    GenerateMelodyPatternFor(&melodyVoice);
                    GenerateMelodyPatternFor(&melodyMidiVoice);
                    SaveSettings();
                }
                else if((int)currentVoiceDetail == 1 && currentVoiceMenuItem == VOICE_RHYTHM)
                {
                    rhythmPlayerConfig.mode = (rhythmPlayerConfig.mode == themis::RHYTHM_MODE_MANUAL) ?
                        themis::RHYTHM_MODE_MORPH : themis::RHYTHM_MODE_MANUAL;
                    SaveSettings();
                }
                else
                {
                    // Octave edit: enter VOICE_EDIT
                    currentDisplayState = DISPLAY_VOICE_EDIT;
                }
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;
        }

        case DISPLAY_VOICE_EDIT:
            if(inc != 0)
            {
                int oct;
                if(currentVoiceMenuItem == VOICE_BASS)
                {
                    oct = (int)bassVoiceConfig.octaveOffset + inc;
                    if(oct < -2) oct = -2;
                    if(oct > 2) oct = 2;
                    bassVoiceConfig.octaveOffset = (int8_t)oct;
                }
                else if(currentVoiceMenuItem == VOICE_RHYTHM)
                {
                    oct = (int)rhythmPlayerConfig.octaveOffset + inc;
                    if(oct < -2) oct = -2;
                    if(oct > 2) oct = 2;
                    rhythmPlayerConfig.octaveOffset = (int8_t)oct;
                }
                lastEncoderActivity = now;
            }
            else if(buttonPressed)
            {
                SaveSettings();
                currentDisplayState = DISPLAY_VOICE_DETAIL;
                lastEncoderActivity = now;
            }
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
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

        // Trigger reset on step 0 of pattern
        if(currentStep == 0 && barCounter == 0)
        {
            TriggerGateReset();
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

    // Set GATE OUT pin to analog drum trigger
    // Note: analogDrumGate timing is managed in AudioCallback at 48kHz
    dsy_gpio_write(&hw.gate_output, analogDrumGate ? 1 : 0);
}
