/**
 * @file groove.cpp
 * @brief Groove timing and trigger queue implementation
 */

#include "groove.h"
#include "melody.h"

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
            grooveAmount[i] = (float)(seed % 36) / 100.0f; // 0-35% timing for kick
        }
        else
        {
            grooveAmount[i] = (float)(seed % 76) / 100.0f; // 0-75% timing for others
        }

        seed = System::GetUs() ^ (i * 98765); // Different seed for velocity
        grooveVelocityAmount[i] = (float)(seed % 101) / 100.0f; // 0-100% velocity
    }

    // Randomize melody groove amount (25-75%)
    uint32_t melSeed = System::GetUs();
    melodyGrooveAmount = 0.25f + (float)(melSeed % 51) / 100.0f;
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
            static_cast<uint8_t>(0x80 | melodyMidiChannel),
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
                // Trigger CV gate
                analogGateHigh = true;
                analogGateCounter = 0;

                // Output CV voltage
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
                        static_cast<uint8_t>(0x80 | melodyMidiChannel),
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
                    static_cast<uint8_t>(0x90 | melodyMidiChannel),
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
    uint8_t noteOn[3] = {
        static_cast<uint8_t>(0x90 | DRM1_MIDI_CHANNEL), // Note On + channel
        drumNotes[voice],                                // Note number
        velocity                                         // Velocity
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
            static_cast<uint8_t>(0x80 | melodyMidiChannel),
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
// POLY VOICE (CHORDS) MIDI OUTPUT
// ============================================================================

void InitPolyVoice()
{
    polyState.Init();
    polyNotesOn = false;
    polyNumActiveNotes = 0;
    for(int i = 0; i < 6; i++)
    {
        polyActiveNotes[i] = -1;
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

void SendPolyChordOn(const int8_t* notes, uint8_t numNotes, uint8_t velocity)
{
    // Store active notes for note-off later
    polyNumActiveNotes = numNotes;
    for(int i = 0; i < 6; i++)
    {
        if(i < numNotes)
            polyActiveNotes[i] = notes[i];
        else
            polyActiveNotes[i] = -1;
    }

    // Send note-on for each note in the chord
    for(uint8_t i = 0; i < numNotes; i++)
    {
        uint8_t noteOn[3] = {
            static_cast<uint8_t>(0x90 | polyVoice.midiChannel),
            static_cast<uint8_t>(notes[i]),
            velocity
        };
        hw.midi.SendMessage(noteOn, 3);
    }

    polyNotesOn = true;
}

void SendPolyNoteOff()
{
    if(!polyNotesOn)
        return;

    // Send note-off for all active notes
    for(uint8_t i = 0; i < polyNumActiveNotes; i++)
    {
        if(polyActiveNotes[i] >= 0)
        {
            uint8_t noteOff[3] = {
                static_cast<uint8_t>(0x80 | polyVoice.midiChannel),
                static_cast<uint8_t>(polyActiveNotes[i]),
                0
            };
            hw.midi.SendMessage(noteOff, 3);
            polyActiveNotes[i] = -1;
        }
    }

    polyNotesOn = false;
    polyNumActiveNotes = 0;
}

void ProcessPolyVoiceStep(uint8_t step)
{
    if(!polyVoice.active)
        return;

    // Get chord rate in steps
    uint8_t rateSteps = chordRateSteps[polyVoice.chordRate];

    // Check if it's time to change chords
    // Change occurs on step 0, and then at intervals based on rate
    bool shouldChange = false;

    if(step == 0)
    {
        // Always change on step 0 (start of 2-bar pattern)
        shouldChange = true;
    }
    else if(rateSteps < 64)
    {
        // For rates faster than 2 bars, check if we're at the interval
        shouldChange = (step % rateSteps) == 0;
    }

    if(shouldChange)
    {
        // Get current progression
        const ChordProgression& prog = progressions[polyVoice.progressionIndex];

        // Send note-off for previous chord
        SendPolyNoteOff();

        // Get current chord from progression
        const ProgressionStep& chordStep = prog.steps[polyState.currentChordIndex];

        // Calculate root note
        int8_t rootNote;
        if(prog.diatonic)
        {
            // Diatonic: use scale degree offset from melody root
            rootNote = melodyRoot + chordStep.rootOffset;
        }
        else
        {
            // Chromatic: absolute semitone offset
            rootNote = chordStep.rootOffset;
        }

        // Get chord notes
        int8_t chordNotes[6];
        uint8_t numNotes = GetChordNotes(rootNote, chordStep.chordType,
                                          polyVoice.octaveOffset, chordNotes);

        // Send note-on for new chord
        if(numNotes > 0)
        {
            SendPolyChordOn(chordNotes, numNotes, polyVoice.velocity);
        }

        // Advance to next chord in progression
        polyState.currentChordIndex++;
        if(polyState.currentChordIndex >= prog.numChords)
        {
            polyState.currentChordIndex = 0;
        }
    }
}
