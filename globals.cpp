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
bool gateReset = false;
bool melodyGate = false;
bool bassGate = false;
bool analogDrumGate = false;
uint32_t gate24ppqnCounter = 0;
uint32_t gate16thCounter = 0;
uint32_t gateResetCounter = 0;
uint32_t melodyGateCounter = 0;
uint32_t bassGateCounter = 0;
uint32_t analogDrumGateCounter = 0;

// ============================================================================
// UI STATE
// ============================================================================

DisplayState currentDisplayState = DISPLAY_DEFAULT;
ConfigOption currentConfigOption = CONFIG_BPM;
bool freezeEnabled = false;
int patternInfoScroll = 0;
uint32_t lastEncoderActivity = 0;
int configScrollOffset = 0;

// Submenu state
FreezeOption currentFreezeOption = FREEZE_ALL;
SystemOption currentSystemOption = SYSTEM_MELODY_CH;
HarmonyOption currentHarmonyOption = HARMONY_SCALE;
VoiceMenuItem currentVoiceMenuItem = VOICE_MELODY;
VoiceDetailItem currentVoiceDetail = VDETAIL_ACTIVE;
int freezeScrollOffset = 0;
int systemScrollOffset = 0;
int harmonyScrollOffset = 0;
int voiceScrollOffset = 0;
int voiceDetailScrollOffset = 0;
uint8_t drumMidiChannel = 9;  // Channel 10 (0-indexed)
themis::ChordRandomizerConfig chordRandomizerConfig;

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

// Default variation config: mode=OFF (variations disabled by default)
static const VariationConfig defaultVoiceVariation = {
    VAR_MODE_OFF,           // mode
    VAR_SEQ_AAAB,           // sequence (default when enabled)
    VAR_GRAN_BAR,           // granularity
    RHYTHM_EUCLIDEAN,       // styleB
    RHYTHM_EUCLIDEAN,       // styleC
    DENSITY_MEDIUM,         // densityB
    DENSITY_MEDIUM          // densityC
};

VoiceConfig generativeVoices[6] = {
    // voice, rhythmStyle, density, interaction, partner, pattern, patternB, patternC, length, active, variation
    {DRUM1, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM2, 0, 0, 0, 32, true, defaultVoiceVariation},
    {DRUM2, RHYTHM_EUCLIDEAN, DENSITY_MEDIUM, INTERACTION_DIVIDED, DRUM1, 0, 0, 0, 32, true, defaultVoiceVariation},
    {MULTI, RHYTHM_SYNCOPATED, DENSITY_LOW, INTERACTION_NONE, MULTI, 0, 0, 0, 32, true, defaultVoiceVariation},
    {SNARE, RHYTHM_STRAIGHT, DENSITY_MEDIUM, INTERACTION_ALTERNATE_BAR, HIHAT2_CLOSED, 0, 0, 0, 32, true, defaultVoiceVariation},
    {HIHAT2_CLOSED, RHYTHM_STRAIGHT, DENSITY_HIGH, INTERACTION_NONE, HIHAT2_CLOSED, 0, 0, 0, 32, true, defaultVoiceVariation},
    {ANALOG, RHYTHM_FOLLOW_KICK, DENSITY_HIGH, INTERACTION_NONE, ANALOG, 0, 0, 0, 32, true, defaultVoiceVariation}
};

// ============================================================================
// BASS VOICE
// ============================================================================

themis::BassConfig bassVoiceConfig;
themis::BassState bassVoiceState;
uint8_t bassMidiChannel = 4;  // Default: channel 5 (0-indexed)
int8_t lastBassMidiNote = -1;
bool bassNotePlaying = false;
uint64_t bassMidiNoteOffSample = 0;

// ============================================================================
// RHYTHM PLAYER
// ============================================================================

themis::RhythmPlayerConfig rhythmPlayerConfig;
themis::RhythmPlayerState rhythmPlayerState;
uint8_t rhythmMidiChannel = 3;       // Default: channel 4 (0-indexed)
int8_t rhythmActiveNotes[6] = {-1, -1, -1, -1, -1, -1};
uint8_t rhythmNumActiveNotes = 0;
bool rhythmNotesPlaying = false;

// ============================================================================
// MELODY SYSTEM
// ============================================================================

// Default melody variation config: mode=OFF (variations disabled by default)
static const VariationConfig defaultMelodyVariation = {
    VAR_MODE_OFF,           // mode
    VAR_SEQ_AAAB,           // sequence (default when enabled)
    VAR_GRAN_BAR,           // granularity
    RHYTHM_EUCLIDEAN,       // styleB
    RHYTHM_EUCLIDEAN,       // styleC
    DENSITY_LOW,            // densityB
    DENSITY_MEDIUM          // densityC
};

MelodyConfig melodyVoice = {
    MELODY_SUPPORTING,      // style
    SUPPORT_FOLLOW_KICK,    // subStyle
    RHYTHM_EUCLIDEAN,       // rhythmStyle
    DENSITY_LOW,            // density (supporting = sparse)
    0,                      // rhythmPattern
    0,                      // rhythmPatternB
    0,                      // rhythmPatternC
    32,                     // patternLength
    {0},                    // noteSequence
    {0},                    // noteSequenceB
    {0},                    // noteSequenceC
    0,                      // sequencePos
    0,                      // currentOctave
    true,                   // active
    defaultMelodyVariation  // variation
};

MelodyConfig melodyMidiVoice = {
    MELODY_ARPEGGIATOR,     // style
    ARP_CHORD_TONES,        // subStyle
    RHYTHM_EUCLIDEAN,       // rhythmStyle
    DENSITY_MEDIUM,         // density (arpeggiator = busier)
    0,                      // rhythmPattern
    0,                      // rhythmPatternB
    0,                      // rhythmPatternC
    32,                     // patternLength
    {0},                    // noteSequence
    {0},                    // noteSequenceB
    {0},                    // noteSequenceC
    0,                      // sequencePos
    0,                      // currentOctave
    true,                   // active
    defaultMelodyVariation  // variation
};

ScaleType melodyScale = SCALE_MINOR;
uint8_t melodyRoot = 0;
uint8_t melodyChannel = 0;
uint8_t lastMidiMelodyNote = 0;
bool midiMelodyNoteOn = false;
uint64_t midiMelodyNoteOffSample = 0;
bool melodyFreezeEnabled = false;
bool tuneModeEnabled = false;

// ============================================================================
// CHORD VOICE
// ============================================================================

ChordVoiceConfig chordVoice = {
    false,      // active
    0,          // progressionIndex (Pop progression)
    CHORD_RATE_1_BAR,  // chordRate
    100,        // velocity
    0,          // octaveOffset
    1           // midiChannel (channel 2, 0-indexed)
};

ChordVoiceState chordState;

int8_t chordActiveNotes[6] = {-1, -1, -1, -1, -1, -1};
uint8_t chordNumActiveNotes = 0;
bool chordNotesOn = false;

// Chord shapes: intervals from root
const ChordShape chordShapes[NUM_CHORD_TYPES] = {
    {{0, 4, 7, -1, -1, -1}, 3},      // CHORD_MAJOR
    {{0, 3, 7, -1, -1, -1}, 3},      // CHORD_MINOR
    {{0, 3, 6, -1, -1, -1}, 3},      // CHORD_DIM
    {{0, 4, 8, -1, -1, -1}, 3},      // CHORD_AUG
    {{0, 2, 7, -1, -1, -1}, 3},      // CHORD_SUS2
    {{0, 5, 7, -1, -1, -1}, 3},      // CHORD_SUS4
    {{0, 4, 7, 11, -1, -1}, 4},      // CHORD_MAJ7
    {{0, 3, 7, 10, -1, -1}, 4},      // CHORD_MIN7
    {{0, 4, 7, 10, -1, -1}, 4},      // CHORD_DOM7
    {{0, 3, 6, 9, -1, -1}, 4},       // CHORD_DIM7
    {{0, 3, 6, 10, -1, -1}, 4},      // CHORD_MIN7B5
    {{0, 4, 7, 14, -1, -1}, 4},      // CHORD_ADD9
    {{0, 3, 7, 14, -1, -1}, 4}       // CHORD_MADD9
};

// Chord progressions (16 progressions)
const ChordProgression progressions[NUM_PROGRESSIONS] = {
    // 0: Pop (I-V-vi-IV in C)
    {"Pop", {{0, CHORD_MAJOR}, {7, CHORD_MAJOR}, {9, CHORD_MINOR}, {5, CHORD_MAJOR}}, 4, true},
    // 1: Rock (I-IV-V-I)
    {"Rock", {{0, CHORD_MAJOR}, {5, CHORD_MAJOR}, {7, CHORD_MAJOR}, {0, CHORD_MAJOR}}, 4, true},
    // 2: Jazz (ii-V-I-vi)
    {"Jazz", {{2, CHORD_MIN7}, {7, CHORD_DOM7}, {0, CHORD_MAJ7}, {9, CHORD_MIN7}}, 4, true},
    // 3: Dreamy (I-iii-IV-iv)
    {"Dreamy", {{0, CHORD_MAJOR}, {4, CHORD_MINOR}, {5, CHORD_MAJOR}, {5, CHORD_MINOR}}, 4, true},
    // 4: Andalusian (Am-G-F-E)
    {"Andalusian", {{9, CHORD_MINOR}, {7, CHORD_MAJOR}, {5, CHORD_MAJOR}, {4, CHORD_MAJOR}}, 4, false},
    // 5: Minor Progression (i-VI-III-VII)
    {"MinorProg", {{0, CHORD_MINOR}, {8, CHORD_MAJOR}, {3, CHORD_MAJOR}, {10, CHORD_MAJOR}}, 4, true},
    // 6: Circle of Fifths (C-G-D-A-E)
    {"Fifths", {{0, CHORD_MAJOR}, {7, CHORD_MAJOR}, {2, CHORD_MAJOR}, {9, CHORD_MAJOR}}, 4, false},
    // 7: Pachelbel Canon (I-V-vi-iii-IV-I-IV-V)
    {"Canon", {{0, CHORD_MAJOR}, {7, CHORD_MAJOR}, {9, CHORD_MINOR}, {4, CHORD_MINOR}, {5, CHORD_MAJOR}, {0, CHORD_MAJOR}, {5, CHORD_MAJOR}, {7, CHORD_MAJOR}}, 8, true},
    // 8: Blues (I7-IV7-I7-V7)
    {"Blues", {{0, CHORD_DOM7}, {5, CHORD_DOM7}, {0, CHORD_DOM7}, {7, CHORD_DOM7}}, 4, false},
    // 9: Gospel (I-I7-IV-iv)
    {"Gospel", {{0, CHORD_MAJOR}, {0, CHORD_DOM7}, {5, CHORD_MAJOR}, {5, CHORD_MINOR}}, 4, true},
    // 10: Cinematic (i-VI-III-VII in minor)
    {"Cinematic", {{0, CHORD_MINOR}, {8, CHORD_MAJOR}, {3, CHORD_MAJOR}, {10, CHORD_MAJOR}}, 4, true},
    // 11: EDM (vi-IV-I-V)
    {"EDM", {{9, CHORD_MINOR}, {5, CHORD_MAJOR}, {0, CHORD_MAJOR}, {7, CHORD_MAJOR}}, 4, true},
    // 12: Chill (Imaj7-vi7-ii7-V7)
    {"Chill", {{0, CHORD_MAJ7}, {9, CHORD_MIN7}, {2, CHORD_MIN7}, {7, CHORD_DOM7}}, 4, true},
    // 13: Ambient (sus2 based)
    {"Ambient", {{0, CHORD_SUS2}, {5, CHORD_SUS2}, {7, CHORD_SUS2}, {2, CHORD_SUS2}}, 4, false},
    // 14: Dark (i-v-VI-iv)
    {"Dark", {{0, CHORD_MINOR}, {7, CHORD_MINOR}, {8, CHORD_MAJOR}, {5, CHORD_MINOR}}, 4, true},
    // 15: Happy (I-IV-V-IV)
    {"Happy", {{0, CHORD_MAJOR}, {5, CHORD_MAJOR}, {7, CHORD_MAJOR}, {5, CHORD_MAJOR}}, 4, true}
};

// Chord rate names and step values
const char* chordRateNames[NUM_CHORD_RATES] = {
    "2 bars", "1 bar", "half", "quarter"
};

const uint8_t chordRateSteps[NUM_CHORD_RATES] = {
    64, 32, 16, 8
};

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
    "BPM", "TuneMode", "Harmony >>", "Voices >>",
    "Randomize!", "Pattern info",
    "Freeze >>", "System >>", "Back"
};

const char* freezeOptionNames[NUM_FREEZE_OPTIONS] = {
    "All", "Drums", "Melody", "Bass", "Rhythm", "Chords", "Back"
};

const char* systemOptionNames[NUM_SYSTEM_OPTIONS] = {
    "Melody Ch", "Drum Ch", "Bass Ch", "Rhythm Ch", "Back"
};

const char* harmonyOptionNames[NUM_HARMONY_OPTIONS] = {
    "Scale", "Root", "Progression", "Rate", "Back"
};

const char* voiceMenuNames[NUM_VOICE_MENU_ITEMS] = {
    "Melody Voice", "Bass Voice", "Rhythm Voice", "Back"
};

const char* voiceDetailNames[NUM_VOICE_DETAIL_ITEMS] = {
    "Active", "Style", "Octave", "Back"
};

const char* rhythmModeNames[2] = {"Manual", "Morph"};

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

// ============================================================================
// VARIATION SEQUENCE PATTERNS
// ============================================================================

// [sequence][segment] -> variation (0=A, 1=B, 2=C)
// Extended to 8 segments for AAABAAAC pattern
const uint8_t variationSequences[NUM_VARIATION_SEQUENCES][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},  // VAR_SEQ_AAAA (all A)
    {0, 0, 0, 1, 0, 0, 0, 1},  // VAR_SEQ_AAAB
    {0, 0, 1, 1, 0, 0, 1, 1},  // VAR_SEQ_AABB
    {0, 1, 0, 1, 0, 1, 0, 1},  // VAR_SEQ_ABAB
    {0, 1, 0, 2, 0, 1, 0, 2},  // VAR_SEQ_ABAC
    {0, 0, 0, 1, 0, 0, 0, 2},  // VAR_SEQ_AAABAAAC
};
