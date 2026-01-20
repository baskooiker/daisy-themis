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
    g_sequencer.onMelodyTrigger = [](int8_t note, bool isMidi) {
        if (isMidi) {
            // Trigger activity indicator
            themis_ui::g_ui.TriggerMelodyMidiActivity();

            // Check mixer solo/mute state
            if (!themis_ui::g_ui.ShouldPlayMelodyMidi()) {
                return;
            }

            // Play internal synth for MIDI melody
            themis_audio::g_audioEngine.TriggerMelodyMidi(note, 100);

            // Also send MIDI out
            uint8_t midiNote = 36 + note;  // C2 = 36
            if (midiNote > 127) midiNote = 127;
            if (themis::g_platform) {
                themis::g_platform->SendMidiNoteOn(0, midiNote, 100);
            }
        } else {
            // Trigger activity indicator
            themis_ui::g_ui.TriggerMelodyCVActivity();

            // Check mixer solo/mute state
            if (!themis_ui::g_ui.ShouldPlayMelodyCV()) {
                return;
            }

            // CV melody - trigger synth for testing
            themis_audio::g_audioEngine.TriggerMelodyCV(note, 100);

            // Update CV output
            float voltage = (float)note / 12.0f;  // 1V/octave
            if (themis::g_platform) {
                themis::g_platform->SetCVOutput(0, voltage);
                themis::g_platform->SetGateOutput(0, true);
            }
        }
    };

    // Melody note off callback
    g_sequencer.onMelodyNoteOff = [](bool isMidi) {
        if (isMidi) {
            themis_audio::g_audioEngine.StopMelodyMidi();
        } else {
            themis_audio::g_audioEngine.StopMelodyCV();
            if (themis::g_platform) {
                themis::g_platform->SetGateOutput(0, false);
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
