#pragma once

void startConfigServer();
void handleConfigClient();

// Periodic remote OTA check (call from network task)
void periodicCheckUpdate();

// Emergency firmware check (call from network task, ~30s interval)
void emergencyCheckUpdate();
