/**
 * @file globals.cpp
 * @brief Global variable definitions for Themis
 */

#include "globals.h"

// ============================================================================
// STRUCT METHOD IMPLEMENTATIONS
// ============================================================================

void GrooveConfig::UpdateOffset(float currentBPM, float sampleRate)
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

void TuringMachine::Init(uint32_t seed)
{
    shiftRegister = seed;
    currentLength = 16;
    targetLength = 16;
    lengthMorph = 0.0f;
    currentPos = 0;
    randomProbability = 0.2f;
    targetProbability = 0.2f;
    cvOutput = 0.0f;
    morphCounter = 0;
    morphInterval = 256;
}

void TuringMachine::Process()
{
    // Shift register
    bool outputBit = (shiftRegister >> (currentLength - 1)) & 0x01;
    shiftRegister <<= 1;

    // Determine new bit: either looped back or random
    bool newBit;
    float randVal = (float)(System::GetUs() % 1000) / 1000.0f;
    if(randVal < randomProbability)
    {
        newBit = (System::GetUs() & 0x01);
    }
    else
    {
        newBit = outputBit;
    }

    if(newBit)
        shiftRegister |= 0x01;

    // Mask to current length
    uint32_t mask = (1 << currentLength) - 1;
    shiftRegister &= mask;

    // Calculate CV output from register state
    uint32_t cvBits = shiftRegister & 0x1F;
    cvOutput = (float)cvBits / 31.0f;

    // Advance position
    currentPos++;
    if(currentPos >= currentLength)
        currentPos = 0;

    // Slow morphing
    morphCounter++;
    if(morphCounter >= morphInterval)
    {
        morphCounter = 0;
        // Slowly drift probability
        float probDrift = ((float)(System::GetUs() % 100) / 1000.0f) - 0.05f;
        targetProbability += probDrift;
        targetProbability = fclamp(targetProbability, 0.05f, 0.5f);
        randomProbability += (targetProbability - randomProbability) * 0.1f;

        // Occasionally change target length
        if((System::GetUs() % 10) == 0)
        {
            uint32_t lengthChoice = System::GetUs() % 3;
            if(lengthChoice == 0) targetLength = 8;
            else if(lengthChoice == 1) targetLength = 16;
            else targetLength = 32;
        }

        // Morph length gradually
        if(currentLength != targetLength)
        {
            lengthMorph += 0.02f;
            if(lengthMorph >= 1.0f)
            {
                lengthMorph = 0.0f;
                currentLength = targetLength;
                uint32_t mask = (1 << currentLength) - 1;
                shiftRegister &= mask;
            }
        }
    }
}

// ============================================================================
// HARDWARE
// ============================================================================

DaisyPatch hw;
Metro clockMetro;

// ============================================================================
// CLOCK & TRANSPORT
// ============================================================================

bool isRunning = false;
bool externalClockMode = false;
float bpm = 120.0f;
uint32_t midiClockCounter = 0;
uint32_t lastMidiClockTime = 0;
uint32_t midiClockTimeout = 500;
bool gateHigh = false;
uint32_t gateHighCounter = 0;

bool gate24ppqn = false;
bool gate16th = false;
bool gate2 = false;
bool gateQuarter = false;
bool gateReset = false;
uint32_t gate24ppqnCounter = 0;
uint32_t gate16thCounter = 0;
uint32_t gate2Counter = 0;
uint32_t gateQuarterCounter = 0;
uint32_t gateResetCounter = 0;

// ============================================================================
// UI STATE
// ============================================================================

DisplayState currentDisplayState = DISPLAY_DEFAULT;
ConfigOption currentConfigOption = CONFIG_BPM;
OutDivision currentOut2Division = DIV_1_8;
OutDivision currentOut3Division = DIV_1_4;
bool freezeEnabled = false;
int patternInfoScroll = 0;
uint32_t lastEncoderActivity = 0;
int configScrollOffset = 0;

// ============================================================================
// PERSISTENT SETTINGS
// ============================================================================

PersistentSettings settings;

// ============================================================================
// GROOVE SYSTEM
// ============================================================================

GrooveConfig voiceGroove[NUM_DRUM_VOICES];
uint8_t currentGroovePattern = 0;
float grooveAmount[NUM_DRUM_VOICES];
float grooveVelocityAmount[NUM_DRUM_VOICES];

MidiTrigger triggerQueue[TRIGGER_QUEUE_SIZE];
uint8_t triggerQueueHead = 0;
uint8_t triggerQueueTail = 0;
MelodyTrigger melodyQueue[MELODY_QUEUE_SIZE];
uint8_t melodyQueueHead = 0;
uint8_t melodyQueueTail = 0;
float melodyGrooveAmount = 0.5f;

volatile uint64_t globalSampleCounter = 0;
volatile uint64_t lastBeatSample = 0;

// ============================================================================
// SEQUENCER STATE
// ============================================================================

uint8_t currentStep = 0;
uint8_t barCounter = 0;
uint8_t cycleCounter = 0;
uint8_t measureCounter = 0;
uint32_t patternChangeInterval = 4;
uint32_t personalityChangeInterval = 4;
uint32_t generationSeed = 0;

bool fillScheduled = false;
bool fillActive = false;
bool fillIsHalfBar = false;
uint8_t fillStartStep = 0;
uint8_t currentFillSnareIndex = 0;
uint8_t currentFillHatClosedIndex = 0;
uint8_t currentFillHatOpenIndex = 0;

// ============================================================================
// DRUM PATTERNS
// ============================================================================

uint8_t currentKickPattern = 0;
uint8_t currentClapPattern = 0;
uint8_t currentHatPattern = 0;
DrumVoice fundamentalBeatVoice = SNARE;

VoiceConfig generativeVoices[6] = {
    {DRUM1, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM2, 0, 32, true},
    {DRUM2, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM1, 0, 32, true},
    {MULTI, RHYTHM_SYNCOPATED, DENSITY_LOW, INTERACTION_NONE, MULTI, 0, 32, true},
    {SNARE, RHYTHM_STRAIGHT, DENSITY_MEDIUM, INTERACTION_ALTERNATE_BAR, HIHAT2_CLOSED, 0, 32, true},
    {HIHAT2_CLOSED, RHYTHM_STRAIGHT, DENSITY_HIGH, INTERACTION_NONE, HIHAT2_CLOSED, 0, 32, true},
    {ANALOG, RHYTHM_FOLLOW_KICK, DENSITY_HIGH, INTERACTION_NONE, ANALOG, 0, 32, true}
};

bool analogGateHigh = false;
uint32_t analogGateCounter = 0;
uint8_t analogVoiceVelocity = 100;

// ============================================================================
// MELODY SYSTEM
// ============================================================================

MelodyConfig melodyVoice = {
    MELODY_SUPPORTING,
    SUPPORT_FOLLOW_KICK,
    RHYTHM_EUCLIDEAN,   // rhythmStyle
    DENSITY_LOW,        // density (supporting = sparse)
    0, 32, {0}, 0, 0, true
};

MelodyConfig melodyMidiVoice = {
    MELODY_ARPEGGIATOR,
    ARP_CHORD_TONES,
    RHYTHM_EUCLIDEAN,   // rhythmStyle
    DENSITY_MEDIUM,     // density (arpeggiator = busier)
    0, 32, {0}, 0, 0, true
};

ScaleType melodyScale = SCALE_MINOR;
uint8_t melodyRoot = 0;
uint8_t melodyMidiChannel = 0;
uint8_t lastMidiMelodyNote = 0;
bool midiMelodyNoteOn = false;
uint64_t midiMelodyNoteOffSample = 0;
bool melodyFreezeEnabled = false;
bool tuneModeEnabled = false;

// ============================================================================
// TURING MACHINES
// ============================================================================

TuringMachine turingCV2;
TuringMachine turingCV3;
TuringMachine turingCV4;

// ============================================================================
// CONST LOOKUP TABLES
// ============================================================================

const uint8_t drumNotes[NUM_DRUM_VOICES] = {
    36, 48, 41, 58, 40, 49, 51, 42, 44, 39, 60
};

const char* drumNames[NUM_DRUM_VOICES] = {
    "Kick", "Drm1", "Drm2", "Mult", "Snare",
    "HH1C", "HH1O", "HH2C", "HH2O", "Clap", "Anlg"
};

const char* configOptionNames[NUM_CONFIG_OPTIONS] = {
    "BPM", "OUT2 div", "OUT3 div", "DrumFreeze", "Scale", "Root",
    "CV Style", "MIDI Style", "MIDI Ch", "MelFreeze", "TuneMode",
    "Randomize!", "Pattern info", "Back"
};

const char* outDivisionNames[NUM_OUT_DIVISIONS] = {
    "1/16", "1/8", "1/4", "1/2", "1", "2", "4"
};

const char* scaleNames[NUM_SCALE_TYPES] = {
    "Minor", "MinBlue", "MinPent", "Gypsy"
};

const char* melodyStyleNames[NUM_MELODY_STYLES] = {
    "Support", "Arpeg"
};

const char* rootNoteNames[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// Scale intervals
const int8_t scaleMinor[7] = {0, 2, 3, 5, 7, 8, 10};
const int8_t scaleMinorBlues[6] = {0, 3, 5, 6, 7, 10};
const int8_t scaleMinorPentatonic[5] = {0, 3, 5, 7, 10};
const int8_t scaleGypsy[7] = {0, 2, 3, 6, 7, 8, 11};
const int8_t scaleLengths[NUM_SCALE_TYPES] = {7, 6, 5, 7};

// ============================================================================
// GROOVE PATTERNS (32 patterns)
// ============================================================================

const int8_t groovePatterns[32][16] = {
    {0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80, 0, 80},     // 0: Swing16
    {0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0, 0, 0, 90, 0},         // 1: Swing8
    {0, 0, 70, 0, 0, 70, 0, 0, 70, 0, 0, 70, 0, 0, 70, 0},        // 2: Swing6
    {0, 0, -12, 30, 0, 0, -18, 0, 0, -12, 30, 0, -18, 0, 0, 18},  // 3: Conga1
    {0, 0, -18, 0, 0, -12, 30, 0, -18, 0, 0, 18, 0, 0, -12, 30},  // 4: Conga2
    {0, 18, -12, 18, 0, 18, -12, 18, 0, 18, -12, 18, 0, 18, -12, 18}, // 5: Bongo1
    {0, 0, 20, -10, 0, 0, 20, -10, 0, 0, 20, -10, 0, 0, 20, -10}, // 6: Bongo2
    {0, -12, -18, -12, 0, -12, -18, -12, 0, -12, -18, -12, 0, -12, -18, -12}, // 7: Cabasa1
    {0, 15, 20, 15, 0, 15, 20, 15, 0, 15, 20, 15, 0, 15, 20, 15}, // 8: Cabasa2
    {0, 0, 0, -18, 0, 0, -24, 0, 0, -18, 0, 0, 0, -24, 0, 0},     // 9: Claves1
    {0, 0, 0, -20, 0, 0, -25, 0, 0, 0, -20, 0, 0, 0, -25, 0},     // 10: Claves2
    {0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12, 0, -12}, // 11: Cowbell
    {0, 0, -18, 12, 0, 0, -18, 12, 0, 0, -18, 12, 0, 0, -18, 12}, // 12: Agogo1
    {0, -15, 0, 20, 0, -15, 0, 20, 0, -15, 0, 20, 0, -15, 0, 20}, // 13: Agogo2
    {0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24, 0, 24},     // 14: Tambourine
    {0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 60, 0, 50},     // 15: Finger1
    {0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 35, 0, 30},     // 16: Finger2
    {0, 48, 36, 48, 24, 48, 36, 48, 24, 48, 36, 48, 24, 48, 36, 48}, // 17: Lofi1
    {-5, 40, -10, 45, 5, 35, -8, 42, -3, 38, -12, 44, 8, 36, -6, 40}, // 18: Lofi2
    {0, 0, -18, 0, 0, -18, 0, 12, 0, 0, -18, 0, 0, -18, 0, 12},   // 19: Baile1
    {0, 0, 50, 0, 0, 0, 50, -10, 0, 0, 50, 0, 0, 0, 50, -10},     // 20: Baile2
    {0, -18, -30, -18, 12, 24, 30, 24, 0, -18, -30, -18, 12, 24, 30, 24}, // 21: OvalGroove
    {0, 0, 0, 25, 0, -15, 0, 30, 0, 0, -10, 25, 0, 0, 0, 28},     // 22: Afrobeat
    {0, 45, 0, 40, 0, 50, 0, 38, 0, 48, 0, 42, 0, 47, 0, 40},     // 23: HipHop1
    {0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55, 0, 55},     // 24: HipHop2
    {-8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8, -8}, // 25: Techno
    {5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0, 5, 0},             // 26: Shuffle
    {0, 12, -6, 12, 0, 12, -6, 12, 0, 12, -6, 12, 0, 12, -6, 12}, // 27: Reggae
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},             // 28: Straight
    {0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20, 0, 20},     // 29: Light Swing
    {0, 30, 0, 30, 0, 30, 0, 30, 0, 30, 0, 30, 0, 30, 0, 30},     // 30: Medium Swing
    {0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40, 0, 40}      // 31: Heavy Swing
};

const int8_t velocityPatterns[32][16] = {
    {100, 85, 95, 80, 100, 85, 95, 80, 100, 85, 95, 80, 100, 85, 95, 80},
    {100, 70, 85, 70, 100, 70, 85, 70, 100, 70, 85, 70, 100, 70, 85, 70},
    {100, 80, 90, 75, 100, 80, 90, 75, 100, 80, 90, 75, 100, 80, 90, 75},
    {100, 75, 95, 85, 100, 75, 95, 85, 100, 75, 95, 85, 100, 75, 95, 85},
    {100, 80, 90, 80, 100, 80, 90, 80, 100, 80, 90, 80, 100, 80, 90, 80},
    {100, 90, 80, 90, 100, 90, 80, 90, 100, 90, 80, 90, 100, 90, 80, 90},
    {100, 85, 95, 85, 100, 85, 95, 85, 100, 85, 95, 85, 100, 85, 95, 85},
    {100, 75, 85, 75, 100, 75, 85, 75, 100, 75, 85, 75, 100, 75, 85, 75},
    {100, 90, 95, 90, 100, 90, 95, 90, 100, 90, 95, 90, 100, 90, 95, 90},
    {100, 80, 100, 70, 100, 80, 100, 70, 100, 80, 100, 70, 100, 80, 100, 70},
    {100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75},
    {100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95},
    {100, 80, 85, 80, 100, 80, 85, 80, 100, 80, 85, 80, 100, 80, 85, 80},
    {100, 85, 80, 85, 100, 85, 80, 85, 100, 85, 80, 85, 100, 85, 80, 85},
    {100, 90, 90, 90, 100, 90, 90, 90, 100, 90, 90, 90, 100, 90, 90, 90},
    {100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70},
    {100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80},
    {100, 65, 90, 60, 100, 65, 90, 60, 100, 65, 90, 60, 100, 65, 90, 60},
    {100, 70, 85, 65, 100, 70, 85, 65, 100, 70, 85, 65, 100, 70, 85, 65},
    {100, 80, 75, 80, 100, 80, 75, 80, 100, 80, 75, 80, 100, 80, 75, 80},
    {100, 85, 90, 85, 100, 85, 90, 85, 100, 85, 90, 85, 100, 85, 90, 85},
    {100, 75, 70, 75, 110, 105, 110, 105, 100, 75, 70, 75, 110, 105, 110, 105},
    {100, 80, 100, 85, 100, 75, 100, 90, 100, 80, 95, 85, 100, 80, 100, 88},
    {100, 60, 100, 55, 100, 65, 100, 58, 100, 62, 100, 57, 100, 63, 100, 55},
    {100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75, 100, 75},
    {110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110, 110},
    {100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95, 100, 95},
    {100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70, 100, 70},
    {100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100, 100},
    {100, 90, 100, 90, 100, 90, 100, 90, 100, 90, 100, 90, 100, 90, 100, 90},
    {100, 85, 100, 85, 100, 85, 100, 85, 100, 85, 100, 85, 100, 85, 100, 85},
    {100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80, 100, 80}
};

// ============================================================================
// KICK PATTERNS (16 patterns, 32 steps each)
// ============================================================================

const uint32_t kickPatterns[16] = {
    0b10001000100010001000100010001000, // 0: Pure four-on-floor
    0b10011000100010001000100010001000, // 1: + 16th bar 1
    0b10001000101010001000100010001000, // 2: + syncopation bar 1
    0b10001001100010001000100010001000, // 3: + double bar 1
    0b10001000100010011000100010001000, // 4: + anticipation
    0b10101000100010001000100010001000, // 5: + offbeat bar 1
    0b10001000100010001001100010001000, // 6: Variation bar 2
    0b10011000101010001000100010001000, // 7: Busier bar 1
    0b10001010100010001000100010001000, // 8: Syncopated bar 1
    0b10001000100010101000100010001000, // 9: Syncopated both
    0b10001000100010001010100010001000, // 10: Double bar 2
    0b10011000100010001001100010001000, // 11: Double both
    0b10001000100110001000100010001000, // 12: Pre-beat bar 1
    0b10001000100010001000100010011000, // 13: Pre-beat bar 2
    0b10001010100010001000101010001000, // 14: Syncopated full
    0b10011000101010001001100010101000  // 15: Maximum variation
};

// Clap patterns (backbeat with variations)
const uint32_t clapPatterns[16] = {
    0b00001000000010000000100000001000, // 0: Classic backbeat
    0b00001000000010010000100000001000, // 1: + anticipation bar 1
    0b00001001000010000000100000001000, // 2: + syncopation bar 1
    0b00001000000110000000100000001000, // 3: + double on 4 bar 1
    0b00001010000010000000100000001000, // 4: Syncopated bar 1
    0b00011000000010000000100000001000, // 5: Early clap bar 1
    0b00001000001010000000100000001000, // 6: Extra claps bar 1
    0b00001000010010000000100000001000, // 7: Syncopated bar 1
    0b00001000000010000010100000001000, // 8: Syncopated bar 2
    0b00001000100010000000100010001000, // 9: Extra on 2-and
    0b00001100000010000000110000001000, // 10: Double clap on 2
    0b00001000000010100000100000001010, // 11: Shuffle on 4
    0b01001000000010000000100000001000, // 12: Very early bar 1
    0b00001000010010010000100001001000, // 13: Busy syncopation
    0b00011000001010000001100000101000, // 14: Complex rhythm
    0b00101000100010000010100010001000  // 15: Techno shuffle
};

// ============================================================================
// HI-HAT PATTERNS
// ============================================================================

const uint32_t hatClosedPatterns[16] = {
    0b10101010101010101010101010101010,
    0b10101010101010101010101010101010,
    0b11101010111010101110101011101010,
    0b10101110101011101010111010101110,
    0b10111010101110101011101010111010,
    0b11101110111011101110111011101110,
    0b10101010101010101010101010101010,
    0b11111010111110101111101011111010,
    0b10101011101010111010101110101011,
    0b11101010101011101110101010101110,
    0b10101110101110101010111010111010,
    0b11111110111111101111111011111110,
    0b10100010101000101010001010100010,
    0b10101010101010101010101010101010,
    0b11101110101011101110111010101110,
    0b10111011101110111011101110111011
};

const uint32_t hatOpenPatterns[16] = {
    0b00000000000000000000000000000000,
    0b00100010001000100010001000100010,
    0b00000010000000100000001000000010,
    0b00100000001000000010000000100000,
    0b00000010001000100000001000100010,
    0b00000000000000000000000000000000,
    0b00100010001000100010001000100010,
    0b00000000001000000000000000100000,
    0b00100010000000100010001000100010,
    0b00100010001000000000001000100010,
    0b00100010001000000000000000100010,
    0b00000000000000000000000000000000,
    0b00100010001000100010001000000010,
    0b00100000001000000010001000100000,
    0b00000010001000100000001000100010,
    0b00100010001000100010001000100010
};

// ============================================================================
// FILL PATTERNS
// ============================================================================

const uint8_t kickFillsHalf[8] = {
    0b00010111, 0b01111111, 0b01010101, 0b00111111,
    0b01011111, 0b00001111, 0b01110111, 0b11111111
};

const uint16_t kickFillsWhole[8] = {
    0b0000000001111111, 0b0000010101111111, 0b0001001001111111, 0b1010101010101010,
    0b0101010111111111, 0b0011001111111111, 0b1101101111111111, 0b1111111111111111
};

const uint8_t snareFillsHalf[8] = {
    0b00001111, 0b01010111, 0b00111111, 0b01111111,
    0b10101010, 0b01011111, 0b11110111, 0b11111111
};

const uint16_t snareFillsOneBar[16] = {
    0b0000000011111111, 0b0000111111111111, 0b0101010111111111, 0b0000000001111111,
    0b0000000000111111, 0b0011111111111111, 0b0001010111111111, 0b0000010111111111,
    0b0000001011111111, 0b0000000111111111, 0b0101011111111111, 0b0000011111111111,
    0b0000101011111111, 0b0001111111111111, 0b0010101011111111, 0b0000000000001111
};

const uint8_t hatClosedFillsHalf[8] = {
    0b01111111, 0b11111111, 0b10101010, 0b11011011,
    0b11101110, 0b01010101, 0b11111110, 0b10111011
};

const uint8_t hatOpenFillsHalf[8] = {
    0b00000001, 0b00000010, 0b10000001, 0b00001000,
    0b10001000, 0b00000000, 0b00010001, 0b10000000
};

const uint16_t hatClosedFillsOneBar[8] = {
    0b1111111111111111, 0b0101010101010101, 0b1010101010101010, 0b1111111100000000,
    0b0000000011111111, 0b1111000011110000, 0b0000111100001111, 0b1100110011001100
};

const uint16_t hatOpenFillsOneBar[8] = {
    0b0000000000000001, 0b0000000000000010, 0b1000000000000001, 0b0000000010000000,
    0b0000100000001000, 0b0000000000000000, 0b1000000010000001, 0b0001000100010001
};

const uint16_t snareFillsWhole[16] = {
    0b1111111111111111, 0b0101010111111111, 0b0000000011111111, 0b0011001111111111,
    0b0000111111111111, 0b0101010101111111, 0b0000010111111111, 0b0001111111111111,
    0b0000001111111111, 0b0010101011111111, 0b0000000111111111, 0b0100010111111111,
    0b0000011111111111, 0b0001010111111111, 0b0000100111111111, 0b0011111111111111
};

const uint16_t hatClosedFillsWhole[8] = {
    0b1111111111111111, 0b1010101010101010, 0b1111111100000000, 0b0000000011111111,
    0b1100110011001100, 0b0011001100110011, 0b1111000000001111, 0b0000111111110000
};

const uint16_t hatOpenFillsWhole[8] = {
    0b0000000000000001, 0b0000000000000010, 0b1000000000000001, 0b0000000010000000,
    0b0000100000001000, 0b0000000000000000, 0b1000000010000001, 0b0001000100010001
};
