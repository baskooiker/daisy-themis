# Themis - Master Clock & Generative Drum Sequencer

## Project Overview
Themis is a master clock generator and generative drum sequencer for the Daisy Patch hardware platform. It outputs MIDI clock, gate clock signals, and MIDI drum patterns designed for the Vermona DRM1 mk3 drum synthesizer.

## Hardware Platform
- **Device**: Daisy Patch (Electrosmith)
- **MCU**: STM32H750IB (ARM Cortex-M7)
- **Clock**: 480 MHz
- **Flash**: 128 KB
- **RAM**: 512 KB SRAM + 288 KB RAM_D2 + 128 KB DTCMRAM + 64 KB RAM_D3
- **Audio**: 48 kHz sample rate
- **Display**: 128x64 pixel OLED (SSD1309 driver)
- **DAC**: 12-bit (0-5V output range)

## Current Build Size
- **Flash**: 105,924 bytes (103.4 KB) / 128 KB - 80.81% used
- **SRAM**: 53,676 bytes (52.4 KB) / 512 KB - 10.24% used

## Hardware I/O Configuration

### Audio Outputs (CV Outputs, 5V = 1.0f)
- **Audio Out 1**: Fixed 16th note clock (10ms gate pulses)
- **Audio Out 2**: Configurable clock division (default: 1/8 notes)
- **Audio Out 3**: Configurable clock division (default: 1/4 notes)
- **Audio Out 4**: Reset pulse (20ms pulse on start)

### CV/Gate Outputs
- **Gate Out 1**: Analog voice gate (10ms pulses, controlled by Analog generative voice)
- **CV Out 1**: Analog voice CV (MIDI velocity mapped to 0-5V, 0=0V, 127=5V)

### MIDI
- **MIDI Out**: MIDI clock (24 PPQN), transport messages (Start/Stop/Continue), drum triggers
- **MIDI In**: External clock sync, transport control
- **MIDI Channel**: 10 (for Vermona DRM1 mk3)

### Controls
- **Encoder**: BPM adjustment, menu navigation, value editing
- **Encoder Button**: Start/Stop, menu selection

### Clock Division Options (Audio Out 2 & 3)
- 1/16 note
- 1/8 note
- 1/4 note (quarter)
- 1/2 note (half)
- 1 bar (4 beats / 16 steps)
- 2 bars (32 steps)
- 4 bars (64 steps)

## Architecture Overview

### Pattern System
- **Resolution**: 32-step patterns (2 bars at 16th note resolution)
- **Cycle Structure**: 32 bars total = 4 cycles × 8 bars
- **Randomization**: Patterns and personalities randomize every 32 bars
- **Bar Counter**: Tracks 2-bar phrases (0-15), displayed as bars 1-8

### Drum Voice Mapping (Vermona DRM1 mk3)
| Voice | MIDI Note | Display Label |
|-------|-----------|---------------|
| Kick | 36 (C) | - |
| Drum 1 | 48 (c) | D1 |
| Drum 2 | 41 (F) | D2 |
| Multi | 58 (b) | ML |
| Snare | 40 (E) | SN |
| Clap | 39 (D#) | CL |
| HiHat1 Closed | 49 (C#) | - |
| HiHat1 Open | 51 (D#) | - |
| HiHat2 Closed | 42 (F#) | H2 |
| HiHat2 Open | 44 (G#) | - |
| Analog | - | An |

### Generative Voice System

#### Voice Roles
1. **Fundamental Beat Voice** (1 voice):
   - Randomly assigned to either CLAP or SNARE at pattern randomization
   - Plays consistent backbeat pattern (beats 2 and 4)
   - Not randomized during performance

2. **Generative Voices** (6 voices):
   - DRUM1, DRUM2, MULTI, (CLAP or SNARE - whichever isn't fundamental), HIHAT2, ANALOG
   - Each has randomized rhythm style, density, pattern length, and interactions
   - Randomized every 32 bars (unless Freeze is enabled)

#### Rhythm Styles
- **RHYTHM_SYNCOPATED**: Syncopated patterns with emphasis on off-beats
- **RHYTHM_STRAIGHT**: Straight on-beat patterns
- **RHYTHM_EUCLIDEAN**: Euclidean distribution of hits across pattern
- **RHYTHM_ANTI_EUCLIDEAN**: Inverted Euclidean patterns (hits where Euclidean has rests)
- **RHYTHM_FOLLOW_KICK**: Mirrors the current kick drum pattern

#### Density Levels
- **DENSITY_LOW**: ~37.5% fill (12 out of 32 hits)
- **DENSITY_MEDIUM**: ~62.5% fill (20 out of 32 hits)
- **DENSITY_HIGH**: ~87.5% fill (28 out of 32 hits)

#### Pattern Lengths (Polyrhythms)
- 12 steps (polyrhythm: 12 against 32)
- 13 steps (polyrhythm: 13 against 32)
- 15 steps (polyrhythm: 15 against 32)
- 17 steps (polyrhythm: 17 against 32)
- 18 steps (polyrhythm: 18 against 32)
- 32 steps (default, in sync)

#### Voice Interactions
- **INTERACTION_NONE**: Voice plays independently
- **INTERACTION_DIVIDED**: Voice divides pattern with partner (alternate 16th notes)
- **INTERACTION_ALTERNATE_BAR**: Voices alternate every bar
- **INTERACTION_ALTERNATE_HALF**: Voices alternate every half bar (8 steps)
- **INTERACTION_ALTERNATE_TWO**: Voices alternate every 2 steps

### Groove Timing System

#### Per-Voice Groove
- Each voice has independent groove timing modulation
- Groove patterns define timing offset for each 16th note step (16 values)
- Timing offset range: ±10ms maximum
- Groove amount per voice: randomized 0-75% (kick: 0-35% to avoid excessive bass swing)

#### Groove Velocity Modulation
- MIDI velocity modulated by groove patterns (up to ±100%)
- Analog voice CV modulated by groove patterns (affects 0-5V output)
- Base velocity: 100 for Analog voice, 95 for MIDI voices

#### Groove Patterns
Multiple predefined groove patterns with different timing feels:
- Various swing patterns
- Triplet feels
- Shuffle patterns
- Straight patterns

### Analog Voice (NEW)
The Analog voice is a generative drum voice that outputs CV/Gate instead of MIDI:
- **CV Output**: MIDI velocity (0-127) scaled to 0-5V on CV OUT 1
- **Gate Output**: 10ms gate pulses on Gate Out 1
- **Integration**: Full participation in generative system (rhythm styles, density, interactions, groove)
- **Default Configuration**: RHYTHM_FOLLOW_KICK, DENSITY_HIGH, no interaction, 32-step pattern

## Menu System & Configuration

### Display States
1. **DISPLAY_DEFAULT**: Main screen showing BPM, clock mode, run status, groove pattern
2. **DISPLAY_CONFIG_MENU**: Configuration menu (accessed via encoder hold)
3. **DISPLAY_CONFIG_EDIT**: Edit mode for config values
4. **DISPLAY_PATTERN_INFO**: Scrollable voice configuration display

### Configuration Options
- **BPM**: 20-300 BPM (0.5 BPM increments)
- **OUT2 div**: Clock division for Audio Out 2
- **OUT3 div**: Clock division for Audio Out 3
- **Freeze**: Prevents pattern/personality randomization when enabled
- **Pattern info**: Displays detailed voice configurations (scrollable)

### Pattern Info Display
Shows 7 lines (4 visible at once, scrollable):
- Line 1: Fundamental beat voice (C or S)
- Lines 2-7: Generative voices (D1, D2, ML, CL/SN, H2, An)

Display format: `[Voice]:Sty Dns L[Length] >[Partner]`
- Voice: 2-char label
- Sty: Rhythm style (Syn/Str/Euc/AEu/FKi)
- Dns: Density (Lo/Md/Hi)
- Length: Pattern length (12/13/15/17/18/32)
- Partner: Interaction partner (if any)

### Persistent Settings (QSPI Flash)
Settings are saved to flash and restored on boot:
- BPM
- OUT2 division
- OUT3 division
- Freeze enabled/disabled
- Magic number for validation: 0x54484D53 ("THMS")

## Clock System

### Internal Clock
- Metro-based clock generation
- BPM range: 20-300 BPM
- 16th note resolution
- Sample-accurate timing at 48 kHz

### External Clock
- Automatic detection of external MIDI clock
- Switches to external mode when MIDI clock received
- Timeout: 500ms (reverts to internal if no clock received)
- Supports MIDI Start/Stop/Continue messages

### Clock Outputs
- **MIDI Clock**: 24 PPQN (standard MIDI clock)
- **Gate Outputs**: Configurable divisions on Audio Outs 2-3
- **Fixed Outputs**: 16th notes on Audio Out 1, Reset pulse on Audio Out 4

## Code Structure (Themis.cpp ~2,500 lines)

### Key Functions
- `AudioCallback()`: 48kHz audio callback, processes MIDI triggers, outputs gate signals
- `ProcessDrumPatterns()`: Main pattern playback engine, handles groove timing
- `RandomizeVoicePersonalities()`: Assigns roles, styles, densities, interactions (every 32 bars)
- `GenerateVoicePatterns()`: Creates bit patterns based on rhythm style and density
- `CalculateGrooveVelocity()`: Applies groove modulation to MIDI velocity
- `CalculateGrooveOffset()`: Calculates sample-accurate timing offset for groove
- `ProcessClock()`: Processes 16th note triggers, calls pattern engine
- `HandleMidiMessage()`: External MIDI clock/transport handling
- `UpdateDisplay()`: Renders OLED display for all display states
- `SaveSettings()`/`LoadSettings()`: Persistent storage to QSPI flash

### Important Constants
- `GATE_PULSE_SAMPLES`: 480 samples (10ms at 48kHz)
- `RESET_PULSE_SAMPLES`: 960 samples (20ms at 48kHz)
- `ANALOG_GATE_SAMPLES`: 480 samples (10ms at 48kHz)
- `MIDI_CLOCK_PPQN`: 24 pulses per quarter note
- `CLOCKS_PER_16TH`: 6 MIDI clocks per 16th note
- `MENU_TIMEOUT_MS`: 10 seconds

### Pattern Generation Details

#### Euclidean Algorithm
Distributes `hits` evenly across `length` steps using Bjorklund's algorithm.

#### Anti-Euclidean
Inverts Euclidean pattern: hits become rests, rests become hits.

#### Follow Kick
Copies the current kick drum pattern directly.

#### High Density Implementation
- Start with all positions filled (all bits set)
- Remove ~12.5% of hits randomly (length / 8 positions)
- Results in ~87.5% density (28/32 hits)

## Development History (This Session)

### Initial State
- Kick groove too heavy
- HAT2 patterns not busy enough at high density
- Basic generative system working

### Major Changes
1. **Kick Groove Reduction**: Reduced max groove from 75% to 35%
2. **High Density Fix**: Proper implementation for ~87.5% fill (28/32 hits)
3. **Voice Personality Randomization**: Every 32 bars, randomizes rhythm styles, densities, interactions
4. **Role Swapping**: Fundamental beat randomly assigned to CLAP or SNARE
5. **Freeze Config**: Option to prevent pattern randomization
6. **Menu System**: Config menu with scrolling, edit mode
7. **OUT3 Division**: Configurable clock division
8. **Pattern Info Display**: Scrollable display showing voice configurations with interactions
9. **Groove Velocity**: Increased to 100% modulation
10. **Analog Voice**: New CV/Gate voice with full generative integration
11. **Follow Kick Rhythm**: New rhythm style that mirrors kick patterns
12. **OUT2 Division**: Added configurable division for Audio Out 2
13. **Display Cleanup**: Removed debug info from default display

## Known Issues & Warnings
- Unused variable `numBitsForCV` in TuringMachine::Process() (legacy code)
- Unused variable `interactionSymbols` in UpdateDisplay() (prepared for future feature)

## Build System
- **Toolchain**: ARM GCC (arm-none-eabi-gcc 13.2.1)
- **Build**: `make` in project directory
- **Dependencies**: libDaisy, DaisySP
- **Linker Script**: STM32H750IB_flash.lds

## Future Considerations
- Remove legacy TuringMachine class (currently has unused code)
- Consider implementing interaction symbols display in pattern info
- Potential for more rhythm styles
- Possible MIDI channel configuration
- Additional groove patterns

## Testing Notes
- Verify Analog voice CV output scaling (0-127 velocity → 0-5V)
- Test Gate Out 1 for Analog voice (10ms pulses)
- Verify all 7 lines visible in pattern info display with scrolling
- Test OUT2 and OUT3 configurable divisions with all division options
- Confirm persistent settings save/load after power cycle
- Verify Freeze mode prevents randomization but allows fills
- Test Follow Kick rhythm style tracks kick patterns correctly
