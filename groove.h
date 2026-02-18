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

// ============================================================================
// CHORD VOICE MIDI OUTPUT
// ============================================================================

/**
 * @brief Get the notes for a chord based on root note and chord type
 * @param rootNote Root note in semitones (0 = C)
 * @param chordType Type of chord (CHORD_MAJOR, CHORD_MINOR, etc.)
 * @param octaveOffset Octave offset (-2 to +2)
 * @param outNotes Array to fill with MIDI note numbers (size 6)
 * @return Number of notes in the chord
 */
uint8_t GetChordNotes(int8_t rootNote, ChordType chordType, int8_t octaveOffset, int8_t* outNotes);

/**
 * @brief Send note-on for all notes in a chord
 * @param notes Array of MIDI note numbers
 * @param numNotes Number of notes to send
 * @param velocity MIDI velocity (1-127)
 */
void SendChordOn(const int8_t* notes, uint8_t numNotes, uint8_t velocity);

/**
 * @brief Send note-off for all currently active chord voice notes
 *
 * Call this when stopping the sequencer, changing chords, or
 * before deactivating the chord voice.
 */
void SendChordNoteOff();

/**
 * @brief Process chord voice for the current step
 * @param step Current step in the pattern (0-31)
 *
 * Handles chord progression timing and triggers chord changes
 * based on the configured chord rate.
 */
void ProcessChordStep(uint8_t step);

/**
 * @brief Initialize chord voice state
 */
void InitChordVoice();

/**
 * @brief Randomize chord voice settings using vibe system
 *
 * Selects a random enabled vibe, picks a progression, randomizes
 * chord rate and octave offset.
 */
void RandomizeChordVoice();

#endif // THEMIS_GROOVE_H
