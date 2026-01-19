# Themis - Master Clock & Generative Drum Sequencer

## IMPORTANT: After Every Build
**Always report the flash memory usage percentage after running `make`.** Look for the line:
```
FLASH:      XXXXX B       128 KB     XX.XX%
```
Report this to the user so they can track memory consumption.

## Project Overview

Themis is a firmware for the **Daisy Patch** Eurorack module that provides:
- Generative drum pattern sequencing (6 voices via MIDI)
- Two melodic voices (CV+Gate and MIDI)
- Master clock with external sync capability
- Groove/swing timing system
- Persistent settings storage

## Hardware Platform

- **MCU**: STM32H750 (Cortex-M7, 480MHz)
- **Display**: 128x64 OLED (SSD1309)
- **Audio**: 48kHz sample rate (used for timing, not audio generation)
- **Storage**: QSPI Flash for persistent settings
- **I/O**:
  - CV1-CV4: Control voltage inputs (0-5V)
  - CTRL1-CTRL4: Mapped to CV1-CV4
  - Gate In: External clock input
  - Gate Out: Melody gate output
  - DAC1: Melody CV output (1V/octave)
  - OUT2, OUT3, OUT4: Clock division outputs
  - MIDI Out: Drum triggers + MIDI melody voice
  - Encoder: Menu navigation

## Architecture

### File Structure
```
Themis/
├── CLAUDE.md       # This file - project documentation for Claude
├── Makefile        # Build configuration
├── types.h         # Type definitions, enums, structs
├── globals.h/cpp   # Global variables & const data tables
├── groove.h/cpp    # Trigger queues & groove timing
├── drums.h/cpp     # Drum pattern generation & rhythm algorithms
├── melody.h/cpp    # Melody generation & scale utilities
├── display.h/cpp   # OLED display rendering
├── config.h/cpp    # Settings, controls, MIDI handling
└── Themis.cpp      # Main & AudioCallback only (~180 lines)
```

### Module Responsibilities

| Module | Description |
|--------|-------------|
| `types.h` | All enums, structs, inline helpers (GrooveConfig, VoiceConfig, etc.) |
| `globals` | Hardware instances, state variables, const pattern data |
| `groove` | Trigger queue management, groove randomization, MIDI output |
| `drums` | Rhythm generation (Euclidean, syncopated), voice config, fills |
| `melody` | Scale utilities, melody pattern & note generation |
| `display` | All OLED rendering for different display states |
| `config` | Persistent storage, clock control, encoder/controls, MIDI handling |
| `Themis` | AudioCallback (sample-accurate timing) and main loop |

### Key Concepts

#### Timing System
- Uses 32-step patterns (2 bars of 16th notes)
- 8-bar cycles: patterns regenerate every 8 bars
- 32-bar cycles: voice "personalities" change every 32 bars
- Sample-accurate groove timing via trigger queues

#### Groove System
- 32 groove patterns (borrowed from Korg Drumlogue)
- Per-voice timing offset (0-75% of 16th note)
- Per-voice velocity variation
- Groove affects both drums and melody

#### Pattern System
- Kick patterns: predefined array of common patterns
- Other drums: generated based on "personality" (density, interaction style)
- Melody: style-based generation (Supporting vs Arpeggiator)

#### Memory Layout
- FLASH: 128KB program memory (~84% used)
- SRAM: 512KB main RAM (~10% used)
- QSPI: Persistent settings at address 0x90000000

## Common Tasks

### Adding a New Config Menu Option
1. Add enum value to `ConfigOption` in types.h
2. Add name string to `configOptionNames[]` in config.cpp
3. Add display handling in `RenderConfigMenu()` in display.cpp
4. Add edit handling in `ProcessConfigEdit()` in config.cpp
5. If persistent: add field to `PersistentSettings`, update Save/LoadSettings

### Adding a New Scale
1. Add enum value to `ScaleType` in types.h
2. Add interval array (e.g., `const int8_t scaleNewScale[]`) in melody.cpp
3. Add to `scaleLengths[]` array
4. Add name to `scaleNames[]`
5. Add case to `GetScaleNote()` switch statement

### Adding a New Drum Voice
1. Add enum value to `DrumVoice` in types.h
2. Update `NUM_DRUM_VOICES`
3. Add MIDI note to `drumNotes[]`
4. Add pattern generation in drums.cpp
5. Add display handling if needed

### Modifying Groove Patterns
- Groove patterns are in `groovePatterns[32][16]` (timing) and `velocityPatterns[32][16]` (dynamics)
- Values are percentage offsets (-100 to +100 for timing, 50-150 for velocity)
- Pattern index selected by `currentGroovePattern`

## Code Conventions

### Naming
- `camelCase` for variables and functions
- `UPPER_CASE` for constants and enum values
- `TypeName` for structs and enums
- Prefix `current` for state variables (e.g., `currentStep`, `currentGroovePattern`)
- Prefix `last` for previous-value tracking (e.g., `lastBeatSample`)

### Patterns
- Bit patterns stored as uint32_t, bit 31 = step 0 (MSB-first)
- Use `IsStepActive(pattern, step)` to check if step triggers
- Pattern length typically 32 steps (2 bars)

### Timing
- All groove-timed triggers go through queue system
- `globalSampleCounter` is the master time reference
- `nextBeatSample` calculated at start of each step for look-ahead scheduling

### State Management
- Global state variables in globals.cpp
- Display state via `DisplayState` enum
- Edit mode tracked per config option

## Known Issues / Technical Debt

1. **Unused variables** - Some legacy variables trigger warnings (interactionSymbols, cstr in display.cpp)
2. **Magic numbers** - Some timing constants could be named defines
3. **No unit tests** - Embedded code, testing is manual

## Build Instructions

```bash
# From Themis directory
make clean && make

# Flash to device
make program-dfu
```

## MIDI Implementation

### Drum Channel (Channel 10, 0-indexed as 9)
| Voice | Note |
|-------|------|
| Kick  | 36   |
| Snare | 38   |
| Clap  | 39   |
| Closed HH | 42 |
| Open HH | 46 |
| Analog | 37 |

### Melody MIDI Channel
- Configurable (default: Channel 1)
- Notes: C2 (36) + semitone offset
- Velocity: 100 (fixed)

## CV Output

### DAC1 (Melody CV)
- 0V = C2
- 1V/octave scaling
- 3 octave range (0-3V)
- Actual output: 0-5V DAC scaled to 0-3V effective range

### Gate Out
- High during melody note
- Gate length: ~10ms (configurable via GATE_LENGTH_SAMPLES)
