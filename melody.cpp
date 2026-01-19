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
    voice->patternLength = 32;

    if(voice->style == MELODY_SUPPORTING)
    {
        // Strong beat positions (quarter notes: 0, 4, 8, 12, 16, 20, 24, 28)
        // Half notes: 0, 8, 16, 24
        // Downbeats of each bar: 0, 16
        const int strongBeats[] = {0, 4, 8, 12, 16, 20, 24, 28};  // Quarter notes
        const int halfBeats[] = {0, 8, 16, 24};  // Half notes

        switch(voice->subStyle)
        {
            case SUPPORT_FOLLOW_KICK:
                // Copy current kick pattern directly
                voice->rhythmPattern = kickPatterns[currentKickPattern];
                break;

            case SUPPORT_OWN_SPARSE:
                // Straight sparse pattern using only strong beats
                // Choose 3-5 quarter note positions
                {
                    int hitCount = 3 + (seed % 3);  // 3-5 notes
                    uint8_t usedPositions = 0;

                    for(int i = 0; i < hitCount; i++)
                    {
                        // Pick from strong beats array, avoid repeats
                        int attempts = 0;
                        int idx;
                        do {
                            idx = (seed >> (i * 3 + attempts)) % 8;
                            attempts++;
                        } while((usedPositions & (1 << idx)) && attempts < 10);

                        usedPositions |= (1 << idx);
                        voice->rhythmPattern |= (1 << strongBeats[idx]);
                    }

                    // Always include downbeat of bar 1
                    voice->rhythmPattern |= (1 << 0);
                }
                break;

            case SUPPORT_SUBSET_KICK:
                // Take kick hits but prefer strong beats
                {
                    uint32_t kickPat = kickPatterns[currentKickPattern];

                    // First, always take kick hits on strong beats (quarter notes)
                    for(int i = 0; i < 8; i++)
                    {
                        int pos = strongBeats[i];
                        if(kickPat & (1 << pos))
                        {
                            voice->rhythmPattern |= (1 << pos);
                        }
                    }

                    // If we have very few hits, add some from half beat positions
                    if(__builtin_popcount(voice->rhythmPattern) < 2)
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            voice->rhythmPattern |= (1 << halfBeats[i]);
                        }
                    }
                }
                break;
        }
    }
    else // MELODY_ARPEGGIATOR
    {
        // Arpeggiator: sparse 4-5 note patterns that repeat
        // 8th note grid positions (every other 16th): 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
        const int eighthNotes[] = {0, 2, 4, 6, 8, 10, 12, 14};  // First bar 8th notes

        switch(voice->subStyle)
        {
            case ARP_CHORD_TONES:
                // 4-5 notes per bar, repeating pattern
                // Create a 1-bar pattern that repeats
                {
                    int notesPerBar = 4 + (seed % 2);  // 4-5 notes
                    uint8_t usedPositions = 0;

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        int attempts = 0;
                        int idx;
                        do {
                            idx = (seed >> (i * 3 + attempts)) % 8;
                            attempts++;
                        } while((usedPositions & (1 << idx)) && attempts < 10);

                        usedPositions |= (1 << idx);
                        // Set in both bars (first bar and second bar)
                        voice->rhythmPattern |= (1 << eighthNotes[idx]);
                        voice->rhythmPattern |= (1 << (eighthNotes[idx] + 16));
                    }
                }
                break;

            case ARP_SCALE_ASCENDING:
                // 5 notes ascending, starting on downbeat
                // Pattern: 1--2--3--4--5--- repeated
                {
                    int notesPerBar = 5;
                    // Space notes evenly across the bar (roughly every 3 16th notes)
                    int positions[] = {0, 3, 6, 10, 13};  // Evenly spaced

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        voice->rhythmPattern |= (1 << positions[i]);
                        voice->rhythmPattern |= (1 << (positions[i] + 16));  // Repeat in bar 2
                    }
                }
                break;

            case ARP_SCALE_RANDOM:
                // 4-6 random positions per bar, slightly syncopated
                {
                    int notesPerBar = 4 + (seed % 3);  // 4-6 notes
                    uint32_t usedPositions = 0;

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        int attempts = 0;
                        int pos;
                        do {
                            // Allow 16th note positions but prefer 8th notes
                            if((seed >> (i + attempts)) % 3 == 0)
                                pos = (seed >> (i * 4 + attempts)) % 16;  // Any 16th in first bar
                            else
                                pos = eighthNotes[(seed >> (i * 3 + attempts)) % 8];  // 8th notes
                            attempts++;
                        } while((usedPositions & (1 << pos)) && attempts < 15);

                        usedPositions |= (1 << pos);
                        voice->rhythmPattern |= (1 << pos);
                        voice->rhythmPattern |= (1 << (pos + 16));  // Mirror to bar 2
                    }
                }
                break;
        }
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
    // Randomize drum patterns
    RandomizePatterns();

    // Randomize drum voice personalities
    RandomizeVoicePersonalities();

    // Randomize groove
    RandomizeGroove();

    // Randomize melody (both voices)
    RandomizeMelodyPersonality();
}
