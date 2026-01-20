/**
 * @file groove.h
 * @brief Groove timing and trigger queue system for Themis
 *
 * This module handles:
 * - Sample-accurate trigger scheduling via queues
 * - Groove pattern timing offsets
 * - Velocity pattern modulation
 * - Both drum and melody trigger queues
 */

#ifndef THEMIS_GROOVE_H
#define THEMIS_GROOVE_H

#include "types.h"
#include "globals.h"

// ============================================================================
// QUEUE MANAGEMENT
// ============================================================================

/**
 * @brief Initialize both drum and melody trigger queues
 */
void InitTriggerQueue();

// ============================================================================
// GROOVE CONFIGURATION
// ============================================================================

/**
 * @brief Randomize groove pattern and per-voice amounts
 *
 * Selects a random groove pattern (0-31) and assigns random
 * timing/velocity amounts to each voice. Kick drum gets reduced
 * timing variation (0-35%) while other voices get 0-75%.
 */
void RandomizeGroove();

// ============================================================================
// DRUM TRIGGER SCHEDULING
// ============================================================================

/**
 * @brief Calculate groove offset in samples for a drum voice
 * @param voice The drum voice
 * @param step Current step in the pattern (0-31)
 * @return Sample offset (positive = delayed, negative = early)
 */
int32_t CalculateGrooveOffset(DrumVoice voice, uint8_t step);

/**
 * @brief Calculate velocity with groove pattern applied
 * @param voice The drum voice
 * @param baseVelocity Base MIDI velocity (0-127)
 * @param step Current step in the pattern (0-31)
 * @return Modified velocity (1-127)
 */
uint8_t CalculateGrooveVelocity(DrumVoice voice, uint8_t baseVelocity, uint8_t step);

/**
 * @brief Schedule a drum trigger at an absolute sample time
 * @param voice The drum voice to trigger
 * @param velocity MIDI velocity (1-127)
 * @param fireSample Absolute sample count when trigger should fire
 */
void ScheduleDrumTrigger(DrumVoice voice, uint8_t velocity, uint64_t fireSample);

/**
 * @brief Schedule a drum trigger with groove timing and velocity applied
 * @param voice The drum voice to trigger
 * @param baseVelocity Base MIDI velocity before groove modulation
 * @param step Current step (for groove lookup)
 * @param nextBeatSample Sample time of the next beat
 */
void ScheduleDrumTriggerWithGroove(DrumVoice voice, uint8_t baseVelocity,
                                    uint8_t step, uint64_t nextBeatSample);

/**
 * @brief Process drum trigger queue (call from audio callback)
 *
 * Checks all queued triggers and fires any that have reached
 * their scheduled sample time.
 */
void ProcessTriggerQueue();

// ============================================================================
// MELODY TRIGGER SCHEDULING
// ============================================================================

/**
 * @brief Calculate groove offset in samples for melody
 * @param step Current step in the pattern (0-31)
 * @return Sample offset (positive = delayed, negative = early)
 */
int32_t CalculateMelodyGrooveOffset(uint8_t step);

/**
 * @brief Schedule a melody trigger with groove timing
 * @param voiceType CV or MIDI melody output
 * @param note Semitone value (0 = C2)
 * @param step Current step (for groove lookup)
 * @param nextBeatSample Sample time of the next beat
 */
void ScheduleMelodyTrigger(MelodyVoiceType voiceType, int8_t note,
                           uint8_t step, uint64_t nextBeatSample);

/**
 * @brief Process melody trigger queue (call from audio callback)
 *
 * Checks all queued melody triggers and fires any that have
 * reached their scheduled sample time.
 */
void ProcessMelodyQueue();

// ============================================================================
// MIDI OUTPUT
// ============================================================================

/**
 * @brief Send a MIDI note-on for a drum voice
 * @param voice The drum voice to trigger
 * @param velocity MIDI velocity (1-127)
 */
void TriggerDrum(DrumVoice voice, uint8_t velocity = 100);

/**
 * @brief Send note-off for currently playing MIDI melody note
 *
 * Call this when stopping the sequencer or before randomizing patterns
 * to ensure clean note termination.
 */
void SendMelodyNoteOff();

#endif // THEMIS_GROOVE_H
