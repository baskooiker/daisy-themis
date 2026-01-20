# Polyphonic Chord Voice Implementation Plan

## Overview

Add a new polyphonic voice that plays chord progressions (pads). The voice will:
- Play diatonic and non-diatonic chord progressions
- Support different chord change rates (per half-bar, bar, 2 bars)
- Transpose based on the configured root note
- Properly handle MIDI note-on/off for polyphonic output

---

## 1. Chord Progression Representation

### 1.1 Chord Definition

Chords will be represented as **intervals from root** (in semitones), making them easily transposable:

```cpp
struct ChordShape {
    int8_t intervals[6];  // Up to 6 notes (root + extensions), -128 = unused
    uint8_t numNotes;     // How many notes in the chord
};

// Example: Major triad = {0, 4, 7, -128, -128, -128}, numNotes=3
// Example: Minor 7th = {0, 3, 7, 10, -128, -128}, numNotes=4
```

### 1.2 Chord Progression Definition

Progressions are sequences of chords defined by **scale degree** + **chord quality**:

```cpp
struct ProgressionStep {
    int8_t scaleDegree;       // 0-6 for diatonic (I-VII), or semitone offset for non-diatonic
    bool isDiatonic;          // If true, scaleDegree is a scale degree; if false, it's semitones
    uint8_t chordType;        // Index into chord shapes (major, minor, dim, etc.)
    int8_t octaveOffset;      // -1, 0, +1 octave adjustment
};

struct ChordProgression {
    const char* name;
    ProgressionStep steps[8];  // Max 8 chords in a progression
    uint8_t length;            // Number of chords in progression
    uint8_t feel;              // Categorization (happy, sad, tense, dreamy, etc.)
};
```

### 1.3 Pre-defined Chord Shapes

```cpp
enum ChordType {
    CHORD_MAJOR,        // {0, 4, 7}
    CHORD_MINOR,        // {0, 3, 7}
    CHORD_DIM,          // {0, 3, 6}
    CHORD_AUG,          // {0, 4, 8}
    CHORD_SUS2,         // {0, 2, 7}
    CHORD_SUS4,         // {0, 5, 7}
    CHORD_MAJ7,         // {0, 4, 7, 11}
    CHORD_MIN7,         // {0, 3, 7, 10}
    CHORD_DOM7,         // {0, 4, 7, 10}
    CHORD_DIM7,         // {0, 3, 6, 9}
    CHORD_MIN7B5,       // {0, 3, 6, 10} (half-diminished)
    CHORD_ADD9,         // {0, 4, 7, 14}
    CHORD_MADD9,        // {0, 3, 7, 14}
    NUM_CHORD_TYPES
};
```

### 1.4 Example Progressions

**Diatonic Progressions** (all chords from the scale):

```cpp
// I - V - vi - IV (Pop progression) - in C: C-G-Am-F
{{"Pop", {{0, true, CHORD_MAJOR}, {4, true, CHORD_MAJOR},
          {5, true, CHORD_MINOR}, {3, true, CHORD_MAJOR}}, 4, FEEL_HAPPY}}

// i - VI - III - VII (Epic minor) - in Am: Am-F-C-G
{{"Epic", {{0, true, CHORD_MINOR}, {5, true, CHORD_MAJOR},
           {2, true, CHORD_MAJOR}, {6, true, CHORD_MAJOR}}, 4, FEEL_EPIC}}

// ii - V - I - I (Jazz turnaround)
{{"Jazz 251", {{1, true, CHORD_MIN7}, {4, true, CHORD_DOM7},
               {0, true, CHORD_MAJ7}, {0, true, CHORD_MAJ7}}, 4, FEEL_JAZZY}}

// I - vi - IV - V (50s progression)
{{"Fifties", {{0, true, CHORD_MAJOR}, {5, true, CHORD_MINOR},
              {3, true, CHORD_MAJOR}, {4, true, CHORD_MAJOR}}, 4, FEEL_NOSTALGIC}}

// vi - IV - I - V (Axis progression)
{{"Axis", {{5, true, CHORD_MINOR}, {3, true, CHORD_MAJOR},
           {0, true, CHORD_MAJOR}, {4, true, CHORD_MAJOR}}, 4, FEEL_EMOTIONAL}}
```

**Non-Diatonic Progressions** (with borrowed chords/chromatic movement):

```cpp
// I - bVII - IV - I (Mixolydian feel) - semitone: 0, -2 from root, etc.
{{"Mixolydian", {{0, true, CHORD_MAJOR}, {-2, false, CHORD_MAJOR},
                 {3, true, CHORD_MAJOR}, {0, true, CHORD_MAJOR}}, 4, FEEL_BRIGHT}}

// i - bVI - bIII - bVII (Aeolian rock)
{{"Aeolian Rock", {{0, true, CHORD_MINOR}, {-4, false, CHORD_MAJOR},
                   {-5, false, CHORD_MAJOR}, {-2, false, CHORD_MAJOR}}, 4, FEEL_DARK}}

// I - #IVdim - V - I (Chromatic passing)
{{"Chromatic", {{0, true, CHORD_MAJOR}, {6, false, CHORD_DIM},
                {4, true, CHORD_MAJOR}, {0, true, CHORD_MAJOR}}, 4, FEEL_TENSE}}

// i - iv - bVII - i (Minor plagal)
{{"Minor Plagal", {{0, true, CHORD_MINOR}, {3, true, CHORD_MINOR},
                   {-2, false, CHORD_MAJOR}, {0, true, CHORD_MINOR}}, 4, FEEL_MELANCHOLIC}}
```

---

## 2. Timing System

### 2.1 Chord Change Rate

```cpp
enum ChordRate {
    CHORD_RATE_2_BARS,    // Change every 64 steps (2 bars of 32 steps)
    CHORD_RATE_1_BAR,     // Change every 32 steps (1 bar)
    CHORD_RATE_HALF_BAR,  // Change every 16 steps (half bar)
    CHORD_RATE_QUARTER,   // Change every 8 steps (quarter bar / 2 beats)
    NUM_CHORD_RATES
};

// Steps per chord for each rate
const uint8_t chordRateSteps[NUM_CHORD_RATES] = {64, 32, 16, 8};
```

### 2.2 Chord Timing State

```cpp
struct PolyVoiceState {
    uint8_t currentChordIndex;    // Which chord in progression (0-7)
    uint8_t stepsUntilChange;     // Countdown to next chord
    int8_t activeNotes[6];        // Currently sounding MIDI notes (for note-off)
    uint8_t numActiveNotes;       // How many notes are active
    bool notesOn;                 // Are notes currently sounding?
};
```

### 2.3 Note Timing

For pad-like behavior:
- **Note-on**: Trigger at chord change
- **Note-off**: Trigger just before next chord change (e.g., 1 step before)
- This creates a small gap preventing MIDI note overlap issues

```
Bar 1                           Bar 2
|----chord 1----|----chord 2----|
ON              OFF ON          OFF ON...
                ^ 1 step before change
```

---

## 3. Configuration Structure

### 3.1 PolyVoiceConfig

```cpp
struct PolyVoiceConfig {
    bool active;

    // Progression selection
    uint8_t progressionIndex;     // Which progression to use
    ChordRate chordRate;          // How fast chords change

    // Sound shaping
    uint8_t velocity;             // 0-127
    int8_t octaveOffset;          // -2 to +2, shifts all chord notes
    bool useInversions;           // If true, may invert chords for voice leading

    // Variation
    VariationConfig variation;    // A/B progressions like other voices
    uint8_t progressionB;         // Alternative progression for B variation
};
```

---

## 4. Core Algorithm: GetChordNotes()

```cpp
/**
 * Calculate actual MIDI notes for a chord at current step
 *
 * @param config      Poly voice configuration
 * @param progression The chord progression to use
 * @param chordIndex  Which chord in the progression (0-7)
 * @param rootNote    Global root note (0-11, C=0)
 * @param scale       Current scale type
 * @param outNotes    Output array for MIDI notes (max 6)
 * @return            Number of notes in chord
 */
uint8_t GetChordNotes(
    const PolyVoiceConfig* config,
    const ChordProgression* progression,
    uint8_t chordIndex,
    uint8_t rootNote,
    ScaleType scale,
    int8_t* outNotes
) {
    const ProgressionStep& step = progression->steps[chordIndex];
    const ChordShape& shape = chordShapes[step.chordType];

    // Calculate chord root
    int8_t chordRoot;
    if (step.isDiatonic) {
        // Get scale degree note
        chordRoot = GetScaleNote(scale, rootNote, step.scaleDegree);
    } else {
        // Direct semitone offset from root
        chordRoot = rootNote + step.scaleDegree;
    }

    // Apply octave offset (base octave = 48 = C3)
    int8_t baseNote = 48 + chordRoot + (config->octaveOffset * 12) + (step.octaveOffset * 12);

    // Build chord notes
    uint8_t count = 0;
    for (int i = 0; i < shape.numNotes && i < 6; i++) {
        int8_t note = baseNote + shape.intervals[i];
        if (note >= 0 && note <= 127) {
            outNotes[count++] = note;
        }
    }

    return count;
}
```

---

## 5. Sequencer Integration

### 5.1 Add to Sequencer class

```cpp
// In themis_sequencer.h
class Sequencer {
    // ... existing members ...

    // Poly voice
    PolyVoiceConfig polyVoice;
    PolyVoiceState polyState;

    // Callback for poly notes
    std::function<void(const int8_t* notes, uint8_t count, bool noteOn)> onPolyTrigger;

    // Methods
    void ProcessPolyVoice(uint8_t step);
    void TriggerPolyChord();
    void ReleasePolyChord();
};
```

### 5.2 ProcessPolyVoice() Logic

```cpp
void Sequencer::ProcessPolyVoice(uint8_t step) {
    if (!polyVoice.active) return;

    uint8_t stepsPerChord = chordRateSteps[polyVoice.chordRate];

    // Check if we need to release notes (1 step before chord change)
    if (polyState.notesOn && polyState.stepsUntilChange == 1) {
        ReleasePolyChord();
    }

    // Check if it's time for a new chord
    if (polyState.stepsUntilChange == 0) {
        // Get progression based on variation
        uint8_t progIndex = polyVoice.progressionIndex;
        uint8_t var = GetCurrentVariation(&polyVoice.variation, step, barCounter);
        if (var == 1 && polyVoice.variation.mode != VAR_MODE_OFF) {
            progIndex = polyVoice.progressionB;
        }

        // Trigger new chord
        TriggerPolyChord();

        // Advance to next chord in progression
        polyState.currentChordIndex = (polyState.currentChordIndex + 1) %
                                       progressions[progIndex].length;

        // Reset countdown
        polyState.stepsUntilChange = stepsPerChord;
    }

    polyState.stepsUntilChange--;
}
```

---

## 6. MIDI Note-Off Handling

### 6.1 Proper Note Tracking

```cpp
void Sequencer::TriggerPolyChord() {
    // First release any currently held notes
    if (polyState.notesOn) {
        ReleasePolyChord();
    }

    // Get the chord notes
    int8_t notes[6];
    uint8_t count = GetChordNotes(&polyVoice,
                                   &progressions[polyVoice.progressionIndex],
                                   polyState.currentChordIndex,
                                   melodyRoot, melodyScale, notes);

    // Store active notes for later release
    polyState.numActiveNotes = count;
    for (int i = 0; i < count; i++) {
        polyState.activeNotes[i] = notes[i];
    }
    polyState.notesOn = true;

    // Trigger callback
    if (onPolyTrigger) {
        onPolyTrigger(notes, count, true);  // noteOn = true
    }
}

void Sequencer::ReleasePolyChord() {
    if (!polyState.notesOn) return;

    // Send note-offs for all active notes
    if (onPolyTrigger) {
        onPolyTrigger(polyState.activeNotes, polyState.numActiveNotes, false);
    }

    polyState.notesOn = false;
    polyState.numActiveNotes = 0;
}
```

---

## 7. Desktop Audio: Polyphonic Pad Synth

### 7.1 Simple Poly Synth Structure

```cpp
// In audio.h
struct PadVoice {
    bool active;
    float phase;
    float freq;
    float env;          // Amplitude envelope (0-1)
    float targetEnv;    // Target for envelope (1 when on, 0 when off)
    float filterState;
};

class PadSynth {
public:
    static constexpr int MAX_VOICES = 6;

    void NoteOn(int8_t note, uint8_t velocity);
    void NoteOff(int8_t note);
    void AllNotesOff();
    float Process(float sampleRate);

private:
    PadVoice voices[MAX_VOICES];
    float attackRate = 0.001f;   // Slow attack for pad
    float releaseRate = 0.0005f; // Slow release for pad
};
```

### 7.2 Pad Sound Characteristics

- **Waveform**: Mix of sine + slight saw for warmth
- **Envelope**: Slow attack (100-500ms), slow release (500ms-2s)
- **Filter**: Low-pass with slight movement
- **Detuning**: Optional slight detune between voices for richness

```cpp
float PadSynth::Process(float sampleRate) {
    float output = 0.0f;

    for (int i = 0; i < MAX_VOICES; i++) {
        PadVoice& v = voices[i];
        if (!v.active && v.env < 0.001f) continue;

        // Smooth envelope
        float envRate = (v.targetEnv > v.env) ? attackRate : releaseRate;
        v.env += (v.targetEnv - v.env) * envRate;

        // Deactivate when envelope is done releasing
        if (v.targetEnv == 0.0f && v.env < 0.001f) {
            v.active = false;
            continue;
        }

        // Oscillator (sine + subtle saw)
        v.phase += v.freq / sampleRate;
        if (v.phase > 1.0f) v.phase -= 1.0f;

        float sine = sinf(v.phase * 2.0f * M_PI);
        float saw = v.phase * 2.0f - 1.0f;
        float osc = sine * 0.7f + saw * 0.3f;

        // Simple low-pass
        v.filterState += 0.1f * (osc - v.filterState);

        output += v.filterState * v.env * 0.15f;
    }

    return output;
}
```

---

## 8. UI Integration

### 8.1 New Melody Tab Section (or separate Tab)

```
┌─────────────────────────────────────────────────────────────┐
│ [Poly Voice / Pads]                                         │
│ ☑ Active                                                    │
│                                                             │
│ Progression: [Pop I-V-vi-IV ▼]    Rate: [1 Bar ▼]          │
│ Octave: [-1 / 0 / +1]             Velocity: [████░░] 80    │
│                                                             │
│ Variation: [AB ▼]  B Progression: [Jazz 251 ▼]             │
│                                                             │
│ Current: Chord 2/4 [Am] ████████░░░░                        │
└─────────────────────────────────────────────────────────────┘
```

### 8.2 Mixer Integration

Add poly voice to mixer with M/S buttons.

---

## 9. Implementation Steps

### Step 1: Data Structures (core/themis_chords.h/cpp)
1. Define ChordShape, ChordType enum, chord shape data
2. Define ProgressionStep, ChordProgression, progression data
3. Define ChordRate enum and timing constants
4. Implement GetChordNotes() function

### Step 2: Config and State (core/themis_types.h)
1. Add PolyVoiceConfig struct
2. Add PolyVoiceState struct

### Step 3: Sequencer Integration (core/themis_sequencer.h/cpp)
1. Add polyVoice config and polyState to Sequencer
2. Add onPolyTrigger callback
3. Implement ProcessPolyVoice(), TriggerPolyChord(), ReleasePolyChord()
4. Call ProcessPolyVoice() from ProcessStep()
5. Add Init/Randomize methods for poly voice

### Step 4: Desktop Audio (desktop/audio.h/cpp)
1. Add PadVoice struct and PadSynth class
2. Implement polyphonic note tracking
3. Add pad-like synthesis with slow attack/release
4. Integrate into AudioEngine

### Step 5: Main Callbacks (desktop/main.cpp)
1. Add onPolyTrigger callback
2. Route to audio engine and MIDI output

### Step 6: UI (desktop/ui.h/cpp)
1. Add poly voice controls in Melody tab (or new tab)
2. Add to mixer with mute/solo

### Step 7: Firmware Integration (future)
1. Add poly voice MIDI output to Daisy platform
2. Add menu items for poly voice config

---

## 10. Progression Ideas

### Happy/Bright
- I - V - vi - IV (Pop)
- I - IV - V - IV (Rock)
- I - vi - ii - V (Jazz standard)

### Sad/Melancholic
- vi - IV - I - V (Axis of Awesome)
- i - VI - III - VII (Aeolian)
- i - iv - v - i (Natural minor)

### Tense/Dramatic
- i - bVI - bVII - i (Epic)
- i - iv - bVI - V (Andalusian cadence)
- I - bVII - IV - I (Mixolydian rock)

### Dreamy/Ethereal
- Imaj7 - IVmaj7 - vi7 - V7 (Jazzy dream)
- I - iii - IV - iv (Major to minor IV)
- Iadd9 - Vadd9 - vi - IV (Modern pop)

### Jazz/Complex
- ii7 - V7 - Imaj7 - vi7 (Jazz turnaround)
- iii7 - VI7 - ii7 - V7 (Circle of fifths)
- Imaj7 - #IVm7b5 - IV - bVIImaj7 (Neo-soul)

---

## 11. File Changes Summary

| File | Changes |
|------|---------|
| core/themis_chords.h | NEW - Chord shapes, progressions, types |
| core/themis_chords.cpp | NEW - Chord data, GetChordNotes() |
| core/themis_types.h | Add PolyVoiceConfig, PolyVoiceState |
| core/themis_sequencer.h | Add poly voice members and callbacks |
| core/themis_sequencer.cpp | Implement poly voice processing |
| desktop/audio.h | Add PadSynth class |
| desktop/audio.cpp | Implement PadSynth |
| desktop/main.cpp | Add onPolyTrigger callback |
| desktop/ui.h | Add poly mixer state |
| desktop/ui.cpp | Add poly voice UI controls |
| desktop/Makefile | Add themis_chords.cpp to build |

---

## 12. Testing Checklist

- [ ] Chord notes calculate correctly for all scale types
- [ ] Non-diatonic progressions transpose correctly
- [ ] Chord changes happen at correct intervals
- [ ] Note-offs are sent 1 step before new chord
- [ ] No stuck notes when stopping playback
- [ ] Pad synth has smooth attack/release
- [ ] Mixer solo/mute works for poly voice
- [ ] Variation A/B switches progressions correctly
- [ ] UI shows current chord and progress
