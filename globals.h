/**
 * @file globals.h
 * @brief Global variable declarations for Themis
 */

#ifndef THEMIS_GLOBALS_H
#define THEMIS_GLOBALS_H

#include "daisy_patch.h"
#include "daisysp.h"
#include "types.h"
#include "core/themis_bass.h"
#include "core/themis_rhythm.h"
#include "core/themis_tr8.h"

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
extern bool gateReset;
extern bool melodyGate;
extern bool bassGate;
extern bool analogDrumGate;
extern uint32_t gate24ppqnCounter;
extern uint32_t gate16thCounter;
extern uint32_t gateResetCounter;
extern uint32_t melodyGateCounter;
extern uint32_t bassGateCounter;
extern uint32_t analogDrumGateCounter;

// ============================================================================
// UI STATE
// ============================================================================

extern DisplayState currentDisplayState;
extern ConfigOption currentConfigOption;
extern bool freezeEnabled;
extern int patternInfoScroll;
extern uint32_t lastEncoderActivity;
extern int configScrollOffset;

// Submenu state
extern FreezeOption currentFreezeOption;
extern SystemOption currentSystemOption;
extern HarmonyOption currentHarmonyOption;
extern VoiceMenuItem currentVoiceMenuItem;
extern VoiceDetailItem currentVoiceDetail;
extern int freezeScrollOffset;
extern int systemScrollOffset;
extern int harmonyScrollOffset;
extern int voiceScrollOffset;
extern int voiceDetailScrollOffset;
extern uint8_t drumMidiChannel;
extern themis::ChordRandomizerConfig chordRandomizerConfig;
extern themis::ChordRandomizerState chordRandomizerState;

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

// ============================================================================
// BASS VOICE
// ============================================================================

extern themis::BassConfig bassVoiceConfig;
extern themis::BassState bassVoiceState;
extern uint8_t bassMidiChannel;
extern int8_t lastBassMidiNote;
extern bool bassNotePlaying;
extern uint64_t bassMidiNoteOffSample;

// ============================================================================
// RHYTHM PLAYER
// ============================================================================

extern themis::RhythmPlayerConfig rhythmPlayerConfig;
extern themis::RhythmPlayerState rhythmPlayerState;
extern uint8_t rhythmMidiChannel;
extern int8_t rhythmActiveNotes[6];   // Currently held MIDI notes
extern uint8_t rhythmNumActiveNotes;  // Number of active notes
extern bool rhythmNotesPlaying;       // Are notes currently held

// ============================================================================
// TR-8 VOICE
// ============================================================================

extern themis::TR8Config tr8VoiceConfig;
extern themis::TR8State tr8VoiceState;
extern uint8_t tr8MidiChannel;

// ============================================================================
// MELODY SYSTEM
// ============================================================================

extern MelodyConfig melodyVoice;
extern MelodyConfig melodyMidiVoice;
extern ScaleType melodyScale;
extern uint8_t melodyRoot;
extern uint8_t melodyChannel;
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
// CHORD VOICE
// ============================================================================

extern ChordVoiceConfig chordVoice;
extern ChordVoiceState chordState;
extern int8_t chordActiveNotes[6];      // Currently held MIDI notes
extern uint8_t chordNumActiveNotes;     // Number of active notes
extern bool chordNotesOn;               // Are notes currently held

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
extern const char* freezeOptionNames[NUM_FREEZE_OPTIONS];
extern const char* systemOptionNames[NUM_SYSTEM_OPTIONS];
extern const char* harmonyOptionNames[NUM_HARMONY_OPTIONS];
extern const char* voiceMenuNames[NUM_VOICE_MENU_ITEMS];
extern const char* voiceDetailNames[NUM_VOICE_DETAIL_ITEMS];
extern const char* rhythmModeNames[2];
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
