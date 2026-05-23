#pragma once

#include <stdint.h>
#include "weather.h"

// ===== Page identifiers =====
enum PageId : uint8_t {
    PAGE_MAIN = 0,
    PAGE_WARNING,
    PAGE_MINUTELY,
    PAGE_SYSTEM_INFO,
    PAGE_BRIGHTNESS,
    PAGE_HELP,
};

// ===== Querying =====
PageId getCurrentPage();

// ===== Page transitions =====
void setCurrentPage(PageId page);

// Short-press cycle: main -> warning -> minutely -> system -> main
void pageNext();

// ===== Brightness page =====
void enterBrightnessPage();
void exitBrightnessPage();

// ===== Force warning popup =====
// Saves current page, enters warning, returns true if popup started
bool tryStartForceWarnings();
// Returns true when the current warning times out and should advance
bool isForceWarningExpired();
// Advances to next warning or dismisses if all shown
void advanceOrDismissForceWarnings();
// Force dismiss and return to saved page
void dismissForceWarnings();
