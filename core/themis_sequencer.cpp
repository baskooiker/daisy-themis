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
#include "themis_acid.h"

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
    melodyVoice.compatMode = COMPAT_CHORD_SCALE;  // Default: most melodic freedom

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
    melodyMidiVoice.compatMode = COMPAT_CHORD_SCALE;  // Default: most melodic freedom

    melodyScale = SCALE_MINOR;
    melodyRoot = 0;

    // Initialize poly voice
    polyVoice.active = true;  // Active by default
    polyVoice.progressionIndex = 0;
    polyVoice.chordRate = CHORD_RATE_1_BAR;
    polyVoice.velocity = 80;
    polyVoice.octaveOffset = 0;
    polyVoice.progressionB = 1;
    polyVoice.variationMode = VAR_MODE_OFF;
    polyState.Init();

    // Initialize chord randomizer
    chordRandomizer.Init();
    chordRandomizerState.Init();

    // Initialize rhythm player voice
    rhythmVoice.Init();
    rhythmState.Init();

    // Initialize acid voice
    acidVoice.Init();
    acidState.Init();

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

    // Reset poly voice state so chord progression starts from beginning
    polyState.currentChordIndex = 0;
    polyState.stepsUntilChange = 0;  // Will be initialized on first step
    polyState.notesOn = false;
    polyState.numActiveNotes = 0;

    // Reset rhythm player state
    rhythmState.barPosition = 0;
    rhythmState.patternPosition = 0;

    // Reset acid voice state
    acidState.stepPosition = 0;

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
    // Release any held poly chord notes
    ReleasePolyChord();

    // Release any held rhythm player notes
    if(rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
        onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
        rhythmState.numActiveNotes = 0;
        rhythmState.noteDuration = 0;
    }

    // Release any held acid voice note
    if(acidState.currentNote >= 0 && onAcidTrigger) {
        onAcidTrigger(acidState.currentNote, 64, false, false);
        acidState.currentNote = -1;
        acidState.gateStepsRemaining = 0;
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
    ReleasePolyChord();

    // Release rhythm player notes
    if(rhythmState.numActiveNotes > 0 && onRhythmTrigger) {
        onRhythmTrigger(rhythmState.activeNotes, rhythmState.numActiveNotes, 0, false);
        rhythmState.numActiveNotes = 0;
    }

    // Release acid voice note
    if(acidState.currentNote >= 0 && onAcidTrigger) {
        onAcidTrigger(acidState.currentNote, 64, false, false);
        acidState.currentNote = -1;
    }

    // Randomize personalities FIRST so patterns are generated with correct interactions
    RandomizeVoicePersonalities();
    RandomizePatterns();
    RandomizeGroove();
    RandomizeMelodyPersonality();
    RandomizePolyVoice();
    RandomizeRhythmVoice();
    RandomizeAcidVoice();
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
    // Get chord context once (reused for both voices if poly is active)
    ChordContext chordCtx;
    bool useChordMapping = polyVoice.active;
    if (useChordMapping) {
        chordCtx = GetCurrentChordContext();
    }

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

            // Apply chord-aware mapping when poly voice is active
            if (useChordMapping) {
                note = MapNoteToChord(note, chordCtx, melodyVoice.compatMode);
            }

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

            // Apply chord-aware mapping when poly voice is active
            if (useChordMapping) {
                note = MapNoteToChord(note, chordCtx, melodyMidiVoice.compatMode);
            }

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
    ProcessPolyVoice();
    ProcessRhythmVoice();
    ProcessAcidVoice();

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

// ============================================================================
// POLY VOICE
// ============================================================================

void Sequencer::ProcessPolyVoice()
{
    // If voice was deactivated while notes are playing, release them
    if (!polyVoice.active) {
        if (polyState.notesOn) {
            ReleasePolyChord();
        }
        return;
    }

    uint8_t stepsPerChord = chordRateSteps[polyVoice.chordRate];

    // Initialize on first step
    if (polyState.stepsUntilChange == 0 && !polyState.notesOn) {
        polyState.stepsUntilChange = stepsPerChord;
        TriggerPolyChord();
        return;
    }

    // Check if we need to release notes (1 step before chord change)
    if (polyState.notesOn && polyState.stepsUntilChange == 1) {
        ReleasePolyChord();
    }

    // Decrement countdown
    polyState.stepsUntilChange--;

    // Check if it's time for a new chord
    if (polyState.stepsUntilChange == 0) {
        // Advance to next chord in progression
        uint8_t progIndex = polyVoice.progressionIndex;

        // Handle variation
        if (polyVoice.variationMode != VAR_MODE_OFF) {
            // Simple A/B switching based on bar
            if ((barCounter % 2) == 1) {
                progIndex = polyVoice.progressionB;
            }
        }

        uint8_t nextChordIndex = (polyState.currentChordIndex + 1) %
                                  progressions[progIndex].length;

        // If we've wrapped around to chord 0, process chord randomization
        if (nextChordIndex == 0) {
            // First apply any pending manual progression change
            if (polyState.pendingProgressionIndex >= 0) {
                polyVoice.progressionIndex = (uint8_t)polyState.pendingProgressionIndex;
                polyState.pendingProgressionIndex = -1;  // Clear pending
                progIndex = polyVoice.progressionIndex;
            } else {
                // Otherwise, process automatic randomization
                ProcessChordRandomization();
                progIndex = polyVoice.progressionIndex;
            }

            // Notify rhythm voice of chord progression cycle
            NotifyRhythmOfChordCycle();
        }

        polyState.currentChordIndex = nextChordIndex;

        // Trigger new chord
        TriggerPolyChord();

        // Reset countdown
        polyState.stepsUntilChange = stepsPerChord;
    }
}

void Sequencer::TriggerPolyChord()
{
    // First release any currently held notes
    if (polyState.notesOn) {
        ReleasePolyChord();
    }

    // Get the correct progression based on variation
    uint8_t progIndex = polyVoice.progressionIndex;
    if (polyVoice.variationMode != VAR_MODE_OFF && (barCounter % 2) == 1) {
        progIndex = polyVoice.progressionB;
    }

    // Get the chord notes
    int8_t notes[6];
    uint8_t count = GetChordNotes(
        &progressions[progIndex],
        polyState.currentChordIndex,
        melodyRoot,
        melodyScale,
        polyVoice.octaveOffset,
        notes
    );

    // Store active notes for later release
    polyState.numActiveNotes = count;
    for (int i = 0; i < count && i < 6; i++) {
        polyState.activeNotes[i] = notes[i];
    }
    polyState.notesOn = true;

    // Trigger callback
    if (onPolyTrigger) {
        onPolyTrigger(notes, count, true);  // noteOn = true
    }
}

void Sequencer::ReleasePolyChord()
{
    if (!polyState.notesOn) return;

    // Send note-offs for all active notes
    if (onPolyTrigger) {
        onPolyTrigger(polyState.activeNotes, polyState.numActiveNotes, false);
    }

    polyState.notesOn = false;
    polyState.numActiveNotes = 0;
}

ChordContext Sequencer::GetCurrentChordContext() const
{
    ChordContext ctx;

    // Default to global root if poly voice is inactive
    if (!polyVoice.active || polyState.numActiveNotes == 0) {
        ctx.chordRoot = melodyRoot;
        ctx.chordType = CHORD_MINOR;  // Default assumption for minor scales
        ctx.isDiatonic = true;
        return ctx;
    }

    // Get the current progression based on variation
    uint8_t progIndex = polyVoice.progressionIndex;
    if (polyVoice.variationMode != VAR_MODE_OFF && (barCounter % 2) == 1) {
        progIndex = polyVoice.progressionB;
    }

    const ChordProgression& prog = progressions[progIndex];
    const ProgressionStep& step = prog.steps[polyState.currentChordIndex];

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

void Sequencer::RandomizePolyVoice()
{
    // IMPORTANT: Release any currently playing notes before randomizing
    // to prevent hanging notes
    ReleasePolyChord();

    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Select a random enabled vibe first
    VibeType vibe = SelectRandomEnabledVibe(seed);
    chordRandomizerState.currentVibe = vibe;

    // Select a progression from that vibe (respecting enabled progressions)
    SelectProgressionFromVibe(vibe, seed);

    // Randomize chord rate (favor 1 bar and half bar)
    int rateRoll = (seed >> 8) % 100;
    if (rateRoll < 20)
        polyVoice.chordRate = CHORD_RATE_2_BARS;
    else if (rateRoll < 60)
        polyVoice.chordRate = CHORD_RATE_1_BAR;
    else if (rateRoll < 90)
        polyVoice.chordRate = CHORD_RATE_HALF_BAR;
    else
        polyVoice.chordRate = CHORD_RATE_QUARTER;

    // Randomize octave offset (-1 to +1)
    polyVoice.octaveOffset = ((seed >> 16) % 3) - 1;

    // Randomize B progression (different from A, same vibe)
    uint8_t vibeIndices[32];
    uint8_t vibeCount = GetProgressionsForVibe(vibe, vibeIndices, 32);
    if (vibeCount > 1) {
        uint8_t bIdx = (seed >> 20) % vibeCount;
        if (vibeIndices[bIdx] == polyVoice.progressionIndex && vibeCount > 1) {
            bIdx = (bIdx + 1) % vibeCount;
        }
        polyVoice.progressionB = vibeIndices[bIdx];
    } else {
        polyVoice.progressionB = polyVoice.progressionIndex;
    }

    // Randomize variation mode (50% off, 50% AB)
    polyVoice.variationMode = ((seed >> 24) % 2) == 0 ? VAR_MODE_OFF : VAR_MODE_AB;

    // Randomize velocity (70-110)
    polyVoice.velocity = 70 + ((seed >> 28) % 41);

    // Reset state for new progression
    polyState.Init();

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
        rhythmVoice.arpDirection = ARP_UP;
        rhythmVoice.followKick = ((seed >> 12) & 0x01) == 0;
    }

    // Reset state
    rhythmState.Init();

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
// ACID VOICE
// ============================================================================

void Sequencer::ProcessAcidVoice()
{
    if (!acidVoice.active) {
        // Release any held note when deactivated
        if (acidState.currentNote >= 0 && onAcidTrigger) {
            onAcidTrigger(acidState.currentNote, 64, false, false);
            acidState.currentNote = -1;
            acidState.gateStepsRemaining = 0;
        }
        return;
    }

    // Get chord context
    ChordContext chordCtx = GetCurrentChordContext();

    // Get random seed for this step
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : (currentStep * 67890);
    seed ^= (uint32_t)currentStep << 12;
    seed ^= (uint32_t)barCounter << 20;

    // Handle note-off for previous note (unless slide is active)
    if (acidState.gateStepsRemaining == 1 && acidState.currentNote >= 0 && !acidState.slideActive) {
        if (onAcidTrigger) {
            onAcidTrigger(acidState.currentNote, 64, false, false);
        }
        acidState.currentNote = -1;
    }

    // Decrement gate counter
    if (acidState.gateStepsRemaining > 0) {
        acidState.gateStepsRemaining--;
    }

    // Check auto mode pattern randomization
    if (acidVoice.mode == ACID_MODE_AUTO) {
        RandomizeAcidPatterns(acidState, acidVoice, seed ^ 0x12345678);
    }

    // Process acid step
    int8_t note;
    uint8_t velocity;
    bool slide;
    uint8_t gateLength;

    bool triggered = ProcessAcidStep(
        acidVoice,
        acidState,
        chordCtx,
        melodyRoot,
        currentStep,
        seed,
        note,
        velocity,
        slide,
        gateLength
    );

    // Trigger note if one was generated
    if (triggered && note >= 0 && onAcidTrigger) {
        // For slides: don't release previous note before triggering new one
        // The overlapping notes trigger the 303's slide behavior
        bool isSlideNote = acidState.slideActive && acidState.previousNote >= 0;

        // If NOT a slide, release previous note first
        if (!isSlideNote && acidState.currentNote >= 0) {
            onAcidTrigger(acidState.currentNote, 64, false, false);
        }

        // Trigger the new note
        onAcidTrigger(note, velocity, true, isSlideNote);

        // Update state
        acidState.currentNote = note;
        acidState.gateStepsRemaining = gateLength;

        // If this note should slide, we'll keep it active until the next note
        acidState.slideActive = slide;
    }
}

void Sequencer::RandomizeAcidVoice()
{
    uint32_t seed = g_platform ? g_platform->GetRandomSeed() : 12345;

    // Release any held note
    if (acidState.currentNote >= 0 && onAcidTrigger) {
        onAcidTrigger(acidState.currentNote, 64, false, false);
    }

    // Randomize mode (40% manual, 60% auto)
    acidVoice.mode = ((seed % 100) < 40) ? ACID_MODE_MANUAL : ACID_MODE_AUTO;

    // Randomize octave offset (-1 to 0, bass register)
    acidVoice.octaveOffset = ((seed >> 4) % 2) - 1;

    // Randomize pattern selection
    acidVoice.rhythmPattern = (seed >> 8) % NUM_ACID_RHYTHM_PATTERNS;
    acidVoice.melodyPattern = (seed >> 12) % NUM_ACID_MELODY_PATTERNS;

    // Randomize activity (favor moderate)
    int actRoll = (seed >> 16) % 100;
    if (actRoll < 25)
        acidVoice.activity = ACID_ACTIVITY_SPARSE;
    else if (actRoll < 75)
        acidVoice.activity = ACID_ACTIVITY_MODERATE;
    else
        acidVoice.activity = ACID_ACTIVITY_BUSY;

    // Randomize probabilities based on activity
    switch (acidVoice.activity) {
        case ACID_ACTIVITY_SPARSE:
            acidVoice.triggerProb = 70 + ((seed >> 20) % 20);   // 70-90%
            acidVoice.accentProb = 10 + ((seed >> 22) % 15);    // 10-25%
            acidVoice.slideProb = 15 + ((seed >> 24) % 20);     // 15-35%
            acidVoice.octaveUpProb = 5 + ((seed >> 26) % 10);   // 5-15%
            acidVoice.octaveDownProb = 5 + ((seed >> 28) % 10); // 5-15%
            acidVoice.fillProb = 20 + ((seed >> 30) % 20);      // 20-40%
            break;
        case ACID_ACTIVITY_BUSY:
            acidVoice.triggerProb = 95 + ((seed >> 20) % 6);    // 95-100%
            acidVoice.accentProb = 25 + ((seed >> 22) % 25);    // 25-50%
            acidVoice.slideProb = 35 + ((seed >> 24) % 30);     // 35-65%
            acidVoice.octaveUpProb = 20 + ((seed >> 26) % 15);  // 20-35%
            acidVoice.octaveDownProb = 10 + ((seed >> 28) % 15);// 10-25%
            acidVoice.fillProb = 50 + ((seed >> 30) % 30);      // 50-80%
            break;
        default:  // MODERATE
            acidVoice.triggerProb = 85 + ((seed >> 20) % 15);   // 85-100%
            acidVoice.accentProb = 15 + ((seed >> 22) % 20);    // 15-35%
            acidVoice.slideProb = 25 + ((seed >> 24) % 25);     // 25-50%
            acidVoice.octaveUpProb = 10 + ((seed >> 26) % 15);  // 10-25%
            acidVoice.octaveDownProb = 8 + ((seed >> 28) % 12); // 8-20%
            acidVoice.fillProb = 35 + ((seed >> 30) % 25);      // 35-60%
            break;
    }

    // Reset state
    acidState.Init();
    acidState.currentRhythmPattern = acidVoice.rhythmPattern;
    acidState.currentMelodyPattern = acidVoice.melodyPattern;
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
        polyVoice.progressionIndex = indices[0];
    } else {
        polyVoice.progressionIndex = enabledIndices[seed % enabledCount];
    }
}

void Sequencer::SelectSteadyChordFromVibe(VibeType vibe, uint32_t seed)
{
    polyVoice.progressionIndex = GetSteadyChordForVibe(vibe, seed);
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
    uint8_t barsPerCycle = chordRateSteps[polyVoice.chordRate] / 32;
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
        uint8_t barsPerCycle = chordRateSteps[polyVoice.chordRate] / 32;
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
        uint8_t barsPerCycle = chordRateSteps[polyVoice.chordRate] / 32;
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
