/**
 * @file themis_platform.cpp
 * @brief Platform abstraction global instance
 */

#include "themis_platform.h"

namespace themis {

// Global platform instance - must be set by platform-specific code at startup
Platform* g_platform = nullptr;

} // namespace themis
