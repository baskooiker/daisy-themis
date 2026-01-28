/**
 * @file ui.h
 * @brief ImGui user interface for Themis desktop
 */

#ifndef THEMIS_UI_H
#define THEMIS_UI_H

#include "themis_sequencer.h"
#include "themis_types.h"
#include "audio.h"

namespace themis_ui {

/**
 * @class ThemisUI
 * @brief Main UI class for Themis desktop application
 */
class ThemisUI {
public:
    /**
     * @brief Initialize UI state
     * @param seq Pointer to sequencer
     */
    void Init(themis::Sequencer* seq);

    /**
     * @brief Render the main UI
     *
     * Call this every frame after ImGui::NewFrame()
     */
    void Render();

    // Mixer state - accessible for audio callback filtering
    bool drumMute[themis::NUM_DRUM_VOICES] = {false};
    bool drumSolo[themis::NUM_DRUM_VOICES] = {false};
    bool melodyCVMute = false;
    bool melodyCVSolo = false;
    bool melodyMidiMute = false;
    bool melodyMidiSolo = false;
    bool polyMute = false;
    bool polySolo = false;
    bool rhythmMute = false;
    bool rhythmSolo = false;
    bool acidMute = false;
    bool acidSolo = false;

    // Check if any solo is active
    bool IsAnySoloActive() const;

    // Check if a drum voice should sound (considering mute/solo)
    bool ShouldPlayDrum(themis::DrumVoice voice) const;

    // Check if melody should sound
    bool ShouldPlayMelodyCV() const;
    bool ShouldPlayMelodyMidi() const;

    // Check if poly voice should sound
    bool ShouldPlayPoly() const;

    // Check if rhythm player should sound
    bool ShouldPlayRhythm() const;

    // Check if acid voice should sound
    bool ShouldPlayAcid() const;

    // Activity triggers (call from audio callbacks)
    void TriggerDrumActivity(themis::DrumVoice voice);
    void TriggerMelodyCVActivity();
    void TriggerMelodyMidiActivity();
    void TriggerPolyActivity();
    void TriggerRhythmActivity();
    void TriggerAcidActivity();

private:
    themis::Sequencer* sequencer = nullptr;

    // UI state
    int selectedMidiPort = -1;
    bool showDemoWindow = false;

    // Trigger activity indicators (for visual feedback)
    float drumActivity[themis::NUM_DRUM_VOICES] = {0.0f};
    float melodyCVActivity = 0.0f;
    float melodyMidiActivity = 0.0f;
    float polyActivity = 0.0f;
    float rhythmActivity = 0.0f;
    float acidActivity = 0.0f;

    // UI sections
    void RenderTransportControls();
    void RenderVoicesAndPatterns();  // Combined view
    void RenderDrumVoices();
    void RenderMelodyVoices();
    void RenderGlobalControls();
    void RenderSynthParams();        // Per-voice synth parameter controls
    void RenderOutputSection();
    void RenderPatternVisualization();
    void RenderMixer();
    void RenderChordsTab();          // Chord vibe randomization controls

    // Helper functions
    void RenderVoicePanel(themis::VoiceConfig* voice, int index);
    void RenderMelodyPanel(themis::MelodyConfig* voice, const char* name, bool isMidi);
    void RenderVariationConfig(themis::VariationConfig* config, const char* id);
    void RenderCompactVoiceRow(themis::VoiceConfig* voice, int index);
    void RenderCompactMelodyRow(themis::MelodyConfig* voice, const char* name);

    const char* GetRhythmStyleName(themis::RhythmStyle style);
    const char* GetDensityName(themis::DensityLevel density);
    const char* GetInteractionStyleName(themis::InteractionStyle style);
    const char* GetMelodyStyleName(themis::MelodyStyle style);
    const char* GetScaleName(themis::ScaleType scale);
    const char* GetVariationModeName(themis::VariationMode mode);
    const char* GetVariationSequenceName(themis::VariationSequence seq);
};

// Global UI instance
extern ThemisUI g_ui;

} // namespace themis_ui

#endif // THEMIS_UI_H
