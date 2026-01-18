#include "daisy_patch.h"
#include "daisysp.h"
#include <string>

using namespace daisy;
using namespace daisysp;

DaisyPatch hw;
Metro      clockMetro; // For generating clock pulses

// Clock state
bool     isRunning = false;
bool     externalClockMode = false;
float    bpm = 120.0f;
uint32_t midiClockCounter = 0;
uint32_t lastMidiClockTime = 0;
uint32_t midiClockTimeout = 500; // ms timeout for external clock detection
bool     gateHigh = false;
uint32_t gateHighCounter = 0;
const uint32_t GATE_PULSE_MS = 10; // Gate pulse width in milliseconds

// Audio output gate states
bool gate24ppqn = false;    // (unused - was Audio Out 1)
bool gate16th = false;      // Audio Out 1: 16th notes
bool gate2 = false;         // Audio Out 2: Configurable division
bool gateQuarter = false;   // Audio Out 3: Configurable division
bool gateReset = false;     // Audio Out 4: Reset pulse

// Gate pulse counters (in audio samples)
uint32_t gate24ppqnCounter = 0;
uint32_t gate16thCounter = 0;
uint32_t gate2Counter = 0;
uint32_t gateQuarterCounter = 0;
uint32_t gateResetCounter = 0;

// MIDI Clock constants
const uint8_t MIDI_CLOCK_PPQN = 24; // Pulses per quarter note
const uint8_t CLOCKS_PER_16TH = 6;  // 24 / 4 = 6 clocks per 16th note
const uint8_t CLOCKS_PER_QUARTER = 24; // 24 clocks per quarter note

// Gate pulse width in audio samples (10ms at 48kHz = 480 samples)
const uint32_t GATE_PULSE_SAMPLES = 480;
const uint32_t RESET_PULSE_SAMPLES = 960; // Longer reset pulse (20ms)

// Vermona DRM1 mk3 MIDI Note Definitions
// Factory default MIDI channel: 10 (0-indexed: 9)
const uint8_t DRM1_MIDI_CHANNEL = 9; // MIDI channel 10 (0-indexed)

// Drum voice MIDI note numbers
enum DrumVoice
{
    KICK = 0,
    DRUM1,
    DRUM2,
    MULTI,
    SNARE,
    HIHAT1_CLOSED,
    HIHAT1_OPEN,
    HIHAT2_CLOSED,
    HIHAT2_OPEN,
    CLAP,
    ANALOG,  // CV/Gate output voice (not MIDI)
    NUM_DRUM_VOICES
};

// MIDI note numbers for each drum voice
const uint8_t drumNotes[NUM_DRUM_VOICES] = {
    36, // KICK (C)
    48, // DRUM1 (c)
    41, // DRUM2 (F)
    58, // MULTI (b)
    40, // SNARE (E)
    49, // HIHAT1_CLOSED (cis)
    51, // HIHAT1_OPEN (dis)
    42, // HIHAT2_CLOSED (FIS)
    44, // HIHAT2_OPEN (GIS)
    39, // CLAP (DIS)
    60  // ANALOG (note value for CV conversion, middle C = 2.5V)
};

// Drum voice names for display
const char* drumNames[NUM_DRUM_VOICES] = {
    "Kick",
    "Drm1",
    "Drm2",
    "Mult",
    "Snare",
    "HH1C",
    "HH1O",
    "HH2C",
    "HH2O",
    "Clap",
    "Anlg"
};

// ========================================
// Display and Menu System
// ========================================

enum DisplayState
{
    DISPLAY_DEFAULT,
    DISPLAY_CONFIG_MENU,
    DISPLAY_CONFIG_EDIT,
    DISPLAY_PATTERN_INFO
};

enum ConfigOption
{
    CONFIG_BPM,
    CONFIG_OUT2_DIVISION,
    CONFIG_OUT3_DIVISION,
    CONFIG_FREEZE,
    CONFIG_MELODY_SCALE,
    CONFIG_MELODY_ROOT,
    CONFIG_CV_STYLE,
    CONFIG_MIDI_STYLE,
    CONFIG_MIDI_MEL_CH,
    CONFIG_MELODY_FREEZE,
    CONFIG_TUNE_MODE,
    CONFIG_RANDOMIZE_ALL,
    CONFIG_PATTERN_INFO,
    CONFIG_BACK,
    NUM_CONFIG_OPTIONS
};

const char* configOptionNames[NUM_CONFIG_OPTIONS] = {
    "BPM",
    "OUT2 div",
    "OUT3 div",
    "DrumFreeze",
    "Scale",
    "Root",
    "CV Style",
    "MIDI Style",
    "MIDI Ch",
    "MelFreeze",
    "TuneMode",
    "Randomize!",
    "Pattern info",
    "Back"
};

enum OutDivision
{
    DIV_1_16,  // 1/16 note
    DIV_1_8,   // 1/8 note
    DIV_1_4,   // 1/4 note (quarter)
    DIV_1_2,   // 1/2 note (half)
    DIV_1,     // 1 bar (4 beats)
    DIV_2,     // 2 bars
    DIV_4,     // 4 bars
    NUM_OUT_DIVISIONS
};

const char* outDivisionNames[NUM_OUT_DIVISIONS] = {
    "1/16",
    "1/8",
    "1/4",
    "1/2",
    "1",
    "2",
    "4"
};

DisplayState currentDisplayState = DISPLAY_DEFAULT;
ConfigOption currentConfigOption = CONFIG_BPM;
OutDivision currentOut2Division = DIV_1_8; // Default to 8th notes
OutDivision currentOut3Division = DIV_1_4; // Default to quarter notes
bool freezeEnabled = false; // When true, patterns/personalities don't randomize
int patternInfoScroll = 0; // Scroll offset for pattern info display
uint32_t lastEncoderActivity = 0;
const uint32_t MENU_TIMEOUT_MS = 10000; // 10 seconds

// ========================================
// Persistent Storage
// ========================================

#define SETTINGS_MAGIC 0x54484D53 // "THMS" magic number for validation

struct PersistentSettings
{
    uint32_t magic;           // Magic number to validate settings
    float bpm;                // BPM setting
    uint8_t out2Division;     // OUT2 division setting
    uint8_t out3Division;     // OUT3 division setting
    uint8_t freezeEnabled;    // Freeze drum pattern/personality randomization
    uint8_t melodyScale;      // Shared melody scale type
    uint8_t melodyRoot;       // Shared melody root note (0-11)
    uint8_t cvMelodyStyle;    // CV Melody style
    uint8_t midiMelodyStyle;  // MIDI Melody style
    uint8_t midiMelChannel;   // MIDI Melody channel (0-15)
    uint8_t melodyFreezeEnabled; // Freeze melody randomization
    uint8_t reserved[17];     // Reserved for future use (total 32 bytes)
};

PersistentSettings settings;

// ========================================
// Per-Voice Groove Timing System
// ========================================

// Groove configuration for each voice
struct GrooveConfig
{
    float groovePercent;    // Offset as % of 16th note (-100.0 to +100.0)
    int32_t offsetSamples;  // Calculated sample offset
    static const int32_t MAX_OFFSET_MS = 10; // Maximum offset in milliseconds

    void Init()
    {
        groovePercent = 0.0f;
        offsetSamples = 0;
    }

    // Calculate sample offset from percentage and current tempo
    // Call whenever BPM changes or groove percentage changes
    void UpdateOffset(float currentBPM, float sampleRate)
    {
        // Calculate 16th note duration in samples
        float samplesPerSixteenth = sampleRate * 15.0f / currentBPM;

        // Calculate offset in samples from percentage
        float offsetFloat = (groovePercent / 100.0f) * samplesPerSixteenth;

        // Clamp to ±10ms maximum
        int32_t maxOffsetSamples = (int32_t)(MAX_OFFSET_MS * sampleRate / 1000.0f);
        offsetSamples = (int32_t)fclamp(offsetFloat,
                                        (float)-maxOffsetSamples,
                                        (float)maxOffsetSamples);
    }
};

// MIDI trigger queue entry
struct MidiTrigger
{
    DrumVoice voice;
    uint8_t velocity;
    uint64_t fireSample;     // Absolute sample counter when to fire
    bool active;

    void Init()
    {
        active = false;
        fireSample = 0;
        voice = KICK;
        velocity = 0;
    }
};

// Global groove configuration array
GrooveConfig voiceGroove[NUM_DRUM_VOICES];

// Ring buffer for scheduled triggers
const uint8_t TRIGGER_QUEUE_SIZE = 32; // Support up to 32 queued triggers
MidiTrigger triggerQueue[TRIGGER_QUEUE_SIZE];
uint8_t triggerQueueHead = 0;
uint8_t triggerQueueTail = 0;

// Melody trigger structure (for groove-timed melody notes)
enum MelodyVoiceType { MELODY_CV, MELODY_MIDI };
struct MelodyTrigger
{
    MelodyVoiceType voiceType;
    int8_t note;             // Semitone value for CV or MIDI note
    uint64_t fireSample;     // When to trigger
    bool active;

    void Init()
    {
        active = false;
        fireSample = 0;
        note = 0;
        voiceType = MELODY_CV;
    }
};

// Melody trigger queue
const uint8_t MELODY_QUEUE_SIZE = 16;
MelodyTrigger melodyQueue[MELODY_QUEUE_SIZE];
uint8_t melodyQueueHead = 0;
uint8_t melodyQueueTail = 0;

// Melody groove amounts (similar to drum voices)
float melodyGrooveAmount = 0.5f;  // 50% groove timing for melody

// Global sample counter (sample-accurate timing)
volatile uint64_t globalSampleCounter = 0;
volatile uint64_t lastBeatSample = 0; // Sample time when last beat occurred

// ========================================
// Groove Pattern System (32 patterns from Korg Drumlogue)
// ========================================
// Timing offsets per step (16 steps per bar) as percentage of 16th note duration
// Positive = delayed/laid back, Negative = early/pushed
// Velocity is percentage multiplier (100 = normal, 120 = louder, 80 = softer)

const int8_t groovePatterns[32][16] = {
    // 0: Swing16 - Classic 16th note swing
    {0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80},
    // 1: Swing8 - 8th note swing
    {0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0},
    // 2: Swing6 - Triplet swing
    {0, 0, 70, 0, 0, 70, 0, 0, 70, 0, 0, 70, 0, 0, 70, 0},
    // 3: Conga1 - Latin 2-3 clave feel
    {0, 0, -12, 30, 0, 0, -18, 0, 0, -12, 30, 0, -18, 0, 0, 18},
    // 4: Conga2 - 3-2 reverse clave
    {0, 0, -18, 0, 0, -12, 30, 0, -18, 0, 0, 18, 0, 0, -12, 30},
    // 5: Bongo1 - Alternating high/low
    {0, 18, -12, 18, 0, 18, -12, 18, 0, 18, -12, 18, 0, 18, -12, 18},
    // 6: Bongo2 - Galloping rhythm
    {0, 0, 20, -10, 0, 0, 20, -10, 0, 0, 20, -10, 0, 0, 20, -10},
    // 7: Cabasa1 - Rushed shuffle
    {0, -12, -18, -12, 0, -12, -18, -12, 0, -12, -18, -12, 0, -12, -18, -12},
    // 8: Cabasa2 - Lazy shuffle
    {0, 15, 20, 15, 0, 15, 20, 15, 0, 15, 20, 15, 0, 15, 20, 15},
    // 9: Claves1 - Son clave
    {0, 0, 0, -18, 0, 0, -24, 0, 0, -18, 0, 0, 0, -24, 0, 0},
    // 10: Claves2 - Rumba clave
    {0, 0, 0, -20, 0, 0, -25, 0, 0, 0, -20, 0, 0, 0, -25, 0},
    // 11: Cowbell - Driving, ahead of beat
    {0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12},
    // 12: Agogo1 - Brazilian call-response
    {0, 0, -18, 12, 0, 0, -18, 12, 0, 0, -18, 12, 0, 0, -18, 12},
    // 13: Agogo2 - Samba pattern
    {0, -15, 0, 20, 0, -15, 0, 20, 0, -15, 0, 20, 0, -15, 0, 20},
    // 14: Tambourine - Offbeats delayed
    {0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24},
    // 15: Finger1 - Bold swing
    {0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 50},
    // 16: Finger2 - Subtle swing
    {0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 30},
    // 17: Lofi1 - Drunk drummer
    {0, 48, 36, 48, 24, 48, 36, 48, 24, 48, 36, 48, 24, 48, 36, 48},
    // 18: Lofi2 - Tape flutter
    {-5, 40, -10, 45, 5, 35, -8, 42, -3, 38, -12, 44, 8, 36, -6, 40},
    // 19: Baile1 - Baile funk dotted
    {0, 0, -18, 0, 0, -18, 0, 12, 0, 0, -18, 0, 0, -18, 0, 12},
    // 20: Baile2 - Funk swing
    {0, 0, 50, 0, 0, 0, 50, -10, 0, 0, 50, 0, 0, 0, 50, -10},
    // 21: OvalGroove - Elliptical breathing
    {0, -18, -30, -18, 12, 24, 30, 24, 0, -18, -30, -18, 12, 24, 30, 24},
    // 22: Afrobeat - Fela-style polyrhythm
    {0, 0, 0, 25, 0, -15, 0, 30, 0, 0, -10, 25, 0, 0, 0, 28},
    // 23: HipHop1 - J Dilla-inspired swing
    {0, 45, 0, 40, 0, 50, 0, 38, 0, 48, 0, 42, 0, 47, 0, 40},
    // 24: HipHop2 - MPC swing
    {0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55},
    // 25: Techno - Driving forward momentum
    {-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8},
    // 26: House - Classic 4/4 shuffle
    {0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20},
    // 27: Broken - UK garage 2-step
    {0, 0, 40, 0, 0, 0, -15, 35, 0, 0, 45, 0, 0, -10, 0, 38},
    // 28: DnB - Jungle breaks
    {0, -20, 60, -25, 0, -18, 65, -22, 0, -20, 62, -24, 0, -19, 64, -23},
    // 29: Syncopation - Last 16th anticipated
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -50},
    // 30: Crescendo - Progressive acceleration
    {48, 42, 36, 30, 24, 18, 12, 6, 0, -6, -12, -18, -24, -30, -36, -42},
    // 31: Decrescendo - Progressive deceleration
    {-42, -36, -30, -24, -18, -12, -6, 0, 6, 12, 18, 24, 30, 36, 42, 48}
};

// Velocity patterns (percentage of base velocity: 100 = normal, 120 = +20%, 80 = -20%)
const int8_t velocityPatterns[32][16] = {
    // 0: Swing16 - Emphasize swung notes
    {100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110},
    // 1: Swing8 - Strong offbeats
    {100, 100, 115, 100, 100, 100, 115, 100, 100, 100, 115, 100, 100, 100, 115, 100},
    // 2: Swing6 - Triplet accents
    {105, 95, 110, 105, 95, 110, 105, 95, 110, 105, 95, 110, 105, 95, 110, 105},
    // 3: Conga1 - Clave accents
    {100, 100, 90, 115, 100, 100, 85, 100, 100, 90, 115, 100, 85, 100, 100, 110},
    // 4: Conga2 - Reverse clave accents
    {100, 100, 85, 100, 100, 90, 115, 100, 85, 100, 100, 110, 100, 100, 90, 115},
    // 5: Bongo1 - High/low dynamics
    {100, 115, 85, 115, 100, 115, 85, 115, 100, 115, 85, 115, 100, 115, 85, 115},
    // 6: Bongo2 - Gallop accents
    {105, 95, 115, 90, 105, 95, 115, 90, 105, 95, 115, 90, 105, 95, 115, 90},
    // 7: Cabasa1 - Even shuffle
    {100, 95, 90, 95, 100, 95, 90, 95, 100, 95, 90, 95, 100, 95, 90, 95},
    // 8: Cabasa2 - Lazy feel
    {100, 105, 110, 105, 100, 105, 110, 105, 100, 105, 110, 105, 100, 105, 110, 105},
    // 9: Claves1 - Son clave dynamics
    {105, 100, 100, 90, 100, 100, 85, 100, 100, 90, 100, 100, 100, 85, 100, 100},
    // 10: Claves2 - Rumba accents
    {105, 100, 100, 88, 100, 100, 83, 100, 100, 100, 88, 100, 100, 100, 83, 100},
    // 11: Cowbell - Driving pulse
    {110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105},
    // 12: Agogo1 - Call and response
    {100, 100, 90, 110, 100, 100, 90, 110, 100, 100, 90, 110, 100, 100, 90, 110},
    // 13: Agogo2 - Samba bounce
    {105, 95, 100, 115, 105, 95, 100, 115, 105, 95, 100, 115, 105, 95, 100, 115},
    // 14: Tambourine - Offbeat accents
    {100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110, 100, 110},
    // 15: Finger1 - Bold accents
    {100, 120, 100, 120, 100, 120, 100, 120, 100, 120, 100, 120, 100, 120, 100, 115},
    // 16: Finger2 - Subtle variations
    {100, 108, 100, 108, 100, 108, 100, 108, 100, 108, 100, 108, 100, 108, 100, 105},
    // 17: Lofi1 - Random variations
    {95, 115, 105, 110, 92, 112, 108, 116, 98, 114, 102, 118, 96, 113, 106, 117},
    // 18: Lofi2 - Tape compression
    {88, 118, 85, 120, 92, 112, 87, 117, 90, 115, 83, 119, 94, 110, 86, 116},
    // 19: Baile1 - Funk ghost notes
    {100, 100, 85, 100, 100, 85, 100, 110, 100, 100, 85, 100, 100, 85, 100, 110},
    // 20: Baile2 - Pocket groove
    {100, 100, 115, 100, 100, 100, 115, 90, 100, 100, 115, 100, 100, 100, 115, 90},
    // 21: OvalGroove - Wave dynamics
    {100, 95, 85, 90, 105, 115, 120, 115, 100, 95, 85, 90, 105, 115, 120, 115},
    // 22: Afrobeat - Polyrhythmic accents
    {105, 100, 100, 115, 100, 90, 100, 118, 105, 100, 92, 115, 100, 100, 100, 116},
    // 23: HipHop1 - J Dilla dynamics
    {100, 108, 100, 105, 100, 112, 100, 103, 100, 110, 100, 106, 100, 109, 100, 104},
    // 24: HipHop2 - MPC punch
    {100, 115, 100, 115, 100, 115, 100, 115, 100, 115, 100, 115, 100, 115, 100, 120},
    // 25: Techno - Machine precision
    {105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105, 105},
    // 26: House - 4/4 bounce
    {105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110, 105, 110},
    // 27: Broken - UK dynamics
    {100, 100, 115, 100, 100, 100, 90, 112, 100, 100, 118, 100, 100, 88, 100, 114},
    // 28: DnB - Amen breaks
    {105, 90, 120, 88, 105, 92, 122, 87, 105, 90, 121, 89, 105, 91, 123, 88},
    // 29: Syncopation - Build to anticipation
    {95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 110, 85},
    // 30: Crescendo - Growing intensity
    {80, 84, 88, 92, 96, 100, 104, 108, 112, 116, 120, 124, 127, 127, 127, 127},
    // 31: Decrescendo - Fading intensity
    {127, 127, 127, 127, 124, 120, 116, 112, 108, 104, 100, 96, 92, 88, 84, 80}
};

// Groove pattern names for display
const char* groovePatternNames[32] = {
    "Swing16",      // 0
    "Swing8",       // 1
    "Swing6",       // 2
    "Conga1",       // 3
    "Conga2",       // 4
    "Bongo1",       // 5
    "Bongo2",       // 6
    "Cabasa1",      // 7
    "Cabasa2",      // 8
    "Claves1",      // 9
    "Claves2",      // 10
    "Cowbell",      // 11
    "Agogo1",       // 12
    "Agogo2",       // 13
    "Tambourine",   // 14
    "Finger1",      // 15
    "Finger2",      // 16
    "Lofi1",        // 17
    "Lofi2",        // 18
    "Baile1",       // 19
    "Baile2",       // 20
    "OvalGroove",   // 21
    "Afrobeat",     // 22
    "HipHop1",      // 23
    "HipHop2",      // 24
    "Techno",       // 25
    "House",        // 26
    "Broken",       // 27
    "DnB",          // 28
    "Syncopation",  // 29
    "Crescendo",    // 30
    "Decrescendo"   // 31
};

// Current groove state
uint8_t currentGroovePattern = 0;  // Index into groovePatterns (0-31)
float grooveAmount[NUM_DRUM_VOICES]; // Per-voice timing groove amount (0.0 - 0.75, max 75%)
float grooveVelocityAmount[NUM_DRUM_VOICES]; // Per-voice velocity groove amount (0.0 - 1.0, max 100%)

// ========================================
// Generative Drum Pattern System
// ========================================
// Patterns are 32-step (16th notes = 2 bars in 4/4)
// Each bit represents a step (1 = trigger, 0 = silence)
// Bit 0 = step 1, Bit 31 = step 32

// 16 Kick Patterns - Four-to-the-floor emphasis with variations (32 steps)
const uint32_t kickPatterns[16] = {
    0b10001000100010001000100010001000, // 0: Pure four-on-floor
    0b10011000100010001000100010001000, // 1: Four-floor + 16th bar 1
    0b10001000101010001000100010001000, // 2: Four-floor + syncopation bar 1
    0b10001001100010001000100010001000, // 3: Four-floor + double bar 1
    0b10001000100010011000100010001000, // 4: Four-floor + anticipation
    0b10101000100010001000100010001000, // 5: Four-floor + offbeat bar 1
    0b10001000100010001001100010001000, // 6: Variation in bar 2
    0b10011000101010001000100010001000, // 7: Busier bar 1
    0b10001010100010001000100010001000, // 8: Syncopated bar 1
    0b10001000100010101000100010001000, // 9: Syncopated both bars
    0b10101000101010001010100010101000, // 10: More offbeats throughout
    0b10001100100010001000110010001000, // 11: Double kicks both bars
    0b10001000100011001000100010001100, // 12: Double kick beat 4 both bars
    0b10011001100010001000100010001000, // 13: Busy first bar
    0b10001000100010001001100110001000, // 14: Busy second bar
    0b10001000100010000000100010001000  // 15: Broken four-floor (more subtle)
};

// 16 Clap Patterns - Mix of straight (2,4) and syncopated (32 steps)
const uint32_t clapPatterns[16] = {
    0b00001000000010000000100000001000, // 0: Classic backbeat both bars
    0b00001000000010010000100000001000, // 1: Backbeat + anticipation bar 1
    0b00001001000010000000100000001000, // 2: Backbeat + syncopation bar 1
    0b00001000000110000000100000001000, // 3: Backbeat + double on 4 bar 1
    0b00001010000010000000100000001000, // 4: Syncopated backbeat bar 1
    0b00011000000010000000100000001000, // 5: Early clap bar 1
    0b00001000001010000000100000001000, // 6: Extra claps bar 1
    0b00001000010010000000100000001000, // 7: Syncopated bar 1
    0b00001000000010000010100000001000, // 8: Syncopated bar 2
    0b00001000100010000000100010001000, // 9: Extra on 2-and both bars
    0b00001100000010000000110000001000, // 10: Double clap on 2 both bars
    0b00001000000010100000100000001010, // 11: Shuffle on 4 both bars
    0b01001000000010000000100000001000, // 12: Very early bar 1
    0b00001000010010010000100001001000, // 13: Busy syncopation both bars
    0b00011000001010000001100000101000, // 14: Complex rhythm both bars
    0b00101000100010000010100010001000  // 15: Techno shuffle both bars
};

// 16 Hi-Hat Patterns - Combined Open/Closed (32 steps)
// Closed hi-hat patterns - more subtle, emphasis on off-beats
const uint32_t hatClosedPatterns[16] = {
    0b01010101010101010101010101010101, // 0: Classic 8th note offbeats
    0b01010101010001010101010101010101, // 1: Missing one hit bar 1
    0b01010101000101010101010100010101, // 2: Gaps both bars
    0b01010001010100010101000101010001, // 3: Sparser, gaps in both bars
    0b01010101010101010101010101010001, // 4: Missing last hit bar 2
    0b01010101010101010101010001010101, // 5: Gap in bar 2
    0b01010001010101010101010101010101, // 6: Gap early bar 1
    0b01010101010001010101010101010001, // 7: Two gaps
    0b01010101010101010001010101010101, // 8: Gap mid bar 2
    0b01010001010101010101010001010101, // 9: Alternating gaps
    0b01010101010100010101010101010001, // 10: Late gaps both bars
    0b01010101010101010101010101010101, // 11: Classic 8th (was full 16ths)
    0b01010101000101010101010101000101, // 12: Regular gaps
    0b01000101010101010100010101010101, // 13: Early gaps
    0b01010101010101000101010101010100, // 14: End gaps
    0b01000101010001010100010101000101  // 15: Sparse pattern
};

// Open hi-hat patterns - 8th note off-beats (steps 2,6,10,14 per bar)
const uint32_t hatOpenPatterns[16] = {
    0b00100010001000100010001000100010, // 0: Classic 8th off-beats all
    0b00100010001000100010001000000000, // 1: Off-beats bar 1 only
    0b00000000000000000010001000100010, // 2: Off-beats bar 2 only
    0b00100010001000000010001000100010, // 3: Missing one off-beat
    0b00100010001000100000001000100010, // 4: Missing bar 2 off-beat
    0b00100000001000100010001000100010, // 5: Missing one bar 1 off-beat
    0b00100010001000100010001000100000, // 6: Missing last off-beat
    0b00000010001000100010001000100010, // 7: Missing first off-beat
    0b00100010000000100010001000100010, // 8: Gap mid bar 1
    0b00100010001000100010000000100010, // 9: Gap mid bar 2
    0b00100010001000000000001000100010, // 10: Sparse bar 1 end
    0b00000000000000000000000000000000, // 11: No opens (minimal)
    0b00100010001000100010001000000010, // 12: Mostly full
    0b00100000001000000010001000100000, // 13: Alternating gaps
    0b00000010001000100000001000100010, // 14: First and mid gaps
    0b00100010001000100010001000100010  // 15: Full 8th off-beats
};

// ========================================
// Drum Fills - Half-bar (8 steps) and Whole-bar (16 steps)
// ========================================

// Kick fills - Half-bar (8 steps, for last half of bar)
const uint8_t kickFillsHalf[8] = {
    0b00010111, // 0: Simple build
    0b01111111, // 1: Rapid hits
    0b01010101, // 2: 8th notes
    0b00111111, // 3: Build last 6
    0b10101010, // 4: On-beats
    0b11011011, // 5: Triplet feel
    0b01110111, // 6: Gallop
    0b11111111  // 7: Full roll
};

// Kick fills - Whole-bar (16 steps)
const uint16_t kickFillsWhole[8] = {
    0b0000000001111111, // 0: Build second half
    0b0000010101111111, // 1: Rolling build
    0b0001001001111111, // 2: Gradual build
    0b1010101010101010, // 3: 8ths throughout
    0b0101010111111111, // 4: Sparse to dense
    0b0011001111111111, // 5: Stepped build
    0b1101101111111111, // 6: Triplet to roll
    0b1111111111111111  // 7: Full 16th roll
};

// Clap fills - Half-bar (8 steps)
const uint8_t clapFillsHalf[8] = {
    0b00001111, // 0: Simple build
    0b01010111, // 1: Syncopated build
    0b00111111, // 2: Build last 6
    0b01111111, // 3: Rapid claps
    0b10101010, // 4: 8th beats
    0b01011111, // 5: Mixed build
    0b11110111, // 6: Almost full
    0b11111111  // 7: Full roll
};

// Clap fills - Whole-bar (16 steps)
const uint16_t clapFillsWhole[8] = {
    0b0000000000111111, // 0: Build last 6
    0b0000000011111111, // 1: Build second half
    0b0000010101111111, // 2: Rolling build
    0b0001010111111111, // 3: Gradual increase
    0b0000110011111111, // 4: Stepped build
    0b0101010101010101, // 5: 8th notes throughout
    0b0011011111111111, // 6: Quick build
    0b1111111111111111  // 7: Full roll
};

// Hi-hat fills - Half-bar (8 steps, closed)
const uint8_t hatClosedFillsHalf[8] = {
    0b01111111, // 0: Almost full
    0b11111111, // 1: Full 16th roll
    0b10101010, // 2: 8th notes
    0b11011011, // 3: Triplet
    0b11101110, // 4: Gallop
    0b01010101, // 5: Off-beat 8ths
    0b11111110, // 6: Almost full alt
    0b10111011  // 7: Broken triplet
};

// Hi-hat fills - Whole-bar (16 steps, closed)
const uint16_t hatClosedFillsWhole[8] = {
    0b0101010111111111, // 0: Build to roll
    0b1010101011111111, // 1: On-beat to roll
    0b1111111111111111, // 2: Full 16th roll
    0b1010101010101010, // 3: 8ths throughout
    0b1101101111111111, // 4: Triplet to roll
    0b0011001111111111, // 5: Stepped to roll
    0b1110111011101110, // 6: Gallop throughout
    0b1011101111111111  // 7: Broken to roll
};

// Hi-hat fills - Half-bar (8 steps, open - crash accent)
const uint8_t hatOpenFillsHalf[8] = {
    0b00000001, // 0: Crash at end
    0b00000010, // 1: Crash on last beat
    0b10000001, // 2: Crashes at start and end
    0b00001000, // 3: Crash mid-fill
    0b10001000, // 4: Two crashes
    0b00000000, // 5: No opens (closed only)
    0b00010001, // 6: Scattered opens
    0b10000000  // 7: Crash at start
};

// Hi-hat fills - Whole-bar (16 steps, open - crash accents)
const uint16_t hatOpenFillsWhole[8] = {
    0b0000000000000001, // 0: Crash at very end
    0b0000000000000010, // 1: Crash on last beat
    0b1000000000000001, // 2: Crashes at start and end
    0b0000000010000000, // 3: Crash at halfway
    0b0000100000001000, // 4: Two crashes mid and end
    0b0000000000000000, // 5: No opens (all closed)
    0b1000000010000001, // 6: Three crashes
    0b0001000100010001  // 7: Multiple opens
};

// ========================================
// Modular Rhythm Generation System
// ========================================

// Rhythm Style Types - Extensible
enum RhythmStyle
{
    RHYTHM_SYNCOPATED,      // Off-beat emphasis
    RHYTHM_STRAIGHT,        // On-beat emphasis
    RHYTHM_EUCLIDEAN,       // Evenly spaced hits
    RHYTHM_ANTI_EUCLIDEAN,  // Clustered/grouped hits
    RHYTHM_FOLLOW_KICK,     // Follows kick drum pattern
    NUM_RHYTHM_STYLES
};

// ========================================
// Melody Generation System
// ========================================

// Scale types
enum ScaleType
{
    SCALE_MINOR,            // Natural minor: 0, 2, 3, 5, 7, 8, 10
    SCALE_MINOR_BLUES,      // Minor blues: 0, 3, 5, 6, 7, 10
    SCALE_MINOR_PENTATONIC, // Minor pentatonic: 0, 3, 5, 7, 10
    SCALE_GYPSY,            // Hungarian Gypsy: 0, 2, 3, 6, 7, 8, 11
    NUM_SCALE_TYPES
};

const char* scaleNames[NUM_SCALE_TYPES] = {
    "Minor",
    "MinBlue",
    "MinPent",
    "Gypsy"
};

// Scale intervals (semitones from root)
const int8_t scaleMinor[] = {0, 2, 3, 5, 7, 8, 10};
const int8_t scaleMinorBlues[] = {0, 3, 5, 6, 7, 10};
const int8_t scaleMinorPentatonic[] = {0, 3, 5, 7, 10};
const int8_t scaleGypsy[] = {0, 2, 3, 6, 7, 8, 11};

const int8_t scaleLengths[NUM_SCALE_TYPES] = {7, 6, 5, 7};

// Root note names (0-11 = C to B)
const char* rootNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Melody style types
enum MelodyStyle
{
    MELODY_SUPPORTING,      // Sparse, follows rhythm, few note changes
    MELODY_ARPEGGIATOR,     // Dense, cyclic chord/scale patterns
    NUM_MELODY_STYLES
};

const char* melodyStyleNames[NUM_MELODY_STYLES] = {
    "Support",
    "Arpeg"
};

// Supporting style sub-types (randomized)
enum SupportingSubStyle
{
    SUPPORT_FOLLOW_KICK,    // Notes trigger on kick hits
    SUPPORT_OWN_SPARSE,     // Independent sparse pattern
    SUPPORT_SUBSET_KICK,    // Subset of kick hits (even sparser)
    NUM_SUPPORTING_SUBSTYLES
};

// Arpeggiator sub-types (randomized)
enum ArpSubStyle
{
    ARP_CHORD_TONES,        // Cycles through root, 3rd, 5th, (7th)
    ARP_SCALE_ASCENDING,    // Moves up through scale degrees
    ARP_SCALE_RANDOM,       // Random notes from scale, dense rhythm
    NUM_ARP_SUBSTYLES
};

// Melody voice configuration
struct MelodyConfig
{
    MelodyStyle style;
    uint8_t subStyle;           // Supporting or Arp sub-style
    uint32_t rhythmPattern;     // When notes trigger
    uint8_t patternLength;      // Pattern length (32 steps default)
    int8_t noteSequence[32];    // Pre-generated note sequence (semitones from C2)
    uint8_t sequencePos;        // Current position in note sequence
    uint8_t currentOctave;      // Current octave offset (0-2 for 3 octaves)
    bool active;
};

MelodyConfig melodyVoice = {
    MELODY_SUPPORTING,  // Default style
    SUPPORT_FOLLOW_KICK,// Default sub-style
    0,                  // Rhythm pattern (generated)
    32,                 // Pattern length
    {0},                // Note sequence (generated)
    0,                  // Sequence position
    0,                  // Current octave
    true                // Active
};

// MIDI Melody Voice (same structure, outputs via MIDI)
MelodyConfig melodyMidiVoice = {
    MELODY_ARPEGGIATOR, // Default style (different from CV voice)
    ARP_CHORD_TONES,    // Default sub-style
    0,                  // Rhythm pattern (generated)
    32,                 // Pattern length
    {0},                // Note sequence (generated)
    0,                  // Sequence position
    0,                  // Current octave
    true                // Active
};

// Shared melody settings (both voices use the same root and scale)
ScaleType melodyScale = SCALE_MINOR;
uint8_t melodyRoot = 0;  // 0 = C

// MIDI melody voice settings
uint8_t melodyMidiChannel = 0;  // MIDI channel 1 (0-indexed)
uint8_t lastMidiMelodyNote = 0; // Track last note for note-off
bool midiMelodyNoteOn = false;  // Track if a note is currently on

// Melody freeze (independent from drum freeze)
bool melodyFreezeEnabled = false;

// Tune mode - outputs middle C quarter notes on both melody channels for VCO tuning
bool tuneModeEnabled = false;

// Function prototypes for melody generation
void GenerateMelodyPattern();
void RandomizeMelodyPersonality();
int8_t GetScaleNote(ScaleType scale, uint8_t root, int8_t degree);
float MelodyNoteToCV(int8_t semitone);

// Density levels for pattern generation
enum DensityLevel
{
    DENSITY_LOW,     // Sparse (few hits)
    DENSITY_MEDIUM,  // Moderate
    DENSITY_HIGH,    // Dense (many hits)
    NUM_DENSITY_LEVELS
};

// Interaction Style Types - Extensible
enum InteractionStyle
{
    INTERACTION_NONE,           // Independent voices
    INTERACTION_DIVIDED,        // Hits divided between two voices
    INTERACTION_ALTERNATE_BAR,  // Voices alternate every bar
    INTERACTION_ALTERNATE_HALF, // Voices alternate every half bar
    INTERACTION_ALTERNATE_TWO,  // Voices alternate every two bars
    NUM_INTERACTION_STYLES
};

// Voice configuration structure
struct VoiceConfig
{
    DrumVoice       voice;
    RhythmStyle     rhythmStyle;
    DensityLevel    density;
    InteractionStyle interaction;
    DrumVoice       interactionPartner; // Voice to interact with (if any)
    uint32_t        pattern; // Generated pattern
    uint8_t         patternLength; // Pattern length (12, 13, 15, 17, 18, or 32 for polyrhythms)
    bool            active;  // Is this voice active in current section
};

// Rhythm generation functions (extensible - add new generators here)
uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length);
uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length);
uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length);
uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length);

// Interaction processing functions (extensible - add new processors here)
void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2);
void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2);
void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2);
void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2);
void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2);

// Pattern generation
void RandomizeVoicePersonalities();
void GenerateVoicePatterns();

// Voice configurations for remaining drum elements
VoiceConfig generativeVoices[6] = {
    {DRUM1, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM2, 0, 32, true},
    {DRUM2, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM1, 0, 32, true},
    {MULTI, RHYTHM_SYNCOPATED, DENSITY_LOW, INTERACTION_NONE, MULTI, 0, 32, true},
    {SNARE, RHYTHM_STRAIGHT, DENSITY_MEDIUM, INTERACTION_ALTERNATE_BAR, HIHAT2_CLOSED, 0, 32, true},
    {HIHAT2_CLOSED, RHYTHM_STRAIGHT, DENSITY_HIGH, INTERACTION_NONE, HIHAT2_CLOSED, 0, 32, true},
    {ANALOG, RHYTHM_FOLLOW_KICK, DENSITY_HIGH, INTERACTION_NONE, ANALOG, 0, 32, true} // CV/Gate output
};

// ========================================
// Turing Machine CV Sequencers
// ========================================

struct TuringMachine
{
    uint32_t shiftRegister;      // 32-bit shift register
    uint8_t  currentLength;      // Active length: 8, 16, or 32
    uint8_t  targetLength;       // Target length for morphing
    float    lengthMorph;        // Morph progress (0.0 - 1.0)
    uint8_t  currentPos;         // Current position in sequence
    float    randomProbability;  // Probability of random bit (0.0 - 1.0)
    float    targetProbability;  // Target probability for slow drift
    float    cvOutput;           // Current CV output value (0.0 - 1.0)
    uint32_t morphCounter;       // Counter for slow morphing
    uint32_t morphInterval;      // Steps between morph updates (slower = minutes)

    // Constructor
    void Init(uint32_t seed)
    {
        shiftRegister = seed;
        currentLength = 16;
        targetLength = 16;
        lengthMorph = 0.0f;
        currentPos = 0;
        randomProbability = 0.2f; // 20% chance of random bit
        targetProbability = 0.2f;
        cvOutput = 0.0f;
        morphCounter = 0;
        morphInterval = 256; // Morph every 256 steps (about 16 bars at 16ths)
    }

    // Process one step
    void Process()
    {
        // Shift register
        bool outputBit = (shiftRegister >> (currentLength - 1)) & 0x01;
        shiftRegister <<= 1;

        // Determine new bit: either looped back or random
        bool newBit;
        float randVal = (float)(System::GetUs() % 1000) / 1000.0f;
        if(randVal < randomProbability)
        {
            // Random bit
            newBit = (System::GetUs() & 0x01);
        }
        else
        {
            // Loop back the output bit
            newBit = outputBit;
        }

        // Insert new bit
        if(newBit)
            shiftRegister |= 0x01;

        // Mask to current length
        uint32_t mask = (1 << currentLength) - 1;
        shiftRegister &= mask;

        // Calculate CV output from register state
        // Use multiple bits to create voltage
        uint8_t numBitsForCV = 5; // Use 5 bits = 32 levels
        uint32_t cvBits = shiftRegister & 0x1F; // Bottom 5 bits
        cvOutput = (float)cvBits / 31.0f; // Normalize to 0.0 - 1.0

        // Advance position
        currentPos++;
        if(currentPos >= currentLength)
            currentPos = 0;

        // Slow morphing
        morphCounter++;
        if(morphCounter >= morphInterval)
        {
            morphCounter = 0;
            UpdateMorphing();
        }
    }

    // Update slow morphing parameters
    void UpdateMorphing()
    {
        // Slowly drift probability
        float probDrift = ((float)(System::GetUs() % 100) / 1000.0f) - 0.05f; // -0.05 to +0.05
        targetProbability += probDrift;
        targetProbability = fclamp(targetProbability, 0.05f, 0.5f);

        // Gradually move toward target probability
        randomProbability += (targetProbability - randomProbability) * 0.1f;

        // Occasionally change target length
        if((System::GetUs() % 10) == 0) // 10% chance
        {
            uint32_t lengthChoice = System::GetUs() % 3;
            if(lengthChoice == 0) targetLength = 8;
            else if(lengthChoice == 1) targetLength = 16;
            else targetLength = 32;
        }

        // Morph length gradually
        if(currentLength != targetLength)
        {
            lengthMorph += 0.02f; // 2% per morph interval
            if(lengthMorph >= 1.0f)
            {
                lengthMorph = 0.0f;
                currentLength = targetLength;

                // Adjust register for new length
                uint32_t mask = (1 << currentLength) - 1;
                shiftRegister &= mask;
            }
        }
    }

    // Get current CV output
    float GetCV()
    {
        // During length morph, blend between current and target length outputs
        if(lengthMorph > 0.0f && currentLength != targetLength)
        {
            // Create a smooth transition
            return cvOutput * (1.0f - lengthMorph * 0.5f);
        }
        return cvOutput;
    }
};

// Analog voice CV/Gate state
uint8_t analogVoiceVelocity = 100; // MIDI velocity for CV conversion (0-127 = 0-5V)
bool analogGateHigh = false;
uint32_t analogGateCounter = 0;
const uint32_t ANALOG_GATE_SAMPLES = 480; // 10ms gate pulse at 48kHz

// Pattern state
uint8_t  currentKickPattern = 0;
uint8_t  currentClapPattern = 0;
uint8_t  currentHatPattern = 0;
uint8_t  currentStep = 0; // Current step in pattern (0-31)
uint32_t barCounter = 0;  // Count 2-bar phrases for pattern rotation
uint32_t patternChangeInterval = 4; // Change patterns every N 2-bar phrases (4 = 8 bars)
uint32_t personalityChangeInterval = 4; // Change voice personalities every N 8-bar cycles (4 = 32 bars)
uint32_t cycleCounter = 0; // Count 8-bar cycles for personality changes
uint32_t generationSeed = 0; // Seed for pattern generation
DrumVoice fundamentalBeatVoice = CLAP; // Which voice plays fundamental beat patterns (CLAP or SNARE)

// Fill state
bool     fillActive = false;
bool     fillIsHalfBar = false; // true = half-bar (8 steps), false = whole-bar (16 steps)
uint8_t  fillKickIdx = 0;
uint8_t  fillClapIdx = 0;
uint8_t  fillHatIdx = 0;
uint8_t  fillStartStep = 0; // Step number when fill starts

// Timing flag - set in audio callback, processed in main loop
volatile bool trigger16thNote = false;

// Trigger a gate pulse on an output
void TriggerGate24ppqn() { gate24ppqn = true; gate24ppqnCounter = 0; }
void TriggerGate16th() { gate16th = true; gate16thCounter = 0; }
void TriggerGate2() { gate2 = true; gate2Counter = 0; }
void TriggerGateQuarter() { gateQuarter = true; gateQuarterCounter = 0; }
void TriggerGateReset() { gateReset = true; gateResetCounter = 0; }

// Check if OUT2 should trigger based on division setting
bool ShouldTriggerOut2(uint8_t step, uint8_t bar)
{
    uint16_t totalStep = (bar * 16) + step; // Total 16th notes since start

    switch(currentOut2Division)
    {
        case DIV_1_16: return true;                    // Every 16th note
        case DIV_1_8:  return (step % 2) == 0;         // Every 8th note
        case DIV_1_4:  return (step % 4) == 0;         // Every quarter note
        case DIV_1_2:  return (step % 8) == 0;         // Every half note
        case DIV_1:    return (step == 0);             // Every bar (16 steps)
        case DIV_2:    return (totalStep % 32) == 0;   // Every 2 bars
        case DIV_4:    return (totalStep % 64) == 0;   // Every 4 bars
        default: return false;
    }
}

// Check if OUT3 should trigger based on division setting
bool ShouldTriggerOut3(uint8_t step, uint8_t bar)
{
    uint16_t totalStep = (bar * 16) + step; // Total 16th notes since start

    switch(currentOut3Division)
    {
        case DIV_1_16: return true;                    // Every 16th note
        case DIV_1_8:  return (step % 2) == 0;         // Every 8th note
        case DIV_1_4:  return (step % 4) == 0;         // Every quarter note
        case DIV_1_2:  return (step % 8) == 0;         // Every half note
        case DIV_1:    return step == 0;               // Every bar
        case DIV_2:    return (totalStep % 32) == 0;   // Every 2 bars
        case DIV_4:    return (totalStep % 64) == 0;   // Every 4 bars
        default:       return (step % 4) == 0;         // Default to quarter
    }
}

// Save settings to persistent storage
void SaveSettings()
{
    settings.magic = SETTINGS_MAGIC;
    settings.bpm = bpm;
    settings.out2Division = (uint8_t)currentOut2Division;
    settings.out3Division = (uint8_t)currentOut3Division;
    settings.freezeEnabled = freezeEnabled ? 1 : 0;
    settings.melodyScale = (uint8_t)melodyScale;
    settings.melodyRoot = melodyRoot;
    settings.cvMelodyStyle = (uint8_t)melodyVoice.style;
    settings.midiMelodyStyle = (uint8_t)melodyMidiVoice.style;
    settings.midiMelChannel = melodyMidiChannel;
    settings.melodyFreezeEnabled = melodyFreezeEnabled ? 1 : 0;

    // Write to QSPI flash
    size_t size = sizeof(PersistentSettings);
    uint32_t addr = 0x90000000; // QSPI memory-mapped base address
    hw.seed.qspi.Erase(addr, addr + size);
    hw.seed.qspi.Write(addr, size, (uint8_t*)&settings);
}

// Load settings from persistent storage
void LoadSettings()
{
    // Read from QSPI flash using memory-mapped access
    uint32_t addr = 0x90000000; // QSPI memory-mapped base address
    PersistentSettings* flash_settings = (PersistentSettings*)addr;

    // Copy from flash to RAM
    memcpy(&settings, flash_settings, sizeof(PersistentSettings));

    // Validate magic number
    if(settings.magic == SETTINGS_MAGIC)
    {
        // Valid settings found, apply them
        bpm = settings.bpm;

        // Validate BPM range
        if(bpm < 20.0f || bpm > 300.0f)
        {
            bpm = 120.0f; // Reset to default if out of range
        }

        // Validate OUT2 division
        if(settings.out2Division >= NUM_OUT_DIVISIONS)
        {
            currentOut2Division = DIV_1_8; // Reset to default
        }
        else
        {
            currentOut2Division = (OutDivision)settings.out2Division;
        }

        // Validate OUT3 division
        if(settings.out3Division >= NUM_OUT_DIVISIONS)
        {
            currentOut3Division = DIV_1_4; // Reset to default
        }
        else
        {
            currentOut3Division = (OutDivision)settings.out3Division;
        }

        // Load freeze setting
        freezeEnabled = (settings.freezeEnabled != 0);

        // Load shared melody scale and root
        if(settings.melodyScale < NUM_SCALE_TYPES)
        {
            melodyScale = (ScaleType)settings.melodyScale;
        }
        else
        {
            melodyScale = SCALE_MINOR;
        }

        if(settings.melodyRoot < 12)
        {
            melodyRoot = settings.melodyRoot;
        }
        else
        {
            melodyRoot = 0; // Default to C
        }

        // Load CV melody style
        if(settings.cvMelodyStyle < NUM_MELODY_STYLES)
        {
            melodyVoice.style = (MelodyStyle)settings.cvMelodyStyle;
        }
        else
        {
            melodyVoice.style = MELODY_SUPPORTING;
        }

        // Load MIDI melody style
        if(settings.midiMelodyStyle < NUM_MELODY_STYLES)
        {
            melodyMidiVoice.style = (MelodyStyle)settings.midiMelodyStyle;
        }
        else
        {
            melodyMidiVoice.style = MELODY_ARPEGGIATOR;
        }

        if(settings.midiMelChannel < 16)
        {
            melodyMidiChannel = settings.midiMelChannel;
        }
        else
        {
            melodyMidiChannel = 0; // Default to channel 1
        }

        melodyFreezeEnabled = (settings.melodyFreezeEnabled != 0);
    }
    else
    {
        // No valid settings found, use defaults
        bpm = 120.0f;
        currentOut2Division = DIV_1_8;
        currentOut3Division = DIV_1_4;
        freezeEnabled = false;
        melodyScale = SCALE_MINOR;
        melodyRoot = 0;
        melodyVoice.style = MELODY_SUPPORTING;
        melodyMidiVoice.style = MELODY_ARPEGGIATOR;
        melodyMidiChannel = 0;
        melodyFreezeEnabled = false;

        // Save defaults
        SaveSettings();
    }
}

// Send a MIDI note for drum trigger
void TriggerDrum(DrumVoice voice, uint8_t velocity = 100)
{
    uint8_t noteOn[3] = {
        static_cast<uint8_t>(0x90 | DRM1_MIDI_CHANNEL), // Note On + channel
        drumNotes[voice],                                // Note number
        velocity                                         // Velocity
    };
    hw.midi.SendMessage(noteOn, 3);

    // Note: We don't send Note Off for drums, they're self-decaying
}

// ========================================
// Groove Timing Queue Management
// ========================================

// Initialize the trigger queue
void InitTriggerQueue()
{
    for(int i = 0; i < TRIGGER_QUEUE_SIZE; i++)
    {
        triggerQueue[i].Init();
    }
    triggerQueueHead = 0;
    triggerQueueTail = 0;

    // Initialize melody queue
    for(int i = 0; i < MELODY_QUEUE_SIZE; i++)
    {
        melodyQueue[i].Init();
    }
    melodyQueueHead = 0;
    melodyQueueTail = 0;
}

// Randomize groove pattern and per-voice amounts
void RandomizeGroove()
{
    // Random groove pattern (0-31)
    uint32_t seed = System::GetUs();
    currentGroovePattern = seed % 32;

    // Random timing and velocity amounts for each voice
    // Kick drum: 0-35% (reduced to avoid too heavy bass timing variation)
    // Other voices: 0-75%
    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        seed = System::GetUs() ^ (i * 54321); // Unique seed per voice

        // Kick drum gets lower max groove to avoid excessive timing variation
        if(i == KICK)
        {
            grooveAmount[i] = (float)(seed % 36) / 100.0f; // 0-35% timing for kick
        }
        else
        {
            grooveAmount[i] = (float)(seed % 76) / 100.0f; // 0-75% timing for others
        }

        seed = System::GetUs() ^ (i * 98765); // Different seed for velocity
        grooveVelocityAmount[i] = (float)(seed % 101) / 100.0f; // 0-100% velocity
    }

    // Randomize melody groove amount (25-75%)
    uint32_t melSeed = System::GetUs();
    melodyGrooveAmount = 0.25f + (float)(melSeed % 51) / 100.0f;
}

// Calculate groove offset in samples for melody (uses same patterns as drums)
int32_t CalculateMelodyGrooveOffset(uint8_t step)
{
    // Get step within 16-step pattern
    uint8_t patternStep = step % 16;

    // Get base offset percentage from current groove pattern
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];

    // Scale by melody groove amount
    float scaledOffsetPercent = (float)baseOffsetPercent * melodyGrooveAmount;

    // Convert to samples based on current BPM
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    float offsetFloat = (scaledOffsetPercent / 100.0f) * samplesPerSixteenth;

    return (int32_t)offsetFloat;
}

// Schedule a melody trigger with groove timing
void ScheduleMelodyTrigger(MelodyVoiceType voiceType, int8_t note, uint8_t step, uint64_t nextBeatSample)
{
    int32_t offset = CalculateMelodyGrooveOffset(step);
    uint64_t fireSample = nextBeatSample + offset;

    // If fire time is in the past or very close, skip (note will be handled next step)
    if(fireSample <= globalSampleCounter + 48)
    {
        // Fire immediately - find queue slot
    }

    // Find next available queue slot
    uint8_t nextHead = (melodyQueueHead + 1) % MELODY_QUEUE_SIZE;

    // Check for queue overflow - skip if full
    if(nextHead == melodyQueueTail)
    {
        return; // Queue full, skip this trigger
    }

    // Add to queue
    melodyQueue[melodyQueueHead].voiceType = voiceType;
    melodyQueue[melodyQueueHead].note = note;
    melodyQueue[melodyQueueHead].fireSample = fireSample;
    melodyQueue[melodyQueueHead].active = true;

    melodyQueueHead = nextHead;
}

// Process melody trigger queue (called from audio callback)
void ProcessMelodyQueue()
{
    uint8_t checkCount = 0;
    uint8_t currentIndex = melodyQueueTail;

    while(currentIndex != melodyQueueHead && checkCount < MELODY_QUEUE_SIZE)
    {
        MelodyTrigger* trigger = &melodyQueue[currentIndex];

        if(trigger->active && globalSampleCounter >= trigger->fireSample)
        {
            if(trigger->voiceType == MELODY_CV)
            {
                // Trigger CV gate
                analogGateHigh = true;
                analogGateCounter = 0;

                // Output CV voltage
                float cvVoltage = MelodyNoteToCV(trigger->note);
                uint16_t cv1 = (uint16_t)(cvVoltage / 5.0f * 4095.0f);
                hw.seed.dac.WriteValue(DacHandle::Channel::ONE, cv1);
            }
            else // MELODY_MIDI
            {
                // Send note-off for previous note if playing
                if(midiMelodyNoteOn)
                {
                    uint8_t noteOff[3] = {
                        static_cast<uint8_t>(0x80 | melodyMidiChannel),
                        lastMidiMelodyNote,
                        0
                    };
                    hw.midi.SendMessage(noteOff, 3);
                }

                // Calculate MIDI note (C2 = 36, add semitones)
                uint8_t midiNote = 36 + trigger->note;
                if(midiNote > 127) midiNote = 127;

                // Send note-on
                uint8_t noteOn[3] = {
                    static_cast<uint8_t>(0x90 | melodyMidiChannel),
                    midiNote,
                    100  // Fixed velocity for melody
                };
                hw.midi.SendMessage(noteOn, 3);

                lastMidiMelodyNote = midiNote;
                midiMelodyNoteOn = true;
            }

            // Mark as processed
            trigger->active = false;

            // Advance tail if this was the tail entry
            if(currentIndex == melodyQueueTail)
            {
                while(melodyQueueTail != melodyQueueHead && !melodyQueue[melodyQueueTail].active)
                {
                    melodyQueueTail = (melodyQueueTail + 1) % MELODY_QUEUE_SIZE;
                }
            }
        }

        currentIndex = (currentIndex + 1) % MELODY_QUEUE_SIZE;
        checkCount++;
    }
}

// Calculate groove offset in samples for a specific voice and step
int32_t CalculateGrooveOffset(DrumVoice voice, uint8_t step)
{
    // Get step within 16-step pattern (handle 32-step patterns)
    uint8_t patternStep = step % 16;

    // Get base offset percentage from pattern
    int8_t baseOffsetPercent = groovePatterns[currentGroovePattern][patternStep];

    // Scale by voice amount
    float scaledOffsetPercent = (float)baseOffsetPercent * grooveAmount[voice];

    // Convert to samples based on current BPM
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    float offsetFloat = (scaledOffsetPercent / 100.0f) * samplesPerSixteenth;

    // No clamping - groove amount percentage is the only limit
    return (int32_t)offsetFloat;
}

// Calculate velocity with groove pattern applied
uint8_t CalculateGrooveVelocity(DrumVoice voice, uint8_t baseVelocity, uint8_t step)
{
    // Get step within 16-step pattern
    uint8_t patternStep = step % 16;

    // Get velocity multiplier from pattern (100 = normal, 120 = +20%, 80 = -20%)
    int8_t velocityPercent = velocityPatterns[currentGroovePattern][patternStep];

    // Apply pattern and voice velocity groove amount (separate from timing amount)
    // Full groove amount = full velocity variation, zero groove = no velocity variation
    float velocityMultiplier = 100.0f + ((velocityPercent - 100.0f) * grooveVelocityAmount[voice]);

    // Calculate final velocity
    float finalVelocity = (float)baseVelocity * (velocityMultiplier / 100.0f);

    // Clamp to MIDI range 1-127
    if(finalVelocity < 1.0f) finalVelocity = 1.0f;
    if(finalVelocity > 127.0f) finalVelocity = 127.0f;

    return (uint8_t)finalVelocity;
}

// Helper: Schedule drum with groove timing AND velocity applied
void ScheduleDrumTriggerWithGroove(DrumVoice voice, uint8_t baseVelocity, uint8_t step, uint64_t nextBeatSample)
{
    uint8_t velocity = CalculateGrooveVelocity(voice, baseVelocity, step);
    int32_t offset = CalculateGrooveOffset(voice, step);
    uint64_t fireSample = nextBeatSample + offset;

    // If fire time is in the past or very close (within 48 samples), fire immediately
    if(fireSample <= globalSampleCounter || fireSample <= globalSampleCounter + 48)
    {
        TriggerDrum(voice, velocity);
        return;
    }

    // Find next available queue slot
    uint8_t nextHead = (triggerQueueHead + 1) % TRIGGER_QUEUE_SIZE;

    // Check for queue overflow
    if(nextHead == triggerQueueTail)
    {
        // Queue full - fire immediately as fallback
        TriggerDrum(voice, velocity);
        return;
    }

    // Add to queue
    triggerQueue[triggerQueueHead].voice = voice;
    triggerQueue[triggerQueueHead].velocity = velocity;
    triggerQueue[triggerQueueHead].fireSample = fireSample;
    triggerQueue[triggerQueueHead].active = true;

    triggerQueueHead = nextHead;
}

// Schedule a drum trigger at an absolute sample time (with look-ahead support)
void ScheduleDrumTrigger(DrumVoice voice, uint8_t velocity, uint64_t fireSample)
{
    // If fire time is in the past or very close (within 48 samples), fire immediately
    if(fireSample <= globalSampleCounter || fireSample <= globalSampleCounter + 48)
    {
        TriggerDrum(voice, velocity);
        return;
    }

    // Find next available queue slot
    uint8_t nextHead = (triggerQueueHead + 1) % TRIGGER_QUEUE_SIZE;

    // Check for queue overflow
    if(nextHead == triggerQueueTail)
    {
        // Queue full - fire immediately as fallback
        TriggerDrum(voice, velocity);
        return;
    }

    // Add to queue
    triggerQueue[triggerQueueHead].voice = voice;
    triggerQueue[triggerQueueHead].velocity = velocity;
    triggerQueue[triggerQueueHead].fireSample = fireSample;
    triggerQueue[triggerQueueHead].active = true;

    triggerQueueHead = nextHead;
}

// Process trigger queue in audio callback (sample-accurate)
void ProcessTriggerQueue()
{
    // Check all queue entries (triggers may not be time-ordered)
    uint8_t checkCount = 0;
    uint8_t currentIndex = triggerQueueTail;

    while(currentIndex != triggerQueueHead && checkCount < TRIGGER_QUEUE_SIZE)
    {
        MidiTrigger* trigger = &triggerQueue[currentIndex];

        if(trigger->active && globalSampleCounter >= trigger->fireSample)
        {
            // Fire the trigger
            TriggerDrum(trigger->voice, trigger->velocity);

            // Mark as processed
            trigger->active = false;

            // If this is the tail entry, advance tail
            if(currentIndex == triggerQueueTail)
            {
                // Advance tail past all inactive entries
                while(triggerQueueTail != triggerQueueHead && !triggerQueue[triggerQueueTail].active)
                {
                    triggerQueueTail = (triggerQueueTail + 1) % TRIGGER_QUEUE_SIZE;
                }
            }
        }

        currentIndex = (currentIndex + 1) % TRIGGER_QUEUE_SIZE;
        checkCount++;
    }
}

// Check if a step should trigger in a pattern
bool IsStepActive(uint32_t pattern, uint8_t step)
{
    // Read from left (MSB) since patterns are written MSB-first
    return (pattern >> (31 - step)) & 0x01;
}

// Check if a step should trigger in an 8-bit fill pattern
bool IsStepActive8(uint8_t pattern, uint8_t step)
{
    // Read from left (MSB) since patterns are written MSB-first
    return (pattern >> (7 - step)) & 0x01;
}

// Check if a step should trigger in a 16-bit fill pattern
bool IsStepActive16(uint16_t pattern, uint8_t step)
{
    // Read from left (MSB) since patterns are written MSB-first
    return (pattern >> (15 - step)) & 0x01;
}

// Schedule a fill for the end of the current 8-bar cycle
void ScheduleFill()
{
    // Randomly decide: 30% chance of fill
    uint32_t seed = System::GetUs();
    if((seed % 100) < 30) // 30% chance
    {
        fillActive = true;

        // Randomly choose half-bar or whole-bar fill (50/50)
        fillIsHalfBar = ((seed >> 8) % 2) == 0;

        // Calculate when fill should start
        // 8 bars = 128 steps total
        // Whole-bar fill: starts at step 112 (last 16 steps)
        // Half-bar fill: starts at step 120 (last 8 steps)
        fillStartStep = fillIsHalfBar ? 120 : 112;

        // Randomly select fill patterns
        fillKickIdx = (seed >> 16) % 8;
        fillClapIdx = (seed >> 20) % 8;
        fillHatIdx = (seed >> 24) % 8;
    }
    else
    {
        fillActive = false;
    }
}

// Randomize drum patterns
void RandomizePatterns()
{
    // Use System::GetUs() for pseudo-random seed
    uint32_t seed = System::GetUs();

    // Random pattern selection (0-15)
    currentKickPattern = (seed % 16);
    currentClapPattern = ((seed >> 4) % 16);
    currentHatPattern = ((seed >> 8) % 16);

    // Update generation seed for generative voices
    generationSeed = seed;

    // Generate new patterns for generative voices
    GenerateVoicePatterns();
}

// ========================================
// Rhythm Generation Functions
// ========================================

// Generate syncopated rhythm (off-beat emphasis)
uint32_t GenerateSyncopated(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits randomly (to get ~87.5% density)
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1 << pos); // Remove this hit
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize off-beat positions (odd steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2 + 1; // Force odd positions
            if(pos < length) pattern |= (1 << pos);
        }
    }
    return pattern;
}

// Generate straight rhythm (on-beat emphasis)
uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits randomly (to get ~87.5% density)
        int removeCount = length / 8;
        for(int i = 0; i < removeCount; i++)
        {
            int pos = ((seed >> (i * 3)) % length);
            pattern &= ~(1 << pos); // Remove this hit
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);
        // Emphasize on-beat positions (even steps)
        for(int i = 0; i < hitCount; i++)
        {
            int pos = ((seed >> (i * 2)) % (length / 2)) * 2; // Force even positions
            if(pos < length) pattern |= (1 << pos);
        }
    }
    return pattern;
}

// Generate Euclidean rhythm (evenly spaced hits)
uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;
    int hitCount = (density == DENSITY_LOW) ? (length / 8) : (density == DENSITY_MEDIUM) ? (length / 2) : ((length * 7) / 8);

    // Bjorklund's algorithm for Euclidean rhythms works correctly even for high density
    int bucket = 0;
    for(int i = 0; i < length; i++)
    {
        bucket += hitCount;
        if(bucket >= length)
        {
            bucket -= length;
            pattern |= (1 << i);
        }
    }
    return pattern;
}

// Generate anti-Euclidean rhythm (clustered hits)
uint32_t GenerateAntiEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;

    if(density == DENSITY_HIGH)
    {
        // For high density, start with all positions filled and remove a few in clusters
        for(int i = 0; i < length; i++)
        {
            pattern |= (1 << i);
        }
        // Remove ~12.5% of hits in clusters (to get ~87.5% density)
        int removeCount = length / 8;
        int clustersCount = (seed % 2) + 1; // 1-2 silence clusters
        int removesPerCluster = removeCount / clustersCount;
        if(removesPerCluster < 1) removesPerCluster = 1;

        for(int c = 0; c < clustersCount; c++)
        {
            int maxStart = length - removesPerCluster;
            if(maxStart < 0) maxStart = 0;
            int clusterStart = ((seed >> (c * 4)) % (maxStart + 1));
            for(int h = 0; h < removesPerCluster; h++)
            {
                int pos = (clusterStart + h) % length;
                pattern &= ~(1 << pos); // Remove this hit
            }
        }
    }
    else
    {
        int hitCount = (density == DENSITY_LOW) ? (length / 8) : (length / 2);

        // Create clusters of hits
        int clustersCount = (seed % 3) + 2; // 2-4 clusters
        int hitsPerCluster = hitCount / clustersCount;
        if(hitsPerCluster < 1) hitsPerCluster = 1;

        for(int c = 0; c < clustersCount; c++)
        {
            int maxStart = length - hitsPerCluster;
            if(maxStart < 0) maxStart = 0;
            int clusterStart = ((seed >> (c * 4)) % (maxStart + 1));
            for(int h = 0; h < hitsPerCluster; h++)
            {
                int pos = clusterStart + h;
                if(pos < length) pattern |= (1 << pos);
            }
        }
    }
    return pattern;
}

// ========================================
// Interaction Processing Functions
// ========================================

// No interaction - voices are independent
void ProcessInteractionNone(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Patterns remain as generated
    voice1->active = true;
    voice2->active = true;
}

// Divided interaction - hits split between two voices
void ProcessInteractionDivided(VoiceConfig* voice1, VoiceConfig* voice2)
{
    uint32_t combinedPattern = voice1->pattern | voice2->pattern;
    voice1->pattern = 0;
    voice2->pattern = 0;

    // Alternate hits between voices
    bool voice1Turn = true;
    for(int i = 0; i < 32; i++)
    {
        if(combinedPattern & (1 << i))
        {
            if(voice1Turn)
                voice1->pattern |= (1 << i);
            else
                voice2->pattern |= (1 << i);
            voice1Turn = !voice1Turn;
        }
    }
    voice1->active = true;
    voice2->active = true;
}

// Alternate bar - voices take turns playing full bars
void ProcessInteractionAlternateBar(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Bar 1 (steps 0-15): voice1
    // Bar 2 (steps 16-31): voice2
    uint32_t mask1 = 0x0000FFFF; // First 16 bits
    uint32_t mask2 = 0xFFFF0000; // Last 16 bits

    voice1->pattern &= mask1;
    voice2->pattern &= mask2;
    voice1->active = true;
    voice2->active = true;
}

// Alternate half bar - voices take turns every half bar
void ProcessInteractionAlternateHalf(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // Half bar 1 (0-7): voice1
    // Half bar 2 (8-15): voice2
    // Half bar 3 (16-23): voice1
    // Half bar 4 (24-31): voice2
    uint32_t mask1 = 0x00FF00FF; // Steps 0-7, 16-23
    uint32_t mask2 = 0xFF00FF00; // Steps 8-15, 24-31

    voice1->pattern &= mask1;
    voice2->pattern &= mask2;
    voice1->active = true;
    voice2->active = true;
}

// Alternate two bars - voices take turns playing 2 bars
void ProcessInteractionAlternateTwo(VoiceConfig* voice1, VoiceConfig* voice2)
{
    // This pattern plays over 2 iterations (4 bars total)
    // Use barCounter to determine which voice is active
    if(barCounter % 2 == 0)
    {
        voice1->active = true;
        voice2->active = false;
    }
    else
    {
        voice1->active = false;
        voice2->active = true;
    }
}

// Randomize voice personalities (rhythm style, density, interactions)
void RandomizeVoicePersonalities()
{
    uint32_t seed = System::GetUs();

    // Randomly assign fundamental beat role to either CLAP or SNARE
    fundamentalBeatVoice = ((seed % 2) == 0) ? CLAP : SNARE;

    // The other voice becomes a generative voice (at index 3)
    DrumVoice generativeBackbeatVoice = (fundamentalBeatVoice == CLAP) ? SNARE : CLAP;
    generativeVoices[3].voice = generativeBackbeatVoice;

    // Randomize rhythm styles for all voices
    RhythmStyle styles[] = {RHYTHM_SYNCOPATED, RHYTHM_STRAIGHT, RHYTHM_EUCLIDEAN, RHYTHM_ANTI_EUCLIDEAN, RHYTHM_FOLLOW_KICK};
    for(int i = 0; i < 6; i++)
    {
        seed = System::GetUs() ^ (i * 11111);
        generativeVoices[i].rhythmStyle = styles[seed % 5];
    }

    // Reset all interactions to NONE first
    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].interaction = INTERACTION_NONE;
        generativeVoices[i].interactionPartner = generativeVoices[i].voice;
    }

    // Pick alternating pair (50% probability)
    seed = System::GetUs();
    int alternatePair[2] = {-1, -1};
    if((seed % 100) < 50)
    {
        alternatePair[0] = (seed >> 8) % 6;
        alternatePair[1] = ((seed >> 16) % 5);
        if(alternatePair[1] >= alternatePair[0]) alternatePair[1]++; // Ensure different voice

        // Choose which alternate style
        InteractionStyle alternateStyles[] = {INTERACTION_ALTERNATE_BAR, INTERACTION_ALTERNATE_HALF, INTERACTION_ALTERNATE_TWO};
        InteractionStyle altStyle = alternateStyles[(seed >> 20) % 3];

        generativeVoices[alternatePair[0]].interaction = altStyle;
        generativeVoices[alternatePair[0]].interactionPartner = generativeVoices[alternatePair[1]].voice;
        generativeVoices[alternatePair[1]].interaction = altStyle;
        generativeVoices[alternatePair[1]].interactionPartner = generativeVoices[alternatePair[0]].voice;
    }

    // Pick divided pair (50% probability), ensuring no overlap with alternate pair
    seed = System::GetUs();
    if((seed % 100) < 50)
    {
        int voice1 = -1, voice2 = -1;
        int attempts = 0;

        // Find first voice not in alternate pair
        do {
            seed = System::GetUs() ^ (attempts * 7777);
            voice1 = (seed >> 8) % 6;
            attempts++;
        } while((voice1 == alternatePair[0] || voice1 == alternatePair[1]) && attempts < 10);

        // Find second voice not in alternate pair and different from voice1
        attempts = 0;
        do {
            seed = System::GetUs() ^ (attempts * 9999);
            voice2 = (seed >> 8) % 6;
            attempts++;
        } while((voice2 == voice1 || voice2 == alternatePair[0] || voice2 == alternatePair[1]) && attempts < 10);

        // Only apply if we found valid voices
        if(voice1 != -1 && voice2 != -1 && voice1 != voice2)
        {
            generativeVoices[voice1].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice1].interactionPartner = generativeVoices[voice2].voice;
            generativeVoices[voice2].interaction = INTERACTION_DIVIDED;
            generativeVoices[voice2].interactionPartner = generativeVoices[voice1].voice;
        }
    }

    // Randomize densities ensuring at least one of each type
    DensityLevel densities[6];
    densities[0] = DENSITY_LOW;
    densities[1] = DENSITY_MEDIUM;
    densities[2] = DENSITY_HIGH;

    // Randomly fill the remaining 3 slots
    seed = System::GetUs();
    densities[3] = (DensityLevel)((seed >> 8) % 3);
    densities[4] = (DensityLevel)((seed >> 16) % 3);
    densities[5] = (DensityLevel)((seed >> 24) % 3);

    // Shuffle the density array
    for(int i = 5; i > 0; i--)
    {
        seed = System::GetUs();
        int j = seed % (i + 1);
        DensityLevel temp = densities[i];
        densities[i] = densities[j];
        densities[j] = temp;
    }

    // Assign shuffled densities to voices
    for(int i = 0; i < 6; i++)
    {
        generativeVoices[i].density = densities[i];
    }
}

// Generate patterns for all generative voices
void GenerateVoicePatterns()
{
    // Odd lengths for polyrhythms
    const uint8_t oddLengths[] = {12, 13, 15, 17, 18};

    // Generate base patterns for each voice
    for(int i = 0; i < 6; i++)
    {

        VoiceConfig* voice = &generativeVoices[i];
        uint32_t seed = generationSeed ^ (i * 12345); // Unique seed per voice

        // Follow kick always uses 32-step pattern length to match kick patterns
        if(voice->rhythmStyle == RHYTHM_FOLLOW_KICK)
        {
            voice->patternLength = 32;
        }
        // Randomly choose pattern length (15% chance of polyrhythm)
        else if((seed % 100) < 15)
        {
            // Choose one of the odd lengths
            voice->patternLength = oddLengths[(seed >> 8) % 5];
        }
        else
        {
            // Standard 32-step pattern
            voice->patternLength = 32;
        }

        // Generate pattern based on rhythm style
        switch(voice->rhythmStyle)
        {
            case RHYTHM_SYNCOPATED:
                voice->pattern = GenerateSyncopated(seed, voice->density, voice->patternLength);
                break;
            case RHYTHM_STRAIGHT:
                voice->pattern = GenerateStraight(seed, voice->density, voice->patternLength);
                break;
            case RHYTHM_EUCLIDEAN:
                voice->pattern = GenerateEuclidean(seed, voice->density, voice->patternLength);
                break;
            case RHYTHM_ANTI_EUCLIDEAN:
                voice->pattern = GenerateAntiEuclidean(seed, voice->density, voice->patternLength);
                break;
            case RHYTHM_FOLLOW_KICK:
                voice->pattern = kickPatterns[currentKickPattern];
                break;
            default:
                voice->pattern = GenerateEuclidean(seed, voice->density, voice->patternLength);
                break;
        }
    }

    // Process interactions between voice pairs (dynamically based on current config)
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->interaction == INTERACTION_NONE)
            continue;

        // Find the partner voice
        VoiceConfig* partner = nullptr;
        for(int j = 0; j < 6; j++)
        {
            if(generativeVoices[j].voice == voice->interactionPartner)
            {
                partner = &generativeVoices[j];
                break;
            }
        }

        if(partner == nullptr)
            continue;

        // Process interaction (only once per pair)
        if(i < (partner - generativeVoices)) // Process only if this voice comes first in array
        {
            switch(voice->interaction)
            {
                case INTERACTION_DIVIDED:
                    ProcessInteractionDivided(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_BAR:
                    ProcessInteractionAlternateBar(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_HALF:
                    ProcessInteractionAlternateHalf(voice, partner);
                    break;
                case INTERACTION_ALTERNATE_TWO:
                    ProcessInteractionAlternateTwo(voice, partner);
                    break;
                default:
                    break;
            }
        }
    }
}

// ========================================
// Melody Generation Functions
// ========================================

// Get a note from the scale at a given degree (can span multiple octaves)
int8_t GetScaleNote(ScaleType scale, uint8_t root, int8_t degree)
{
    const int8_t* scaleNotes;
    int8_t scaleLen;

    switch(scale)
    {
        case SCALE_MINOR:
            scaleNotes = scaleMinor;
            scaleLen = 7;
            break;
        case SCALE_MINOR_BLUES:
            scaleNotes = scaleMinorBlues;
            scaleLen = 6;
            break;
        case SCALE_MINOR_PENTATONIC:
            scaleNotes = scaleMinorPentatonic;
            scaleLen = 5;
            break;
        case SCALE_GYPSY:
            scaleNotes = scaleGypsy;
            scaleLen = 7;
            break;
        default:
            scaleNotes = scaleMinor;
            scaleLen = 7;
    }

    // Handle negative degrees and octave wrapping
    int8_t octave = 0;
    while(degree < 0)
    {
        degree += scaleLen;
        octave--;
    }
    octave += degree / scaleLen;
    degree = degree % scaleLen;

    // Calculate final semitone (relative to C2 = 0V)
    return root + scaleNotes[degree] + (octave * 12);
}

// Convert semitone to CV voltage (1V/octave, 0V = C2)
float MelodyNoteToCV(int8_t semitone)
{
    // 0V = C2 (semitone 0), 3V = C5 (semitone 36)
    // 1V/octave = 1V per 12 semitones
    float voltage = (float)semitone / 12.0f;
    // Clamp to 0-3V range (3 octaves)
    if(voltage < 0.0f) voltage = 0.0f;
    if(voltage > 3.0f) voltage = 3.0f;
    return voltage;
}

// Generate rhythm pattern for a melody voice based on style and sub-style
void GenerateMelodyRhythmFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();
    voice->rhythmPattern = 0;
    voice->patternLength = 32;

    if(voice->style == MELODY_SUPPORTING)
    {
        // Strong beat positions (quarter notes: 0, 4, 8, 12, 16, 20, 24, 28)
        // Half notes: 0, 8, 16, 24
        // Downbeats of each bar: 0, 16
        const int strongBeats[] = {0, 4, 8, 12, 16, 20, 24, 28};  // Quarter notes
        const int halfBeats[] = {0, 8, 16, 24};  // Half notes

        switch(voice->subStyle)
        {
            case SUPPORT_FOLLOW_KICK:
                // Copy current kick pattern directly
                voice->rhythmPattern = kickPatterns[currentKickPattern];
                break;

            case SUPPORT_OWN_SPARSE:
                // Straight sparse pattern using only strong beats
                // Choose 3-5 quarter note positions
                {
                    int hitCount = 3 + (seed % 3);  // 3-5 notes
                    uint8_t usedPositions = 0;

                    for(int i = 0; i < hitCount; i++)
                    {
                        // Pick from strong beats array, avoid repeats
                        int attempts = 0;
                        int idx;
                        do {
                            idx = (seed >> (i * 3 + attempts)) % 8;
                            attempts++;
                        } while((usedPositions & (1 << idx)) && attempts < 10);

                        usedPositions |= (1 << idx);
                        voice->rhythmPattern |= (1 << strongBeats[idx]);
                    }

                    // Always include downbeat of bar 1
                    voice->rhythmPattern |= (1 << 0);
                }
                break;

            case SUPPORT_SUBSET_KICK:
                // Take kick hits but prefer strong beats
                {
                    uint32_t kickPat = kickPatterns[currentKickPattern];

                    // First, always take kick hits on strong beats (quarter notes)
                    for(int i = 0; i < 8; i++)
                    {
                        int pos = strongBeats[i];
                        if(kickPat & (1 << pos))
                        {
                            voice->rhythmPattern |= (1 << pos);
                        }
                    }

                    // If we have very few hits, add some from half beat positions
                    if(__builtin_popcount(voice->rhythmPattern) < 2)
                    {
                        for(int i = 0; i < 4; i++)
                        {
                            voice->rhythmPattern |= (1 << halfBeats[i]);
                        }
                    }
                }
                break;
        }
    }
    else // MELODY_ARPEGGIATOR
    {
        // Arpeggiator: sparse 4-5 note patterns that repeat
        // 8th note grid positions (every other 16th): 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30
        const int eighthNotes[] = {0, 2, 4, 6, 8, 10, 12, 14};  // First bar 8th notes

        switch(voice->subStyle)
        {
            case ARP_CHORD_TONES:
                // 4-5 notes per bar, repeating pattern
                // Create a 1-bar pattern that repeats
                {
                    int notesPerBar = 4 + (seed % 2);  // 4-5 notes
                    uint8_t usedPositions = 0;

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        int attempts = 0;
                        int idx;
                        do {
                            idx = (seed >> (i * 3 + attempts)) % 8;
                            attempts++;
                        } while((usedPositions & (1 << idx)) && attempts < 10);

                        usedPositions |= (1 << idx);
                        // Set in both bars (first bar and second bar)
                        voice->rhythmPattern |= (1 << eighthNotes[idx]);
                        voice->rhythmPattern |= (1 << (eighthNotes[idx] + 16));
                    }
                }
                break;

            case ARP_SCALE_ASCENDING:
                // 5 notes ascending, starting on downbeat
                // Pattern: 1--2--3--4--5--- repeated
                {
                    int notesPerBar = 5;
                    // Space notes evenly across the bar (roughly every 3 16th notes)
                    int positions[] = {0, 3, 6, 10, 13};  // Evenly spaced

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        voice->rhythmPattern |= (1 << positions[i]);
                        voice->rhythmPattern |= (1 << (positions[i] + 16));  // Repeat in bar 2
                    }
                }
                break;

            case ARP_SCALE_RANDOM:
                // 4-6 random positions per bar, slightly syncopated
                {
                    int notesPerBar = 4 + (seed % 3);  // 4-6 notes
                    uint32_t usedPositions = 0;

                    for(int i = 0; i < notesPerBar; i++)
                    {
                        int attempts = 0;
                        int pos;
                        do {
                            // Allow 16th note positions but prefer 8th notes
                            if((seed >> (i + attempts)) % 3 == 0)
                                pos = (seed >> (i * 4 + attempts)) % 16;  // Any 16th in first bar
                            else
                                pos = eighthNotes[(seed >> (i * 3 + attempts)) % 8];  // 8th notes
                            attempts++;
                        } while((usedPositions & (1 << pos)) && attempts < 15);

                        usedPositions |= (1 << pos);
                        voice->rhythmPattern |= (1 << pos);
                        voice->rhythmPattern |= (1 << (pos + 16));  // Mirror to bar 2
                    }
                }
                break;
        }
    }
}

// Generate note sequence for a melody voice based on style
// Uses shared melodyScale and melodyRoot for both voices
void GenerateMelodyNotesFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();
    voice->sequencePos = 0;

    if(voice->style == MELODY_SUPPORTING)
    {
        // Supporting: Few different notes, low octave, minimal movement
        // Use 2-4 distinct notes, mostly root and 5th
        int8_t baseNotes[4];
        baseNotes[0] = GetScaleNote(melodyScale, melodyRoot, 0);  // Root
        baseNotes[1] = GetScaleNote(melodyScale, melodyRoot, 4);  // 5th
        baseNotes[2] = GetScaleNote(melodyScale, melodyRoot, 2);  // 3rd
        baseNotes[3] = GetScaleNote(melodyScale, melodyRoot, -1); // 7th below

        // Fill sequence with mostly root and 5th
        for(int i = 0; i < 32; i++)
        {
            int noteChoice = (seed >> (i * 2)) % 10;
            if(noteChoice < 5)
                voice->noteSequence[i] = baseNotes[0]; // Root 50%
            else if(noteChoice < 8)
                voice->noteSequence[i] = baseNotes[1]; // 5th 30%
            else if(noteChoice < 9)
                voice->noteSequence[i] = baseNotes[2]; // 3rd 10%
            else
                voice->noteSequence[i] = baseNotes[3]; // 7th 10%
        }

        // Occasional octave jump (10% chance per note)
        for(int i = 0; i < 32; i++)
        {
            if(((seed >> (i + 8)) % 10) == 0)
            {
                voice->noteSequence[i] += 12; // Jump up one octave
            }
        }
    }
    else // MELODY_ARPEGGIATOR
    {
        int8_t scaleLen = scaleLengths[melodyScale];

        switch(voice->subStyle)
        {
            case ARP_CHORD_TONES:
                // 4-5 chord tones that repeat: root, 3rd, 5th, (octave), (7th)
                {
                    int8_t chordNotes[5];
                    int numNotes = 4 + (seed % 2);  // 4-5 notes
                    int8_t chordDegrees[] = {0, 2, 4, 7, 9}; // root, 3rd, 5th, octave, 9th

                    // Generate the chord note set
                    for(int n = 0; n < numNotes; n++)
                    {
                        chordNotes[n] = GetScaleNote(melodyScale, melodyRoot, chordDegrees[n]);
                    }

                    // Fill sequence by cycling through chord notes
                    for(int i = 0; i < 32; i++)
                    {
                        // Same pattern in bar 1 and bar 2
                        int barPos = i % 16;
                        voice->noteSequence[i] = chordNotes[barPos % numNotes];
                    }
                }
                break;

            case ARP_SCALE_ASCENDING:
                // 5 ascending scale notes that repeat
                {
                    int8_t arpNotes[5];
                    for(int n = 0; n < 5; n++)
                    {
                        arpNotes[n] = GetScaleNote(melodyScale, melodyRoot, n);
                    }

                    // Fill sequence - same 5-note pattern repeats
                    for(int i = 0; i < 32; i++)
                    {
                        int barPos = i % 16;
                        voice->noteSequence[i] = arpNotes[barPos % 5];
                    }
                }
                break;

            case ARP_SCALE_RANDOM:
                // 4-6 random scale notes that repeat
                {
                    int8_t arpNotes[6];
                    int numNotes = 4 + (seed % 3);  // 4-6 notes

                    // Generate random scale degrees within 2 octaves
                    for(int n = 0; n < numNotes; n++)
                    {
                        int degree = (seed >> (n * 4)) % (scaleLen * 2);
                        arpNotes[n] = GetScaleNote(melodyScale, melodyRoot, degree);
                    }

                    // Fill sequence by cycling through random notes
                    for(int i = 0; i < 32; i++)
                    {
                        int barPos = i % 16;
                        voice->noteSequence[i] = arpNotes[barPos % numNotes];
                    }
                }
                break;
        }
    }
}

// Generate complete melody pattern for a voice (rhythm + notes)
void GenerateMelodyPatternFor(MelodyConfig* voice)
{
    GenerateMelodyRhythmFor(voice);
    GenerateMelodyNotesFor(voice);
}

// Generate patterns for both melody voices
void GenerateMelodyPattern()
{
    GenerateMelodyPatternFor(&melodyVoice);
    GenerateMelodyPatternFor(&melodyMidiVoice);
}

// Randomize personality for a single melody voice
void RandomizeMelodyPersonalityFor(MelodyConfig* voice)
{
    uint32_t seed = System::GetUs();

    // Randomize main style
    voice->style = (MelodyStyle)(seed % NUM_MELODY_STYLES);

    // Randomize sub-style based on main style
    if(voice->style == MELODY_SUPPORTING)
    {
        voice->subStyle = (seed >> 4) % NUM_SUPPORTING_SUBSTYLES;
    }
    else
    {
        voice->subStyle = (seed >> 4) % NUM_ARP_SUBSTYLES;
    }

    // Generate new pattern with new personality
    GenerateMelodyPatternFor(voice);
}

// Randomize melody personality (style and sub-style) for both voices
void RandomizeMelodyPersonality()
{
    RandomizeMelodyPersonalityFor(&melodyVoice);
    RandomizeMelodyPersonalityFor(&melodyMidiVoice);
}

// Randomize ALL parameters (drums + melody) - for config menu
void RandomizeAllParameters()
{
    // Randomize drum patterns
    RandomizePatterns();

    // Randomize drum voice personalities
    RandomizeVoicePersonalities();

    // Randomize groove
    RandomizeGroove();

    // Randomize melody (both voices)
    RandomizeMelodyPersonality();
}

// Process drum patterns on each 16th note
void ProcessDrumPatterns()
{
    if(!isRunning)
        return;

    // Calculate when the NEXT beat will occur (look-ahead scheduling)
    float samplesPerSixteenth = hw.AudioSampleRate() * 15.0f / bpm;
    uint64_t nextBeatSample = lastBeatSample + (uint64_t)samplesPerSixteenth;

    // Calculate total step within 8-bar cycle (0-127)
    uint8_t totalStep = (barCounter * 32) + currentStep;

    // Randomize groove at start of new 8-bar cycle (25% probability)
    if(barCounter == 0 && currentStep == 0)
    {
        uint32_t seed = System::GetUs();
        if((seed % 100) < 25) // 25% chance
        {
            RandomizeGroove();
        }
    }

    // Schedule fill at the start of the 7th-8th bar (barCounter == 3, step == 0)
    if(barCounter == 3 && currentStep == 0)
    {
        ScheduleFill();
    }

    // Check if we're in fill mode
    bool inFill = fillActive && (totalStep >= fillStartStep);

    if(inFill)
    {
        // Calculate step within fill (0-7 for half-bar, 0-15 for whole-bar)
        uint8_t fillStep = totalStep - fillStartStep;

        // Trigger fills
        if(fillIsHalfBar)
        {
            // Half-bar fills (8 steps)
            if(IsStepActive8(kickFillsHalf[fillKickIdx], fillStep))
                ScheduleDrumTriggerWithGroove(KICK, 120, currentStep, nextBeatSample); // Louder for fills

            if(IsStepActive8(clapFillsHalf[fillClapIdx], fillStep))
                ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 115, currentStep, nextBeatSample);

            if(IsStepActive8(hatClosedFillsHalf[fillHatIdx], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 100, currentStep, nextBeatSample);

            if(IsStepActive8(hatOpenFillsHalf[fillHatIdx], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 110, currentStep, nextBeatSample);
        }
        else
        {
            // Whole-bar fills (16 steps)
            if(IsStepActive16(kickFillsWhole[fillKickIdx], fillStep))
                ScheduleDrumTriggerWithGroove(KICK, 120, currentStep, nextBeatSample); // Louder for fills

            if(IsStepActive16(clapFillsWhole[fillClapIdx], fillStep))
                ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 115, currentStep, nextBeatSample);

            if(IsStepActive16(hatClosedFillsWhole[fillHatIdx], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 100, currentStep, nextBeatSample);

            if(IsStepActive16(hatOpenFillsWhole[fillHatIdx], fillStep))
                ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 110, currentStep, nextBeatSample);
        }
    }
    else
    {
        // Normal patterns
        if(IsStepActive(kickPatterns[currentKickPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(KICK, 110, currentStep, nextBeatSample); // Kick slightly louder
        }

        if(IsStepActive(clapPatterns[currentClapPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(fundamentalBeatVoice, 100, currentStep, nextBeatSample);
        }

        // Check both closed and open hi-hat patterns
        if(IsStepActive(hatClosedPatterns[currentHatPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(HIHAT1_CLOSED, 70, currentStep, nextBeatSample); // Closed hi-hat subtle
        }

        if(IsStepActive(hatOpenPatterns[currentHatPattern], currentStep))
        {
            ScheduleDrumTriggerWithGroove(HIHAT1_OPEN, 100, currentStep, nextBeatSample); // Open hi-hat emphasized
        }
    }

    // Process generative voices (with polyrhythm support)
    // Note: ANALOG voice is now handled by melody system, skip it in generative voices
    for(int i = 0; i < 6; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->active && voice->voice != ANALOG)
        {
            // Use modulo to loop shorter patterns (polyrhythms)
            uint8_t voiceStep = currentStep % voice->patternLength;
            if(IsStepActive(voice->pattern, voiceStep))
            {
                // Regular MIDI voice
                ScheduleDrumTriggerWithGroove(voice->voice, 95, currentStep, nextBeatSample);
            }
        }
    }

    // Tune mode: output middle C quarter notes on both melody channels
    if(tuneModeEnabled)
    {
        // Quarter notes = every 4 steps (steps 0, 4, 8, 12, 16, 20, 24, 28)
        bool isQuarterNote = (currentStep % 4) == 0;

        if(isQuarterNote)
        {
            // CV output: Middle C = C3 = 1V (semitone 12 from C2)
            const int8_t middleC_semitone = 12;
            float cvVoltage = MelodyNoteToCV(middleC_semitone); // 1.0V
            uint16_t cv1 = (uint16_t)(cvVoltage / 5.0f * 4095.0f);
            hw.seed.dac.WriteValue(DacHandle::Channel::ONE, cv1);

            // Trigger CV gate
            analogGateHigh = true;
            analogGateCounter = 0;

            // MIDI output: Middle C = MIDI note 60
            const uint8_t middleC_midi = 60;

            // Send note-off for previous note if playing
            if(midiMelodyNoteOn)
            {
                uint8_t noteOff[3] = {
                    static_cast<uint8_t>(0x80 | melodyMidiChannel),
                    lastMidiMelodyNote,
                    0
                };
                hw.midi.SendMessage(noteOff, 3);
            }

            // Send note-on for middle C
            uint8_t noteOn[3] = {
                static_cast<uint8_t>(0x90 | melodyMidiChannel),
                middleC_midi,
                100
            };
            hw.midi.SendMessage(noteOn, 3);

            lastMidiMelodyNote = middleC_midi;
            midiMelodyNoteOn = true;
        }
    }
    else
    {
        // Normal melody processing with groove timing (only when tune mode is off)

        // Schedule CV melody voice with groove timing
        if(melodyVoice.active)
        {
            uint8_t melodyStep = currentStep % melodyVoice.patternLength;
            if(IsStepActive(melodyVoice.rhythmPattern, melodyStep))
            {
                int8_t note = melodyVoice.noteSequence[melodyStep];
                ScheduleMelodyTrigger(MELODY_CV, note, currentStep, nextBeatSample);
            }
        }

        // Schedule MIDI melody voice with groove timing
        if(melodyMidiVoice.active)
        {
            uint8_t midiMelStep = currentStep % melodyMidiVoice.patternLength;
            if(IsStepActive(melodyMidiVoice.rhythmPattern, midiMelStep))
            {
                int8_t note = melodyMidiVoice.noteSequence[midiMelStep];
                ScheduleMelodyTrigger(MELODY_MIDI, note, currentStep, nextBeatSample);
            }
        }
    }

    // Advance step
    currentStep++;
    if(currentStep >= 32) // Now 32 steps = 2 bars
    {
        currentStep = 0;
        barCounter++; // Counts 2-bar phrases

        // Auto-randomize patterns after interval (every 8 bars)
        if(barCounter >= patternChangeInterval)
        {
            if(!freezeEnabled)
            {
                RandomizePatterns();
            }

            // Melody has independent freeze control
            if(!melodyFreezeEnabled)
            {
                GenerateMelodyPattern();
            }

            barCounter = 0;
            fillActive = false; // Reset fill for next cycle
            cycleCounter++; // Count 8-bar cycles

            // Randomize voice personalities after longer interval (every 32 bars)
            if(cycleCounter >= personalityChangeInterval)
            {
                if(!freezeEnabled)
                {
                    RandomizeVoicePersonalities();
                }

                // Melody personality also changes at this interval (if not frozen)
                if(!melodyFreezeEnabled)
                {
                    RandomizeMelodyPersonality();
                }

                cycleCounter = 0;
            }
        }
    }
}

// Audio callback - outputs gate signals on the four audio outputs
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    for(size_t i = 0; i < size; i++)
    {
        // Process scheduled MIDI triggers (sample-accurate groove timing)
        ProcessTriggerQueue();

        // Process scheduled melody triggers (sample-accurate groove timing)
        ProcessMelodyQueue();

        // Increment global sample counter
        globalSampleCounter++;

        // Process Metro for timing (only in internal clock mode)
        if(!externalClockMode && isRunning)
        {
            if(clockMetro.Process())
            {
                trigger16thNote = true; // Signal main loop to process patterns
                lastBeatSample = globalSampleCounter; // Record exact sample time of beat
            }
        }

        // Update gate pulse counters and turn off gates after pulse width
        if(analogGateHigh)
        {
            analogGateCounter++;
            if(analogGateCounter >= ANALOG_GATE_SAMPLES)
                analogGateHigh = false;
        }

        if(gate16th)
        {
            gate16thCounter++;
            if(gate16thCounter >= GATE_PULSE_SAMPLES)
                gate16th = false;
        }

        if(gate2)
        {
            gate2Counter++;
            if(gate2Counter >= GATE_PULSE_SAMPLES)
                gate2 = false;
        }

        if(gateQuarter)
        {
            gateQuarterCounter++;
            if(gateQuarterCounter >= GATE_PULSE_SAMPLES)
                gateQuarter = false;
        }

        if(gateReset)
        {
            gateResetCounter++;
            if(gateResetCounter >= RESET_PULSE_SAMPLES)
                gateReset = false;
        }

        // Output gate signals (5V = 1.0f, 0V = 0.0f for CV outputs)
        out[0][i] = gate16th ? 1.0f : 0.0f;       // Out 1: 16th notes
        out[1][i] = gate2 ? 1.0f : 0.0f;          // Out 2: Configurable division
        out[2][i] = gateQuarter ? 1.0f : 0.0f;    // Out 3: Configurable division
        out[3][i] = gateReset ? 1.0f : 0.0f;      // Out 4: Reset pulse
    }
}

void UpdateClockFrequency()
{
    // Convert BPM to frequency for 16th notes
    // 16th notes per minute = BPM * 4
    // Frequency = (BPM * 4) / 60
    float sixteenthNotesPerSecond = (bpm * 4.0f) / 60.0f;
    clockMetro.SetFreq(sixteenthNotesPerSecond);

    // Update groove offsets when BPM changes
    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        voiceGroove[i].UpdateOffset(bpm, hw.AudioSampleRate());
    }
}

void SendMidiClock()
{
    uint8_t clockMsg[1] = {0xF8}; // MIDI Clock message
    hw.midi.SendMessage(clockMsg, 1);
}

void SendMidiStart()
{
    uint8_t startMsg[1] = {0xFA}; // MIDI Start message
    hw.midi.SendMessage(startMsg, 1);
    midiClockCounter = 0;
}

void SendMidiStop()
{
    uint8_t stopMsg[1] = {0xFC}; // MIDI Stop message
    hw.midi.SendMessage(stopMsg, 1);
}

void SendMidiContinue()
{
    uint8_t continueMsg[1] = {0xFB}; // MIDI Continue message
    hw.midi.SendMessage(continueMsg, 1);
}

void ToggleRunState()
{
    if(!externalClockMode)
    {
        isRunning = !isRunning;
        if(isRunning)
        {
            SendMidiStart();
            clockMetro.Reset();
            TriggerGateReset(); // Trigger reset pulse on start
            currentStep = 0;    // Reset pattern position
            barCounter = 0;     // Reset bar counter
            cycleCounter = 0;   // Reset cycle counter
            midiClockCounter = 0; // Reset MIDI clock counter
            generationSeed = System::GetUs(); // Initialize seed
            RandomizeVoicePersonalities(); // Randomize voice personalities at start
            GenerateVoicePatterns(); // Generate initial patterns
            RandomizeMelodyPersonality(); // Randomize melody personality at start

            // Initialize groove configurations
            for(int i = 0; i < NUM_DRUM_VOICES; i++)
            {
                voiceGroove[i].Init();
                voiceGroove[i].UpdateOffset(bpm, hw.AudioSampleRate());
            }

            // Randomize groove at start
            RandomizeGroove();

            // Initialize trigger queue
            InitTriggerQueue();
            globalSampleCounter = 0;
            lastBeatSample = 0;

            // Trigger first step immediately to avoid missing first downbeat
            trigger16thNote = true;
        }
        else
        {
            SendMidiStop();
        }
    }
}

void HandleMidiMessage(MidiEvent m)
{
    switch(m.type)
    {
        case SystemRealTime:
        {
            SystemRealTimeType rt = m.srt_type;
            switch(rt)
            {
                case TimingClock:
                    // Receiving external MIDI clock
                    externalClockMode = true;
                    lastMidiClockTime = System::GetNow();

                    if(isRunning)
                    {
                        midiClockCounter++;

                        // Send our own MIDI clock through
                        SendMidiClock();

                        // Trigger 24 PPQN gate on every clock
                        TriggerGate24ppqn();

                        // Trigger 16th note gate on every 6th clock
                        if(midiClockCounter % CLOCKS_PER_16TH == 0)
                        {
                            gateHigh = true;
                            gateHighCounter = 0;
                            TriggerGate16th();

                            // Signal main loop to process patterns (same as internal clock)
                            trigger16thNote = true;
                            lastBeatSample = globalSampleCounter; // Record beat time for look-ahead

                            // OUT3 gate will be triggered in ProcessClock() based on division setting
                        }
                    }
                    break;

                case Start:
                    // External start
                    externalClockMode = true;
                    isRunning = true;
                    midiClockCounter = 0;
                    lastMidiClockTime = System::GetNow();
                    SendMidiStart();
                    TriggerGateReset(); // Trigger reset pulse on external start
                    currentStep = 0;    // Reset pattern position
                    barCounter = 0;     // Reset bar counter
                    cycleCounter = 0;   // Reset cycle counter
                    break;

                case Stop:
                    // External stop
                    externalClockMode = true;
                    isRunning = false;
                    lastMidiClockTime = System::GetNow();
                    SendMidiStop();
                    gateHigh = false;
                    break;

                case Continue:
                    // External continue
                    externalClockMode = true;
                    isRunning = true;
                    lastMidiClockTime = System::GetNow();
                    SendMidiContinue();
                    break;

                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

void UpdateDisplay()
{
    hw.display.Fill(false);

    std::string str;
    char*       cstr;
    char        buffer[30];
    int         configScrollOffset = 0;
    int         displayRow = 0;

    switch(currentDisplayState)
    {
        case DISPLAY_DEFAULT:
            // BPM, Mode, Status, and Bar:Beat counter on one line
            hw.display.SetCursor(0, 0);
            if(isRunning)
            {
                // Calculate bar (1-8) using barCounter (counts 2-bar phrases)
                int bar = (barCounter * 2) + (currentStep / 16) + 1;  // 1-8
                int beat = (currentStep % 16) / 4 + 1; // 1-4
                sprintf(buffer, "BPM:%d %s %s %d:%d",
                        (int)bpm,
                        externalClockMode ? "EXT" : "INT",
                        isRunning ? "RUN" : "STOP",
                        bar, beat);
            }
            else
            {
                sprintf(buffer, "BPM:%d %s %s",
                        (int)bpm,
                        externalClockMode ? "EXT" : "INT",
                        isRunning ? "RUN" : "STOP");
            }
            hw.display.WriteString(buffer, Font_6x8, true);

            // Show tune mode indicator or normal melody info
            if(tuneModeEnabled)
            {
                // Tune mode active - show clear indicator
                hw.display.SetCursor(0, 12);
                hw.display.WriteString(">>> TUNE MODE <<<", Font_6x8, true);
                hw.display.SetCursor(0, 22);
                hw.display.WriteString("Middle C (1V/60)", Font_6x8, true);
            }
            else
            {
                // Shared scale/root info (Line 2)
                hw.display.SetCursor(0, 12);
                sprintf(buffer, "Key: %s %s",
                        rootNoteNames[melodyRoot],
                        scaleNames[melodyScale]);
                hw.display.WriteString(buffer, Font_6x8, true);

                // CV voice info (Line 3)
                hw.display.SetCursor(0, 22);
                sprintf(buffer, "CV: %s", melodyStyleNames[melodyVoice.style]);
                hw.display.WriteString(buffer, Font_6x8, true);

                // MIDI voice info (Line 4)
                hw.display.SetCursor(0, 32);
                sprintf(buffer, "MIDI: %s Ch:%d",
                        melodyStyleNames[melodyMidiVoice.style],
                        melodyMidiChannel + 1);
                hw.display.WriteString(buffer, Font_6x8, true);
            }
            break;

        case DISPLAY_CONFIG_MENU:
        {
            // Show config menu with values (arrow on config name)
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            // Calculate scroll offset to keep selected item visible
            // Display can show 5 items (rows 10, 20, 30, 40, 50)
            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue; // Skip items outside visible area
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, ">%-11s%d",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            (int)bpm);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%d", configOptionNames[i], (int)bpm);
                    }
                }
                else if(i == CONFIG_OUT2_DIVISION)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            outDivisionNames[currentOut2Division]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], outDivisionNames[currentOut2Division]);
                    }
                }
                else if(i == CONFIG_OUT3_DIVISION)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            outDivisionNames[currentOut3Division]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], outDivisionNames[currentOut3Division]);
                    }
                }
                else if(i == CONFIG_FREEZE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            freezeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], freezeEnabled ? "On" : "Off");
                    }
                }
                else if(i == CONFIG_MELODY_SCALE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            scaleNames[melodyScale]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], scaleNames[melodyScale]);
                    }
                }
                else if(i == CONFIG_MELODY_ROOT)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            rootNoteNames[melodyRoot]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], rootNoteNames[melodyRoot]);
                    }
                }
                else if(i == CONFIG_CV_STYLE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyStyleNames[melodyVoice.style]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyStyleNames[melodyVoice.style]);
                    }
                }
                else if(i == CONFIG_MIDI_STYLE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyStyleNames[melodyMidiVoice.style]);
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyStyleNames[melodyMidiVoice.style]);
                    }
                }
                else if(i == CONFIG_MIDI_MEL_CH)
                {
                    sprintf(buffer, ">%-11s%d",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyMidiChannel + 1);  // Display 1-indexed
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%d", configOptionNames[i], melodyMidiChannel + 1);
                    }
                }
                else if(i == CONFIG_MELODY_FREEZE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            melodyFreezeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], melodyFreezeEnabled ? "On" : "Off");
                    }
                }
                else if(i == CONFIG_TUNE_MODE)
                {
                    sprintf(buffer, ">%-11s%s",
                            (i == currentConfigOption) ? configOptionNames[i] : "",
                            tuneModeEnabled ? "On" : "Off");
                    if(i != currentConfigOption)
                    {
                        sprintf(buffer, " %-11s%s", configOptionNames[i], tuneModeEnabled ? "On" : "Off");
                    }
                }
                else // CONFIG_PATTERN_INFO, CONFIG_BACK, or CONFIG_RANDOMIZE_ALL
                {
                    sprintf(buffer, "%s%s",
                            (i == currentConfigOption) ? ">" : " ",
                            configOptionNames[i]);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }
        }
        break;

        case DISPLAY_CONFIG_EDIT:
        {
            // Show config menu with values (arrow on value being edited)
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=== CONFIG ===", Font_6x8, true);

            // Calculate scroll offset to keep selected item visible
            // Display can show 5 items (rows 10, 20, 30, 40, 50)
            configScrollOffset = 0;
            if(currentConfigOption > 4) configScrollOffset = currentConfigOption - 4;

            for(int i = 0; i < NUM_CONFIG_OPTIONS; i++)
            {
                displayRow = i - configScrollOffset;
                if(displayRow < 0 || displayRow > 4) continue; // Skip items outside visible area
                hw.display.SetCursor(0, 10 + displayRow * 10);

                if(i == CONFIG_BPM)
                {
                    sprintf(buffer, " %-10s%s%d",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            (int)bpm);
                }
                else if(i == CONFIG_OUT2_DIVISION)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            outDivisionNames[currentOut2Division]);
                }
                else if(i == CONFIG_OUT3_DIVISION)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            outDivisionNames[currentOut3Division]);
                }
                else if(i == CONFIG_FREEZE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            freezeEnabled ? "On" : "Off");
                }
                else if(i == CONFIG_MELODY_SCALE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            scaleNames[melodyScale]);
                }
                else if(i == CONFIG_MELODY_ROOT)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            rootNoteNames[melodyRoot]);
                }
                else if(i == CONFIG_CV_STYLE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyStyleNames[melodyVoice.style]);
                }
                else if(i == CONFIG_MIDI_STYLE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyStyleNames[melodyMidiVoice.style]);
                }
                else if(i == CONFIG_MIDI_MEL_CH)
                {
                    sprintf(buffer, " %-10s%s%d",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyMidiChannel + 1);  // Display 1-indexed
                }
                else if(i == CONFIG_MELODY_FREEZE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            melodyFreezeEnabled ? "On" : "Off");
                }
                else if(i == CONFIG_TUNE_MODE)
                {
                    sprintf(buffer, " %-10s%s%s",
                            configOptionNames[i],
                            (i == currentConfigOption) ? ">" : " ",
                            tuneModeEnabled ? "On" : "Off");
                }
                else // CONFIG_PATTERN_INFO, CONFIG_BACK, or CONFIG_RANDOMIZE_ALL
                {
                    sprintf(buffer, " %s", configOptionNames[i]);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }

            // Show hint at bottom
            if(externalClockMode && currentConfigOption == CONFIG_BPM)
            {
                hw.display.SetCursor(0, 50);
                hw.display.WriteString("(External Clock)", Font_6x8, true);
            }
        }
        break;

        case DISPLAY_PATTERN_INFO:
            // Pattern info display
            hw.display.SetCursor(0, 0);
            hw.display.WriteString("=PATTERN INFO=", Font_6x8, true);

            // Voice labels (will be updated for index 3 based on which plays backbeat)
            const char* voiceLabels[] = {"D1", "D2", "ML", "??", "H2", "An"};
            voiceLabels[3] = (fundamentalBeatVoice == CLAP) ? "SN" : "CL";

            const char* styleNames[] = {"Syn", "Str", "Euc", "AEu", "FKi"};
            const char* densityNames[] = {"Lo", "Md", "Hi"};
            const char* interactionSymbols[] = {"", "Div", "AB", "AH", "A2"};

            // We have 7 total lines: 1 fundamental beat + 6 generative voices
            // Display can show 4 lines at once (rows 10, 24, 38, 52) - tighter spacing for 64px display
            int totalLines = 7;
            int maxScroll = totalLines - 4;

            // Display 4 lines starting from scroll offset
            for(int i = 0; i < 4; i++)
            {
                int lineIdx = i + patternInfoScroll;
                if(lineIdx >= totalLines) break;

                hw.display.SetCursor(0, 10 + i * 14);

                if(lineIdx == 0)
                {
                    // First line: fundamental beat
                    char beatVoiceName = (fundamentalBeatVoice == CLAP) ? 'C' : 'S';
                    sprintf(buffer, "%-2c:Fund Beat", beatVoiceName);
                }
                else
                {
                    // Generative voices (lineIdx 1-6 = voiceIdx 0-5)
                    int voiceIdx = lineIdx - 1;
                    VoiceConfig* voice = &generativeVoices[voiceIdx];

                    // Find interaction partner name
                    char partnerName[4] = "";
                    if(voice->interaction != INTERACTION_NONE)
                    {
                        // Find which voice index is the partner
                        for(int j = 0; j < 6; j++)
                        {
                            if(generativeVoices[j].voice == voice->interactionPartner)
                            {
                                strncpy(partnerName, voiceLabels[j], 3);
                                partnerName[2] = '\0';
                                break;
                            }
                        }
                    }

                    // Format: "D1 :Euc Hi L32 >D2" (aligned columns)
                    sprintf(buffer, "%-2s:%s %s L%-2d%s%s",
                            voiceLabels[voiceIdx],
                            styleNames[voice->rhythmStyle],
                            densityNames[voice->density],
                            voice->patternLength,
                            (voice->interaction != INTERACTION_NONE) ? " >" : "",
                            partnerName);
                }

                hw.display.WriteString(buffer, Font_6x8, true);
            }

            // Show scroll indicators
            if(patternInfoScroll > 0)
            {
                hw.display.SetCursor(120, 10);
                hw.display.WriteString("^", Font_6x8, true);
            }
            if(patternInfoScroll < maxScroll)
            {
                hw.display.SetCursor(120, 50);
                hw.display.WriteString("v", Font_6x8, true);
            }
            break;
    }

    hw.display.Update();
}

void ProcessControls()
{
    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();

    int32_t inc = hw.encoder.Increment();
    bool buttonPressed = hw.encoder.RisingEdge();
    uint32_t now = System::GetNow();

    // Handle encoder based on display state
    switch(currentDisplayState)
    {
        case DISPLAY_DEFAULT:
            // In default mode, encoder rotation enters config menu
            if(inc != 0)
            {
                currentDisplayState = DISPLAY_CONFIG_MENU;
                currentConfigOption = CONFIG_BPM;
                lastEncoderActivity = now;
            }
            // Encoder button toggles run/stop
            else if(buttonPressed)
            {
                ToggleRunState();
            }
            break;

        case DISPLAY_CONFIG_MENU:
            // In config menu, encoder rotates through options
            if(inc != 0)
            {
                int option = (int)currentConfigOption + inc;
                if(option < 0) option = 0;
                if(option >= NUM_CONFIG_OPTIONS) option = NUM_CONFIG_OPTIONS - 1;
                currentConfigOption = (ConfigOption)option;
                lastEncoderActivity = now;
            }
            // Encoder button selects option
            else if(buttonPressed)
            {
                if(currentConfigOption == CONFIG_BACK)
                {
                    // Back to default display
                    currentDisplayState = DISPLAY_DEFAULT;
                }
                else if(currentConfigOption == CONFIG_PATTERN_INFO)
                {
                    // Enter pattern info display
                    currentDisplayState = DISPLAY_PATTERN_INFO;
                    patternInfoScroll = 0;
                }
                else if(currentConfigOption == CONFIG_FREEZE)
                {
                    // Toggle freeze directly (binary option)
                    freezeEnabled = !freezeEnabled;
                    SaveSettings();
                }
                else if(currentConfigOption == CONFIG_MELODY_FREEZE)
                {
                    // Toggle melody freeze directly (binary option)
                    melodyFreezeEnabled = !melodyFreezeEnabled;
                    SaveSettings();
                }
                else if(currentConfigOption == CONFIG_TUNE_MODE)
                {
                    // Toggle tune mode directly (binary option)
                    tuneModeEnabled = !tuneModeEnabled;
                    // Note: tune mode is not saved to flash - it's a temporary mode
                }
                else if(currentConfigOption == CONFIG_RANDOMIZE_ALL)
                {
                    // Randomize all parameters immediately
                    RandomizeAllParameters();
                }
                else
                {
                    // Enter edit mode for selected config
                    currentDisplayState = DISPLAY_CONFIG_EDIT;
                }
                lastEncoderActivity = now;
            }
            // Timeout check (only in config menu, not in edit mode)
            else if(now - lastEncoderActivity > MENU_TIMEOUT_MS)
            {
                currentDisplayState = DISPLAY_DEFAULT;
            }
            break;

        case DISPLAY_CONFIG_EDIT:
            // In edit mode, encoder changes value
            if(inc != 0)
            {
                switch(currentConfigOption)
                {
                    case CONFIG_BPM:
                        if(!externalClockMode)
                        {
                            bpm += inc * 0.5f;
                            bpm = fclamp(bpm, 20.0f, 300.0f);
                            UpdateClockFrequency();
                        }
                        break;

                    case CONFIG_OUT2_DIVISION:
                        {
                            int div = (int)currentOut2Division + inc;
                            if(div < 0) div = 0;
                            if(div >= NUM_OUT_DIVISIONS) div = NUM_OUT_DIVISIONS - 1;
                            currentOut2Division = (OutDivision)div;
                        }
                        break;

                    case CONFIG_OUT3_DIVISION:
                        {
                            int div = (int)currentOut3Division + inc;
                            if(div < 0) div = 0;
                            if(div >= NUM_OUT_DIVISIONS) div = NUM_OUT_DIVISIONS - 1;
                            currentOut3Division = (OutDivision)div;
                        }
                        break;

                    // CONFIG_FREEZE is handled directly in menu, not in edit mode

                    case CONFIG_MELODY_SCALE:
                        {
                            int scale = (int)melodyScale + inc;
                            if(scale < 0) scale = 0;
                            if(scale >= NUM_SCALE_TYPES) scale = NUM_SCALE_TYPES - 1;
                            melodyScale = (ScaleType)scale;
                            // Regenerate both melody voices with new scale
                            GenerateMelodyPatternFor(&melodyVoice);
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_MELODY_ROOT:
                        {
                            int root = (int)melodyRoot + inc;
                            if(root < 0) root = 0;
                            if(root >= 12) root = 11;
                            melodyRoot = (uint8_t)root;
                            // Regenerate both melody voices with new root
                            GenerateMelodyPatternFor(&melodyVoice);
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_CV_STYLE:
                        {
                            int style = (int)melodyVoice.style + inc;
                            if(style < 0) style = 0;
                            if(style >= NUM_MELODY_STYLES) style = NUM_MELODY_STYLES - 1;
                            melodyVoice.style = (MelodyStyle)style;
                            // Also randomize sub-style when changing style
                            if(melodyVoice.style == MELODY_SUPPORTING)
                                melodyVoice.subStyle = System::GetUs() % NUM_SUPPORTING_SUBSTYLES;
                            else
                                melodyVoice.subStyle = System::GetUs() % NUM_ARP_SUBSTYLES;
                            // Regenerate CV melody with new style
                            GenerateMelodyPatternFor(&melodyVoice);
                        }
                        break;

                    case CONFIG_MIDI_STYLE:
                        {
                            int style = (int)melodyMidiVoice.style + inc;
                            if(style < 0) style = 0;
                            if(style >= NUM_MELODY_STYLES) style = NUM_MELODY_STYLES - 1;
                            melodyMidiVoice.style = (MelodyStyle)style;
                            // Also randomize sub-style when changing style
                            if(melodyMidiVoice.style == MELODY_SUPPORTING)
                                melodyMidiVoice.subStyle = System::GetUs() % NUM_SUPPORTING_SUBSTYLES;
                            else
                                melodyMidiVoice.subStyle = System::GetUs() % NUM_ARP_SUBSTYLES;
                            // Regenerate MIDI melody with new style
                            GenerateMelodyPatternFor(&melodyMidiVoice);
                        }
                        break;

                    case CONFIG_MIDI_MEL_CH:
                        {
                            int ch = (int)melodyMidiChannel + inc;
                            if(ch < 0) ch = 0;
                            if(ch >= 16) ch = 15;
                            melodyMidiChannel = (uint8_t)ch;
                        }
                        break;

                    // CONFIG_MELODY_FREEZE is handled directly in menu, not in edit mode

                    default:
                        break;
                }
            }
            // Encoder button confirms and returns to config menu
            else if(buttonPressed)
            {
                // Save settings to persistent storage
                SaveSettings();

                currentDisplayState = DISPLAY_CONFIG_MENU;
                lastEncoderActivity = now;
            }
            break;

        case DISPLAY_PATTERN_INFO:
            // In pattern info mode, encoder scrolls
            if(inc != 0)
            {
                patternInfoScroll += inc;
                if(patternInfoScroll < 0) patternInfoScroll = 0;
                int maxScroll = 3; // 7 total lines - 4 visible = 3 max scroll
                if(patternInfoScroll > maxScroll) patternInfoScroll = maxScroll;
            }
            // Encoder button returns to config menu
            else if(buttonPressed)
            {
                currentDisplayState = DISPLAY_CONFIG_MENU;
            }
            break;
    }

    // Check for external clock timeout
    if(externalClockMode)
    {
        if(now - lastMidiClockTime > midiClockTimeout)
        {
            // No external clock received recently, switch to internal
            externalClockMode = false;
        }
    }
}

void ProcessClock()
{
    // Check if we got a 16th note trigger from audio callback
    if(trigger16thNote)
    {
        trigger16thNote = false; // Clear flag

        // Trigger gate output on gate out port
        gateHigh = true;
        gateHighCounter = 0;

        // Trigger 16th note gate
        TriggerGate16th();

        // Trigger OUT2 and OUT3 gates based on division settings
        // Calculate actual bar number (0-7) and step within bar (0-15)
        uint8_t actualBar = (barCounter * 2) + (currentStep / 16);
        uint8_t stepInBar = currentStep % 16;
        if(ShouldTriggerOut2(stepInBar, actualBar))
        {
            TriggerGate2();
        }
        if(ShouldTriggerOut3(stepInBar, actualBar))
        {
            TriggerGateQuarter();
        }

        // Process drum patterns on each 16th note
        ProcessDrumPatterns();

        // Send MIDI clock (24 PPQN)
        // For each 16th note, send 6 MIDI clocks and trigger 24ppqn gate
        for(int i = 0; i < CLOCKS_PER_16TH; i++)
        {
            SendMidiClock();
            TriggerGate24ppqn();
            midiClockCounter++;
        }
    }

    // Handle gate pulse timing for 16th note gate (on Audio Out 1)
    if(gateHigh)
    {
        gateHighCounter++;
        // Lower gate after GATE_PULSE_MS milliseconds
        // Assuming we're called at ~1kHz, this gives us ~10ms pulses
        if(gateHighCounter > GATE_PULSE_MS)
        {
            gateHigh = false;
        }
    }

    // Set gate output to Analog voice gate
    // Note: analogGateHigh timing is managed in AudioCallback at 48kHz
    dsy_gpio_write(&hw.gate_output, analogGateHigh ? 1 : 0);
}

int main(void)
{
    // Initialize hardware
    hw.Init();
    float samplerate = hw.AudioSampleRate();

    // Load saved settings from persistent storage
    LoadSettings();

    // Initialize clock metro
    clockMetro.Init(1.0f, samplerate);
    UpdateClockFrequency();

    // Display initial state
    hw.display.Fill(false);
    std::string str = "Themis";
    char*       cstr = &str[0];
    hw.display.WriteString(cstr, Font_7x10, true);
    hw.display.Update();

    // Start MIDI and Audio
    hw.midi.StartReceive();
    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    // Main loop
    uint32_t lastDisplayUpdate = System::GetNow();
    const uint32_t DISPLAY_UPDATE_RATE = 100; // Update display every 100ms

    for(;;)
    {
        uint32_t now = System::GetNow();

        // Process controls
        ProcessControls();

        // Process clock and gates
        ProcessClock();

        // Handle MIDI input
        hw.midi.Listen();
        while(hw.midi.HasEvents())
        {
            HandleMidiMessage(hw.midi.PopEvent());
        }

        // Update display at regular intervals
        if(now - lastDisplayUpdate > DISPLAY_UPDATE_RATE)
        {
            UpdateDisplay();
            lastDisplayUpdate = now;
        }

        // Small delay to prevent tight loop
        System::Delay(1);
    }
}
