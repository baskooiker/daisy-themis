/**
 * @file themis_sequencer.cpp
 * @brief Sequencer state machine implementation
 */

#include "themis_sequencer.h"
#include "themis_platform.h"
#include "themis_data.h"
#include "themis_patterns.h"
#include "themis_melody.h"

namespace themis {

// ============================================================================
// INITIALIZATION
// ============================================================================

VariationConfig Sequencer::GetDefaultVariationConfig()
{
    return {
        VAR_MODE_OFF,
        VAR_SEQ_AAAB,
        VAR_GRAN_BAR,
        RHYTHM_EUCLIDEAN,
        RHYTHM_EUCLIDEAN,
        DENSITY_MEDIUM,
        DENSITY_MEDIUM
    };
}

VariationConfig Sequencer::GetDefaultMelodyVariationConfig()
{
    return {
        VAR_MODE_OFF,
        VAR_SEQ_AAAB,
        VAR_GRAN_BAR,
        RHYTHM_EUCLIDEAN,
        RHYTHM_EUCLIDEAN,
        DENSITY_LOW,
        DENSITY_MEDIUM
    };
}

void Sequencer::Init()
{
    VariationConfig defaultVar = GetDefaultVariationConfig();

    // Initialize generative voices
    generativeVoices[0] = {DRUM1, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM2, 0, 0, 0, 32, true, defaultVar};
    generativeVoices[1] = {DRUM2, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM1, 0, 0, 0, 32, true, defaultVar};
    generativeVoices[2] = {MULTI, RHYTHM_SYNCOPATED, DENSITY_LOW, INTERACTION_NONE, MULTI, 0, 0, 0, 32, true, defaultVar};
    generativeVoices[3] = {SNARE, RHYTHM_STRAIGHT, DENSITY_MEDIUM, INTERACTION_ALTERNATE_BAR, HIHAT2_CLOSED, 0, 0, 0, 32, true, defaultVar};
    generativeVoices[4] = {HIHAT2_CLOSED, RHYTHM_STRAIGHT, DENSITY_HIGH, INTERACTION_NONE, HIHAT2_CLOSED, 0, 0, 0, 32, true, defaultVar};
    generativeVoices[5] = {ANALOG, RHYTHM_FOLLOW_KICK, DENSITY_HIGH, INTERACTION_NONE, ANALOG, 0, 0, 0, 32, true, defaultVar};

    fundamentalBeatVoice = SNARE;

    // Initialize melody voices
    VariationConfig defaultMelVar = GetDefaultMelodyVariationConfig();

    melodyVoice.style = MELODY_SUPPORTING;
    melodyVoice.subStyle = SUPPORT_FOLLOW_KICK;
    melodyVoice.rhythmStyle = RHYTHM_EUCLIDEAN;
    melodyVoice.density = DENSITY_LOW;
    melodyVoice.rhythmPattern = 0;
    melodyVoice.rhythmPatternB = 0;
    melodyVoice.rhythmPatternC = 0;
    melodyVoice.patternLength = 32;
    for(int i = 0; i < 32; i++) {
        melodyVoice.noteSequence[i] = 0;
        melodyVoice.noteSequenceB[i] = 0;
        melodyVoice.noteSequenceC[i] = 0;
    }
    melodyVoice.sequencePos = 0;
    melodyVoice.currentOctave = 0;
    melodyVoice.active = true;
    melodyVoice.variation = defaultMelVar;

    melodyMidiVoice.style = MELODY_ARPEGGIATOR;
    melodyMidiVoice.subStyle = ARP_CHORD_TONES;
    melodyMidiVoice.rhythmStyle = RHYTHM_EUCLIDEAN;
    melodyMidiVoice.density = DENSITY_MEDIUM;
    melodyMidiVoice.rhythmPattern = 0;
    melodyMidiVoice.rhythmPatternB = 0;
    melodyMidiVoice.rhythmPatternC = 0;
    melodyMidiVoice.patternLength = 32;
    for(int i = 0; i < 32; i++) {
        melodyMidiVoice.noteSequence[i] = 0;
        melodyMidiVoice.noteSequenceB[i] = 0;
        melodyMidiVoice.noteSequenceC[i] = 0;
    }
    melodyMidiVoice.sequencePos = 0;
    melodyMidiVoice.currentOctave = 0;
    melodyMidiVoice.active = true;
    melodyMidiVoice.variation = defaultMelVar;

    melodyScale = SCALE_MINOR;
    melodyRoot = 0;

    // Initialize groove amounts
    for(int i = 0; i < NUM_DRUM_VOICES; i++) {
        grooveAmount[i] = 0.5f;
        grooveVelocityAmount[i] = 0.5f;
    }
    melodyGrooveAmount = 0.5f;

    // Initialize state
    currentStep = 0;
    barCounter = 0;
    cycleCounter = 0;
    currentKickPattern = 0;
    currentClapPattern = 0;
    currentHatPattern = 0;
    currentGroovePattern = 0;
    generationSeed = 0;

    bpm = 120.0f;
    isRunning = false;
    freezeEnabled = false;
    melodyFreezeEnabled = false;

    fillActive = false;
    fillIsHalfBar = false;
    fillStartStep = 0;
}

// ============================================================================
// TRANSPORT
// ============================================================================

void Sequencer::Start()
{
    isRunning = true;
    currentStep = 0;
    barCounter = 0;

    // Generate initial patterns
    if(g_platform) {
        generationSeed = g_platform->GetRandomSeed();
    }
    RandomizePatterns();
    GenerateMelodyPatterns();
}

void Sequencer::Stop()
{
    isRunning = false;
    if(onMelodyNoteOff) {
        onMelodyNoteOff(false);  // CV
        onMelodyNoteOff(true);   // MIDI
    }
}

// ============================================================================
// GROOVE CALCULATIONS
// ============================================================================

int32_t Sequencer::CalculateGrooveOffset(DrumVoice voice, uint8_t step, float sampleRate)
{
    uint8_t patternStep = step % 16;
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];
    float scaledOffsetPercent = (float)baseOffsetPercent * grooveAmount[voice];
    float samplesPerSixteenth = sampleRate * 15.0f / bpm;
    float offsetFloat = (scaledOffsetPercent / 100.0f) * samplesPerSixteenth;
    return (int32_t)offsetFloat;
}

uint8_t Sequencer::CalculateGrooveVelocity(DrumVoice voice, uint8_t baseVelocity, uint8_t step)
{
    uint8_t patternStep = step % 16;
    int8_t velocityPercent = velocityPatterns[currentGroovePattern][patternStep];
    float velocityMultiplier = 100.0f + ((velocityPercent - 100.0f) * grooveVelocityAmount[voice]);
    float finalVelocity = (float)baseVelocity * (velocityMultiplier / 100.0f);

    if(finalVelocity < 1.0f) finalVelocity = 1.0f;
    if(finalVelocity > 127.0f) finalVelocity = 127.0f;

    return (uint8_t)finalVelocity;
}

int32_t Sequencer::CalculateMelodyGrooveOffset(uint8_t step, float sampleRate)
{
    uint8_t patternStep = step % 16;
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];
    float scaledOffsetPercent = (float)baseOffsetPercent * melodyGrooveAmount;
    float samplesPerSixteenth = sampleRate * 15.0f / bpm;
    return (int32_t)((scaledOffsetPercent / 100.0f) * samplesPerSixteenth);
}

// ============================================================================
// PATTERN GENERATION
// ============================================================================

void Sequencer::RandomizePatterns()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    currentKickPattern = (seed % 16);
    currentClapPattern = ((seed >> 4) % 16);
    currentHatPattern = ((seed >> 8) % 16);

    generationSeed = seed;
    GenerateVoicePatterns();
}

void Sequencer::GenerateVoicePatterns()
{
    const uint8_t oddLengths[] = {12, 13, 15, 17, 18};

    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        uint32_t seed = generationSeed ^ (i * 12345);

        // Follow kick always uses 32-step pattern
        if(voice->rhythmStyle == RHYTHM_FOLLOW_KICK)
        {
            voice->patternLength = 32;
        }
        else if((seed % 100) < 15)
        {
            voice->patternLength = oddLengths[(seed >> 8) % 5];
        }
        else
        {
            voice->patternLength = 32;
        }

        // Generate A pattern
        voice->pattern = GeneratePatternForStyle(seed, voice->rhythmStyle, voice->density,
                                                  voice->patternLength, currentKickPattern);

        // Always generate unique B and C patterns (so they're available when user enables variations)
        uint32_t seedB = seed ^ 0xB5B5B5B5;
        uint32_t seedC = seed ^ 0xC3C3C3C3;

        // Use variation style/density if set, otherwise use main voice settings with different seeds
        voice->patternB = GeneratePatternForStyle(seedB,
                                                   voice->variation.styleB,
                                                   voice->variation.densityB,
                                                   voice->patternLength, currentKickPattern);

        voice->patternC = GeneratePatternForStyle(seedC,
                                                   voice->variation.styleC,
                                                   voice->variation.densityC,
                                                   voice->patternLength, currentKickPattern);

        // Ensure B and C are different from A (regenerate with modified seed if same)
        if(voice->patternB == voice->pattern)
        {
            seedB ^= 0x12345678;
            voice->patternB = GeneratePatternForStyle(seedB, voice->variation.styleB,
                                                       voice->variation.densityB,
                                                       voice->patternLength, currentKickPattern);
        }
        if(voice->patternC == voice->pattern || voice->patternC == voice->patternB)
        {
            seedC ^= 0x87654321;
            voice->patternC = GeneratePatternForStyle(seedC, voice->variation.styleC,
                                                       voice->variation.densityC,
                                                       voice->patternLength, currentKickPattern);
        }
    }

    // Process interactions
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->interaction == INTERACTION_NONE)
            continue;

        VoiceConfig* partner = nullptr;
        for(int j = 0; j < 6; j++)
        {
            if(generativeVoices[j].voice == voice->interactionPartner)
            {
                partner = &generativeVoices[j];
                break;
            }
        }

        if(partner == nullptr)
            continue;

        if(i < (partner - generativeVoices))
        {
            switch(voice->interaction)
            {
                case INTERACTION_DIVIDED:
                    ProcessInteractionDivided(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_BAR:
                    ProcessInteractionAlternateBar(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_HALF:
                    ProcessInteractionAlternateHalf(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_TWO:
                    ProcessInteractionAlternateTwo(voice, partner, barCounter);
                    break;
                default:
                    break;
            }
        }
    }

}

void Sequencer::RandomizeVoicePersonalities()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Randomly assign fundamental beat role
    fundamentalBeatVoice = ((seed % 2) == 0) ? CLAP : SNARE;
    DrumVoice generativeBackbeatVoice = (fundamentalBeatVoice == CLAP) ? SNARE : CLAP;
    generativeVoices[3].voice = generativeBackbeatVoice;

    // Randomize rhythm styles
    RhythmStyle styles[] = {RHYTHM_SYNCOPATED, RHYTHM_STRAIGHT, RHYTHM_EUCLIDEAN, RHYTHM_ANTI_EUCLIDEAN, RHYTHM_FOLLOW_KICK};
    for(int i = 0; i < 6; i++)
    {
        seed = g_platform ? g_platform->GetRandomSeed() ^ (i * 11111) : seed ^ (i * 11111);
        generativeVoices[i].rhythmStyle = styles[seed % 5];
    }

    // Reset interactions
    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].interaction = INTERACTION_NONE;
        generativeVoices[i].interactionPartner = generativeVoices[i].voice;
    }

    // Pick alternating pair (50% probability)
    seed = g_platform ? g_platform->GetRandomSeed() : seed;
    int alternatePair[2] = {-1, -1};
    if((seed % 100) < 50)
    {
        alternatePair[0] = (seed >> 8) % 6;
        alternatePair[1] = ((seed >> 16) % 5);
        if(alternatePair[1] >= alternatePair[0]) alternatePair[1]++;

        InteractionStyle alternateStyles[] = {INTERACTION_ALTERNATE_BAR, INTERACTION_ALTERNATE_HALF, INTERACTION_ALTERNATE_TWO};
        InteractionStyle altStyle = alternateStyles[(seed >> 20) % 3];

        generativeVoices[alternatePair[0]].interaction = altStyle;
        generativeVoices[alternatePair[0]].interactionPartner = generativeVoices[alternatePair[1]].voice;
        generativeVoices[alternatePair[1]].interaction = altStyle;
        generativeVoices[alternatePair[1]].interactionPartner = generativeVoices[alternatePair[0]].voice;
    }

    // Pick divided pair (50% probability)
    seed = g_platform ? g_platform->GetRandomSeed() : seed;
    if((seed % 100) < 50)
    {
        int voice1 = -1, voice2 = -1;
        int attempts = 0;

        do {
            seed = g_platform ? g_platform->GetRandomSeed() ^ (attempts * 7777) : seed ^ (attempts * 7777);
            voice1 = (seed >> 8) % 6;
            attempts++;
        } while((voice1 == alternatePair[0] || voice1 == alternatePair[1]) && attempts < 10);

        attempts = 0;
        do {
            seed = g_platform ? g_platform->GetRandomSeed() ^ (attempts * 9999) : seed ^ (attempts * 9999);
            voice2 = (seed >> 8) % 6;
            attempts++;
        } while((voice2 == voice1 || voice2 == alternatePair[0] || voice2 == alternatePair[1]) && attempts < 10);

        if(voice1 != -1 && voice2 != -1 && voice1 != voice2)
        {
            generativeVoices[voice1].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice1].interactionPartner = generativeVoices[voice2].voice;
            generativeVoices[voice2].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice2].interactionPartner = generativeVoices[voice1].voice;
        }
    }

    // Randomize densities
    DensityLevel densities[6];
    densities[0] = DENSITY_LOW;
    densities[1] = DENSITY_MEDIUM;
    densities[2] = DENSITY_HIGH;

    seed = g_platform ? g_platform->GetRandomSeed() : seed;
    densities[3] = (DensityLevel)((seed >> 8) % 3);
    densities[4] = (DensityLevel)((seed >> 16) % 3);
    densities[5] = (DensityLevel)((seed >> 24) % 3);

    // Shuffle
    for(int i = 5; i > 0; i--)
    {
        seed = g_platform ? g_platform->GetRandomSeed() : seed;
        int j = seed % (i + 1);
        DensityLevel temp = densities[i];
        densities[i] = densities[j];
        densities[j] = temp;
    }

    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].density = densities[i];
    }
}

void Sequencer::GenerateMelodyPatterns()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;
    GenerateMelodyPatternFor(seed, &melodyVoice, melodyScale, melodyRoot, currentKickPattern);

    seed = g_platform ? g_platform->GetRandomSeed() : seed ^ 0x55555555;
    GenerateMelodyPatternFor(seed, &melodyMidiVoice, melodyScale, melodyRoot, currentKickPattern);
}

void Sequencer::RandomizeMelodyPersonality()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;
    RandomizeMelodyPersonalityFor(seed, &melodyVoice, melodyScale, melodyRoot, currentKickPattern);

    seed = g_platform ? g_platform->GetRandomSeed() : seed ^ 0xAAAAAAAA;
    RandomizeMelodyPersonalityFor(seed, &melodyMidiVoice, melodyScale, melodyRoot, currentKickPattern);
}

void Sequencer::RandomizeGroove()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;
    currentGroovePattern = seed % 32;

    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        seed = g_platform ? g_platform->GetRandomSeed() ^ (i * 54321) : seed ^ (i * 54321);

        if(i == KICK)
        {
            grooveAmount[i] = (float)(seed % 36) / 100.0f;
        }
        else
        {
            grooveAmount[i] = (float)(seed % 76) / 100.0f;
        }

        seed = g_platform ? g_platform->GetRandomSeed() ^ (i * 98765) : seed ^ (i * 98765);
        grooveVelocityAmount[i] = (float)(seed % 101) / 100.0f;
    }

    uint32_t melSeed = g_platform ? g_platform->GetRandomSeed() : seed;
    melodyGrooveAmount = 0.25f + (float)(melSeed % 51) / 100.0f;
}

void Sequencer::RandomizeAll()
{
    if(onMelodyNoteOff) {
        onMelodyNoteOff(false);
        onMelodyNoteOff(true);
    }

    // Randomize personalities FIRST so patterns are generated with correct interactions
    RandomizeVoicePersonalities();
    RandomizePatterns();
    RandomizeGroove();
    RandomizeMelodyPersonality();
}

void Sequencer::ScheduleFill()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;
    if((seed % 100) < 30)
    {
        fillActive = true;
        fillIsHalfBar = ((seed >> 8) % 2) == 0;
        fillStartStep = fillIsHalfBar ? 120 : 112;
        currentFillSnareIndex = (seed >> 16) % 8;
        currentFillHatClosedIndex = (seed >> 20) % 8;
        currentFillHatOpenIndex = (seed >> 24) % 8;
    }
    else
    {
        fillActive = false;
    }
}

// ============================================================================
// STEP PROCESSING
// ============================================================================

void Sequencer::ProcessFillPatterns(uint8_t totalStep)
{
    uint8_t fillStep = totalStep - fillStartStep;

    if(fillIsHalfBar)
    {
        if(IsStepActive8(kickFillsHalf[currentFillSnareIndex % 8], fillStep) && onDrumTrigger)
            onDrumTrigger(KICK, 120);

        if(IsStepActive8(snareFillsHalf[currentFillSnareIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(fundamentalBeatVoice, 115);

        if(IsStepActive8(hatClosedFillsHalf[currentFillHatClosedIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(HIHAT1_CLOSED, 100);

        if(IsStepActive8(hatOpenFillsHalf[currentFillHatOpenIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(HIHAT1_OPEN, 110);
    }
    else
    {
        if(IsStepActive16(kickFillsWhole[currentFillSnareIndex % 8], fillStep) && onDrumTrigger)
            onDrumTrigger(KICK, 120);

        if(IsStepActive16(snareFillsWhole[currentFillSnareIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(fundamentalBeatVoice, 115);

        if(IsStepActive16(hatClosedFillsWhole[currentFillHatClosedIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(HIHAT1_CLOSED, 100);

        if(IsStepActive16(hatOpenFillsWhole[currentFillHatOpenIndex], fillStep) && onDrumTrigger)
            onDrumTrigger(HIHAT1_OPEN, 110);
    }
}

void Sequencer::ProcessDrumPatterns(float sampleRate)
{
    (void)sampleRate; // Will be used for groove timing in full implementation

    uint8_t totalStep = (barCounter * 32) + currentStep;
    bool inFill = fillActive && (totalStep >= fillStartStep);

    if(inFill)
    {
        ProcessFillPatterns(totalStep);
    }
    else
    {
        // Normal patterns
        if(IsStepActive(kickPatterns[currentKickPattern], currentStep) && onDrumTrigger)
        {
            uint8_t vel = CalculateGrooveVelocity(KICK, 110, currentStep);
            onDrumTrigger(KICK, vel);
        }

        if(IsStepActive(clapPatterns[currentClapPattern], currentStep) && onDrumTrigger)
        {
            uint8_t vel = CalculateGrooveVelocity(fundamentalBeatVoice, 100, currentStep);
            onDrumTrigger(fundamentalBeatVoice, vel);
        }

        if(IsStepActive(hatClosedPatterns[currentHatPattern], currentStep) && onDrumTrigger)
        {
            uint8_t vel = CalculateGrooveVelocity(HIHAT1_CLOSED, 70, currentStep);
            onDrumTrigger(HIHAT1_CLOSED, vel);
        }

        if(IsStepActive(hatOpenPatterns[currentHatPattern], currentStep) && onDrumTrigger)
        {
            uint8_t vel = CalculateGrooveVelocity(HIHAT1_OPEN, 100, currentStep);
            onDrumTrigger(HIHAT1_OPEN, vel);
        }
    }

    // Process generative voices
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->active && voice->voice != ANALOG)
        {
            uint8_t var = GetCurrentVariation(&voice->variation, currentStep, barCounter);

            uint32_t activePattern;
            if(var == 1)
                activePattern = voice->patternB;
            else if(var == 2)
                activePattern = voice->patternC;
            else
                activePattern = voice->pattern;

            uint8_t voiceStep = currentStep % voice->patternLength;
            if(IsStepActive(activePattern, voiceStep) && onDrumTrigger)
            {
                uint8_t vel = CalculateGrooveVelocity(voice->voice, 95, currentStep);
                onDrumTrigger(voice->voice, vel);
            }
        }
    }
}

void Sequencer::ProcessMelodyPatterns()
{
    // CV melody voice
    if(melodyVoice.active)
    {
        uint8_t var = GetCurrentVariation(&melodyVoice.variation, currentStep, barCounter);

        uint32_t activeRhythm;
        const int8_t* activeNotes;
        if(var == 1) {
            activeRhythm = melodyVoice.rhythmPatternB;
            activeNotes = melodyVoice.noteSequenceB;
        } else if(var == 2) {
            activeRhythm = melodyVoice.rhythmPatternC;
            activeNotes = melodyVoice.noteSequenceC;
        } else {
            activeRhythm = melodyVoice.rhythmPattern;
            activeNotes = melodyVoice.noteSequence;
        }

        uint8_t melodyStep = currentStep % melodyVoice.patternLength;
        if(IsStepActive(activeRhythm, melodyStep) && onMelodyTrigger)
        {
            int8_t note = activeNotes[melodyStep];
            onMelodyTrigger(note, false);  // CV
        }
    }

    // MIDI melody voice
    if(melodyMidiVoice.active)
    {
        uint8_t var = GetCurrentVariation(&melodyMidiVoice.variation, currentStep, barCounter);

        uint32_t activeRhythm;
        const int8_t* activeNotes;
        if(var == 1) {
            activeRhythm = melodyMidiVoice.rhythmPatternB;
            activeNotes = melodyMidiVoice.noteSequenceB;
        } else if(var == 2) {
            activeRhythm = melodyMidiVoice.rhythmPatternC;
            activeNotes = melodyMidiVoice.noteSequenceC;
        } else {
            activeRhythm = melodyMidiVoice.rhythmPattern;
            activeNotes = melodyMidiVoice.noteSequence;
        }

        uint8_t midiMelStep = currentStep % melodyMidiVoice.patternLength;
        if(IsStepActive(activeRhythm, midiMelStep) && onMelodyTrigger)
        {
            int8_t note = activeNotes[midiMelStep];
            onMelodyTrigger(note, true);  // MIDI
        }
    }
}

void Sequencer::ProcessStep(float sampleRate)
{
    if(!isRunning)
        return;

    // Randomize groove at start of new 8-bar cycle (25% probability)
    if(barCounter == 0 && currentStep == 0)
    {
        uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;
        if((seed % 100) < 25)
        {
            RandomizeGroove();
        }
    }

    // Schedule fill at start of 7th-8th bar
    if(barCounter == 3 && currentStep == 0)
    {
        ScheduleFill();
    }

    // Process patterns
    ProcessDrumPatterns(sampleRate);
    ProcessMelodyPatterns();

    // Advance step
    currentStep++;
    if(currentStep >= 32)
    {
        currentStep = 0;
        barCounter++;

        if(barCounter >= patternChangeInterval)
        {
            if(!freezeEnabled)
            {
                RandomizePatterns();
            }

            if(!melodyFreezeEnabled)
            {
                if(onMelodyNoteOff) {
                    onMelodyNoteOff(false);
                    onMelodyNoteOff(true);
                }
                GenerateMelodyPatterns();
            }

            barCounter = 0;
            fillActive = false;
            cycleCounter++;

            if(cycleCounter >= personalityChangeInterval)
            {
                if(!freezeEnabled)
                {
                    RandomizeVoicePersonalities();
                }

                if(!melodyFreezeEnabled)
                {
                    if(onMelodyNoteOff) {
                        onMelodyNoteOff(false);
                        onMelodyNoteOff(true);
                    }
                    RandomizeMelodyPersonality();
                }

                cycleCounter = 0;
            }
        }
    }
}

} // namespace themis
