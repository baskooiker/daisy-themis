/**
 * @file ui_internal.h
 * @brief Internal header for UI implementation files
 *
 * This header is included by all ui_*.cpp files to share common includes.
 */

#ifndef THEMIS_UI_INTERNAL_H
#define THEMIS_UI_INTERNAL_H

#include "ui.h"
#include "platform_desktop.h"
#include "themis_data.h"
#include "themis_patterns.h"
#include "themis_chords.h"
#include "themis_acid.h"
#include "audio.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <functional>
#include <SDL2/SDL.h>

#ifdef THEMIS_ENABLE_MIDI
#include "midi_out.h"
#endif

#endif // THEMIS_UI_INTERNAL_H
