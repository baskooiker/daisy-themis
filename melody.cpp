/**
 * @file melody.cpp
 * @brief Melody generation implementation
 */

#include "melody.h"
#include "drums.h"
#include "groove.h"

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
// PATTERN GENERATION
// ============================================================================

void GenerateMelodyRhythmFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();
    voice->rhythmPattern = 0;

    // Odd lengths for polyrhythms (same as drums)
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

    // Generate pattern using the same algorithms as drums
    switch(voice->rhythmStyle)
    {
        case RHYTHM_SYNCOPATED:
            voice->rhythmPattern = GenerateSyncopated(seed, voice->density, voice->patternLength);
            break;
        case RHYTHM_STRAIGHT:
            voice->rhythmPattern = GenerateStraight(seed, voice->density, voice->patternLength);
            break;
        case RHYTHM_EUCLIDEAN:
            voice->rhythmPattern = GenerateEuclidean(seed, voice->density, voice->patternLength);
            break;
        case RHYTHM_ANTI_EUCLIDEAN:
            voice->rhythmPattern = GenerateAntiEuclidean(seed, voice->density, voice->patternLength);
            break;
        case RHYTHM_FOLLOW_KICK:
            // Already handled above, but include for completeness
            voice->rhythmPattern = kickPatterns[currentKickPattern];
            break;
        default:
            voice->rhythmPattern = GenerateEuclidean(seed, voice->density, voice->patternLength);
            break;
    }
}

void GenerateMelodyNotesFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();
    voice->sequencePos = 0;

    if(voice->style == MELODY_SUPPORTING)
    {
        // Supporting: Few different notes, low octave, minimal movement
        // Use 2-4 distinct notes, mostly root and 5th
        int8_t baseNotes[4];
        baseNotes[0] = GetScaleNote(melodyScale, melodyRoot, 0);  // Root
        baseNotes[1] = GetScaleNote(melodyScale, melodyRoot, 4);  // 5th
        baseNotes[2] = GetScaleNote(melodyScale, melodyRoot, 2);  // 3rd
        baseNotes[3] = GetScaleNote(melodyScale, melodyRoot, -1); // 7th below

        // Fill sequence with mostly root and 5th
        for(int i = 0; i < 32; i++)
        {
            int noteChoice = (seed >> (i * 2)) % 10;
            if(noteChoice < 5)
                voice->noteSequence[i] = baseNotes[0]; // Root 50%
            else if(noteChoice < 8)
                voice->noteSequence[i] = baseNotes[1]; // 5th 30%
            else if(noteChoice < 9)
                voice->noteSequence[i] = baseNotes[2]; // 3rd 10%
            else
                voice->noteSequence[i] = baseNotes[3]; // 7th 10%
        }

        // Occasional octave jump (10% chance per note)
        for(int i = 0; i < 32; i++)
        {
            if(((seed >> (i + 8)) % 10) == 0)
            {
                voice->noteSequence[i] += 12; // Jump up one octave
            }
        }
    }
    else // MELODY_ARPEGGIATOR
    {
        int8_t scaleLen = scaleLengths[melodyScale];

        switch(voice->subStyle)
        {
            case ARP_CHORD_TONES:
                // 4-5 chord tones that repeat: root, 3rd, 5th, (octave), (7th)
                {
                    int8_t chordNotes[5];
                    int numNotes = 4 + (seed % 2);  // 4-5 notes
                    int8_t chordDegrees[] = {0, 2, 4, 7, 9}; // root, 3rd, 5th, octave, 9th

                    // Generate the chord note set
                    for(int n = 0; n < numNotes; n++)
                    {
                        chordNotes[n] = GetScaleNote(melodyScale, melodyRoot, chordDegrees[n]);
                    }

                    // Fill sequence by cycling through chord notes
                    for(int i = 0; i < 32; i++)
                    {
                        // Same pattern in bar 1 and bar 2
                        int barPos = i % 16;
                        voice->noteSequence[i] = chordNotes[barPos % numNotes];
                    }
                }
                break;

            case ARP_SCALE_ASCENDING:
                // 5 ascending scale notes that repeat
                {
                    int8_t arpNotes[5];
                    for(int n = 0; n < 5; n++)
                    {
                        arpNotes[n] = GetScaleNote(melodyScale, melodyRoot, n);
                    }

                    // Fill sequence - same 5-note pattern repeats
                    for(int i = 0; i < 32; i++)
                    {
                        int barPos = i % 16;
                        voice->noteSequence[i] = arpNotes[barPos % 5];
                    }
                }
                break;

            case ARP_SCALE_RANDOM:
                // 4-6 random scale notes that repeat
                {
                    int8_t arpNotes[6];
                    int numNotes = 4 + (seed % 3);  // 4-6 notes

                    // Generate random scale degrees within 2 octaves
                    for(int n = 0; n < numNotes; n++)
                    {
                        int degree = (seed >> (n * 4)) % (scaleLen * 2);
                        arpNotes[n] = GetScaleNote(melodyScale, melodyRoot, degree);
                    }

                    // Fill sequence by cycling through random notes
                    for(int i = 0; i < 32; i++)
                    {
                        int barPos = i % 16;
                        voice->noteSequence[i] = arpNotes[barPos % numNotes];
                    }
                }
                break;
        }
    }
}

void GenerateMelodyPatternFor(MelodyConfig* voice)
{
    GenerateMelodyRhythmFor(voice);
    GenerateMelodyNotesFor(voice);
}

void GenerateMelodyPattern()
{
    GenerateMelodyPatternFor(&melodyVoice);
    GenerateMelodyPatternFor(&melodyMidiVoice);
}

// ============================================================================
// PERSONALITY RANDOMIZATION
// ============================================================================

void RandomizeMelodyPersonalityFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();

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

    // Randomize rhythm style (same options as drums)
    // Weight toward more musical styles: Euclidean (40%), Straight (30%), Syncopated (20%), Anti-Euclidean (10%)
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
    // Supporting: prefer lower densities (sparse basslines)
    // Arpeggiator: prefer medium-high densities (busier arpeggios)
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
    GenerateMelodyPatternFor(voice);
}

void RandomizeMelodyPersonality()
{
    RandomizeMelodyPersonalityFor(&melodyVoice);
    RandomizeMelodyPersonalityFor(&melodyMidiVoice);
}

void RandomizeAllParameters()
{
    // Send note-off for any playing melody note before randomizing
    SendMelodyNoteOff();

    // Randomize drum patterns
    RandomizePatterns();

    // Randomize drum voice personalities
    RandomizeVoicePersonalities();

    // Randomize groove
    RandomizeGroove();

    // Randomize melody (both voices)
    RandomizeMelodyPersonality();
}
