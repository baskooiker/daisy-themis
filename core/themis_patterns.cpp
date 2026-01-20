/**
 * @file themis_patterns.cpp
 * @brief Pattern generation algorithms implementation
 */

#include "themis_patterns.h"
#include "themis_data.h"

namespace themis {

// ============================================================================
// PATTERN GENERATION
// ============================================================================

uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few
        // MSB-first: bit (31-i) = step i
        for(int i = 0; i < length; i++)
        {
            pattern |= (1U << (31 - i));
        }
        // Remove ~12.5% of hits randomly (to get ~87.5% density)
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1U << (31 - pos));
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize off-beat positions (odd steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2 + 1;
            if(pos < length) pattern |= (1U << (31 - pos));
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
        // MSB-first: bit (31-i) = step i
        for(int i = 0; i < length; i++)
        {
            pattern |= (1U << (31 - i));
        }
        // Remove ~12.5% of hits randomly
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1U << (31 - pos));
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize on-beat positions (even steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2;
            if(pos < length) pattern |= (1U << (31 - pos));
        }
    }
    return pattern;
}

uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    // For low density, use slightly off-grid hit counts for more interest
    // E.g., 3 or 5 hits instead of 4 creates more rhythmic variety
    int hitCount;
    if(density == DENSITY_LOW)
    {
        // Use 3, 4, or 5 hits based on seed for variety (not always 4)
        int options[] = {3, 4, 5};
        hitCount = options[seed % 3];
        // But cap at length/4 to stay sparse
        if(hitCount > length / 4) hitCount = length / 4;
        if(hitCount < 1) hitCount = 1;
    }
    else if(density == DENSITY_MEDIUM)
    {
        hitCount = length / 2;
    }
    else // DENSITY_HIGH
    {
        hitCount = (length * 7) / 8;
    }

    // Bjorklund's algorithm for Euclidean rhythms
    // MSB-first: bit (31-i) = step i
    // Initialize bucket = length so first hit lands on step 0 (downbeat)
    int bucket = length;
    for(int i = 0; i < length; i++)
    {
        if(bucket >= length)
        {
            bucket -= length;
            pattern |= (1U << (31 - i));
        }
        bucket += hitCount;
    }

    // Apply seed-based rotation for variety (but keep step 0 as an anchor sometimes)
    if(density == DENSITY_LOW && (seed & 0x100))
    {
        // Rotate pattern by 1-3 steps for rhythmic interest
        int rotation = 1 + ((seed >> 4) % 3);
        if(rotation < length)
        {
            // Rotate MSB-first pattern: shift right and wrap high bits
            uint32_t mask = (length < 32) ? ((1U << length) - 1) << (32 - length) : 0xFFFFFFFF;
            uint32_t rotatedPart = (pattern >> rotation) & mask;
            uint32_t wrappedPart = (pattern << (length - rotation)) & mask;
            pattern = (rotatedPart | wrappedPart) & mask;
        }
    }

    return pattern;
}

uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove in clusters
        // MSB-first: bit (31-i) = step i
        for(int i = 0; i < length; i++)
        {
            pattern |= (1U << (31 - i));
        }
        // Remove ~12.5% of hits in clusters
        int removeCount = length / 8;
        int clustersCount = (seed % 2) + 1;
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
                pattern &= ~(1U << (31 - pos));
            }
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);

        // Create clusters of hits
        int clustersCount = (seed % 3) + 2;
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
                if(pos < length) pattern |= (1U << (31 - pos));
            }
        }
    }
    return pattern;
}

uint32_t GeneratePatternForStyle(uint32_t seed, RhythmStyle style,
                                  DensityLevel density, uint8_t length,
                                  uint8_t currentKickPattern)
{
    uint32_t pattern = 0;

    switch(style)
    {
        case RHYTHM_SYNCOPATED:
            pattern = GenerateSyncopated(seed, density, length);
            break;
        case RHYTHM_STRAIGHT:
            pattern = GenerateStraight(seed, density, length);
            break;
        case RHYTHM_EUCLIDEAN:
            pattern = GenerateEuclidean(seed, density, length);
            break;
        case RHYTHM_ANTI_EUCLIDEAN:
            pattern = GenerateAntiEuclidean(seed, density, length);
            break;
        case RHYTHM_FOLLOW_KICK:
            pattern = kickPatterns[currentKickPattern];
            break;
        default:
            pattern = GenerateEuclidean(seed, density, length);
            break;
    }

    // Guarantee at least one hit - never return an empty pattern
    // MSB-first: bit (31-pos) = step pos
    if(pattern == 0 && length > 0)
    {
        // Add a hit at a position based on the seed
        int pos = seed % length;
        pattern = (1U << (31 - pos));
    }

    return pattern;
}

// ============================================================================
// INTERACTION PROCESSING
// ============================================================================

void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2)
{
    voice1->active = true;
    voice2->active = true;
}

static void DividePattern(uint32_t* pattern1, uint32_t* pattern2)
{
    uint32_t combined = *pattern1 | *pattern2;
    *pattern1 = 0;
    *pattern2 = 0;

    // Alternate hits between voices
    // MSB-first: bit (31-step) = step, so we iterate through steps in order
    bool voice1Turn = true;
    for(int step = 0; step < 32; step++)
    {
        uint32_t bit = (1U << (31 - step));
        if(combined & bit)
        {
            if(voice1Turn)
                *pattern1 |= bit;
            else
                *pattern2 |= bit;
            voice1Turn = !voice1Turn;
        }
    }
}

void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Divide all pattern variations (A, B, C)
    DividePattern(&voice1->pattern, &voice2->pattern);
    DividePattern(&voice1->patternB, &voice2->patternB);
    DividePattern(&voice1->patternC, &voice2->patternC);

    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Bar 1 (steps 0-15): voice1 - MSB-first: bits 31-16 = 0xFFFF0000
    // Bar 2 (steps 16-31): voice2 - MSB-first: bits 15-0 = 0x0000FFFF
    uint32_t mask1 = 0xFFFF0000;
    uint32_t mask2 = 0x0000FFFF;

    // Apply to all pattern variations (A, B, C)
    voice1->pattern &= mask1;
    voice1->patternB &= mask1;
    voice1->patternC &= mask1;
    voice2->pattern &= mask2;
    voice2->patternB &= mask2;
    voice2->patternC &= mask2;

    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // MSB-first bit ordering:
    // Half bar 1 (steps 0-7): bits 31-24 = 0xFF000000
    // Half bar 2 (steps 8-15): bits 23-16 = 0x00FF0000
    // Half bar 3 (steps 16-23): bits 15-8 = 0x0000FF00
    // Half bar 4 (steps 24-31): bits 7-0 = 0x000000FF
    // voice1 gets halves 1 and 3, voice2 gets halves 2 and 4
    uint32_t mask1 = 0xFF00FF00;
    uint32_t mask2 = 0x00FF00FF;

    // Apply to all pattern variations (A, B, C)
    voice1->pattern &= mask1;
    voice1->patternB &= mask1;
    voice1->patternC &= mask1;
    voice2->pattern &= mask2;
    voice2->patternB &= mask2;
    voice2->patternC &= mask2;

    voice1->active = true;
    voice2->active = true;
}

void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2, uint8_t barCounter)
{
    // This pattern plays over 2 iterations (4 bars total)
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
    uint8_t segment = 0;

    switch(config->granularity)
    {
        case VAR_GRAN_BAR:
            // 16 steps per segment (1 bar)
            segment = (barInCycle * 2) + (step / 16);
            break;
        case VAR_GRAN_HALF_BAR:
            // 8 steps per segment (half bar)
            segment = ((barInCycle * 4) + (step / 8)) % 8;
            break;
        case VAR_GRAN_QUARTER:
            // 4 steps per segment (quarter bar)
            segment = ((barInCycle * 8) + (step / 4)) % 8;
            break;
        default:
            segment = barInCycle;
            break;
    }

    // Look up variation from sequence table
    uint8_t variation = variationSequences[config->sequence][segment % 8];

    // Clamp to valid variation for the mode
    if(config->mode == VAR_MODE_AB && variation > 1)
        variation = 1;

    return variation;
}

} // namespace themis
