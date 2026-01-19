/**
 * @file melody.h
 * @brief Melody generation system for Themis
 *
 * This module handles:
 * - Scale note calculation
 * - Melody rhythm pattern generation
 * - Melody note sequence generation
 * - CV/MIDI melody voice configuration
 */

#ifndef THEMIS_MELODY_H
#define THEMIS_MELODY_H

#include "types.h"
#include "globals.h"

// ============================================================================
// SCALE UTILITIES
// ============================================================================

/**
 * @brief Get a note from the scale at a given degree
 * @param scale The scale type to use
 * @param root Root note (0-11, where 0 = C)
 * @param degree Scale degree (can span multiple octaves)
 * @return Semitone value relative to C2 (0 = C2)
 */
int8_t GetScaleNote(ScaleType scale, uint8_t root, int8_t degree);

/**
 * @brief Convert semitone to CV voltage
 * @param semitone Semitone value (0 = C2)
 * @return CV voltage (0-3V for 3 octaves, 1V/octave)
 */
float MelodyNoteToCV(int8_t semitone);

// ============================================================================
// PATTERN GENERATION
// ============================================================================

/**
 * @brief Generate rhythm pattern for a melody voice
 * @param voice Pointer to the melody voice config
 *
 * Generates rhythm based on the voice's style and sub-style.
 * Supporting style uses sparse patterns following kick or own rhythm.
 * Arpeggiator uses denser repeating patterns.
 */
void GenerateMelodyRhythmFor(MelodyConfig* voice);

/**
 * @brief Generate note sequence for a melody voice
 * @param voice Pointer to the melody voice config
 *
 * Uses shared melodyScale and melodyRoot for all voices.
 * Supporting style uses few distinct notes (root, 5th, 3rd).
 * Arpeggiator cycles through chord tones or scale degrees.
 */
void GenerateMelodyNotesFor(MelodyConfig* voice);

/**
 * @brief Generate complete melody pattern (rhythm + notes)
 * @param voice Pointer to the melody voice config
 */
void GenerateMelodyPatternFor(MelodyConfig* voice);

/**
 * @brief Generate patterns for both CV and MIDI melody voices
 */
void GenerateMelodyPattern();

// ============================================================================
// PERSONALITY RANDOMIZATION
// ============================================================================

/**
 * @brief Randomize personality for a single melody voice
 * @param voice Pointer to the melody voice config
 *
 * Randomizes style, sub-style, and generates new pattern.
 */
void RandomizeMelodyPersonalityFor(MelodyConfig* voice);

/**
 * @brief Randomize personality for both CV and MIDI melody voices
 */
void RandomizeMelodyPersonality();

/**
 * @brief Randomize all parameters (drums + melody)
 *
 * Called from config menu "Randomize!" option.
 */
void RandomizeAllParameters();

#endif // THEMIS_MELODY_H
