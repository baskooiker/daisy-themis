/**
 * @file globals.h
 * @brief Global variable declarations for Themis
 */

#ifndef THEMIS_GLOBALS_H
#define THEMIS_GLOBALS_H

#include "daisy_patch.h"
#include "daisysp.h"
#include "types.h"

using namespace daisy;
using namespace daisysp;

// ============================================================================
// HARDWARE
// ============================================================================

extern DaisyPatch hw;
extern Metro clockMetro;

// ============================================================================
// CLOCK & TRANSPORT
// ============================================================================

extern bool isRunning;
extern bool externalClockMode;
extern float bpm;
extern uint32_t midiClockCounter;
extern uint32_t lastMidiClockTime;
extern uint32_t midiClockTimeout;
extern bool gateHigh;
extern uint32_t gateHighCounter;

// Gate outputs
extern bool gate24ppqn;
extern bool gate16th;
extern bool gate2;
extern bool gateQuarter;
extern bool gateReset;
extern uint32_t gate24ppqnCounter;
extern uint32_t gate16thCounter;
extern uint32_t gate2Counter;
extern uint32_t gateQuarterCounter;
extern uint32_t gateResetCounter;

// ============================================================================
// UI STATE
// ============================================================================

extern DisplayState currentDisplayState;
extern ConfigOption currentConfigOption;
extern OutDivision currentOut2Division;
extern OutDivision currentOut3Division;
extern bool freezeEnabled;
extern int patternInfoScroll;
extern uint32_t lastEncoderActivity;
extern int configScrollOffset;

// ============================================================================
// PERSISTENT SETTINGS
// ============================================================================

extern PersistentSettings settings;

// ============================================================================
// GROOVE SYSTEM
// ============================================================================

extern GrooveConfig voiceGroove[NUM_DRUM_VOICES];
extern uint8_t currentGroovePattern;
extern float grooveAmount[NUM_DRUM_VOICES];
extern float grooveVelocityAmount[NUM_DRUM_VOICES];

// Trigger queues
extern MidiTrigger triggerQueue[TRIGGER_QUEUE_SIZE];
extern uint8_t triggerQueueHead;
extern uint8_t triggerQueueTail;
extern MelodyTrigger melodyQueue[MELODY_QUEUE_SIZE];
extern uint8_t melodyQueueHead;
extern uint8_t melodyQueueTail;
extern float melodyGrooveAmount;

// Sample timing
extern volatile uint64_t globalSampleCounter;
extern volatile uint64_t lastBeatSample;

// ============================================================================
// SEQUENCER STATE
// ============================================================================

extern uint8_t currentStep;
extern uint8_t barCounter;
extern uint8_t cycleCounter;
extern uint8_t measureCounter;
extern uint32_t patternChangeInterval;
extern uint32_t personalityChangeInterval;
extern uint32_t generationSeed;

// Fill state
extern bool fillScheduled;
extern bool fillActive;
extern bool fillIsHalfBar;
extern uint8_t fillStartStep;
extern uint8_t currentFillSnareIndex;
extern uint8_t currentFillHatClosedIndex;
extern uint8_t currentFillHatOpenIndex;

// ============================================================================
// DRUM PATTERNS
// ============================================================================

extern uint8_t currentKickPattern;
extern DrumVoice fundamentalBeatVoice;
extern VoiceConfig generativeVoices[6];

// Analog voice
extern bool analogGateHigh;
extern uint32_t analogGateCounter;
extern uint8_t analogVoiceVelocity;

// ============================================================================
// MELODY SYSTEM
// ============================================================================

extern MelodyConfig melodyVoice;
extern MelodyConfig melodyMidiVoice;
extern ScaleType melodyScale;
extern uint8_t melodyRoot;
extern uint8_t melodyMidiChannel;
extern uint8_t lastMidiMelodyNote;
extern bool midiMelodyNoteOn;
extern uint64_t midiMelodyNoteOffSample;  // When to send note-off (sample time)
extern bool melodyFreezeEnabled;
extern bool tuneModeEnabled;

// ============================================================================
// TURING MACHINES
// ============================================================================

extern TuringMachine turingCV2;
extern TuringMachine turingCV3;
extern TuringMachine turingCV4;

// ============================================================================
// POLY VOICE (CHORDS)
// ============================================================================

extern PolyVoiceConfig polyVoice;
extern PolyVoiceState polyState;
extern int8_t polyActiveNotes[6];      // Currently held MIDI notes
extern uint8_t polyNumActiveNotes;     // Number of active notes
extern bool polyNotesOn;               // Are notes currently held

// Chord data
extern const ChordShape chordShapes[NUM_CHORD_TYPES];
extern const ChordProgression progressions[NUM_PROGRESSIONS];
extern const char* chordRateNames[NUM_CHORD_RATES];
extern const uint8_t chordRateSteps[NUM_CHORD_RATES];

// ============================================================================
// CONST DATA (defined in globals.cpp)
// ============================================================================

extern const uint8_t drumNotes[NUM_DRUM_VOICES];
extern const char* drumNames[NUM_DRUM_VOICES];
extern const char* configOptionNames[NUM_CONFIG_OPTIONS];
extern const char* outDivisionNames[NUM_OUT_DIVISIONS];
extern const char* scaleNames[NUM_SCALE_TYPES];
extern const char* melodyStyleNames[NUM_MELODY_STYLES];
extern const char* rootNoteNames[12];

// Groove patterns
extern const int8_t groovePatterns[32][16];
extern const int8_t velocityPatterns[32][16];

// Kick patterns
extern const uint32_t kickPatterns[16];

// Clap patterns
extern const uint32_t clapPatterns[16];
extern uint8_t currentClapPattern;
extern uint8_t currentHatPattern;

// Hi-hat patterns
extern const uint32_t hatClosedPatterns[16];
extern const uint32_t hatOpenPatterns[16];

// Fill patterns - Half bar (8 steps)
extern const uint8_t kickFillsHalf[8];
extern const uint8_t snareFillsHalf[8];
extern const uint8_t hatClosedFillsHalf[8];
extern const uint8_t hatOpenFillsHalf[8];

// Fill patterns - Whole bar (16 steps)
extern const uint16_t kickFillsWhole[8];
extern const uint16_t snareFillsWhole[16];
extern const uint16_t hatClosedFillsWhole[8];
extern const uint16_t hatOpenFillsWhole[8];

// Scale data
extern const int8_t scaleMinor[7];
extern const int8_t scaleMinorBlues[6];
extern const int8_t scaleMinorPentatonic[5];
extern const int8_t scaleGypsy[7];
extern const int8_t scaleLengths[NUM_SCALE_TYPES];

// Variation sequence patterns [sequence][segment] -> variation (0=A, 1=B, 2=C)
extern const uint8_t variationSequences[NUM_VARIATION_SEQUENCES][8];

#endif // THEMIS_GLOBALS_H
