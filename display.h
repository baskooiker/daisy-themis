/**
 * @file display.h
 * @brief OLED display rendering for Themis
 *
 * This module handles:
 * - Default display (BPM, mode, melody info)
 * - Config menu display
 * - Config edit display
 * - Pattern info display
 */

#ifndef THEMIS_DISPLAY_H
#define THEMIS_DISPLAY_H

#include "types.h"
#include "globals.h"

/**
 * @brief Update the OLED display based on current state
 *
 * Renders the appropriate display based on currentDisplayState:
 * - DISPLAY_DEFAULT: BPM, mode, key, voice info
 * - DISPLAY_CONFIG_MENU: Scrollable config menu
 * - DISPLAY_CONFIG_EDIT: Config value editing
 * - DISPLAY_PATTERN_INFO: Pattern details with scroll
 */
void UpdateDisplay();

#endif // THEMIS_DISPLAY_H
