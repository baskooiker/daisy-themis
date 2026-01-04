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
bool gate24ppqn = false;    // Audio Out 1: 24 PPQN
bool gate16th = false;      // Audio Out 2: 16th notes
bool gateQuarter = false;   // Audio Out 3: Quarter notes
bool gateReset = false;     // Audio Out 4: Reset pulse

// Gate pulse counters (in audio samples)
uint32_t gate24ppqnCounter = 0;
uint32_t gate16thCounter = 0;
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
    39  // CLAP (DIS)
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
    "Clap"
};

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
float grooveVelocityAmount[NUM_DRUM_VOICES]; // Per-voice velocity groove amount (0.0 - 0.75, max 75%)

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
    NUM_RHYTHM_STYLES
};

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
void GenerateVoicePatterns();

// Voice configurations for remaining drum elements
VoiceConfig generativeVoices[5] = {
    {DRUM1, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM2, 0, 32, true},
    {DRUM2, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM1, 0, 32, true},
    {MULTI, RHYTHM_SYNCOPATED, DENSITY_LOW, INTERACTION_NONE, MULTI, 0, 32, true},
    {SNARE, RHYTHM_STRAIGHT, DENSITY_MEDIUM, INTERACTION_ALTERNATE_BAR, HIHAT2_CLOSED, 0, 32, true},
    {HIHAT2_CLOSED, RHYTHM_STRAIGHT, DENSITY_HIGH, INTERACTION_ALTERNATE_BAR, SNARE, 0, 32, true}
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

// Two Turing Machine instances
TuringMachine turingMachine1;
TuringMachine turingMachine2;

// Pattern state
uint8_t  currentKickPattern = 0;
uint8_t  currentClapPattern = 0;
uint8_t  currentHatPattern = 0;
uint8_t  currentStep = 0; // Current step in pattern (0-31)
uint32_t barCounter = 0;  // Count 2-bar phrases for pattern rotation
uint32_t patternChangeInterval = 4; // Change patterns every N 2-bar phrases (4 = 8 bars)
uint32_t generationSeed = 0; // Seed for pattern generation

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
void TriggerGateQuarter() { gateQuarter = true; gateQuarterCounter = 0; }
void TriggerGateReset() { gateReset = true; gateResetCounter = 0; }

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
}

// Randomize groove pattern and per-voice amounts
void RandomizeGroove()
{
    // Random groove pattern (0-31)
    uint32_t seed = System::GetUs();
    currentGroovePattern = seed % 32;

    // Random timing and velocity amounts for each voice (0.0 - 0.75, max 75%)
    for(int i = 0; i < NUM_DRUM_VOICES; i++)
    {
        seed = System::GetUs() ^ (i * 54321); // Unique seed per voice
        grooveAmount[i] = (float)(seed % 76) / 100.0f; // 0-75% timing

        seed = System::GetUs() ^ (i * 98765); // Different seed for velocity
        grooveVelocityAmount[i] = (float)(seed % 76) / 100.0f; // 0-75% velocity
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
    int hitCount = (density == DENSITY_LOW) ? (length / 8) : (density == DENSITY_MEDIUM) ? (length / 4) : (length / 3);

    // Emphasize off-beat positions (odd steps)
    for(int i = 0; i < hitCount; i++)
    {
        int pos = ((seed >> (i * 2)) % (length / 2)) * 2 + 1; // Force odd positions
        if(pos < length) pattern |= (1 << pos);
    }
    return pattern;
}

// Generate straight rhythm (on-beat emphasis)
uint32_t GenerateStraight(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;
    int hitCount = (density == DENSITY_LOW) ? (length / 8) : (density == DENSITY_MEDIUM) ? (length / 4) : (length / 2);

    // Emphasize on-beat positions (even steps)
    for(int i = 0; i < hitCount; i++)
    {
        int pos = ((seed >> (i * 2)) % (length / 2)) * 2; // Force even positions
        if(pos < length) pattern |= (1 << pos);
    }
    return pattern;
}

// Generate Euclidean rhythm (evenly spaced hits)
uint32_t GenerateEuclidean(uint32_t seed, DensityLevel density, uint8_t length)
{
    uint32_t pattern = 0;
    int hitCount = (density == DENSITY_LOW) ? (length / 10) : (density == DENSITY_MEDIUM) ? (length / 5) : (length / 2);

    // Bjorklund's algorithm for Euclidean rhythms
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
    int hitCount = (density == DENSITY_LOW) ? (length / 8) : (density == DENSITY_MEDIUM) ? (length / 4) : (length / 3);

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

// Generate patterns for all generative voices
void GenerateVoicePatterns()
{
    // Odd lengths for polyrhythms
    const uint8_t oddLengths[] = {12, 13, 15, 17, 18};

    // Generate base patterns for each voice
    for(int i = 0; i < 5; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        uint32_t seed = generationSeed ^ (i * 12345); // Unique seed per voice

        // Randomly choose pattern length (15% chance of polyrhythm)
        if((seed % 100) < 15)
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
            default:
                voice->pattern = GenerateEuclidean(seed, voice->density, voice->patternLength);
                break;
        }
    }

    // Process interactions between voice pairs
    // DRUM1 & DRUM2 (divided)
    if(generativeVoices[0].interaction == INTERACTION_DIVIDED)
        ProcessInteractionDivided(&generativeVoices[0], &generativeVoices[1]);

    // SNARE & HIHAT2_CLOSED (alternate bar)
    if(generativeVoices[3].interaction == INTERACTION_ALTERNATE_BAR)
        ProcessInteractionAlternateBar(&generativeVoices[3], &generativeVoices[4]);
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
                ScheduleDrumTriggerWithGroove(CLAP, 115, currentStep, nextBeatSample);

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
                ScheduleDrumTriggerWithGroove(CLAP, 115, currentStep, nextBeatSample);

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
            ScheduleDrumTriggerWithGroove(CLAP, 100, currentStep, nextBeatSample);
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
    for(int i = 0; i < 5; i++)
    {
        VoiceConfig* voice = &generativeVoices[i];
        if(voice->active)
        {
            // Use modulo to loop shorter patterns (polyrhythms)
            uint8_t voiceStep = currentStep % voice->patternLength;
            if(IsStepActive(voice->pattern, voiceStep))
            {
                ScheduleDrumTriggerWithGroove(voice->voice, 95, currentStep, nextBeatSample); // Standard velocity for generative voices
            }
        }
    }

    // Process Turing Machines (evolving CV sequencers)
    turingMachine1.Process();
    turingMachine2.Process();

    // Output Turing Machine CV values to DAC
    // DaisyPatch has 2 CV outputs via seed.dac
    // Convert 0.0-1.0 to 0-4095 for 12-bit DAC
    uint16_t cv1 = (uint16_t)(turingMachine1.GetCV() * 4095.0f);
    uint16_t cv2 = (uint16_t)(turingMachine2.GetCV() * 4095.0f);
    hw.seed.dac.WriteValue(DacHandle::Channel::ONE, cv1);
    hw.seed.dac.WriteValue(DacHandle::Channel::TWO, cv2);

    // Advance step
    currentStep++;
    if(currentStep >= 32) // Now 32 steps = 2 bars
    {
        currentStep = 0;
        barCounter++; // Counts 2-bar phrases

        // Auto-randomize patterns after interval
        if(barCounter >= patternChangeInterval)
        {
            RandomizePatterns();
            barCounter = 0;
            fillActive = false; // Reset fill for next cycle
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
        if(gate24ppqn)
        {
            gate24ppqnCounter++;
            if(gate24ppqnCounter >= GATE_PULSE_SAMPLES)
                gate24ppqn = false;
        }

        if(gate16th)
        {
            gate16thCounter++;
            if(gate16thCounter >= GATE_PULSE_SAMPLES)
                gate16th = false;
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
        out[0][i] = gate24ppqn ? 1.0f : 0.0f;    // Out 1: 24 PPQN
        out[1][i] = gate16th ? 1.0f : 0.0f;      // Out 2: 16th notes
        out[2][i] = gateQuarter ? 1.0f : 0.0f;   // Out 3: Quarter notes
        out[3][i] = gateReset ? 1.0f : 0.0f;     // Out 4: Reset pulse
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
            midiClockCounter = 0; // Reset MIDI clock counter
            generationSeed = System::GetUs(); // Initialize seed
            GenerateVoicePatterns(); // Generate initial patterns

            // Initialize Turing Machines with different seeds
            turingMachine1.Init(System::GetUs());
            System::Delay(1); // Small delay for different seed
            turingMachine2.Init(System::GetUs() ^ 0xA5A5A5A5); // Different seed

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
                        }

                        // Trigger quarter note gate on every 24th clock
                        if(midiClockCounter % CLOCKS_PER_QUARTER == 0)
                        {
                            TriggerGateQuarter();
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

    // BPM, Mode, and Status on one line
    hw.display.SetCursor(0, 0);
    char topLine[30];
    sprintf(topLine, "BPM:%d %s %s",
            (int)bpm,
            externalClockMode ? "EXT" : "INT",
            isRunning ? "RUN" : "STOP");
    hw.display.WriteString(topLine, Font_6x8, true);

    // Pattern info
    if(isRunning)
    {
        hw.display.SetCursor(0, 10);
        char patStr[20];
        sprintf(patStr, "Ptn: K%d C%d H%d", currentKickPattern, currentClapPattern, currentHatPattern);
        hw.display.WriteString(patStr, Font_6x8, true);
    }

    // Groove pattern display
    hw.display.SetCursor(0, 20);
    str = "Groove: ";
    str += groovePatternNames[currentGroovePattern];
    cstr = &str[0];
    hw.display.WriteString(cstr, Font_6x8, true);

    // Debug: Show groove amounts for kick, snare, and hihats
    hw.display.SetCursor(0, 30);
    char grooveAmtStr[30];
    sprintf(grooveAmtStr, "K:%d S:%d H1:%d H2:%d",
            (int)(grooveAmount[KICK] * 100),
            (int)(grooveAmount[SNARE] * 100),
            (int)(grooveAmount[HIHAT1_CLOSED] * 100),
            (int)(grooveAmount[HIHAT2_CLOSED] * 100));
    hw.display.WriteString(grooveAmtStr, Font_6x8, true);

    // Debug: Show groove amount and offset for hi-hat
    if(isRunning)
    {
        hw.display.SetCursor(0, 40);
        int32_t hatOffset = CalculateGrooveOffset(HIHAT1_CLOSED, currentStep);
        int8_t patternValue = groovePatterns[currentGroovePattern][currentStep % 16];
        char offsetStr[30];
        sprintf(offsetStr, "S:%2d P:%+4d O:%+4d",
                currentStep,
                (int)patternValue,
                (int)(hatOffset * 1000 / hw.AudioSampleRate()));
        hw.display.WriteString(offsetStr, Font_6x8, true);
    }

    hw.display.Update();
}

void ProcessControls()
{
    hw.ProcessDigitalControls();
    hw.ProcessAnalogControls();

    // Encoder for BPM control
    int32_t inc = hw.encoder.Increment();
    if(inc != 0 && !externalClockMode)
    {
        bpm += inc * 0.5f; // Increment by 0.5 BPM
        bpm = fclamp(bpm, 20.0f, 300.0f); // Limit BPM range
        UpdateClockFrequency();
    }

    // Encoder button for start/stop
    if(hw.encoder.RisingEdge())
    {
        ToggleRunState();
    }

    // Check for external clock timeout
    if(externalClockMode)
    {
        uint32_t now = System::GetNow();
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

        // Process drum patterns on each 16th note
        ProcessDrumPatterns();

        // Send MIDI clock (24 PPQN)
        // For each 16th note, send 6 MIDI clocks and trigger 24ppqn gate
        for(int i = 0; i < CLOCKS_PER_16TH; i++)
        {
            SendMidiClock();
            TriggerGate24ppqn();
            midiClockCounter++;

            // Trigger quarter note gate on every 24th clock
            if(midiClockCounter % CLOCKS_PER_QUARTER == 0)
            {
                TriggerGateQuarter();
            }
        }
    }

    // Handle gate pulse timing for gate out port
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

    // Set gate output
    dsy_gpio_write(&hw.gate_output, gateHigh ? 1 : 0);
}

int main(void)
{
    // Initialize hardware
    hw.Init();
    float samplerate = hw.AudioSampleRate();

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
