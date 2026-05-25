#pragma once

void startConfigServer();
void handleConfigClient();

// Periodic remote OTA check (call from network task)
void periodicCheckUpdate();
