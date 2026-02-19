/**
 * @file groove.cpp
 * @brief Groove timing and trigger queue implementation
 */

#include "groove.h"
#include "melody.h"
#include "config.h"
#include "core/themis_rhythm.h"
#include "core/themis_chords.h"
#include "core/themis_melody.h"

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

void InitTriggerQueue()
{
    for(int i = 0; i < TRIGGER_QUEUE_SIZE; i++)
    {
        triggerQueue[i].Init();
    }
    triggerQueueHead = 0;
    triggerQueueTail = 0;

    // Initialize melody queue
    for(int i = 0; i < MELODY_QUEUE_SIZE; i++)
    {
        melodyQueue[i].Init();
    }
    melodyQueueHead = 0;
    melodyQueueTail = 0;
}

// ============================================================================
// GROOVE CONFIGURATION
// ============================================================================

void RandomizeGroove()
{
    // Random groove pattern (0-31)
    uint32_t seed = System::GetUs();
    currentGroovePattern = seed % 32;

    // Random timing and velocity amounts for each voice
    // Kick drum: 0-35% (reduced to avoid too heavy bass timing variation)
    // Other voices: 0-75%
    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        seed = System::GetUs() ^ (i * 54321); // Unique seed per voice

        // Kick drum gets lower max groove to avoid excessive timing variation
        if(i == KICK)
        {
            grooveAmount[i] = (float)(seed % 21) / 100.0f; // 0-20% timing for kick
        }
        else
        {
            grooveAmount[i] = (float)(seed % 51) / 100.0f; // 0-50% timing for others
        }

        seed = System::GetUs() ^ (i * 98765); // Different seed for velocity
        grooveVelocityAmount[i] = (float)(seed % 101) / 100.0f; // 0-100% velocity
    }

    // Randomize melody groove amount (15-50%)
    uint32_t melSeed = System::GetUs();
    melodyGrooveAmount = 0.15f + (float)(melSeed % 36) / 100.0f;
}

// ============================================================================
// DRUM TRIGGER SCHEDULING
// ============================================================================

int32_t CalculateGrooveOffset(DrumVoice voice, uint8_t step)
{
    // Get step within 16-step pattern (handle 32-step patterns)
    uint8_t patternStep = step % 16;

    // Get base offset percentage from pattern
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];

    // Scale by voice amount
    float scaledOffsetPercent = (float)baseOffsetPercent * grooveAmount[voice];

    // Convert to samples based on current BPM
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    float offsetFloat = (scaledOffsetPercent / 100.0f) * samplesPerSixteenth;

    // No clamping - groove amount percentage is the only limit
    return (int32_t)offsetFloat;
}

uint8_t CalculateGrooveVelocity(DrumVoice voice, uint8_t baseVelocity, uint8_t step)
{
    // Get step within 16-step pattern
    uint8_t patternStep = step % 16;

    // Get velocity multiplier from pattern (100 = normal, 120 = +20%, 80 = -20%)
    int8_t velocityPercent = velocityPatterns[currentGroovePattern][patternStep];

    // Apply pattern and voice velocity groove amount (separate from timing amount)
    // Full groove amount = full velocity variation, zero groove = no velocity variation
    float velocityMultiplier = 100.0f + ((velocityPercent - 100.0f) * grooveVelocityAmount[voice]);

    // Calculate final velocity
    float finalVelocity = (float)baseVelocity * (velocityMultiplier / 100.0f);

    // Clamp to MIDI range 1-127
    if(finalVelocity < 1.0f) finalVelocity = 1.0f;
    if(finalVelocity > 127.0f) finalVelocity = 127.0f;

    return (uint8_t)finalVelocity;
}

void ScheduleDrumTrigger(DrumVoice voice, uint8_t velocity, uint64_t fireSample)
{
    // If fire time is in the past or very close (within 48 samples), fire immediately
    if(fireSample <= globalSampleCounter || fireSample <= globalSampleCounter + 48)
    {
        TriggerDrum(voice, velocity);
        return;
    }

    // Find next available queue slot
    uint8_t nextHead = (triggerQueueHead + 1) % TRIGGER_QUEUE_SIZE;

    // Check for queue overflow
    if(nextHead == triggerQueueTail)
    {
        // Queue full - fire immediately as fallback
        TriggerDrum(voice, velocity);
        return;
    }

    // Add to queue
    triggerQueue[triggerQueueHead].voice = voice;
    triggerQueue[triggerQueueHead].velocity = velocity;
    triggerQueue[triggerQueueHead].fireSample = fireSample;
    triggerQueue[triggerQueueHead].active = true;

    triggerQueueHead = nextHead;
}

void ScheduleDrumTriggerWithGroove(DrumVoice voice, uint8_t baseVelocity,
                                    uint8_t step, uint64_t nextBeatSample)
{
    uint8_t velocity = CalculateGrooveVelocity(voice, baseVelocity, step);
    int32_t offset = CalculateGrooveOffset(voice, step);
    uint64_t fireSample = nextBeatSample + offset;

    // If fire time is in the past or very close (within 48 samples), fire immediately
    if(fireSample <= globalSampleCounter || fireSample <= globalSampleCounter + 48)
    {
        TriggerDrum(voice, velocity);
        return;
    }

    // Find next available queue slot
    uint8_t nextHead = (triggerQueueHead + 1) % TRIGGER_QUEUE_SIZE;

    // Check for queue overflow
    if(nextHead == triggerQueueTail)
    {
        // Queue full - fire immediately as fallback
        TriggerDrum(voice, velocity);
        return;
    }

    // Add to queue
    triggerQueue[triggerQueueHead].voice = voice;
    triggerQueue[triggerQueueHead].velocity = velocity;
    triggerQueue[triggerQueueHead].fireSample = fireSample;
    triggerQueue[triggerQueueHead].active = true;

    triggerQueueHead = nextHead;
}

void ProcessTriggerQueue()
{
    // Check all queue entries (triggers may not be time-ordered)
    uint8_t checkCount = 0;
    uint8_t currentIndex = triggerQueueTail;

    while(currentIndex != triggerQueueHead && checkCount < TRIGGER_QUEUE_SIZE)
    {
        MidiTrigger* trigger = &triggerQueue[currentIndex];

        if(trigger->active && globalSampleCounter >= trigger->fireSample)
        {
            // Fire the trigger
            TriggerDrum(trigger->voice, trigger->velocity);

            // Mark as processed
            trigger->active = false;

            // If this is the tail entry, advance tail
            if(currentIndex == triggerQueueTail)
            {
                // Advance tail past all inactive entries
                while(triggerQueueTail != triggerQueueHead && !triggerQueue[triggerQueueTail].active)
                {
                    triggerQueueTail = (triggerQueueTail + 1) % TRIGGER_QUEUE_SIZE;
                }
            }
        }

        currentIndex = (currentIndex + 1) % TRIGGER_QUEUE_SIZE;
        checkCount++;
    }
}

// ============================================================================
// MELODY TRIGGER SCHEDULING
// ============================================================================

int32_t CalculateMelodyGrooveOffset(uint8_t step)
{
    // Get step within 16-step pattern
    uint8_t patternStep = step % 16;

    // Get base offset percentage from current groove pattern
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];

    // Scale by melody groove amount
    float scaledOffsetPercent = (float)baseOffsetPercent * melodyGrooveAmount;

    // Convert to samples based on current BPM
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    float offsetFloat = (scaledOffsetPercent / 100.0f) * samplesPerSixteenth;

    return (int32_t)offsetFloat;
}

void ScheduleMelodyTrigger(MelodyVoiceType voiceType, int8_t note,
                           uint8_t step, uint64_t nextBeatSample)
{
    int32_t offset = CalculateMelodyGrooveOffset(step);
    uint64_t fireSample = nextBeatSample + offset;

    // If fire time is in the past or very close, skip (note will be handled next step)
    if(fireSample <= globalSampleCounter + 48)
    {
        // Fire immediately - find queue slot
    }

    // Find next available queue slot
    uint8_t nextHead = (melodyQueueHead + 1) % MELODY_QUEUE_SIZE;

    // Check for queue overflow - skip if full
    if(nextHead == melodyQueueTail)
    {
        return; // Queue full, skip this trigger
    }

    // Add to queue
    melodyQueue[melodyQueueHead].voiceType = voiceType;
    melodyQueue[melodyQueueHead].note = note;
    melodyQueue[melodyQueueHead].fireSample = fireSample;
    melodyQueue[melodyQueueHead].active = true;

    melodyQueueHead = nextHead;
}

void ProcessMelodyQueue()
{
    // Check for automatic note-off (1/32 note duration)
    if(midiMelodyNoteOn && globalSampleCounter >= midiMelodyNoteOffSample)
    {
        uint8_t noteOff[3] = {
            static_cast<uint8_t>(0x80 | melodyChannel),
            lastMidiMelodyNote,
            0
        };
        hw.midi.SendMessage(noteOff, 3);
        midiMelodyNoteOn = false;
    }

    uint8_t checkCount = 0;
    uint8_t currentIndex = melodyQueueTail;

    while(currentIndex != melodyQueueHead && checkCount < MELODY_QUEUE_SIZE)
    {
        MelodyTrigger* trigger = &melodyQueue[currentIndex];

        if(trigger->active && globalSampleCounter >= trigger->fireSample)
        {
            if(trigger->voiceType == MELODY_CV)
            {
                // Trigger melody gate on OUT3
                TriggerMelodyGate();

                // Output melody CV voltage on DAC1
                float cvVoltage = MelodyNoteToCV(trigger->note);
                uint16_t cv1 = (uint16_t)(cvVoltage / 5.0f * 4095.0f);
                hw.seed.dac.WriteValue(DacHandle::Channel::ONE, cv1);
            }
            else // MELODY_MIDI
            {
                // Send note-off for previous note if still playing
                if(midiMelodyNoteOn)
                {
                    uint8_t noteOff[3] = {
                        static_cast<uint8_t>(0x80 | melodyChannel),
                        lastMidiMelodyNote,
                        0
                    };
                    hw.midi.SendMessage(noteOff, 3);
                }

                // Calculate MIDI note (C2 = 36, add semitones)
                uint8_t midiNote = 36 + trigger->note;
                if(midiNote > 127) midiNote = 127;

                // Send note-on
                uint8_t noteOn[3] = {
                    static_cast<uint8_t>(0x90 | melodyChannel),
                    midiNote,
                    100  // Fixed velocity for melody
                };
                hw.midi.SendMessage(noteOn, 3);

                lastMidiMelodyNote = midiNote;
                midiMelodyNoteOn = true;

                // Schedule note-off after 1/32 note duration
                // 1/32 note = samplesPerSixteenth / 2 = sampleRate * 7.5 / bpm
                float noteLengthSamples = hw.AudioSampleRate() * 7.5f / bpm;
                midiMelodyNoteOffSample = globalSampleCounter + (uint64_t)noteLengthSamples;
            }

            // Mark as processed
            trigger->active = false;

            // Advance tail if this was the tail entry
            if(currentIndex == melodyQueueTail)
            {
                while(melodyQueueTail != melodyQueueHead && !melodyQueue[melodyQueueTail].active)
                {
                    melodyQueueTail = (melodyQueueTail + 1) % MELODY_QUEUE_SIZE;
                }
            }
        }

        currentIndex = (currentIndex + 1) % MELODY_QUEUE_SIZE;
        checkCount++;
    }
}

// ============================================================================
// MIDI OUTPUT
// ============================================================================

void TriggerDrum(DrumVoice voice, uint8_t velocity)
{
    // ANALOG voice triggers GATE OUT pin instead of MIDI
    if(voice == ANALOG)
    {
        TriggerAnalogDrumGate();
        return;
    }

    uint8_t noteOn[3] = {
        static_cast<uint8_t>(0x90 | drumMidiChannel), // Note On + channel
        drumNotes[voice],                              // Note number
        velocity                                       // Velocity
    };
    hw.midi.SendMessage(noteOn, 3);

    // Note: We don't send Note Off for drums, they're self-decaying
}

void SendMelodyNoteOff()
{
    // Send note-off for MIDI melody if currently playing
    if(midiMelodyNoteOn)
    {
        uint8_t noteOff[3] = {
            static_cast<uint8_t>(0x80 | melodyChannel),
            lastMidiMelodyNote,
            0
        };
        hw.midi.SendMessage(noteOff, 3);
        midiMelodyNoteOn = false;
    }

    // Also clear the melody queue to prevent pending notes from firing
    for(int i = 0; i < MELODY_QUEUE_SIZE; i++)
    {
        melodyQueue[i].active = false;
    }
    melodyQueueHead = 0;
    melodyQueueTail = 0;
}

// ============================================================================
// CHORD VOICE MIDI OUTPUT
// ============================================================================

void InitChordVoice()
{
    chordState.Init();
    chordNotesOn = false;
    chordNumActiveNotes = 0;
    for(int i = 0; i < 6; i++)
    {
        chordActiveNotes[i] = -1;
    }
}

uint8_t GetChordNotes(int8_t rootNote, ChordType chordType, int8_t octaveOffset, int8_t* outNotes)
{
    if(chordType >= NUM_CHORD_TYPES)
        return 0;

    const ChordShape& shape = chordShapes[chordType];

    // Base MIDI note: C3 (48) + root + octave offset
    int8_t baseNote = 48 + rootNote + (octaveOffset * 12);

    uint8_t numNotes = 0;
    for(int i = 0; i < shape.numNotes && i < 6; i++)
    {
        int8_t note = baseNote + shape.intervals[i];
        // Clamp to valid MIDI range
        if(note >= 0 && note <= 127)
        {
            outNotes[numNotes++] = note;
        }
    }

    return numNotes;
}

void SendChordOn(const int8_t* notes, uint8_t numNotes, uint8_t velocity)
{
    // Store active notes for note-off later
    chordNumActiveNotes = numNotes;
    for(int i = 0; i < 6; i++)
    {
        if(i < numNotes)
            chordActiveNotes[i] = notes[i];
        else
            chordActiveNotes[i] = -1;
    }

    // Send note-on for each note in the chord
    for(uint8_t i = 0; i < numNotes; i++)
    {
        uint8_t noteOn[3] = {
            static_cast<uint8_t>(0x90 | chordVoice.midiChannel),
            static_cast<uint8_t>(notes[i]),
            velocity
        };
        hw.midi.SendMessage(noteOn, 3);
    }

    chordNotesOn = true;
}

void SendChordNoteOff()
{
    if(!chordNotesOn)
        return;

    // Send note-off for all active notes
    for(uint8_t i = 0; i < chordNumActiveNotes; i++)
    {
        if(chordActiveNotes[i] >= 0)
        {
            uint8_t noteOff[3] = {
                static_cast<uint8_t>(0x80 | chordVoice.midiChannel),
                static_cast<uint8_t>(chordActiveNotes[i]),
                0
            };
            hw.midi.SendMessage(noteOff, 3);
            chordActiveNotes[i] = -1;
        }
    }

    chordNotesOn = false;
    chordNumActiveNotes = 0;
}

// ============================================================================
// CHORD RANDOMIZATION (VIBE SYSTEM)
// ============================================================================

static themis::VibeType SelectRandomEnabledVibe(uint32_t seed)
{
    uint8_t enabledCount = 0;
    uint8_t enabledVibesArr[themis::NUM_VIBE_TYPES];

    for(int v = 0; v < themis::NUM_VIBE_TYPES; v++)
    {
        if(chordRandomizerConfig.enabledVibes & (1 << v))
        {
            enabledVibesArr[enabledCount++] = v;
        }
    }

    if(enabledCount == 0)
        return themis::VIBE_MINOR;

    return (themis::VibeType)enabledVibesArr[seed % enabledCount];
}

static void SelectProgressionFromVibe(themis::VibeType vibe, uint32_t seed)
{
    uint8_t indices[32];
    uint8_t count = themis::GetProgressionsForVibe(vibe, indices, 32);

    if(count == 0)
        return;

    // Filter by enabled progressions
    uint8_t enabledIndices[32];
    uint8_t enabledCount = 0;

    for(int i = 0; i < count; i++)
    {
        uint8_t progIdx = indices[i];
        uint8_t vibePos = 0;
        for(uint8_t j = 0; j < progIdx; j++)
        {
            if(themis::progressions[j].vibe == vibe)
                vibePos++;
        }
        if(chordRandomizerConfig.enabledProgressions[vibe] & (1 << vibePos))
        {
            enabledIndices[enabledCount++] = progIdx;
        }
    }

    if(enabledCount == 0)
    {
        chordVoice.progressionIndex = indices[0];
    }
    else
    {
        chordVoice.progressionIndex = enabledIndices[seed % enabledCount];
    }
}

static void ProcessChordRandomization()
{
    if(chordRandomizerConfig.freezeEnabled)
        return;

    uint32_t seed = System::GetUs();

    // Check if current vibe is still enabled
    bool currentVibeEnabled = chordRandomizerConfig.enabledVibes
                              & (1 << chordRandomizerState.currentVibe);
    if(!currentVibeEnabled && !chordRandomizerState.inTransition)
    {
        // Start transition to steady chord, then switch vibe
        chordVoice.progressionIndex = themis::GetSteadyChordForVibe(
            chordRandomizerState.currentVibe, seed);
        chordRandomizerState.inTransition = true;
        uint8_t barsPerCycle = themis::chordRateSteps[chordVoice.chordRate] / 32;
        if(barsPerCycle == 0) barsPerCycle = 1;
        chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
        if(chordRandomizerState.transitionBarsRemaining < 2)
            chordRandomizerState.transitionBarsRemaining = 2;
        chordRandomizerState.changeTimer = 0;
        return;
    }

    // Handle ongoing transition
    if(chordRandomizerState.inTransition)
    {
        chordRandomizerState.transitionBarsRemaining--;

        if(chordRandomizerState.transitionBarsRemaining <= 0)
        {
            chordRandomizerState.inTransition = false;

            bool currentVibeStillEnabled = chordRandomizerConfig.enabledVibes
                                           & (1 << chordRandomizerState.currentVibe);
            if(!currentVibeStillEnabled)
            {
                themis::VibeType newVibe = SelectRandomEnabledVibe(seed);
                int8_t rootShift = themis::CalculateVibeRootShift(
                    chordRandomizerState.currentVibe, newVibe, seed);
                melodyRoot = (melodyRoot + rootShift + 12) % 12;
                chordRandomizerState.currentVibe = newVibe;
            }

            SelectProgressionFromVibe(chordRandomizerState.currentVibe, seed);
            chordRandomizerState.changeTimer = 2 + (seed % 3);
        }
        return;
    }

    // Decrement change timer
    if(chordRandomizerState.changeTimer > 0)
    {
        chordRandomizerState.changeTimer--;
        return;
    }

    // Timer expired - consider changing
    uint8_t enabledVibeCount = 0;
    for(int v = 0; v < themis::NUM_VIBE_TYPES; v++)
    {
        if(chordRandomizerConfig.enabledVibes & (1 << v))
            enabledVibeCount++;
    }

    // Change vibe? (20% probability if multiple vibes enabled)
    bool changeVibe = (enabledVibeCount > 1) && ((seed % 100) < 20);

    if(changeVibe)
    {
        themis::VibeType newVibe = SelectRandomEnabledVibe(seed);
        if(newVibe != chordRandomizerState.currentVibe)
        {
            themis::VibeType oldVibe = chordRandomizerState.currentVibe;
            int8_t rootShift = themis::CalculateVibeRootShift(oldVibe, newVibe, seed);
            melodyRoot = (melodyRoot + rootShift + 12) % 12;
            chordRandomizerState.currentVibe = newVibe;

            // Select steady chord for transition
            chordVoice.progressionIndex = themis::GetSteadyChordForVibe(newVibe, seed);
            chordRandomizerState.inTransition = true;
            uint8_t barsPerCycle = themis::chordRateSteps[chordVoice.chordRate] / 32;
            if(barsPerCycle == 0) barsPerCycle = 1;
            chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
            if(chordRandomizerState.transitionBarsRemaining < 2)
                chordRandomizerState.transitionBarsRemaining = 2;
            chordRandomizerState.changeTimer = 0;
            return;
        }
    }

    // Stay in vibe - maybe change root? (10% chance)
    bool changeRoot = ((seed >> 4) % 100) < 10;
    if(changeRoot)
    {
        int8_t shift = ((seed >> 8) % 2) == 0 ? 7 : 1;
        melodyRoot = (melodyRoot + shift) % 12;

        chordVoice.progressionIndex = themis::GetSteadyChordForVibe(
            chordRandomizerState.currentVibe, seed);
        chordRandomizerState.inTransition = true;
        uint8_t barsPerCycle = themis::chordRateSteps[chordVoice.chordRate] / 32;
        if(barsPerCycle == 0) barsPerCycle = 1;
        chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
        if(chordRandomizerState.transitionBarsRemaining < 2)
            chordRandomizerState.transitionBarsRemaining = 2;
        chordRandomizerState.changeTimer = 0;
    }
    else
    {
        // Just pick new progression from same vibe
        SelectProgressionFromVibe(chordRandomizerState.currentVibe, seed);
        chordRandomizerState.changeTimer = 2 + ((seed >> 12) % 3);
    }
}

void RandomizeChordVoice()
{
    SendChordNoteOff();

    uint32_t seed = System::GetUs();

    // Select a random enabled vibe
    themis::VibeType vibe = SelectRandomEnabledVibe(seed);
    chordRandomizerState.currentVibe = vibe;

    // Select a progression from that vibe
    SelectProgressionFromVibe(vibe, seed);

    // Randomize chord rate (favor 1 bar and half bar)
    int rateRoll = (seed >> 8) % 100;
    if(rateRoll < 20)
        chordVoice.chordRate = CHORD_RATE_2_BARS;
    else if(rateRoll < 60)
        chordVoice.chordRate = CHORD_RATE_1_BAR;
    else if(rateRoll < 90)
        chordVoice.chordRate = CHORD_RATE_HALF_BAR;
    else
        chordVoice.chordRate = CHORD_RATE_QUARTER;

    // Randomize octave offset (-1 to +1)
    chordVoice.octaveOffset = ((seed >> 16) % 3) - 1;

    // Randomize velocity (70-110)
    chordVoice.velocity = 70 + ((seed >> 28) % 41);

    // Reset state
    chordState.Init();
    chordRandomizerState.inTransition = false;
    chordRandomizerState.transitionBarsRemaining = 0;
    chordRandomizerState.changeTimer = 2 + (seed % 3);
}

// ============================================================================
// CHORD VOICE STEP PROCESSING
// ============================================================================

void ProcessChordStep(uint8_t step)
{
    if(!chordVoice.active)
        return;

    // Get chord rate in steps
    uint8_t rateSteps = themis::chordRateSteps[chordVoice.chordRate];

    // Check if it's time to change chords
    bool shouldChange = false;

    if(step == 0)
    {
        shouldChange = true;
    }
    else if(rateSteps < 64)
    {
        shouldChange = (step % rateSteps) == 0;
    }

    if(shouldChange)
    {
        // Clamp progression index to valid range for core library progressions
        if(chordVoice.progressionIndex >= themis::NUM_PROGRESSIONS)
            chordVoice.progressionIndex = 0;

        // Get current progression from core library
        const themis::ChordProgression& prog =
            themis::progressions[chordVoice.progressionIndex];

        // Send note-off for previous chord
        SendChordNoteOff();

        // Get chord notes using core library (handles diatonic/chromatic, scale degrees)
        int8_t chordNotes[6];
        uint8_t numNotes = themis::GetChordNotes(
            &prog,
            chordState.currentChordIndex,
            melodyRoot,
            (uint8_t)melodyScale,
            chordVoice.octaveOffset,
            chordNotes
        );

        // Send note-on for new chord
        if(numNotes > 0)
        {
            SendChordOn(chordNotes, numNotes, chordVoice.velocity);
        }

        // Advance to next chord in progression
        chordState.currentChordIndex++;
        if(chordState.currentChordIndex >= prog.length)
        {
            chordState.currentChordIndex = 0;

            // Process chord randomization (vibe system)
            ProcessChordRandomization();

            // Notify rhythm player of chord cycle boundary
            if(rhythmPlayerConfig.active && rhythmPlayerConfig.mode == themis::RHYTHM_MODE_MORPH
               && !rhythmPlayerConfig.freezeStyle)
            {
                uint32_t seed = System::GetUs();

                // Style morph check
                if(rhythmPlayerState.morphTimer == 0 || rhythmPlayerState.morphTimer <= 16)
                {
                    themis::RhythmPlayStyle newStyle =
                        (themis::RhythmPlayStyle)(seed % themis::NUM_RHYTHM_PLAY_STYLES);
                    if(newStyle == rhythmPlayerState.currentStyle)
                        newStyle = (themis::RhythmPlayStyle)((newStyle + 1) % themis::NUM_RHYTHM_PLAY_STYLES);

                    rhythmPlayerState.targetStyle = newStyle;
                    rhythmPlayerState.styleMorphProgress = 0.0f;
                    rhythmPlayerState.morphTimer = 128 + ((seed >> 8) % 128);
                }

                // Parameter randomization check
                if(rhythmPlayerState.randomizeTimer == 0 || rhythmPlayerState.randomizeTimer <= 32)
                {
                    themis::RandomizeRhythmParams(rhythmPlayerConfig, rhythmPlayerState, seed ^ 0xFEDCBA98);
                }
            }

            // Randomize bass pattern at chord cycle boundary
            if(bassVoiceConfig.active && !bassVoiceConfig.freezePattern)
            {
                if(bassVoiceState.chordCyclesUntilChange == 0)
                {
                    uint32_t bassSeed = System::GetUs();
                    themis::RandomizeBassPattern(bassVoiceState, bassVoiceConfig, bassSeed);
                    bassVoiceState.chordCyclesUntilChange = 2 + (bassSeed % 3);
                }
                else
                {
                    bassVoiceState.chordCyclesUntilChange--;
                }
            }
        }
    }
}
