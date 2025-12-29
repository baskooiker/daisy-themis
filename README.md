# Themis - Master Clock & Generative Drum Sequencer for Daisy Patch

Themis is a master clock generator and generative drum sequencer for the Daisy Patch that provides MIDI clock, gate clock outputs, and MIDI drum pattern generation for the Vermona DRM1 mk3.

## Features

- **Internal Clock Generation**: Generates a stable master clock with adjustable BPM (20-300 BPM)
- **BPM Control**: Use the encoder to adjust tempo in 0.5 BPM increments
- **Start/Stop Control**: Push the encoder button to start/stop the clock
- **Gate Output**: Sends 16th note clock pulses on the gate output
- **MIDI Clock Output**: Sends standard MIDI clock messages (24 PPQN)
- **External MIDI Clock Sync**: Automatically switches to external clock mode when MIDI clock is received
- **MIDI Transport Control**: Responds to MIDI Start, Stop, and Continue messages

## Controls

- **Encoder Rotation**: Adjust BPM (when in internal clock mode)
- **Encoder Push**: Start/Stop the clock

## Outputs

- **Gate Out**: 16th note clock pulses (10ms pulses)
- **MIDI Out**: MIDI clock (24 PPQN) and transport messages
- **Audio Out 1**: 24 PPQN clock (same rate as MIDI clock, 10ms pulses)
- **Audio Out 2**: 16th note clock (10ms pulses)
- **Audio Out 3**: Quarter note clock (10ms pulses)
- **Audio Out 4**: Reset pulse (20ms pulse on start)

## MIDI Input

When MIDI clock messages are received on the MIDI input:
- Themis automatically switches to external clock mode
- The internal BPM setting is ignored
- Start/Stop state is controlled by incoming MIDI transport messages
- Themis passes through the MIDI clock and generates corresponding gate outputs
- After 500ms of no MIDI clock, Themis reverts to internal clock mode

## Display

The OLED display shows:
- Current BPM setting
- Clock mode (INT for internal, EXT for external)
- Running status (RUNNING or STOPPED)
- Active pattern numbers when running (K=Kick, C=Clap, H=Hat)

## Generative Drum Sequencer

Themis includes a techno-inspired generative drum sequencer designed specifically for the Vermona DRM1 mk3 drum synthesizer.

### Pattern-Based System

- **32-step patterns** (two bars in 4/4 time at 16th note resolution)
- **16 variations per element** for kick, clap, and hi-hat
- **Automatic pattern rotation** every 8 bars (4 x 2-bar phrases)
- Patterns are combined independently for endless variation
- Hi-hats use combined open/closed patterns for authentic groove

### Pattern Characteristics

**Kick Patterns (16 variations, 32 steps)**
- Emphasis on four-on-the-floor across both bars
- Variations include syncopation, double kicks, and busy fills
- Pattern 0: Pure four-on-the-floor
- Patterns 1-14: Various techno-inspired variations with bar-to-bar development
- Pattern 15: Full 8th notes (very busy)

**Clap Patterns (16 variations, 32 steps)**
- Based on classic backbeat (beats 2 and 4 of each bar)
- Mix of straight and syncopated rhythms across 2 bars
- Pattern 0: Classic backbeat both bars
- Patterns 1-14: Syncopated and shuffle variations with development
- Pattern 15: Complex techno shuffle across both bars

**Hi-Hat Patterns (16 variations, 32 steps)**
- **Combined closed/open patterns** for authentic techno grooves
- Closed: Emphasis on off-beat (8th note upbeats)
- Open: Strategic placement on beat 4 and variations for accent
- Pattern 0: Classic 8th note offbeats with open on beat 4
- Pattern 11: Full 16th notes closed (no opens)
- Patterns vary between triplet feels, shuffles, and gallop rhythms

### Vermona DRM1 mk3 MIDI Mapping

MIDI Channel: 10

| Voice | MIDI Note |
|-------|-----------|
| Kick | 36 (C) |
| Clap | 39 (D#) |
| Hi-Hat 1 Closed | 49 (C#) |
| Hi-Hat 1 Open | 51 (D#) |

### How It Works

1. When the clock is started, patterns begin playing
2. Every 8 bars, the system automatically randomizes to new pattern combinations
3. Patterns are sent as MIDI Note On messages to channel 10
4. Works in both internal and external clock modes
5. Patterns stay synchronized with the master clock
