/**
 * @file config.h
 * @brief Configuration, settings, and control handling for Themis
 *
 * This module handles:
 * - Persistent settings (save/load to QSPI)
 * - Encoder and button processing
 * - Clock frequency updates
 * - MIDI transport messages
 * - Gate output triggers
 * - Output division calculations
 */

#ifndef THEMIS_CONFIG_H
#define THEMIS_CONFIG_H

#include "types.h"
#include "globals.h"

// ============================================================================
// PERSISTENT STORAGE
// ============================================================================

/**
 * @brief Save current settings to QSPI flash
 */
void SaveSettings();

/**
 * @brief Load settings from QSPI flash
 *
 * If no valid settings found (magic number mismatch),
 * uses defaults and saves them.
 */
void LoadSettings();

// ============================================================================
// CLOCK MANAGEMENT
// ============================================================================

/**
 * @brief Update clock frequency based on current BPM
 *
 * Call when BPM changes. Updates Metro frequency and
 * recalculates groove offsets for all voices.
 */
void UpdateClockFrequency();

// ============================================================================
// GATE TRIGGERS
// ============================================================================

/**
 * @brief Trigger 24 PPQN gate output
 */
void TriggerGate24ppqn();

/**
 * @brief Trigger 16th note gate output (OUT1)
 */
void TriggerGate16th();

/**
 * @brief Trigger reset pulse output (OUT2)
 */
void TriggerGateReset();

/**
 * @brief Trigger melody gate pulse (OUT3)
 */
void TriggerMelodyGate();

/**
 * @brief Trigger bass gate pulse (OUT4)
 */
void TriggerBassGate();

/**
 * @brief Trigger analog drum gate pulse (GATE OUT pin)
 */
void TriggerAnalogDrumGate();

// ============================================================================
// MIDI TRANSPORT
// ============================================================================

/**
 * @brief Send MIDI clock message (0xF8)
 */
void SendMidiClock();

/**
 * @brief Send MIDI start message (0xFA)
 */
void SendMidiStart();

/**
 * @brief Send MIDI stop message (0xFC)
 */
void SendMidiStop();

/**
 * @brief Send MIDI continue message (0xFB)
 */
void SendMidiContinue();

/**
 * @brief Toggle transport run/stop state
 */
void ToggleRunState();

/**
 * @brief Handle incoming MIDI messages
 * @param m The MIDI event to process
 */
void HandleMidiMessage(MidiEvent m);

// ============================================================================
// CONTROLS
// ============================================================================

/**
 * @brief Process encoder and button controls
 *
 * Handles display state navigation, config menu,
 * value editing, and menu timeout.
 */
void ProcessControls();

/**
 * @brief Process clock timing and gate outputs
 *
 * Called in main loop. Handles 16th note trigger flag,
 * gate output timing, and drum pattern processing.
 */
void ProcessClock();

#endif // THEMIS_CONFIG_H
