/**
 * @file drums.cpp
 * @brief Drum pattern generation and processing implementation
 */

#include "drums.h"
#include "groove.h"
#include "melody.h"

// ============================================================================
// PATTERN HELPERS
// ============================================================================

// IsStepActive is defined inline in types.h

bool IsStepActive8(uint8_t pattern, uint8_t step)
{
    // Read from left (MSB) since patterns are written MSB-first
    return (pattern >> (7 - step)) & 0x01;
}

bool IsStepActive16(uint16_t pattern, uint8_t step)
{
    // Read from left (MSB) since patterns are written MSB-first
    return (pattern >> (15 - step)) & 0x01;
}

// ============================================================================
// RHYTHM GENERATION
// ============================================================================

uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits randomly (to get ~87.5% density)
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1 << pos); // Remove this hit
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize off-beat positions (odd steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2 + 1; // Force odd positions
            if(pos < length) pattern |= (1 << pos);
        }
    }
    return pattern;
}

uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits randomly (to get ~87.5% density)
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1 << pos); // Remove this hit
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize on-beat positions (even steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2; // Force even positions
            if(pos < length) pattern |= (1 << pos);
        }
    }
    return pattern;
}

uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;
    int hitCount = (density == DENSITY_LOW) ? (length / 8) : (density == DENSITY_MEDIUM) ? (length / 2) : ((length * 7) / 8);

    // Bjorklund's algorithm for Euclidean rhythms works correctly even for high density
    int bucket = 0;
    for(int i = 0; i < length; i++)
    {
        bucket += hitCount;
        if(bucket >= length)
        {
            bucket -= length;
            pattern |= (1 << i);
        }
    }
    return pattern;
}

uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few in clusters
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits in clusters (to get ~87.5% density)
        int removeCount = length / 8;
        int clustersCount = (seed % 2) + 1; // 1-2 silence clusters
        int removesPerCluster = removeCount / clustersCount;
        if(removesPerCluster < 1) removesPerCluster = 1;

        for(int c = 0; c < clustersCount; c++)
        {
            int maxStart = length - removesPerCluster;
            if(maxStart < 0) maxStart = 0;
            int clusterStart = ((seed >> (c * 4)) % (maxStart + 1));
            for(int h = 0; h < removesPerCluster; h++)
            {
                int pos = (clusterStart + h) % length;
                pattern &= ~(1 << pos); // Remove this hit
            }
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);

        // Create clusters of hits
        int clustersCount = (seed % 3) + 2; // 2-4 clusters
        int hitsPerCluster = hitCount / clustersCount;
        if(hitsPerCluster < 1) hitsPerCluster = 1;

        for(int c = 0; c < clustersCount; c++)
        {
            int maxStart = length - hitsPerCluster;
            if(maxStart < 0) maxStart = 0;
            int clusterStart = ((seed >> (c * 4)) % (maxStart + 1));
            for(int h = 0; h < hitsPerCluster; h++)
            {
                int pos = clusterStart + h;
                if(pos < length) pattern |= (1 << pos);
            }
        }
    }
    return pattern;
}

// ============================================================================
// INTERACTION PROCESSING
// ============================================================================

void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Patterns remain as generated
    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2)
{
    uint32_t combinedPattern = voice1->pattern | voice2->pattern;
    voice1->pattern = 0;
    voice2->pattern = 0;

    // Alternate hits between voices
    bool voice1Turn = true;
    for(int i = 0; i < 32; i++)
    {
        if(combinedPattern & (1 << i))
        {
            if(voice1Turn)
                voice1->pattern |= (1 << i);
            else
                voice2->pattern |= (1 << i);
            voice1Turn = !voice1Turn;
        }
    }
    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Bar 1 (steps 0-15): voice1
    // Bar 2 (steps 16-31): voice2
    uint32_t mask1 = 0x0000FFFF; // First 16 bits
    uint32_t mask2 = 0xFFFF0000; // Last 16 bits

    voice1->pattern &= mask1;
    voice2->pattern &= mask2;
    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Half bar 1 (0-7): voice1
    // Half bar 2 (8-15): voice2
    // Half bar 3 (16-23): voice1
    // Half bar 4 (24-31): voice2
    uint32_t mask1 = 0x00FF00FF; // Steps 0-7, 16-23
    uint32_t mask2 = 0xFF00FF00; // Steps 8-15, 24-31

    voice1->pattern &= mask1;
    voice2->pattern &= mask2;
    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // This pattern plays over 2 iterations (4 bars total)
    // Use barCounter to determine which voice is active
    if(barCounter % 2 == 0)
    {
        voice1->active = true;
        voice2->active = false;
    }
    else
    {
        voice1->active = false;
        voice2->active = true;
    }
}

// ============================================================================
// VARIATION SYSTEM
// ============================================================================

uint8_t GetCurrentVariation(const VariationConfig* config, uint8_t step, uint8_t barInCycle)
{
    // If variation is off, always return A (0)
    if(config->mode == VAR_MODE_OFF)
        return 0;

    // Calculate segment based on granularity
    // Each barInCycle represents a 2-bar phrase (32 steps)
    // step is 0-31 within that phrase
    uint8_t segment = 0;

    switch(config->granularity)
    {
        case VAR_GRAN_BAR:
            // 16 steps per segment (1 bar)
            // barInCycle 0-3 (each is 2 bars), step 0-31
            // Total segments in 8-bar cycle: 8
            segment = (barInCycle * 2) + (step / 16);
            break;
        case VAR_GRAN_HALF_BAR:
            // 8 steps per segment (half bar)
            // Total segments in 8-bar cycle: 16, but we use modulo 8
            segment = ((barInCycle * 4) + (step / 8)) % 8;
            break;
        case VAR_GRAN_QUARTER:
            // 4 steps per segment (quarter bar)
            // Total segments in 8-bar cycle: 32, but we use modulo 8
            segment = ((barInCycle * 8) + (step / 4)) % 8;
            break;
        default:
            segment = barInCycle;
            break;
    }

    // Look up variation from sequence table (modulo 8 for safety)
    uint8_t variation = variationSequences[config->sequence][segment % 8];

    // Clamp to valid variation for the mode
    if(config->mode == VAR_MODE_AB && variation > 1)
        variation = 1;  // Clamp C to B for AB mode

    return variation;
}

// ============================================================================
// VOICE CONFIGURATION
// ============================================================================

void RandomizeVoicePersonalities()
{
    uint32_t seed = System::GetUs();

    // Randomly assign fundamental beat role to either CLAP or SNARE
    fundamentalBeatVoice = ((seed % 2) == 0) ? CLAP : SNARE;

    // The other voice becomes a generative voice (at index 3)
    DrumVoice generativeBackbeatVoice = (fundamentalBeatVoice == CLAP) ? SNARE : CLAP;
    generativeVoices[3].voice = generativeBackbeatVoice;

    // Randomize rhythm styles for all voices
    RhythmStyle styles[] = {RHYTHM_SYNCOPATED, RHYTHM_STRAIGHT, RHYTHM_EUCLIDEAN, RHYTHM_ANTI_EUCLIDEAN, RHYTHM_FOLLOW_KICK};
    for(int i = 0; i < 6; i++)
    {
        seed = System::GetUs() ^ (i * 11111);
        generativeVoices[i].rhythmStyle = styles[seed % 5];
    }

    // Reset all interactions to NONE first
    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].interaction = INTERACTION_NONE;
        generativeVoices[i].interactionPartner = generativeVoices[i].voice;
    }

    // Pick alternating pair (50% probability)
    seed = System::GetUs();
    int alternatePair[2] = {-1, -1};
    if((seed % 100) < 50)
    {
        alternatePair[0] = (seed >> 8) % 6;
        alternatePair[1] = ((seed >> 16) % 5);
        if(alternatePair[1] >= alternatePair[0]) alternatePair[1]++; // Ensure different voice

        // Choose which alternate style
        InteractionStyle alternateStyles[] = {INTERACTION_ALTERNATE_BAR, INTERACTION_ALTERNATE_HALF, INTERACTION_ALTERNATE_TWO};
        InteractionStyle altStyle = alternateStyles[(seed >> 20) % 3];

        generativeVoices[alternatePair[0]].interaction = altStyle;
        generativeVoices[alternatePair[0]].interactionPartner = generativeVoices[alternatePair[1]].voice;
        generativeVoices[alternatePair[1]].interaction = altStyle;
        generativeVoices[alternatePair[1]].interactionPartner = generativeVoices[alternatePair[0]].voice;
    }

    // Pick divided pair (50% probability), ensuring no overlap with alternate pair
    seed = System::GetUs();
    if((seed % 100) < 50)
    {
        int voice1 = -1, voice2 = -1;
        int attempts = 0;

        // Find first voice not in alternate pair
        do {
            seed = System::GetUs() ^ (attempts * 7777);
            voice1 = (seed >> 8) % 6;
            attempts++;
        } while((voice1 == alternatePair[0] || voice1 == alternatePair[1]) && attempts < 10);

        // Find second voice not in alternate pair and different from voice1
        attempts = 0;
        do {
            seed = System::GetUs() ^ (attempts * 9999);
            voice2 = (seed >> 8) % 6;
            attempts++;
        } while((voice2 == voice1 || voice2 == alternatePair[0] || voice2 == alternatePair[1]) && attempts < 10);

        // Only apply if we found valid voices
        if(voice1 != -1 && voice2 != -1 && voice1 != voice2)
        {
            generativeVoices[voice1].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice1].interactionPartner = generativeVoices[voice2].voice;
            generativeVoices[voice2].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice2].interactionPartner = generativeVoices[voice1].voice;
        }
    }

    // Randomize densities ensuring at least one of each type
    DensityLevel densities[6];
    densities[0] = DENSITY_LOW;
    densities[1] = DENSITY_MEDIUM;
    densities[2] = DENSITY_HIGH;

    // Randomly fill the remaining 3 slots
    seed = System::GetUs();
    densities[3] = (DensityLevel)((seed >> 8) % 3);
    densities[4] = (DensityLevel)((seed >> 16) % 3);
    densities[5] = (DensityLevel)((seed >> 24) % 3);

    // Shuffle the density array
    for(int i = 5; i > 0; i--)
    {
        seed = System::GetUs();
        int j = seed % (i + 1);
        DensityLevel temp = densities[i];
        densities[i] = densities[j];
        densities[j] = temp;
    }

    // Assign shuffled densities to voices
    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].density = densities[i];
    }
}

// Helper to generate a pattern for a given style and density
static uint32_t GeneratePatternForStyle(uint32_t seed, RhythmStyle style, DensityLevel density, uint8_t length)
{
    switch(style)
    {
        case RHYTHM_SYNCOPATED:
            return GenerateSyncopated(seed, density, length);
        case RHYTHM_STRAIGHT:
            return GenerateStraight(seed, density, length);
        case RHYTHM_EUCLIDEAN:
            return GenerateEuclidean(seed, density, length);
        case RHYTHM_ANTI_EUCLIDEAN:
            return GenerateAntiEuclidean(seed, density, length);
        case RHYTHM_FOLLOW_KICK:
            return kickPatterns[currentKickPattern];
        default:
            return GenerateEuclidean(seed, density, length);
    }
}

void GenerateVoicePatterns()
{
    // Odd lengths for polyrhythms
    const uint8_t oddLengths[] = {12, 13, 15, 17, 18};

    // Generate base patterns for each voice
    for(int i = 0; i < 6; i++)
    {

        VoiceConfig* voice = &generativeVoices[i];
        uint32_t seed = generationSeed ^ (i * 12345); // Unique seed per voice

        // Follow kick always uses 32-step pattern length to match kick patterns
        if(voice->rhythmStyle == RHYTHM_FOLLOW_KICK)
        {
            voice->patternLength = 32;
        }
        // Randomly choose pattern length (15% chance of polyrhythm)
        else if((seed % 100) < 15)
        {
            // Choose one of the odd lengths
            voice->patternLength = oddLengths[(seed >> 8) % 5];
        }
        else
        {
            // Standard 32-step pattern
            voice->patternLength = 32;
        }

        // Generate A pattern based on rhythm style
        voice->pattern = GeneratePatternForStyle(seed, voice->rhythmStyle, voice->density, voice->patternLength);

        // Generate B and C variations if enabled and pattern length is compatible
        // Skip variation generation for polyrhythm lengths (not 4, 8, 16, or 32)
        bool compatibleLength = (voice->patternLength == 4 || voice->patternLength == 8 ||
                                 voice->patternLength == 16 || voice->patternLength == 32);

        if(voice->variation.mode != VAR_MODE_OFF && compatibleLength)
        {
            // Generate B pattern with different seed and potentially different style/density
            uint32_t seedB = seed ^ 0xB5B5B5B5;
            voice->patternB = GeneratePatternForStyle(seedB, voice->variation.styleB,
                                                       voice->variation.densityB,
                                                       voice->patternLength);

            // Generate C pattern if in ABC mode
            if(voice->variation.mode == VAR_MODE_ABC)
            {
                uint32_t seedC = seed ^ 0xC3C3C3C3;
                voice->patternC = GeneratePatternForStyle(seedC, voice->variation.styleC,
                                                           voice->variation.densityC,
                                                           voice->patternLength);
            }
        }
    }

    // Process interactions between voice pairs (dynamically based on current config)
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->interaction == INTERACTION_NONE)
            continue;

        // Find the partner voice
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

        // Process interaction (only once per pair)
        if(i < (partner - generativeVoices)) // Process only if this voice comes first in array
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
                    ProcessInteractionAlternateTwo(voice, partner);
                    break;
                default:
                    break;
            }
        }
    }
}

void RandomizePatterns()
{
    // Use System::GetUs() for pseudo-random seed
    uint32_t seed = System::GetUs();

    // Random pattern selection (0-15)
    currentKickPattern = (seed % 16);
    currentClapPattern = ((seed >> 4) % 16);
    currentHatPattern = ((seed >> 8) % 16);

    // Update generation seed for generative voices
    generationSeed = seed;

    // Generate new patterns for generative voices
    GenerateVoicePatterns();
}

// ============================================================================
// FILL SYSTEM
// ============================================================================

void ScheduleFill()
{
    // Randomly decide: 30% chance of fill
    uint32_t seed = System::GetUs();
    if((seed % 100) < 30) // 30% chance
    {
        fillActive = true;

        // Randomly choose half-bar or whole-bar fill (50/50)
        fillIsHalfBar = ((seed >> 8) % 2) == 0;

        // Calculate when fill should start
        // 8 bars = 128 steps total
        // Whole-bar fill: starts at step 112 (last 16 steps)
        // Half-bar fill: starts at step 120 (last 8 steps)
        fillStartStep = fillIsHalfBar ? 120 : 112;

        // Randomly select fill patterns
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
// MAIN PROCESSING
// ============================================================================

void ProcessDrumPatterns()
{
    if(!isRunning)
        return;

    // Calculate when the NEXT beat will occur (look-ahead scheduling)
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    uint64_t nextBeatSample = lastBeatSample + (uint64_t)samplesPerSixteenth;

    // Calculate total step within 8-bar cycle (0-127)
    uint8_t totalStep = (barCounter * 32) + currentStep;

    // Randomize groove at start of new 8-bar cycle (25% probability)
    if(barCounter == 0 && currentStep == 0)
    {
        uint32_t seed = System::GetUs();
        if((seed % 100) < 25) // 25% chance
        {
            RandomizeGroove();
        }
    }

    // Schedule fill at the start of the 7th-8th bar (barCounter == 3, step == 0)
    if(barCounter == 3 && currentStep == 0)
    {
        ScheduleFill();
    }

    // Check if we're in fill mode
    bool inFill = fillActive && (totalStep >= fillStartStep);

    if(inFill)
    {
        // Calculate step within fill (0-7 for half-bar, 0-15 for whole-bar)
        uint8_t fillStep = totalStep - fillStartStep;

        // Trigger fills
        if(fillIsHalfBar)
        {
            // Half-bar fills (8 steps)
            if(IsStepActive8(kickFillsHalf[currentFillSnareIndex % 8], fillStep))
                ScheduleDrumTriggerWithGroove(KICK, 120, currentStep, nextBeatSample); // Louder for fills

            if(IsStepActive8(snareFillsHalf[currentFillSnareIndex], fillStep))
                ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 115, currentStep, nextBeatSample);

            if(IsStepActive8(hatClosedFillsHalf[currentFillHatClosedIndex], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 100, currentStep, nextBeatSample);

            if(IsStepActive8(hatOpenFillsHalf[currentFillHatOpenIndex], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 110, currentStep, nextBeatSample);
        }
        else
        {
            // Whole-bar fills (16 steps)
            if(IsStepActive16(kickFillsWhole[currentFillSnareIndex % 8], fillStep))
                ScheduleDrumTriggerWithGroove(KICK, 120, currentStep, nextBeatSample); // Louder for fills

            if(IsStepActive16(snareFillsWhole[currentFillSnareIndex], fillStep))
                ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 115, currentStep, nextBeatSample);

            if(IsStepActive16(hatClosedFillsWhole[currentFillHatClosedIndex], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 100, currentStep, nextBeatSample);

            if(IsStepActive16(hatOpenFillsWhole[currentFillHatOpenIndex], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 110, currentStep, nextBeatSample);
        }
    }
    else
    {
        // Normal patterns
        if(IsStepActive(kickPatterns[currentKickPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(KICK, 110, currentStep, nextBeatSample); // Kick slightly louder
        }

        if(IsStepActive(clapPatterns[currentClapPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 100, currentStep, nextBeatSample);
        }

        // Check both closed and open hi-hat patterns
        if(IsStepActive(hatClosedPatterns[currentHatPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 70, currentStep, nextBeatSample); // Closed hi-hat subtle
        }

        if(IsStepActive(hatOpenPatterns[currentHatPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 100, currentStep, nextBeatSample); // Open hi-hat emphasized
        }
    }

    // Process generative voices (with polyrhythm support and AB/ABC variations)
    // Note: ANALOG voice is now handled by melody system, skip it in generative voices
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->active && voice->voice != ANALOG)
        {
            // Get which variation to use (0=A, 1=B, 2=C)
            uint8_t var = GetCurrentVariation(&voice->variation, currentStep, barCounter);

            // Select the appropriate pattern based on variation
            uint32_t activePattern;
            if(var == 1)
                activePattern = voice->patternB;
            else if(var == 2)
                activePattern = voice->patternC;
            else
                activePattern = voice->pattern;

            // Use modulo to loop shorter patterns (polyrhythms)
            uint8_t voiceStep = currentStep % voice->patternLength;
            if(IsStepActive(activePattern, voiceStep))
            {
                // Regular MIDI voice
                ScheduleDrumTriggerWithGroove(voice->voice, 95, currentStep, nextBeatSample);
            }
        }
    }

    // Tune mode: output middle C quarter notes on both melody channels
    if(tuneModeEnabled)
    {
        // Quarter notes = every 4 steps (steps 0, 4, 8, 12, 16, 20, 24, 28)
        bool isQuarterNote = (currentStep % 4) == 0;

        if(isQuarterNote)
        {
            // CV output: Middle C = C3 = 1V (semitone 12 from C2)
            const int8_t middleC_semitone = 12;
            float cvVoltage = MelodyNoteToCV(middleC_semitone); // 1.0V
            uint16_t cv1 = (uint16_t)(cvVoltage / 5.0f * 4095.0f);
            hw.seed.dac.WriteValue(DacHandle::Channel::ONE, cv1);

            // Trigger CV gate
            analogGateHigh = true;
            analogGateCounter = 0;

            // MIDI output: Middle C = MIDI note 60
            const uint8_t middleC_midi = 60;

            // Send note-off for previous note if playing
            if(midiMelodyNoteOn)
            {
                uint8_t noteOff[3] = {
                    static_cast<uint8_t>(0x80 | melodyMidiChannel),
                    lastMidiMelodyNote,
                    0
                };
                hw.midi.SendMessage(noteOff, 3);
            }

            // Send note-on for middle C
            uint8_t noteOn[3] = {
                static_cast<uint8_t>(0x90 | melodyMidiChannel),
                middleC_midi,
                100
            };
            hw.midi.SendMessage(noteOn, 3);

            lastMidiMelodyNote = middleC_midi;
            midiMelodyNoteOn = true;
        }
    }
    else
    {
        // Normal melody processing with groove timing (only when tune mode is off)

        // Schedule CV melody voice with groove timing and AB/ABC variations
        if(melodyVoice.active)
        {
            // Get which variation to use
            uint8_t var = GetCurrentVariation(&melodyVoice.variation, currentStep, barCounter);

            // Select rhythm pattern based on variation
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
            if(IsStepActive(activeRhythm, melodyStep))
            {
                int8_t note = activeNotes[melodyStep];
                ScheduleMelodyTrigger(MELODY_CV, note, currentStep, nextBeatSample);
            }
        }

        // Schedule MIDI melody voice with groove timing and AB/ABC variations
        if(melodyMidiVoice.active)
        {
            // Get which variation to use
            uint8_t var = GetCurrentVariation(&melodyMidiVoice.variation, currentStep, barCounter);

            // Select rhythm pattern based on variation
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
            if(IsStepActive(activeRhythm, midiMelStep))
            {
                int8_t note = activeNotes[midiMelStep];
                ScheduleMelodyTrigger(MELODY_MIDI, note, currentStep, nextBeatSample);
            }
        }
    }

    // Advance step
    currentStep++;
    if(currentStep >= 32) // Now 32 steps = 2 bars
    {
        currentStep = 0;
        barCounter++; // Counts 2-bar phrases

        // Auto-randomize patterns after interval (every 8 bars)
        if(barCounter >= patternChangeInterval)
        {
            if(!freezeEnabled)
            {
                RandomizePatterns();
            }

            // Melody has independent freeze control
            if(!melodyFreezeEnabled)
            {
                SendMelodyNoteOff();  // Clean note-off before new pattern
                GenerateMelodyPattern();
            }

            barCounter = 0;
            fillActive = false; // Reset fill for next cycle
            cycleCounter++; // Count 8-bar cycles

            // Randomize voice personalities after longer interval (every 32 bars)
            if(cycleCounter >= personalityChangeInterval)
            {
                if(!freezeEnabled)
                {
                    RandomizeVoicePersonalities();
                }

                // Melody personality also changes at this interval (if not frozen)
                if(!melodyFreezeEnabled)
                {
                    SendMelodyNoteOff();  // Clean note-off before personality change
                    RandomizeMelodyPersonality();
                }

                cycleCounter = 0;
            }
        }
    }
}
