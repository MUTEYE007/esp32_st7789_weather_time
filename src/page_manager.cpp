#include "page_manager.h"
#include "config.h"

// ===== Querying =====
PageId getCurrentPage() {
    if (state.showingWarning) return PAGE_WARNING;
    if (state.showingMinutely) return PAGE_MINUTELY;
    if (state.showingSystemInfo) return PAGE_SYSTEM_INFO;
    return PAGE_MAIN;
}

// ===== Page transitions =====
void setCurrentPage(PageId page) {
    state.showingWarning    = (page == PAGE_WARNING);
    state.showingMinutely   = (page == PAGE_MINUTELY);
    state.showingSystemInfo = (page == PAGE_SYSTEM_INFO);
}

void pageNext() {
    if (state.showingWarning) {
        if (state.forceWarnActive) {
            // Force warning: advance or dismiss
            advanceOrDismissForceWarnings();
            return;
        }
        if (warningCount > 1 && state.warningIndex < warningCount - 1) {
            // Manual next warning
            state.warningIndex++;
            return;
        }
        // Warning → Minutely
        setCurrentPage(PAGE_MINUTELY);
        return;
    }

    switch (getCurrentPage()) {
        case PAGE_MINUTELY:
            setCurrentPage(PAGE_SYSTEM_INFO);
            state.systemInfoDirty = true;
            break;
        case PAGE_SYSTEM_INFO:
            setCurrentPage(PAGE_MAIN);
            break;
        default: // PAGE_MAIN → Warning
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
