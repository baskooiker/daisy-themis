/**
 * @file themis_melody.cpp
 * @brief Melody generation algorithms implementation
 */

#include "themis_melody.h"
#include "themis_data.h"
#include "themis_patterns.h"

namespace themis {

// ============================================================================
// SCALE UTILITIES
// ============================================================================

int8_t GetScaleNote(ScaleType scale, uint8_t root, int8_t degree)
{
    const int8_t* scaleNotes;
    int8_t scaleLen;

    switch(scale)
    {
        case SCALE_MINOR:
            scaleNotes = scaleMinor;
            scaleLen = 7;
            break;
        case SCALE_MINOR_BLUES:
            scaleNotes = scaleMinorBlues;
            scaleLen = 6;
            break;
        case SCALE_MINOR_PENTATONIC:
            scaleNotes = scaleMinorPentatonic;
            scaleLen = 5;
            break;
        case SCALE_GYPSY:
            scaleNotes = scaleGypsy;
            scaleLen = 7;
            break;
        case SCALE_WHOLE_HALF:
            scaleNotes = scaleWholeHalf;
            scaleLen = 8;
            break;
        case SCALE_HALF_WHOLE:
            scaleNotes = scaleHalfWhole;
            scaleLen = 8;
            break;
        case SCALE_WHOLE_TONE:
            scaleNotes = scaleWholeTone;
            scaleLen = 6;
            break;
        default:
            scaleNotes = scaleMinor;
            scaleLen = 7;
    }

    // Handle negative degrees and octave wrapping
    int8_t octave = 0;
    while(degree < 0)
    {
        degree += scaleLen;
        octave--;
    }
    octave += degree / scaleLen;
    degree = degree % scaleLen;

    // Calculate final semitone (relative to C2 = 0V)
    return root + scaleNotes[degree] + (octave * 12);
}

float MelodyNoteToCV(int8_t semitone)
{
    // 0V = C2 (semitone 0), 3V = C5 (semitone 36)
    // 1V/octave = 1V per 12 semitones
    float voltage = (float)semitone / 12.0f;
    // Clamp to 0-3V range (3 octaves)
    if(voltage < 0.0f) voltage = 0.0f;
    if(voltage > 3.0f) voltage = 3.0f;
    return voltage;
}

// ============================================================================
// RHYTHM GENERATION
// ============================================================================

uint32_t GenerateMelodyRhythmWithParams(uint32_t seed, MelodyConfig* voice,
                                         RhythmStyle style, DensityLevel density,
                                         uint8_t currentKickPattern)
{
    // FOLLOW_KICK always uses kick pattern
    if(style == RHYTHM_FOLLOW_KICK)
    {
        return kickPatterns[currentKickPattern];
    }

    // Generate pattern using the same algorithms as drums
    return GeneratePatternForStyle(seed, style, density, voice->patternLength, currentKickPattern);
}

void GenerateMelodyRhythmFor(uint32_t seed, MelodyConfig* voice, uint8_t currentKickPattern)
{
    voice->rhythmPattern = 0;

    // Odd lengths for polyrhythms
    const uint8_t oddLengths[] = {12, 13, 15, 17, 18};

    // FOLLOW_KICK always uses 32-step pattern to match kick
    if(voice->rhythmStyle == RHYTHM_FOLLOW_KICK)
    {
        voice->patternLength = 32;
        voice->rhythmPattern = kickPatterns[currentKickPattern];
        return;
    }

    // Determine pattern length
    // Supporting style: less likely to use polyrhythms (10%)
    // Arpeggiator style: more likely to use polyrhythms (20%)
    uint32_t polyrhythmChance = (voice->style == MELODY_SUPPORTING) ? 10 : 20;

    if((seed % 100) < polyrhythmChance)
    {
        voice->patternLength = oddLengths[(seed >> 8) % 5];
    }
    else
    {
        voice->patternLength = 32;
    }

    // Generate pattern
    voice->rhythmPattern = GeneratePatternForStyle(seed, voice->rhythmStyle, voice->density,
                                                    voice->patternLength, currentKickPattern);
}

// ============================================================================
// NOTE GENERATION
// ============================================================================

void GenerateMelodyNotesWithSeed(uint32_t seed, MelodyConfig* voice,
                                  int8_t* noteArray, ScaleType scale, uint8_t root)
{
    if(voice->style == MELODY_SUPPORTING)
    {
        // Supporting: Few different notes, low octave, minimal movement
        int8_t baseNotes[4];
        baseNotes[0] = GetScaleNote(scale, root, 0);   // Root
        baseNotes[1] = GetScaleNote(scale, root, 4);   // 5th
        baseNotes[2] = GetScaleNote(scale, root, 2);   // 3rd
        baseNotes[3] = GetScaleNote(scale, root, -1);  // 7th below

        // Fill sequence with mostly root and 5th
        for(int i = 0; i < 32; i++)
        {
            int noteChoice = (seed >> (i * 2)) % 10;
            if(noteChoice < 5)
                noteArray[i] = baseNotes[0]; // Root 50%
            else if(noteChoice < 8)
                noteArray[i] = baseNotes[1]; // 5th 30%
            else if(noteChoice < 9)
                noteArray[i] = baseNotes[2]; // 3rd 10%
            else
                noteArray[i] = baseNotes[3]; // 7th 10%
        }

        // Occasional octave jump (10% chance per note)
        for(int i = 0; i < 32; i++)
        {
            if(((seed >> (i + 8)) % 10) == 0)
            {
                noteArray[i] += 12;
            }
        }
    }
    else // MELODY_ARPEGGIATOR
    {
        int8_t scaleLen = scaleLengths[scale];

        switch(voice->subStyle)
        {
            case ARP_CHORD_TONES:
                {
                    int8_t chordNotes[5];
                    int numNotes = 4 + (seed % 2);
                    int8_t chordDegrees[] = {0, 2, 4, 7, 9};

                    for(int n = 0; n < numNotes; n++)
                    {
                        chordNotes[n] = GetScaleNote(scale, root, chordDegrees[n]);
                    }

                    // Shuffle chord notes based on seed to vary the order
                    // This prevents always hitting the same subset when combined with regular rhythms
                    for(int n = numNotes - 1; n > 0; n--)
                    {
                        int swapIdx = ((seed >> (n * 3)) % (n + 1));
                        int8_t temp = chordNotes[n];
                        chordNotes[n] = chordNotes[swapIdx];
                        chordNotes[swapIdx] = temp;
                    }

                    // Use a step size that's coprime with numNotes to ensure all notes are hit
                    // For 4 notes: step sizes 1 or 3 both work (hit all 4)
                    // For 5 notes: step sizes 1, 2, 3, or 4 all work (hit all 5)
                    int stepSize = 1;
                    if(numNotes == 4)
                    {
                        stepSize = ((seed >> 8) % 2) ? 3 : 1;  // 50% chance of 1 or 3
                    }
                    else // numNotes == 5
                    {
                        int stepOptions[] = {1, 2, 3, 4};
                        stepSize = stepOptions[(seed >> 8) % 4];
                    }

                    // Fill sequence with varied starting offsets per half-bar
                    // This creates more melodic interest and avoids hitting same notes
                    for(int i = 0; i < 32; i++)
                    {
                        int halfBar = i / 8;  // 0, 1, 2, or 3
                        int posInHalf = i % 8;

                        // Each half-bar starts at a different offset
                        int startOffset = (halfBar * 2 + ((seed >> (12 + halfBar)) % 2)) % numNotes;

                        // Use coprime step to cycle through all notes
                        int noteIdx = (startOffset + posInHalf * stepSize) % numNotes;
                        noteArray[i] = chordNotes[noteIdx];
                    }

                    // Add occasional octave variation (20% chance per step)
                    for(int i = 0; i < 32; i++)
                    {
                        if(((seed >> ((i % 16) + 4)) % 5) == 0)
                        {
                            noteArray[i] += 12;  // Octave up
                        }
                    }
                }
                break;

            case ARP_SCALE_ASCENDING:
                {
                    int numNotes = 5;
                    int8_t arpNotes[5];
                    for(int n = 0; n < numNotes; n++)
                    {
                        arpNotes[n] = GetScaleNote(scale, root, n);
                    }

                    // Use coprime step sizes to ensure all notes are hit
                    // For 5 notes: 1, 2, 3, 4 are all coprime with 5
                    int stepOptions[] = {1, 2, 3, 4};
                    int stepSize = stepOptions[(seed >> 8) % 4];

                    // Fill with varied starting offsets per half-bar
                    for(int i = 0; i < 32; i++)
                    {
                        int halfBar = i / 8;
                        int posInHalf = i % 8;

                        // Each half-bar starts at different offset
                        int startOffset = (halfBar * 2 + ((seed >> (12 + halfBar)) % 2)) % numNotes;
                        int noteIdx = (startOffset + posInHalf * stepSize) % numNotes;
                        noteArray[i] = arpNotes[noteIdx];
                    }

                    // Add occasional octave variation (15% chance)
                    for(int i = 0; i < 32; i++)
                    {
                        int roll = (seed >> ((i % 16) + 4)) % 100;
                        if(roll < 10)
                            noteArray[i] += 12;  // Octave up
                        else if(roll < 15)
                            noteArray[i] -= 12;  // Octave down (if above bass)
                    }
                }
                break;

            case ARP_SCALE_RANDOM:
            default:
                {
                    int8_t arpNotes[6];
                    int numNotes = 4 + (seed % 3);  // 4, 5, or 6 notes

                    for(int n = 0; n < numNotes; n++)
                    {
                        int degree = (seed >> (n * 4)) % (scaleLen * 2);
                        arpNotes[n] = GetScaleNote(scale, root, degree);
                    }

                    // Shuffle the notes to randomize order
                    for(int n = numNotes - 1; n > 0; n--)
                    {
                        int swapIdx = ((seed >> (n * 3 + 8)) % (n + 1));
                        int8_t temp = arpNotes[n];
                        arpNotes[n] = arpNotes[swapIdx];
                        arpNotes[swapIdx] = temp;
                    }

                    // Use coprime step size to hit all notes
                    // For 4: use 1 or 3; for 5: use 1,2,3,4; for 6: use 1 or 5
                    int stepSize = 1;
                    if(numNotes == 4)
                        stepSize = ((seed >> 16) % 2) ? 3 : 1;
                    else if(numNotes == 5)
                        stepSize = 1 + ((seed >> 16) % 4);
                    else if(numNotes == 6)
                        stepSize = ((seed >> 16) % 2) ? 5 : 1;

                    // Fill with varied starting offsets and some true randomization
                    for(int i = 0; i < 32; i++)
                    {
                        int halfBar = i / 8;
                        int posInHalf = i % 8;

                        // Each half-bar gets a different start offset
                        int startOffset = (halfBar * 2 + ((seed >> (20 + halfBar)) % 3)) % numNotes;

                        // 25% chance of truly random note selection for variety
                        if(((seed >> (i % 16)) % 4) == 0)
                        {
                            noteArray[i] = arpNotes[(seed >> (i + 8)) % numNotes];
                        }
                        else
                        {
                            int noteIdx = (startOffset + posInHalf * stepSize) % numNotes;
                            noteArray[i] = arpNotes[noteIdx];
                        }
                    }

                    // Add octave variation (20% chance)
                    for(int i = 0; i < 32; i++)
                    {
                        int roll = (seed >> ((i % 16) + 4)) % 100;
                        if(roll < 12)
                            noteArray[i] += 12;
                        else if(roll < 20)
                            noteArray[i] -= 12;
                    }
                }
                break;
        }
    }
}

void GenerateMelodyNotesFor(uint32_t seed, MelodyConfig* voice, ScaleType scale, uint8_t root)
{
    voice->sequencePos = 0;
    GenerateMelodyNotesWithSeed(seed, voice, voice->noteSequence, scale, root);
}

// ============================================================================
// FULL PATTERN GENERATION
// ============================================================================

void GenerateMelodyPatternFor(uint32_t seed, MelodyConfig* voice,
                               ScaleType scale, uint8_t root, uint8_t currentKickPattern)
{
    GenerateMelodyRhythmFor(seed, voice, currentKickPattern);
    GenerateMelodyNotesFor(seed, voice, scale, root);

    // Always generate unique B and C patterns (so they're available when user enables variations)
    uint32_t seedB = seed ^ 0xBEEFBEEF;
    uint32_t seedC = seed ^ 0xCAFECAFE;

    // Generate B rhythm pattern using variation settings
    voice->rhythmPatternB = GenerateMelodyRhythmWithParams(seedB, voice,
                                                            voice->variation.styleB,
                                                            voice->variation.densityB,
                                                            currentKickPattern);

    // Generate B note sequence
    GenerateMelodyNotesWithSeed(seedB, voice, voice->noteSequenceB, scale, root);

    // Generate C rhythm pattern using variation settings
    voice->rhythmPatternC = GenerateMelodyRhythmWithParams(seedC, voice,
                                                            voice->variation.styleC,
                                                            voice->variation.densityC,
                                                            currentKickPattern);

    // Generate C note sequence
    GenerateMelodyNotesWithSeed(seedC, voice, voice->noteSequenceC, scale, root);

    // Ensure B is different from A (regenerate with modified seed if same)
    if(voice->rhythmPatternB == voice->rhythmPattern)
    {
        seedB ^= 0x12345678;
        voice->rhythmPatternB = GenerateMelodyRhythmWithParams(seedB, voice,
                                                                voice->variation.styleB,
                                                                voice->variation.densityB,
                                                                currentKickPattern);
        GenerateMelodyNotesWithSeed(seedB, voice, voice->noteSequenceB, scale, root);
    }

    // Ensure C is different from A and B
    if(voice->rhythmPatternC == voice->rhythmPattern || voice->rhythmPatternC == voice->rhythmPatternB)
    {
        seedC ^= 0x87654321;
        voice->rhythmPatternC = GenerateMelodyRhythmWithParams(seedC, voice,
                                                                voice->variation.styleC,
                                                                voice->variation.densityC,
                                                                currentKickPattern);
        GenerateMelodyNotesWithSeed(seedC, voice, voice->noteSequenceC, scale, root);
    }
}

// ============================================================================
// PERSONALITY RANDOMIZATION
// ============================================================================

void RandomizeMelodyPersonalityFor(uint32_t seed, MelodyConfig* voice,
                                    ScaleType scale, uint8_t root, uint8_t currentKickPattern)
{
    // Randomize main style
    voice->style = (MelodyStyle)(seed % NUM_MELODY_STYLES);

    // Randomize sub-style based on main style
    if(voice->style == MELODY_SUPPORTING)
    {
        voice->subStyle = (seed >> 4) % NUM_SUPPORTING_SUBSTYLES;
    }
    else
    {
        voice->subStyle = (seed >> 4) % NUM_ARP_SUBSTYLES;
    }

    // Randomize rhythm style
    // Weight toward more musical styles
    int styleRoll = (seed >> 8) % 100;
    if(styleRoll < 40)
        voice->rhythmStyle = RHYTHM_EUCLIDEAN;
    else if(styleRoll < 70)
        voice->rhythmStyle = RHYTHM_STRAIGHT;
    else if(styleRoll < 90)
        voice->rhythmStyle = RHYTHM_SYNCOPATED;
    else
        voice->rhythmStyle = RHYTHM_ANTI_EUCLIDEAN;

    // Randomize density based on main style
    if(voice->style == MELODY_SUPPORTING)
    {
        // 50% LOW, 40% MEDIUM, 10% HIGH
        int densityRoll = (seed >> 16) % 100;
        if(densityRoll < 50)
            voice->density = DENSITY_LOW;
        else if(densityRoll < 90)
            voice->density = DENSITY_MEDIUM;
        else
            voice->density = DENSITY_HIGH;
    }
    else // MELODY_ARPEGGIATOR
    {
        // 20% LOW, 50% MEDIUM, 30% HIGH
        int densityRoll = (seed >> 16) % 100;
        if(densityRoll < 20)
            voice->density = DENSITY_LOW;
        else if(densityRoll < 70)
            voice->density = DENSITY_MEDIUM;
        else
            voice->density = DENSITY_HIGH;
    }

    // Generate new pattern with new personality
    GenerateMelodyPatternFor(seed, voice, scale, root, currentKickPattern);
}

} // namespace themis
