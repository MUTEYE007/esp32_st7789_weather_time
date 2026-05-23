#include "page_manager.h"
#include "config.h"

static PageId savedBeforeDim = PAGE_MAIN;

// ===== Querying =====
PageId getCurrentPage() {
    if (state.dimmingActive) return PAGE_BRIGHTNESS;
    if (state.showingWarning) return PAGE_WARNING;
    if (state.showingMinutely) return PAGE_MINUTELY;
    if (state.showingSystemInfo) return PAGE_SYSTEM_INFO;
    if (state.showingHelp) return PAGE_HELP;
    return PAGE_MAIN;
}

// ===== Brightness page =====
void enterBrightnessPage() {
    savedBeforeDim = getCurrentPage();
    state.dimmingActive = true;
    state.showingWarning = false;
    state.showingMinutely = false;
    state.showingSystemInfo = false;
    state.showingHelp = false;
}

void exitBrightnessPage() {
    state.dimmingActive = false;
    setCurrentPage(savedBeforeDim);
}

// ===== Page transitions =====
void setCurrentPage(PageId page) {
    state.showingWarning    = (page == PAGE_WARNING);
    state.showingMinutely   = (page == PAGE_MINUTELY);
    state.showingSystemInfo = (page == PAGE_SYSTEM_INFO);
    state.showingHelp       = (page == PAGE_HELP);
}

void pageNext() {
    if (state.showingWarning) {
        if (state.forceWarnActive) {
            advanceOrDismissForceWarnings();
            return;
        }
        if (warningCount > 1 && state.warningIndex < warningCount - 1) {
            state.warningIndex++;
            return;
        }
        setCurrentPage(PAGE_MINUTELY);
        return;
    }

    switch (getCurrentPage()) {
        case PAGE_MINUTELY:
            setCurrentPage(PAGE_SYSTEM_INFO);
            state.systemInfoDirty = true;
            break;
        case PAGE_SYSTEM_INFO:
            setCurrentPage(PAGE_HELP);
            break;
        case PAGE_HELP:
            setCurrentPage(PAGE_MAIN);
            break;
        default:
            state.warningIndex = 0;
            setCurrentPage(PAGE_WARNING);
            break;
    }
}

// ===== Force warning popup =====
bool tryStartForceWarnings() {
    if (state.provisioningMode) return false;
    if (getCurrentPage() == PAGE_WARNING) return false;
    if (warningCount <= state.forceWarnShownCount) return false;
    if (warningCount <= 0) return false;

    state.forceWarnActive = true;
    state.forceWarnStartMs = millis();
    state.savedShowingMinutely = state.showingMinutely;
    state.savedShowingSystemInfo = state.showingSystemInfo;
    state.warningIndex = state.forceWarnShownCount;
    setCurrentPage(PAGE_WARNING);
    return true;
}

bool isForceWarningExpired() {
    if (!state.forceWarnActive) return false;
    return (millis() - state.forceWarnStartMs) >= 30000;
}

void advanceOrDismissForceWarnings() {
    state.forceWarnShownCount++;
    if (state.forceWarnShownCount >= warningCount) {
        dismissForceWarnings();
    } else {
        state.warningIndex = state.forceWarnShownCount;
        state.forceWarnStartMs = millis();
    }
}

void dismissForceWarnings() {
    state.forceWarnActive = false;
    state.showingWarning = false;
    state.showingMinutely = state.savedShowingMinutely;
    state.showingSystemInfo = state.savedShowingSystemInfo;
}
