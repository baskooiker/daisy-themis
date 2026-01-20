/**
 * @file themis_data.h
 * @brief Const data tables for Themis (patterns, scales, grooves)
 */

#ifndef THEMIS_DATA_H
#define THEMIS_DATA_H

#include "themis_types.h"

namespace themis {

// ============================================================================
// DRUM DATA
// ============================================================================

extern const uint8_t drumNotes[NUM_DRUM_VOICES];
extern const char* drumNames[NUM_DRUM_VOICES];

// ============================================================================
// GROOVE PATTERNS (32 patterns, 16 steps each)
// ============================================================================

extern const int8_t groovePatterns[32][16];
extern const int8_t velocityPatterns[32][16];

// ============================================================================
// KICK PATTERNS (16 patterns, 32 steps each)
// ============================================================================

extern const uint32_t kickPatterns[16];

// ============================================================================
// CLAP PATTERNS
// ============================================================================

extern const uint32_t clapPatterns[16];

// ============================================================================
// HI-HAT PATTERNS
// ============================================================================

extern const uint32_t hatClosedPatterns[16];
extern const uint32_t hatOpenPatterns[16];

// ============================================================================
// FILL PATTERNS - HALF BAR (8 steps)
// ============================================================================

extern const uint8_t kickFillsHalf[8];
extern const uint8_t snareFillsHalf[8];
extern const uint8_t hatClosedFillsHalf[8];
extern const uint8_t hatOpenFillsHalf[8];

// ============================================================================
// FILL PATTERNS - WHOLE BAR (16 steps)
// ============================================================================

extern const uint16_t kickFillsWhole[8];
extern const uint16_t snareFillsWhole[16];
extern const uint16_t snareFillsOneBar[16];
extern const uint16_t hatClosedFillsWhole[8];
extern const uint16_t hatOpenFillsWhole[8];
extern const uint16_t hatClosedFillsOneBar[8];
extern const uint16_t hatOpenFillsOneBar[8];

// ============================================================================
// SCALE DATA
// ============================================================================

extern const int8_t scaleMinor[7];
extern const int8_t scaleMinorBlues[6];
extern const int8_t scaleMinorPentatonic[5];
extern const int8_t scaleGypsy[7];
extern const int8_t scaleLengths[NUM_SCALE_TYPES];
extern const char* scaleNames[NUM_SCALE_TYPES];

// ============================================================================
// VARIATION SEQUENCES
// ============================================================================

extern const uint8_t variationSequences[NUM_VARIATION_SEQUENCES][8];

// ============================================================================
// NAME STRINGS
// ============================================================================

extern const char* outDivisionNames[NUM_OUT_DIVISIONS];
extern const char* melodyStyleNames[NUM_MELODY_STYLES];
extern const char* rootNoteNames[12];
extern const char* rhythmStyleNames[NUM_RHYTHM_STYLES];
extern const char* densityNames[NUM_DENSITY_LEVELS];
extern const char* variationModeNames[NUM_VARIATION_MODES];
extern const char* variationSequenceNames[NUM_VARIATION_SEQUENCES];
extern const char* interactionStyleNames[NUM_INTERACTION_STYLES];

} // namespace themis

#endif // THEMIS_DATA_H
