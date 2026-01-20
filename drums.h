/**
 * @file drums.h
 * @brief Drum pattern generation and processing for Themis
 *
 * This module handles:
 * - Rhythm pattern generation (syncopated, straight, euclidean, etc.)
 * - Voice interaction processing (divided, alternating, etc.)
 * - Fill scheduling and processing
 * - Voice personality randomization
 */

#ifndef THEMIS_DRUMS_H
#define THEMIS_DRUMS_H

#include "types.h"
#include "globals.h"

// ============================================================================
// PATTERN HELPERS
// ============================================================================

/**
 * @brief Check if a step should trigger in a 32-bit pattern
 * @param pattern 32-bit pattern (MSB = step 0)
 * @param step Step number (0-31)
 * @return true if step should trigger
 */
bool IsStepActive(uint32_t pattern, uint8_t step);

/**
 * @brief Check if a step should trigger in an 8-bit fill pattern
 * @param pattern 8-bit pattern (MSB = step 0)
 * @param step Step number (0-7)
 * @return true if step should trigger
 */
bool IsStepActive8(uint8_t pattern, uint8_t step);

/**
 * @brief Check if a step should trigger in a 16-bit fill pattern
 * @param pattern 16-bit pattern (MSB = step 0)
 * @param step Step number (0-15)
 * @return true if step should trigger
 */
bool IsStepActive16(uint16_t pattern, uint8_t step);

// ============================================================================
// RHYTHM GENERATION
// ============================================================================

/**
 * @brief Generate syncopated rhythm (off-beat emphasis)
 * @param seed Random seed
 * @param density Pattern density level
 * @param length Pattern length in steps
 * @return Generated bit pattern
 */
uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate straight rhythm (on-beat emphasis)
 * @param seed Random seed
 * @param density Pattern density level
 * @param length Pattern length in steps
 * @return Generated bit pattern
 */
uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate Euclidean rhythm (evenly spaced hits)
 * @param seed Random seed
 * @param density Pattern density level
 * @param length Pattern length in steps
 * @return Generated bit pattern
 */
uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length);

/**
 * @brief Generate anti-Euclidean rhythm (clustered hits)
 * @param seed Random seed
 * @param density Pattern density level
 * @param length Pattern length in steps
 * @return Generated bit pattern
 */
uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length);

// ============================================================================
// INTERACTION PROCESSING
// ============================================================================

/**
 * @brief Process no interaction (voices independent)
 */
void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process divided interaction (hits split between voices)
 */
void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process alternate bar interaction (voices swap each bar)
 */
void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process alternate half-bar interaction
 */
void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2);

/**
 * @brief Process alternate two-bar interaction
 */
void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2);

// ============================================================================
// VARIATION SYSTEM
// ============================================================================

/**
 * @brief Determine which variation (A/B/C) to use based on current position
 * @param config Pointer to the variation configuration
 * @param currentStep Current step within the 32-step pattern (0-31)
 * @param barInCycle Current bar within the 8-bar cycle (0-3)
 * @return 0 for A, 1 for B, 2 for C
 */
uint8_t GetCurrentVariation(const VariationConfig* config, uint8_t currentStep, uint8_t barInCycle);

// ============================================================================
// VOICE CONFIGURATION
// ============================================================================

/**
 * @brief Randomize voice personalities (styles, densities, interactions)
 *
 * Called every 32 bars to evolve the groove.
 */
void RandomizeVoicePersonalities();

/**
 * @brief Generate patterns for all generative voices
 *
 * Called every 8 bars to create new patterns.
 */
void GenerateVoicePatterns();

/**
 * @brief Randomize all drum patterns
 *
 * Selects new kick, clap, and hi-hat patterns.
 */
void RandomizePatterns();

// ============================================================================
// FILL SYSTEM
// ============================================================================

/**
 * @brief Schedule a fill for the end of the current 8-bar cycle
 *
 * 30% chance of triggering a fill. Randomly selects half-bar
 * or whole-bar fill and chooses patterns.
 */
void ScheduleFill();

// ============================================================================
// MAIN PROCESSING
// ============================================================================

/**
 * @brief Process drum patterns on each 16th note
 *
 * Main drum sequencer function. Handles normal patterns,
 * fills, generative voices, and melody trigger scheduling.
 */
void ProcessDrumPatterns();

#endif // THEMIS_DRUMS_H
