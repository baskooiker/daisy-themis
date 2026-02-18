/**
 * @file main.cpp
 * @brief Themis Desktop Application Entry Point
 *
 * Desktop testing application for Themis generative drum/melody algorithms.
 * Uses SDL2 for audio/windowing and Dear ImGui for the UI.
 */

#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <chrono>
#include <iostream>
#include <csignal>
#include <atomic>

#include "themis_platform.h"
#include "themis_sequencer.h"
#include "themis_chords.h"
#include "themis_rhythm.h"
#include "themis_data.h"
#include "platform_desktop.h"
#include "audio.h"
#include "ui.h"
#include "config.h"

#ifdef THEMIS_ENABLE_MIDI
#include "midi_out.h"
#endif

// Application state
static themis::Sequencer g_sequencer;
static DesktopPlatform g_platform;
static uint64_t g_lastStepTime = 0;
static std::atomic<bool> g_shouldQuit{false};
static themis_config::Settings g_settings;

// Signal handler for graceful shutdown
static void SignalHandler(int signal)
{
    if (signal == SIGINT || signal == SIGTERM) {
        g_shouldQuit.store(true);
    }
}

/**
 * @brief Calculate step interval in microseconds based on BPM
 */
static uint64_t GetStepIntervalUs(float bpm)
{
    // 16th note duration: 60 / BPM / 4 seconds = 15 / BPM seconds
    return (uint64_t)(15000000.0 / bpm);
}

/**
 * @brief Set up platform callbacks to route triggers to audio/MIDI
 */
static void SetupCallbacks()
{
    // Drum trigger callback - play synth and/or send MIDI
    g_sequencer.onDrumTrigger = [](themis::DrumVoice voice, uint8_t velocity) {
        // Trigger activity indicator
        themis_ui::g_ui.TriggerDrumActivity(voice);

        // Check mixer solo/mute state
        if (!themis_ui::g_ui.ShouldPlayDrum(voice)) {
            return;
        }

        // Play synth
        themis_audio::g_audioEngine.TriggerDrum(voice, velocity);

        // Send MIDI
        if (themis::g_platform) {
            themis::g_platform->SendMidiNoteOn(themis::DRM1_MIDI_CHANNEL,
                                                themis::drumNotes[voice],
                                                velocity);
        }
    };

    // Melody trigger callback
    static uint8_t lastMelodyMidiNote = 0;
    static uint8_t lastMelodyMidiChannel = 0;
    static bool melodyMidiNoteActive = false;

    g_sequencer.onMelodyTrigger = [](int8_t note) {
        // Trigger activity indicator
        themis_ui::g_ui.TriggerMelodyActivity();

        // Always send note-off for previous note first (even when muted)
        // to prevent hanging notes when mute state changes mid-note
        if (melodyMidiNoteActive && themis::g_platform) {
            themis::g_platform->SendMidiNoteOff(lastMelodyMidiChannel, lastMelodyMidiNote);
            melodyMidiNoteActive = false;
        }

        // Check mixer solo/mute state - only block new note-ons
        if (!themis_ui::g_ui.ShouldPlayMelody()) {
            return;
        }

        // Play internal synth
        themis_audio::g_audioEngine.TriggerMelodyMidi(note, 100);

        // Also send MIDI out
        uint8_t midiNote = 36 + note;  // C2 = 36
        if (midiNote > 127) midiNote = 127;
        lastMelodyMidiNote = midiNote;
        lastMelodyMidiChannel = g_sequencer.melodyChannel;
        melodyMidiNoteActive = true;
        if (themis::g_platform) {
            themis::g_platform->SendMidiNoteOn(g_sequencer.melodyChannel, midiNote, 100);
        }
    };

    // Melody note off callback
    g_sequencer.onMelodyNoteOff = []() {
        themis_audio::g_audioEngine.StopMelodyMidi();
        if (melodyMidiNoteActive && themis::g_platform) {
            themis::g_platform->SendMidiNoteOff(lastMelodyMidiChannel, lastMelodyMidiNote);
            melodyMidiNoteActive = false;
        }
    };

    // Chord voice callback
    static uint8_t lastChordMidiChannel = 1;
    g_sequencer.onChordTrigger = [](const int8_t* notes, uint8_t count, bool noteOn) {
        if (noteOn) {
            // Trigger activity indicator
            themis_ui::g_ui.TriggerChordActivity();

            // Check mixer solo/mute state - only block note-ons
            if (!themis_ui::g_ui.ShouldPlayChords()) {
                return;
            }

            // Trigger synth notes
            themis_audio::g_audioEngine.TriggerChordNotes(notes, count,
                                                          g_sequencer.chordVoice.velocity);

            // Store the channel these notes are sent on
            lastChordMidiChannel = g_sequencer.chordVoice.midiChannel;

            // Send MIDI note-ons
            if (themis::g_platform) {
                for (uint8_t i = 0; i < count; i++) {
                    themis::g_platform->SendMidiNoteOn(
                        lastChordMidiChannel, notes[i],
                        g_sequencer.chordVoice.velocity);
                }
            }
        } else {
            // Always allow note-offs through to prevent hanging notes
            themis_audio::g_audioEngine.ReleaseChordNotes(notes, count);

            // Send MIDI note-offs on the channel they were originally sent on
            if (themis::g_platform) {
                for (uint8_t i = 0; i < count; i++) {
                    themis::g_platform->SendMidiNoteOff(
                        lastChordMidiChannel, notes[i]);
                }
            }
        }
    };

    // Rhythm player callback
    static uint8_t lastRhythmMidiChannel = 3;
    g_sequencer.onRhythmTrigger = [](const int8_t* notes, uint8_t count, uint8_t velocity, bool noteOn) {
        if (noteOn) {
            // Trigger activity indicator
            if (count > 0) {
                themis_ui::g_ui.TriggerRhythmActivity();
            }

            // Check mixer solo/mute state - only block note-ons
            if (!themis_ui::g_ui.ShouldPlayRhythm()) {
                return;
            }

            // Trigger synth notes
            themis_audio::g_audioEngine.TriggerRhythmNotes(notes, count, velocity);

            // Store the channel these notes are sent on
            lastRhythmMidiChannel = g_sequencer.rhythmVoice.midiChannel;

            // Send MIDI note-ons
            if (themis::g_platform) {
                for (uint8_t i = 0; i < count; i++) {
                    themis::g_platform->SendMidiNoteOn(
                        lastRhythmMidiChannel,
                        notes[i],
                        velocity);
                }
            }
        } else {
            // Always allow note-offs through to prevent hanging notes
            themis_audio::g_audioEngine.ReleaseRhythmNotes(notes, count);

            // Send MIDI note-offs on the channel they were originally sent on
            if (themis::g_platform) {
                for (uint8_t i = 0; i < count; i++) {
                    themis::g_platform->SendMidiNoteOff(
                        lastRhythmMidiChannel,
                        notes[i]);
                }
            }
        }
    };

    // Bass voice callback
    static uint8_t lastBassMidiChannel = 4;
    g_sequencer.onBassTrigger = [](int8_t note, uint8_t velocity, bool noteOn) {
        if (noteOn) {
            // Trigger activity indicator
            themis_ui::g_ui.TriggerBassActivity();

            // Check mixer solo/mute state - only block note-ons
            if (!themis_ui::g_ui.ShouldPlayBass()) {
                return;
            }

            // Trigger synth note
            themis_audio::g_audioEngine.TriggerBass(note, velocity);

            // Store the channel this note is sent on
            lastBassMidiChannel = g_sequencer.bassVoice.midiChannel;

            // Send MIDI note-on
            if (themis::g_platform) {
                themis::g_platform->SendMidiNoteOn(
                    lastBassMidiChannel,
                    note,
                    velocity);
            }
        } else {
            // Always allow note-offs through to prevent hanging notes
            themis_audio::g_audioEngine.StopBass(note);

            // Send MIDI note-off on the channel it was originally sent on
            if (themis::g_platform) {
                themis::g_platform->SendMidiNoteOff(
                    lastBassMidiChannel,
                    note);
            }
        }
    };

#ifdef THEMIS_ENABLE_MIDI
    // Connect MIDI callbacks
    g_platform.midiNoteOnCallback = [](uint8_t ch, uint8_t note, uint8_t vel) {
        themis_midi::g_midiOutput.NoteOn(ch, note, vel);
    };
    g_platform.midiNoteOffCallback = [](uint8_t ch, uint8_t note) {
        themis_midi::g_midiOutput.NoteOff(ch, note);
    };
    g_platform.midiClockCallback = []() {
        themis_midi::g_midiOutput.Clock();
    };
    g_platform.midiStartCallback = []() {
        themis_midi::g_midiOutput.Start();
    };
    g_platform.midiStopCallback = []() {
        themis_midi::g_midiOutput.Stop();
    };
#endif
}

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    // Register signal handlers for graceful shutdown
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    // OpenGL settings
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Themis Desktop",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Create OpenGL context
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);  // Enable vsync

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // Note: Removed ImGuiConfigFlags_NavEnableKeyboard to allow spacebar shortcuts
    (void)io;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Initialize platform
    g_desktopPlatform = &g_platform;
    themis::g_platform = &g_platform;
    g_platform.SetSampleRate(48000.0f);

    // Initialize audio
    if (!themis_audio::g_audioEngine.Init(48000)) {
        std::cerr << "Failed to initialize audio engine" << std::endl;
    }

    // Initialize sequencer
    g_sequencer.Init();
    SetupCallbacks();

    // Initialize UI
    themis_ui::g_ui.Init(&g_sequencer);

    // Load settings from config file
    if (themis_config::LoadSettings(g_settings)) {
        std::cout << "Loaded settings from " << themis_config::GetConfigPath() << std::endl;

        // Apply settings to audio engine
        themis_audio::g_audioEngine.SetVolume(g_settings.volume);
        themis_audio::g_audioEngine.SetFilterCutoff(g_settings.filterCutoff);
        themis_audio::g_audioEngine.SetDecayAmount(g_settings.decayAmount);
        themis_audio::g_audioEngine.SetMuted(g_settings.muted);

        // Apply settings to sequencer
        g_sequencer.bpm = g_settings.bpm;
        g_sequencer.patternChangeInterval = g_settings.patternChangeInterval;
        g_sequencer.personalityChangeInterval = g_settings.personalityChangeInterval;

#ifdef THEMIS_ENABLE_MIDI
        // Try to open MIDI port by name first, then by index
        if (!g_settings.midiPortName.empty()) {
            auto ports = themis_midi::g_midiOutput.GetAvailablePorts();
            for (size_t i = 0; i < ports.size(); i++) {
                if (ports[i] == g_settings.midiPortName) {
                    themis_midi::g_midiOutput.OpenPort((int)i);
                    break;
                }
            }
        } else if (g_settings.midiPort >= 0) {
            themis_midi::g_midiOutput.OpenPort(g_settings.midiPort);
        }
#endif

        // Apply mixer mute/solo settings to UI
        for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
            themis_ui::g_ui.drumMute[i] = g_settings.drumMute[i];
            themis_ui::g_ui.drumSolo[i] = g_settings.drumSolo[i];
        }
        themis_ui::g_ui.melodyMute = g_settings.melodyMute;
        themis_ui::g_ui.melodySolo = g_settings.melodySolo;
        themis_ui::g_ui.chordMute = g_settings.chordMute;
        themis_ui::g_ui.chordSolo = g_settings.chordSolo;

        // Apply voice activation settings to sequencer
        g_sequencer.melodyVoice.active = g_settings.melodyActive;
        g_sequencer.melodyChannel = g_settings.melodyChannel;
        g_sequencer.chordVoice.active = g_settings.chordActive;

        // Apply rhythm player settings
        g_sequencer.rhythmVoice.active = g_settings.rhythmActive;
        themis_ui::g_ui.rhythmMute = g_settings.rhythmMute;
        themis_ui::g_ui.rhythmSolo = g_settings.rhythmSolo;
        g_sequencer.rhythmVoice.mode = (themis::RhythmPlayerMode)g_settings.rhythmMode;
        g_sequencer.rhythmVoice.playStyle = (themis::RhythmPlayStyle)(g_settings.rhythmPlayStyle < themis::NUM_RHYTHM_PLAY_STYLES ? g_settings.rhythmPlayStyle : 0);
        g_sequencer.rhythmVoice.midiChannel = g_settings.rhythmMidiChannel;
        g_sequencer.rhythmVoice.octaveOffset = g_settings.rhythmOctaveOffset;
        g_sequencer.rhythmState.padPatternA = g_settings.rhythmPadPatternA < themis::NUM_PAD_PATTERNS ? g_settings.rhythmPadPatternA : 0;
        g_sequencer.rhythmState.padPatternB = g_settings.rhythmPadPatternB < themis::NUM_PAD_PATTERNS ? g_settings.rhythmPadPatternB : 1;
        g_sequencer.rhythmVoice.padVariation.mode = (themis::VariationMode)(g_settings.rhythmPadVarMode < themis::NUM_VARIATION_MODES ? g_settings.rhythmPadVarMode : 0);
        g_sequencer.rhythmVoice.padVariation.sequence = (themis::VariationSequence)(g_settings.rhythmPadVarSequence < themis::NUM_VARIATION_SEQUENCES ? g_settings.rhythmPadVarSequence : 0);

        // Apply bass voice settings
        g_sequencer.bassVoice.active = g_settings.bassActive;
        themis_ui::g_ui.bassMute = g_settings.bassMute;
        themis_ui::g_ui.bassSolo = g_settings.bassSolo;
        g_sequencer.bassVoice.freezePattern = g_settings.bassFreezePattern;
        g_sequencer.bassVoice.fillsEnabled = g_settings.bassFillsEnabled;
        g_sequencer.bassVoice.octaveRandomAmount = g_settings.bassOctaveRandom;
        g_sequencer.bassVoice.midiChannel = g_settings.bassMidiChannel;
        g_sequencer.bassVoice.octaveOffset = g_settings.bassOctaveOffset;
        g_sequencer.bassVoice.rhythmVariation.mode = (themis::VariationMode)(g_settings.bassRhythmVariationMode < themis::NUM_VARIATION_MODES ? g_settings.bassRhythmVariationMode : 0);
        g_sequencer.bassVoice.rhythmVariation.sequence = (themis::VariationSequence)(g_settings.bassRhythmVariationSequence < themis::NUM_VARIATION_SEQUENCES ? g_settings.bassRhythmVariationSequence : 0);
        g_sequencer.bassVoice.pitchVariation.mode = (themis::VariationMode)(g_settings.bassPitchVariationMode < themis::NUM_VARIATION_MODES ? g_settings.bassPitchVariationMode : 0);
        g_sequencer.bassVoice.pitchVariation.sequence = (themis::VariationSequence)(g_settings.bassPitchVariationSequence < themis::NUM_VARIATION_SEQUENCES ? g_settings.bassPitchVariationSequence : 0);

        // Apply chord randomizer settings
        g_sequencer.chordRandomizer.freezeEnabled = g_settings.chordFreezeEnabled;
        g_sequencer.chordRandomizer.enabledVibes = g_settings.chordEnabledVibes;
        for (int i = 0; i < themis::NUM_VIBE_TYPES; i++) {
            g_sequencer.chordRandomizer.enabledProgressions[i] = g_settings.chordEnabledProgressions[i];
        }
        g_sequencer.chordVoice.progressionIndex = g_settings.chordProgressionIndex < themis::NUM_PROGRESSIONS ? g_settings.chordProgressionIndex : 0;
        g_sequencer.chordVoice.chordRate = g_settings.chordRate;
        g_sequencer.chordVoice.octaveOffset = g_settings.chordOctaveOffset;
        g_sequencer.chordVoice.midiChannel = g_settings.chordMidiChannel;
    } else {
        std::cout << "No config file found, using defaults" << std::endl;
    }

    // Main loop
    bool running = true;
    while (running && !g_shouldQuit.load()) {
        // Poll events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }

            // Keyboard shortcuts
            // Note: Space always toggles play, other keys check WantTextInput
            if (event.type == SDL_KEYDOWN) {
                bool wantTextInput = ImGui::GetIO().WantTextInput;
                switch (event.key.keysym.sym) {
                    case SDLK_SPACE:
                        // Space always toggles playback (even when typing)
                        if (g_sequencer.isRunning) {
                            g_sequencer.Stop();
                        } else {
                            g_sequencer.Start();
                        }
                        break;
                    case SDLK_r:
                        if (!wantTextInput) {
                            g_sequencer.RandomizeAll();
                        }
                        break;
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                }
            }
        }

        // Update sequencer timing
        if (g_sequencer.isRunning) {
            uint64_t now = g_platform.GetMicroseconds();
            uint64_t stepInterval = GetStepIntervalUs(g_sequencer.bpm);

            if (now - g_lastStepTime >= stepInterval) {
                g_sequencer.ProcessStep(g_platform.GetSampleRate());
                g_lastStepTime = now;
            }
        } else {
            g_lastStepTime = g_platform.GetMicroseconds();
        }

        // Start ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Render UI
        themis_ui::g_ui.Render();

        // Render ImGui
        ImGui::Render();
        int display_w, display_h;
        SDL_GetWindowSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    if (g_shouldQuit.load()) {
        std::cout << "\nShutting down..." << std::endl;
    }

    // Pause audio FIRST to stop callbacks and prevent mutex deadlock
    // (Stop() triggers note-off callbacks that acquire synthMutex,
    //  which would deadlock if an audio callback is still holding it)
    themis_audio::g_audioEngine.Pause();

    g_sequencer.Stop();

    // Save settings before shutdown
    g_settings.volume = themis_audio::g_audioEngine.GetVolume();
    g_settings.filterCutoff = themis_audio::g_audioEngine.GetFilterCutoff();
    g_settings.decayAmount = themis_audio::g_audioEngine.GetDecayAmount();
    g_settings.muted = themis_audio::g_audioEngine.IsMuted();
    g_settings.bpm = g_sequencer.bpm;
    g_settings.patternChangeInterval = g_sequencer.patternChangeInterval;
    g_settings.personalityChangeInterval = g_sequencer.personalityChangeInterval;
#ifdef THEMIS_ENABLE_MIDI
    if (themis_midi::g_midiOutput.IsOpen()) {
        g_settings.midiPortName = themis_midi::g_midiOutput.GetCurrentPortName();
    }
#endif

    // Save mixer mute/solo settings from UI
    for (int i = 0; i < themis::NUM_DRUM_VOICES; i++) {
        g_settings.drumMute[i] = themis_ui::g_ui.drumMute[i];
        g_settings.drumSolo[i] = themis_ui::g_ui.drumSolo[i];
    }
    g_settings.melodyMute = themis_ui::g_ui.melodyMute;
    g_settings.melodySolo = themis_ui::g_ui.melodySolo;
    g_settings.chordMute = themis_ui::g_ui.chordMute;
    g_settings.chordSolo = themis_ui::g_ui.chordSolo;

    // Save voice activation settings from sequencer
    g_settings.melodyActive = g_sequencer.melodyVoice.active;
    g_settings.melodyChannel = g_sequencer.melodyChannel;
    g_settings.chordActive = g_sequencer.chordVoice.active;

    // Save rhythm player settings
    g_settings.rhythmActive = g_sequencer.rhythmVoice.active;
    g_settings.rhythmMute = themis_ui::g_ui.rhythmMute;
    g_settings.rhythmSolo = themis_ui::g_ui.rhythmSolo;
    g_settings.rhythmMode = g_sequencer.rhythmVoice.mode;
    g_settings.rhythmPlayStyle = g_sequencer.rhythmVoice.playStyle;
    g_settings.rhythmMidiChannel = g_sequencer.rhythmVoice.midiChannel;
    g_settings.rhythmOctaveOffset = g_sequencer.rhythmVoice.octaveOffset;
    g_settings.rhythmPadPatternA = g_sequencer.rhythmState.padPatternA;
    g_settings.rhythmPadPatternB = g_sequencer.rhythmState.padPatternB;
    g_settings.rhythmPadVarMode = g_sequencer.rhythmVoice.padVariation.mode;
    g_settings.rhythmPadVarSequence = g_sequencer.rhythmVoice.padVariation.sequence;

    // Save bass voice settings
    g_settings.bassActive = g_sequencer.bassVoice.active;
    g_settings.bassMute = themis_ui::g_ui.bassMute;
    g_settings.bassSolo = themis_ui::g_ui.bassSolo;
    g_settings.bassFreezePattern = g_sequencer.bassVoice.freezePattern;
    g_settings.bassFillsEnabled = g_sequencer.bassVoice.fillsEnabled;
    g_settings.bassOctaveRandom = g_sequencer.bassVoice.octaveRandomAmount;
    g_settings.bassMidiChannel = g_sequencer.bassVoice.midiChannel;
    g_settings.bassOctaveOffset = g_sequencer.bassVoice.octaveOffset;
    g_settings.bassRhythmVariationMode = g_sequencer.bassVoice.rhythmVariation.mode;
    g_settings.bassRhythmVariationSequence = g_sequencer.bassVoice.rhythmVariation.sequence;
    g_settings.bassPitchVariationMode = g_sequencer.bassVoice.pitchVariation.mode;
    g_settings.bassPitchVariationSequence = g_sequencer.bassVoice.pitchVariation.sequence;

    // Save chord randomizer settings
    g_settings.chordFreezeEnabled = g_sequencer.chordRandomizer.freezeEnabled;
    g_settings.chordEnabledVibes = g_sequencer.chordRandomizer.enabledVibes;
    for (int i = 0; i < themis::NUM_VIBE_TYPES; i++) {
        g_settings.chordEnabledProgressions[i] = g_sequencer.chordRandomizer.enabledProgressions[i];
    }
    g_settings.chordProgressionIndex = g_sequencer.chordVoice.progressionIndex;
    g_settings.chordRate = g_sequencer.chordVoice.chordRate;
    g_settings.chordOctaveOffset = g_sequencer.chordVoice.octaveOffset;
    g_settings.chordMidiChannel = g_sequencer.chordVoice.midiChannel;

    if (themis_config::SaveSettings(g_settings)) {
        std::cout << "Settings saved to " << themis_config::GetConfigPath() << std::endl;
    }

    // Shutdown audio first to release device
    themis_audio::g_audioEngine.Shutdown();

#ifdef THEMIS_ENABLE_MIDI
    // Close MIDI if open
    themis_midi::g_midiOutput.Close();
#endif

    // Shutdown ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    // Cleanup SDL
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
