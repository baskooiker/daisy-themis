/**
 * @file themis_patterns.h
 * @brief Pattern generation algorithms for Themis
 */

#ifndef THEMIS_PATTERNS_H
#define THEMIS_PATTERNS_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// PATTERN GENERATION
// ============================================================================

/**
 * @brief Generate a syncopated rhythm pattern (off-beat emphasis)
 * @param seed Random seed
 * @param density Pattern density
 * @param length Pattern length in steps
 * @return 32-bit pattern with active steps
 */
uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate a straight rhythm pattern (on-beat emphasis)
 */
uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate a Euclidean rhythm pattern (evenly distributed)
 */
uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate an anti-Euclidean rhythm pattern (clustered hits)
 */
uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate a pattern for a given style
 * @param seed Random seed
 * @param style Rhythm style to use
 * @param density Pattern density
 * @param length Pattern length
 * @param currentKickPattern Current kick pattern index (for FOLLOW_KICK style)
 * @return Generated pattern
 */
uint32_t GeneratePatternForStyle(uint32_t seed, RhythmStyle style,
                                  DensityLevel density, uint8_t length,
                                  uint8_t currentKickPattern);

// ============================================================================
// INTERACTION PROCESSING
// ============================================================================

/**
 * @brief Process INTERACTION_NONE (patterns remain independent)
 */
void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process INTERACTION_DIVIDED (hits alternate between voices)
 */
void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process INTERACTION_ALTERNATE_BAR (switch every bar)
 */
void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process INTERACTION_ALTERNATE_HALF (switch every half bar)
 */
void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process INTERACTION_ALTERNATE_TWO (switch every 2 bars)
 * @param barCounter Current bar counter for determining which voice is active
 */
void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2, uint8_t barCounter);

// ============================================================================
// VARIATION SYSTEM
// ============================================================================

/**
 * @brief Get the current variation (A=0, B=1, C=2) based on config and timing
 * @param config Variation configuration
 * @param step Current step (0-31)
 * @param barInCycle Current bar within the 8-bar cycle (0-3)
 * @return Variation index (0=A, 1=B, 2=C)
 */
uint8_t GetCurrentVariation(const VariationConfig* config, uint8_t step, uint8_t barInCycle);

} // namespace themis

#endif // THEMIS_PATTERNS_H
