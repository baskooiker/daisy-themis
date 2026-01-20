/**
 * @file themis_melody.h
 * @brief Melody generation algorithms for Themis
 */

#ifndef THEMIS_MELODY_H
#define THEMIS_MELODY_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// SCALE UTILITIES
// ============================================================================

/**
 * @brief Get a note in semitones for a given scale degree
 * @param scale Scale type to use
 * @param root Root note (0-11, C=0)
 * @param degree Scale degree (can be negative for notes below root)
 * @return Semitone offset from C2
 */
int8_t GetScaleNote(ScaleType scale, uint8_t root, int8_t degree);

/**
 * @brief Convert a semitone value to CV voltage
 * @param semitone Semitone offset from C2
 * @return Voltage (0-3V, 1V/octave)
 */
float MelodyNoteToCV(int8_t semitone);

// ============================================================================
// RHYTHM GENERATION
// ============================================================================

/**
 * @brief Generate melody rhythm pattern with given parameters
 * @param seed Random seed
 * @param voice Melody config to update
 * @param style Rhythm style to use
 * @param density Density level
 * @param currentKickPattern Current kick pattern index (for follow kick)
 * @return Generated rhythm pattern
 */
uint32_t GenerateMelodyRhythmWithParams(uint32_t seed, MelodyConfig* voice,
                                         RhythmStyle style, DensityLevel density,
                                         uint8_t currentKickPattern);

/**
 * @brief Generate rhythm pattern for a melody voice
 * @param seed Random seed
 * @param voice Melody config to update
 * @param currentKickPattern Current kick pattern index
 */
void GenerateMelodyRhythmFor(uint32_t seed, MelodyConfig* voice, uint8_t currentKickPattern);

// ============================================================================
// NOTE GENERATION
// ============================================================================

/**
 * @brief Generate note sequence with specific seed
 * @param seed Random seed
 * @param voice Melody config (for style info)
 * @param noteArray Array to fill with notes
 * @param scale Scale type
 * @param root Root note
 */
void GenerateMelodyNotesWithSeed(uint32_t seed, MelodyConfig* voice,
                                  int8_t* noteArray, ScaleType scale, uint8_t root);

/**
 * @brief Generate notes for a melody voice
 * @param seed Random seed
 * @param voice Melody config to update
 * @param scale Scale type
 * @param root Root note
 */
void GenerateMelodyNotesFor(uint32_t seed, MelodyConfig* voice, ScaleType scale, uint8_t root);

// ============================================================================
// FULL PATTERN GENERATION
// ============================================================================

/**
 * @brief Generate complete pattern (rhythm + notes + variations) for a melody voice
 * @param seed Random seed
 * @param voice Melody config to update
 * @param scale Scale type
 * @param root Root note
 * @param currentKickPattern Current kick pattern index
 */
void GenerateMelodyPatternFor(uint32_t seed, MelodyConfig* voice,
                               ScaleType scale, uint8_t root, uint8_t currentKickPattern);

// ============================================================================
// PERSONALITY RANDOMIZATION
// ============================================================================

/**
 * @brief Randomize personality (style, substyle, rhythm style, density) for a voice
 * @param seed Random seed
 * @param voice Melody config to update
 * @param scale Scale type
 * @param root Root note
 * @param currentKickPattern Current kick pattern index
 */
void RandomizeMelodyPersonalityFor(uint32_t seed, MelodyConfig* voice,
                                    ScaleType scale, uint8_t root, uint8_t currentKickPattern);

} // namespace themis

#endif // THEMIS_MELODY_H
