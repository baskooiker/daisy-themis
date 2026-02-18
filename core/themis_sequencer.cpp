/**
 * @file themis_sequencer.cpp
 * @brief Sequencer state machine implementation
 */

#include "themis_sequencer.h"
#include "themis_platform.h"
#include "themis_data.h"
#include "themis_patterns.h"
#include "themis_melody.h"
#include "themis_chords.h"
#include "themis_rhythm.h"
#include "themis_bass.h"

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

    // Initialize melody voice
    VariationConfig defaultMelVar = GetDefaultMelodyVariationConfig();

    melodyVoice.style = MELODY_ARPEGGIATOR;
    melodyVoice.subStyle = ARP_CHORD_TONES;
    melodyVoice.rhythmStyle = RHYTHM_EUCLIDEAN;
    melodyVoice.density = DENSITY_MEDIUM;
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
    melodyVoice.compatMode = COMPAT_CHORD_SCALE;  // Default: most melodic freedom

    melodyScale = SCALE_MINOR;
    melodyRoot = 0;

    // Initialize chord voice
    chordVoice.active = true;  // Active by default
    chordVoice.progressionIndex = 0;
    chordVoice.chordRate = CHORD_RATE_1_BAR;
    chordVoice.velocity = 80;
    chordVoice.octaveOffset = 0;
    chordVoice.midiChannel = 1;  // Default: channel 2 (0-indexed)
    chordVoice.progressionB = 1;
    chordVoice.variationMode = VAR_MODE_OFF;
    chordState.Init();

    // Initialize chord randomizer
    chordRandomizer.Init();
    chordRandomizerState.Init();

    // Initialize rhythm player voice
    rhythmVoice.Init();
    rhythmState.Init();

    // Initialize bass voice
    bassVoice.Init();
    bassState.Init();

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
    cycleCounter = 0;

    // Reset chord voice state so chord progression starts from beginning
    chordState.currentChordIndex = 0;
    chordState.stepsUntilChange = 0;  // Will be initialized on first step
    chordState.notesOn = false;
    chordState.numActiveNotes = 0;

    // Reset rhythm player state
    rhythmState.barPosition = 0;
    rhythmState.patternPosition = 0;

    // Reset bass voice state
    bassState.currentNote = -1;

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
        onMelodyNoteOff();
    }
    // Release any held chord voice notes
    ReleaseChord();

    // Release any held rhythm player notes
    if(rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
        onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
        rhythmState.numActiveNotes = 0;
        rhythmState.noteDuration = 0;
    }

    // Release any held bass voice note
    if(bassState.currentNote >= 0 && onBassTrigger) {
        onBassTrigger(bassState.currentNote, 70, false);
        bassState.currentNote = -1;
        bassState.gateStepsRemaining = 0;
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
}

void Sequencer::RandomizeMelodyPersonality()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Randomize AB variation
    melodyVoice.variation.mode = (VariationMode)(seed % NUM_VARIATION_MODES);
    melodyVoice.variation.sequence = (VariationSequence)((seed >> 4) % NUM_VARIATION_SEQUENCES);

    // Randomize B/C variation parameters for pattern generation
    uint32_t varSeed = seed ^ 0xABCDABCD;
    melodyVoice.variation.styleB = (RhythmStyle)(varSeed % NUM_RHYTHM_STYLES);
    melodyVoice.variation.densityB = (DensityLevel)((varSeed >> 4) % NUM_DENSITY_LEVELS);
    melodyVoice.variation.styleC = (RhythmStyle)((varSeed >> 8) % NUM_RHYTHM_STYLES);
    melodyVoice.variation.densityC = (DensityLevel)((varSeed >> 12) % NUM_DENSITY_LEVELS);

    RandomizeMelodyPersonalityFor(seed, &melodyVoice, melodyScale, melodyRoot, currentKickPattern);
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
        onMelodyNoteOff();
    }
    ReleaseChord();

    // Release rhythm player notes
    if(rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
        onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
        rhythmState.numActiveNotes = 0;
    }

    // Release bass voice note
    if(bassState.currentNote >= 0 && onBassTrigger) {
        onBassTrigger(bassState.currentNote, 70, false);
        bassState.currentNote = -1;
    }

    // Randomize personalities FIRST so patterns are generated with correct interactions
    RandomizeVoicePersonalities();
    RandomizePatterns();
    RandomizeGroove();
    RandomizeMelodyPersonality();
    RandomizeChordVoice();
    RandomizeRhythmVoice();
    RandomizeBassVoice();
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
    // Release melody note if voice is deactivated
    if (!melodyVoice.active) {
        if (onMelodyNoteOff) {
            onMelodyNoteOff();
        }
        return;
    }

    // Get chord context if chord voice is active
    ChordContext chordCtx;
    bool useChordMapping = chordVoice.active;
    if (useChordMapping) {
        chordCtx = GetCurrentChordContext();
    }

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

            // Apply chord-aware mapping when chord voice is active
            if (useChordMapping) {
                note = MapNoteToChord(note, chordCtx, melodyVoice.compatMode);
            }

            onMelodyTrigger(note);
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
    ProcessChordVoice();
    ProcessRhythmVoice();
    ProcessBassVoice();

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
                    onMelodyNoteOff();
                }
                GenerateMelodyPatterns();
            }

            barCounter = 0;
            fillActive = false;
            bassState.fillActive = false;
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
                        onMelodyNoteOff();
                    }
                    RandomizeMelodyPersonality();
                }

                cycleCounter = 0;
            }
        }
    }
}

// ============================================================================
// CHORD VOICE
// ============================================================================

void Sequencer::ProcessChordVoice()
{
    // If voice was deactivated while notes are playing, release them
    if (!chordVoice.active) {
        if (chordState.notesOn) {
            ReleaseChord();
        }
        return;
    }

    uint8_t stepsPerChord = chordRateSteps[chordVoice.chordRate];

    // Initialize on first step
    if (chordState.stepsUntilChange == 0 && !chordState.notesOn) {
        chordState.stepsUntilChange = stepsPerChord;
        TriggerChord();
        return;
    }

    // Check if we need to release notes (1 step before chord change)
    if (chordState.notesOn && chordState.stepsUntilChange == 1) {
        ReleaseChord();
    }

    // Decrement countdown
    chordState.stepsUntilChange--;

    // Check if it's time for a new chord
    if (chordState.stepsUntilChange == 0) {
        // Advance to next chord in progression
        uint8_t progIndex = chordVoice.progressionIndex;

        // Handle variation
        if (chordVoice.variationMode != VAR_MODE_OFF) {
            // Simple A/B switching based on bar
            if ((barCounter % 2) == 1) {
                progIndex = chordVoice.progressionB;
            }
        }

        uint8_t nextChordIndex = (chordState.currentChordIndex + 1) %
                                  progressions[progIndex].length;

        // If we've wrapped around to chord 0, process chord randomization
        if (nextChordIndex == 0) {
            // First apply any pending manual progression change
            if (chordState.pendingProgressionIndex >= 0) {
                chordVoice.progressionIndex = (uint8_t)chordState.pendingProgressionIndex;
                chordState.pendingProgressionIndex = -1;  // Clear pending
                progIndex = chordVoice.progressionIndex;
            } else {
                // Otherwise, process automatic randomization
                ProcessChordRandomization();
                progIndex = chordVoice.progressionIndex;
            }

            // Notify rhythm voice of chord progression cycle
            NotifyRhythmOfChordCycle();
        }

        chordState.currentChordIndex = nextChordIndex;

        // Trigger new chord
        TriggerChord();

        // Reset countdown
        chordState.stepsUntilChange = stepsPerChord;
    }
}

void Sequencer::TriggerChord()
{
    // First release any currently held notes
    if (chordState.notesOn) {
        ReleaseChord();
    }

    // Get the correct progression based on variation
    uint8_t progIndex = chordVoice.progressionIndex;
    if (chordVoice.variationMode != VAR_MODE_OFF && (barCounter % 2) == 1) {
        progIndex = chordVoice.progressionB;
    }

    // Get the chord notes
    int8_t notes[6];
    uint8_t count = GetChordNotes(
        &progressions[progIndex],
        chordState.currentChordIndex,
        melodyRoot,
        melodyScale,
        chordVoice.octaveOffset,
        notes
    );

    // Store active notes for later release
    chordState.numActiveNotes = count;
    for (int i = 0; i < count && i < 6; i++) {
        chordState.activeNotes[i] = notes[i];
    }
    chordState.notesOn = true;

    // Trigger callback
    if (onChordTrigger) {
        onChordTrigger(notes, count, true);  // noteOn = true
    }
}

void Sequencer::ReleaseChord()
{
    if (!chordState.notesOn) return;

    // Send note-offs for all active notes
    if (onChordTrigger) {
        onChordTrigger(chordState.activeNotes, chordState.numActiveNotes, false);
    }

    chordState.notesOn = false;
    chordState.numActiveNotes = 0;
}

ChordContext Sequencer::GetCurrentChordContext() const
{
    ChordContext ctx;

    // Default to global root if chord voice is inactive
    if (!chordVoice.active || chordState.numActiveNotes == 0) {
        ctx.chordRoot = melodyRoot;
        ctx.chordType = CHORD_MINOR;  // Default assumption for minor scales
        ctx.isDiatonic = true;
        return ctx;
    }

    // Get the current progression based on variation
    uint8_t progIndex = chordVoice.progressionIndex;
    if (chordVoice.variationMode != VAR_MODE_OFF && (barCounter % 2) == 1) {
        progIndex = chordVoice.progressionB;
    }

    const ChordProgression& prog = progressions[progIndex];
    const ProgressionStep& step = prog.steps[chordState.currentChordIndex];

    // Calculate the chord root
    if (step.isDiatonic) {
        // Get scale degree note
        ctx.chordRoot = GetScaleNote((ScaleType)melodyScale, melodyRoot, step.scaleDegree);
    } else {
        // Direct semitone offset from global root
        ctx.chordRoot = melodyRoot + step.scaleDegree;
    }

    // Normalize to 0-11 range
    while (ctx.chordRoot < 0) ctx.chordRoot += 12;
    ctx.chordRoot = ctx.chordRoot % 12;

    ctx.chordType = step.chordType;
    ctx.isDiatonic = step.isDiatonic;

    return ctx;
}

void Sequencer::RandomizeChordVoice()
{
    // IMPORTANT: Release any currently playing notes before randomizing
    // to prevent hanging notes
    ReleaseChord();

    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Select a random enabled vibe first
    VibeType vibe = SelectRandomEnabledVibe(seed);
    chordRandomizerState.currentVibe = vibe;

    // Select a progression from that vibe (respecting enabled progressions)
    SelectProgressionFromVibe(vibe, seed);

    // Randomize chord rate (favor 1 bar and half bar)
    int rateRoll = (seed >> 8) % 100;
    if (rateRoll < 20)
        chordVoice.chordRate = CHORD_RATE_2_BARS;
    else if (rateRoll < 60)
        chordVoice.chordRate = CHORD_RATE_1_BAR;
    else if (rateRoll < 90)
        chordVoice.chordRate = CHORD_RATE_HALF_BAR;
    else
        chordVoice.chordRate = CHORD_RATE_QUARTER;

    // Randomize octave offset (-1 to +1)
    chordVoice.octaveOffset = ((seed >> 16) % 3) - 1;

    // Randomize B progression (different from A, same vibe)
    uint8_t vibeIndices[32];
    uint8_t vibeCount = GetProgressionsForVibe(vibe, vibeIndices, 32);
    if (vibeCount > 1) {
        uint8_t bIdx = (seed >> 20) % vibeCount;
        if (vibeIndices[bIdx] == chordVoice.progressionIndex && vibeCount > 1) {
            bIdx = (bIdx + 1) % vibeCount;
        }
        chordVoice.progressionB = vibeIndices[bIdx];
    } else {
        chordVoice.progressionB = chordVoice.progressionIndex;
    }

    // Randomize variation mode (50% off, 50% AB)
    chordVoice.variationMode = ((seed >> 24) % 2) == 0 ? VAR_MODE_OFF : VAR_MODE_AB;

    // Randomize velocity (70-110)
    chordVoice.velocity = 70 + ((seed >> 28) % 41);

    // Reset state for new progression
    chordState.Init();

    // Reset chord randomizer state
    chordRandomizerState.inTransition = false;
    chordRandomizerState.transitionBarsRemaining = 0;
    chordRandomizerState.changeTimer = 2 + (seed % 3);  // 2-4 progression cycles before first auto-change
}

// ============================================================================
// RHYTHM PLAYER VOICE
// ============================================================================

void Sequencer::ProcessRhythmVoice()
{
    if (!rhythmVoice.active) {
        // Release any held notes when deactivated
        if (rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
            onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
            rhythmState.numActiveNotes = 0;
            rhythmState.noteDuration = 0;
        }
        return;
    }

    // Get chord context
    ChordContext chordCtx = GetCurrentChordContext();

    // Check if kick is active this step
    bool kickActive = IsStepActive(kickPatterns[currentKickPattern], currentStep);

    // Get random seed for this step
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : (currentStep * 54321);
    seed ^= (uint32_t)currentStep << 8;
    seed ^= (uint32_t)barCounter << 16;

    // Handle note-off for previous notes
    if (rhythmState.noteDuration == 1 && rhythmState.numActiveNotes > 0) {
        if (onRhythmTrigger) {
            onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
        }
        rhythmState.numActiveNotes = 0;
    }

    // Morph mode randomization is now triggered by NotifyRhythmOfChordCycle()
    // at chord progression cycle boundaries, not by step-based timers

    // Pad AB variation: swap padPatternA before processing, restore after
    uint8_t savedPadPattern = rhythmState.padPatternA;
    if (rhythmVoice.padVariation.mode != VAR_MODE_OFF &&
        rhythmState.currentStyle == RHYTHM_PLAY_PAD) {
        uint8_t var = GetCurrentVariation(&rhythmVoice.padVariation, currentStep, barCounter);
        if (var >= 1) {
            rhythmState.padPatternA = rhythmState.padPatternB;
        }
    }

    // Process rhythm step
    int8_t notes[6];
    uint8_t numNotes = 0;

    bool triggered = ProcessRhythmStep(
        rhythmVoice,
        rhythmState,
        chordCtx,
        kickActive,
        currentStep,
        seed,
        notes,
        numNotes
    );

    // Restore A pattern
    rhythmState.padPatternA = savedPadPattern;

    // Trigger notes if any were generated
    if (triggered && numNotes > 0 && onRhythmTrigger) {
        uint8_t velocity = CalculateRhythmVelocity(rhythmState, currentStep, seed ^ 0x87654321);
        onRhythmTrigger(notes, numNotes, velocity, true);
    }
}

void Sequencer::RandomizeRhythmVoice()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Release any held notes
    if (rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
        onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
    }

    // Randomize mode (30% manual, 70% morph)
    rhythmVoice.mode = ((seed % 100) < 30) ? RHYTHM_MODE_MANUAL : RHYTHM_MODE_MORPH;

    // Randomize octave offset (-1 to +1)
    rhythmVoice.octaveOffset = ((seed >> 4) % 3) - 1;

    // If morph mode, randomize all parameters
    if (rhythmVoice.mode == RHYTHM_MODE_MORPH) {
        RandomizeRhythmParams(rhythmVoice, rhythmState, seed);
    } else {
        // Manual mode - set moderate defaults
        rhythmVoice.playStyle = (RhythmPlayStyle)((seed >> 8) % NUM_RHYTHM_PLAY_STYLES);
        rhythmVoice.activity = ACTIVITY_MODERATE;
        rhythmVoice.articulation = ARTICULATION_NORMAL;
        rhythmVoice.inversionVariation = INV_VAR_LOW;
        rhythmVoice.followKick = ((seed >> 12) & 0x01) == 0;
    }

    // Randomize pad patterns (ensure different)
    uint32_t padSeed = seed ^ 0xBADCAFE0;
    rhythmState.padPatternA = padSeed % NUM_PAD_PATTERNS;
    rhythmState.padPatternB = ((padSeed >> 8) % (NUM_PAD_PATTERNS - 1));
    if (rhythmState.padPatternB >= rhythmState.padPatternA) rhythmState.padPatternB++;

    // Randomize pad variation
    rhythmVoice.padVariation.mode = ((padSeed >> 16) % 2 == 0) ? VAR_MODE_OFF : VAR_MODE_AB;
    rhythmVoice.padVariation.sequence = (VariationSequence)((padSeed >> 18) % 4);

    // Reset state (preserving pad patterns we just set)
    uint8_t savedPadA = rhythmState.padPatternA;
    uint8_t savedPadB = rhythmState.padPatternB;
    rhythmState.Init();
    rhythmState.padPatternA = savedPadA;
    rhythmState.padPatternB = savedPadB;

    // Set initial style
    rhythmState.currentStyle = rhythmVoice.playStyle;
    rhythmState.targetStyle = rhythmVoice.playStyle;

    // Set initial intensity based on activity
    switch (rhythmVoice.activity) {
        case ACTIVITY_SPARSE:
            rhythmState.intensity = 0.3f;
            rhythmState.intensityTarget = 0.35f;
            break;
        case ACTIVITY_BUSY:
            rhythmState.intensity = 0.65f;
            rhythmState.intensityTarget = 0.7f;
            break;
        default:
            rhythmState.intensity = 0.5f;
            rhythmState.intensityTarget = 0.5f;
            break;
    }
}

// ============================================================================
// BASS VOICE
// ============================================================================

void Sequencer::ProcessBassVoice()
{
    if (!bassVoice.active) {
        // Release any held note when deactivated
        if (bassState.currentNote >= 0 && onBassTrigger) {
            onBassTrigger(bassState.currentNote, 70, false);
            bassState.currentNote = -1;
            bassState.gateStepsRemaining = 0;
        }
        return;
    }

    // Get chord context
    ChordContext chordCtx = GetCurrentChordContext();

    // Handle note-off for previous note
    if (bassState.gateStepsRemaining == 1 && bassState.currentNote >= 0) {
        if (onBassTrigger) {
            onBassTrigger(bassState.currentNote, 70, false);
        }
        bassState.currentNote = -1;
    }

    // Decrement gate counter
    if (bassState.gateStepsRemaining > 0) {
        bassState.gateStepsRemaining--;
    }

    // Schedule bass fill at start of 4-bar periods (barCounter 1 and 3)
    uint8_t totalStep = (barCounter * 32) + currentStep;
    if (currentStep == 0 && (barCounter == 1 || barCounter == 3) && bassVoice.fillsEnabled) {
        uint32_t fillSeed = g_platform ? g_platform->GetRandomSeed() : 12345;
        if ((fillSeed % 100) < 30) {
            bassState.fillActive = true;
            bassState.fillPatternIndex = (fillSeed >> 8) % NUM_BASS_FILLS;
            bassState.fillStartStep = totalStep + 24;  // Last 8 steps of this 2-bar block
        }
    }

    // Check if we're in the 8-step fill window
    bool inFill = bassState.fillActive
                  && (totalStep >= bassState.fillStartStep)
                  && (totalStep < bassState.fillStartStep + 8);

    // Clear fill flag once the window has passed
    if (bassState.fillActive && totalStep >= bassState.fillStartStep + 8) {
        bassState.fillActive = false;
    }

    // Determine active patterns based on independent variations
    uint8_t savedPattern = bassState.currentPattern;
    uint8_t savedPitchPattern = bassState.currentPitchPattern;

    // Rhythm variation (independent)
    if (bassVoice.rhythmVariation.mode != VAR_MODE_OFF) {
        uint8_t var = GetCurrentVariation(&bassVoice.rhythmVariation, currentStep, barCounter);
        if (var >= 1) {
            bassState.currentPattern = bassState.currentPatternB;
        }
    }

    // Pitch variation (independent)
    if (bassVoice.pitchVariation.mode != VAR_MODE_OFF) {
        uint8_t var = GetCurrentVariation(&bassVoice.pitchVariation, currentStep, barCounter);
        if (var >= 1) {
            bassState.currentPitchPattern = bassState.currentPitchPatternB;
        }
    }

    int8_t note;
    uint8_t velocity;
    uint8_t gateLength;
    bool triggered;

    if (inFill) {
        // Fill mode: use fill pattern for rhythm, normal pitch calculation
        uint8_t fillStep = totalStep - bassState.fillStartStep;
        triggered = (fillStep < 8) && IsStepActive8(bassFillsHalf[bassState.fillPatternIndex], fillStep);
        if (triggered) {
            note = CalculateBassNote(bassVoice, bassState, chordCtx, melodyRoot, currentStep);
            velocity = 90;   // Fill velocity: between normal (70) and accent (120)
            gateLength = 1;  // Short/punchy
        }
    } else {
        // Normal mode
        triggered = ProcessBassStep(
            bassVoice,
            bassState,
            chordCtx,
            melodyRoot,
            currentStep,
            note,
            velocity,
            gateLength
        );
    }

    // Restore A pattern indices
    bassState.currentPattern = savedPattern;
    bassState.currentPitchPattern = savedPitchPattern;

    // Apply octave randomization
    if (triggered && bassVoice.octaveRandomAmount > 0) {
        uint32_t hash = (currentStep * 2654435761u) ^ ((uint32_t)barCounter * 40503u) ^ generationSeed;
        uint8_t roll = hash % 100;
        uint8_t p1 = bassVoice.octaveRandomAmount * 40 / 100;
        uint8_t p2 = (bassVoice.octaveRandomAmount > 50)
                     ? (bassVoice.octaveRandomAmount - 50) * 20 / 100 : 0;
        if (roll < p2) note += 24;
        else if (roll < p1 + p2) note += 12;
        while (note > 96) note -= 12;  // Clamp
    }

    // Trigger note if one was generated
    if (triggered && note >= 0 && onBassTrigger) {
        // Release previous note first
        if (bassState.currentNote >= 0) {
            onBassTrigger(bassState.currentNote, 70, false);
        }

        // Trigger the new note
        onBassTrigger(note, velocity, true);

        // Update state
        bassState.currentNote = note;
        bassState.gateStepsRemaining = gateLength;
    }
}

void Sequencer::RandomizeBassVoice()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Release any held note
    if (bassState.currentNote >= 0 && onBassTrigger) {
        onBassTrigger(bassState.currentNote, 70, false);
    }

    // Reset state first (Init sets currentPattern=0), then randomize after
    bassState.Init();

    // Randomize octave offset (-2 to 0, bass register)
    bassVoice.octaveOffset = ((seed >> 4) % 3) - 2;

    // Randomize rhythm AB variation independently
    bassVoice.rhythmVariation.mode = ((seed >> 6) % 2 == 0) ? VAR_MODE_OFF : VAR_MODE_AB;
    bassVoice.rhythmVariation.sequence = (VariationSequence)((seed >> 8) % 4);

    // Randomize pitch AB variation independently
    uint32_t pitchVarSeed = seed ^ 0xDEADBEEF;
    bassVoice.pitchVariation.mode = ((pitchVarSeed >> 6) % 2 == 0) ? VAR_MODE_OFF : VAR_MODE_AB;
    bassVoice.pitchVariation.sequence = (VariationSequence)((pitchVarSeed >> 8) % 4);

    // Randomize fill pattern index
    bassState.fillPatternIndex = (seed >> 12) % NUM_BASS_FILLS;

    // Randomize pattern (after Init so it doesn't get overwritten)
    RandomizeBassPattern(bassState, bassVoice, seed);
}

// ============================================================================
// CHORD RANDOMIZATION (VIBE SYSTEM)
// ============================================================================

VibeType Sequencer::SelectRandomEnabledVibe(uint32_t seed)
{
    // Count enabled vibes
    uint8_t enabledCount = 0;
    uint8_t enabledVibesArr[NUM_VIBE_TYPES];

    for (int v = 0; v < NUM_VIBE_TYPES; v++) {
        if (chordRandomizer.enabledVibes & (1 << v)) {
            enabledVibesArr[enabledCount++] = v;
        }
    }

    if (enabledCount == 0) {
        return VIBE_MINOR;  // Fallback
    }

    return (VibeType)enabledVibesArr[seed % enabledCount];
}

void Sequencer::SelectProgressionFromVibe(VibeType vibe, uint32_t seed)
{
    // Get all progressions for this vibe
    uint8_t indices[32];
    uint8_t count = GetProgressionsForVibe(vibe, indices, 32);

    if (count == 0) {
        return;  // No change
    }

    // Filter by enabled progressions
    uint8_t enabledIndices[32];
    uint8_t enabledCount = 0;

    for (int i = 0; i < count; i++) {
        uint8_t progIdx = indices[i];
        // Find position of this progression within the vibe
        uint8_t vibePos = 0;
        for (uint8_t j = 0; j < progIdx; j++) {
            if (progressions[j].vibe == vibe) {
                vibePos++;
            }
        }
        if (chordRandomizer.enabledProgressions[vibe] & (1 << vibePos)) {
            enabledIndices[enabledCount++] = progIdx;
        }
    }

    if (enabledCount == 0) {
        // All disabled, use first in vibe
        chordVoice.progressionIndex = indices[0];
    } else {
        chordVoice.progressionIndex = enabledIndices[seed % enabledCount];
    }
}

void Sequencer::SelectSteadyChordFromVibe(VibeType vibe, uint32_t seed)
{
    chordVoice.progressionIndex = GetSteadyChordForVibe(vibe, seed);
}

void Sequencer::TransitionToVibe(VibeType newVibe, uint32_t seed)
{
    VibeType oldVibe = chordRandomizerState.currentVibe;

    // Calculate root shift
    int8_t rootShift = CalculateVibeRootShift(oldVibe, newVibe, seed);
    melodyRoot = (melodyRoot + rootShift + 12) % 12;

    // Update vibe
    chordRandomizerState.currentVibe = newVibe;

    // Select a steady chord for transition
    SelectSteadyChordFromVibe(newVibe, seed);

    // Set transition state - calculate how many cycles of steady chord = ~8 bars
    chordRandomizerState.inTransition = true;
    // Steady chords have length 1, so cycles needed = 8 bars / (bars per cycle)
    // Bars per cycle = chordRateSteps[rate] / 32
    uint8_t barsPerCycle = chordRateSteps[chordVoice.chordRate] / 32;
    if (barsPerCycle == 0) barsPerCycle = 1;  // Minimum 1 for quarter-bar rate
    chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
    if (chordRandomizerState.transitionBarsRemaining < 2) {
        chordRandomizerState.transitionBarsRemaining = 2;  // Minimum 2 cycles
    }

    // Timer will be reset after transition completes
    chordRandomizerState.changeTimer = 0;
}

void Sequencer::NotifyRhythmOfChordCycle()
{
    // Called when chord progression loops back to start
    // Use this to trigger rhythm personality changes at musically meaningful moments

    // Randomize bass pattern every 2-4 chord cycles (respect freeze)
    if (bassVoice.active && !bassVoice.freezePattern) {
        if (bassState.chordCyclesUntilChange == 0) {
            uint32_t bassSeed = g_platform ? g_platform->GetRandomSeed() : 12345;
            RandomizeBassPattern(bassState, bassVoice, bassSeed);
            // Wait 2-4 chord cycles before next change
            bassState.chordCyclesUntilChange = 2 + (bassSeed % 3);
        } else {
            bassState.chordCyclesUntilChange--;
        }
    }

    // Skip if not in morph mode or frozen
    if (rhythmVoice.mode != RHYTHM_MODE_MORPH || rhythmVoice.freezeStyle) {
        return;
    }

    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Check if it's time for a style morph (use morphTimer as a "cycles since last change" counter)
    // Style changes less often - roughly every 2-4 chord cycles
    if (rhythmState.morphTimer == 0 || rhythmState.morphTimer <= 16) {
        // Pick a new target style (different from current)
        RhythmPlayStyle newStyle = (RhythmPlayStyle)(seed % NUM_RHYTHM_PLAY_STYLES);
        if (newStyle == rhythmState.currentStyle) {
            newStyle = (RhythmPlayStyle)((newStyle + 1) % NUM_RHYTHM_PLAY_STYLES);
        }

        rhythmState.targetStyle = newStyle;
        rhythmState.styleMorphProgress = 0.0f;

        // Reset morph timer to wait 2-4 chord cycles before next style change
        // Each chord cycle is roughly 4-8 bars depending on progression length and rate
        rhythmState.morphTimer = 128 + ((seed >> 8) % 128);  // 4-8 bars worth of steps
    }

    // Check if it's time for parameter randomization
    // This happens more frequently - roughly every 1-3 chord cycles
    if (rhythmState.randomizeTimer == 0 || rhythmState.randomizeTimer <= 32) {
        RandomizeRhythmParams(rhythmVoice, rhythmState, seed ^ 0xFEDCBA98);
    }
}

void Sequencer::ProcessChordRandomization()
{
    // Don't process if frozen
    if (chordRandomizer.freezeEnabled) {
        return;
    }

    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Check if current vibe is still enabled - if not, plan a transition to an enabled vibe
    bool currentVibeEnabled = chordRandomizer.enabledVibes & (1 << chordRandomizerState.currentVibe);
    if (!currentVibeEnabled && !chordRandomizerState.inTransition) {
        // Start transition: play steady chord from CURRENT vibe while we prepare to switch
        SelectSteadyChordFromVibe(chordRandomizerState.currentVibe, seed);
        chordRandomizerState.inTransition = true;

        // Calculate transition duration
        uint8_t barsPerCycle = chordRateSteps[chordVoice.chordRate] / 32;
        if (barsPerCycle == 0) barsPerCycle = 1;
        chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
        if (chordRandomizerState.transitionBarsRemaining < 2) {
            chordRandomizerState.transitionBarsRemaining = 2;
        }
        chordRandomizerState.changeTimer = 0;
        return;
    }

    // Handle ongoing transition (steady chord playing)
    if (chordRandomizerState.inTransition) {
        chordRandomizerState.transitionBarsRemaining--;

        if (chordRandomizerState.transitionBarsRemaining <= 0) {
            // Transition complete
            chordRandomizerState.inTransition = false;

            // Check if we need to switch vibes (current vibe was disabled)
            bool currentVibeStillEnabled = chordRandomizer.enabledVibes & (1 << chordRandomizerState.currentVibe);
            if (!currentVibeStillEnabled) {
                // Switch to an enabled vibe now
                VibeType newVibe = SelectRandomEnabledVibe(seed);
                int8_t rootShift = CalculateVibeRootShift(chordRandomizerState.currentVibe, newVibe, seed);
                melodyRoot = (melodyRoot + rootShift + 12) % 12;
                chordRandomizerState.currentVibe = newVibe;
            }

            // Pick a real progression from the (possibly new) current vibe
            SelectProgressionFromVibe(chordRandomizerState.currentVibe, seed);

            // Set timer: wait 2-4 progression cycles before next change
            chordRandomizerState.changeTimer = 2 + (seed % 3);
        }
        return;
    }

    // Decrement change timer (counts progression cycles)
    if (chordRandomizerState.changeTimer > 0) {
        chordRandomizerState.changeTimer--;
        return;  // Timer not expired yet
    }

    // Timer expired - consider changing

    // Count enabled vibes
    uint8_t enabledVibeCount = 0;
    for (int v = 0; v < NUM_VIBE_TYPES; v++) {
        if (chordRandomizer.enabledVibes & (1 << v)) {
            enabledVibeCount++;
        }
    }

    // Decide: change vibe? (20% probability if multiple vibes enabled)
    bool changeVibe = (enabledVibeCount > 1) && ((seed % 100) < 20);

    if (changeVibe) {
        VibeType newVibe = SelectRandomEnabledVibe(seed);
        if (newVibe != chordRandomizerState.currentVibe) {
            TransitionToVibe(newVibe, seed);
            return;
        }
    }

    // Stay in vibe - maybe change root? (10% chance)
    bool changeRoot = ((seed >> 4) % 100) < 10;
    if (changeRoot) {
        // Shift root up 7 (fifth) or up 1
        int8_t shift = ((seed >> 8) % 2) == 0 ? 7 : 1;
        melodyRoot = (melodyRoot + shift) % 12;

        // Use steady chord for transition
        SelectSteadyChordFromVibe(chordRandomizerState.currentVibe, seed);
        chordRandomizerState.inTransition = true;

        // Calculate transition duration in cycles (similar to TransitionToVibe)
        uint8_t barsPerCycle = chordRateSteps[chordVoice.chordRate] / 32;
        if (barsPerCycle == 0) barsPerCycle = 1;
        chordRandomizerState.transitionBarsRemaining = 8 / barsPerCycle;
        if (chordRandomizerState.transitionBarsRemaining < 2) {
            chordRandomizerState.transitionBarsRemaining = 2;
        }
        chordRandomizerState.changeTimer = 0;  // Will be set after transition
    } else {
        // Just pick new progression from same vibe
        SelectProgressionFromVibe(chordRandomizerState.currentVibe, seed);

        // Reset timer: 2-4 progression cycles
        chordRandomizerState.changeTimer = 2 + ((seed >> 12) % 3);
    }
}

} // namespace themis
