#include "SiftBackgroundSync.h"

#include <Arduino.h>  // millis
#include <HalGPIO.h>
#include <Logging.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

#include "SiftClient.h"
#include "SiftSync.h"

namespace sift {

namespace {
std::atomic<bool> g_taskRunning{false};
bool g_wasDocked = false;
bool g_syncedThisDock = false;
uint32_t g_lastSyncMs = 0;
constexpr uint32_t RESYNC_INTERVAL_MS = 30u * 60u * 1000u;  // re-pull every 30 min while docked

void syncTask(void*) {
  sift::sync::syncAll(nullptr, nullptr);  // network + SD only; safe off the UI thread
  g_taskRunning.store(false);
  vTaskDelete(nullptr);
}
}  // namespace

void backgroundSyncTick() {
  const bool docked = gpio.isUsbConnected();
  if (!docked) {
    g_wasDocked = false;  // re-arm: next dock triggers a fresh sync
    return;
  }
  if (!g_wasDocked) {
    g_wasDocked = true;
    g_syncedThisDock = false;  // just docked
  }
  if (g_taskRunning.load() || !sift::configured()) return;
  // Wait for Wi-Fi before doing any TLS; retry on later ticks until it's up.
  if (WiFi.status() != WL_CONNECTED) return;

  const uint32_t now = millis();
  const bool due = !g_syncedThisDock || (now - g_lastSyncMs) >= RESYNC_INTERVAL_MS;
  if (!due) return;

  g_syncedThisDock = true;
  g_lastSyncMs = now;
  g_taskRunning.store(true);
  sift::netEnsureInit();  // create the net mutex on this thread before the task can race for it
  TaskHandle_t handle = nullptr;
  // 16 KB: a TLS handshake + JSON parse overflows the old 8 KB stack (which
  // corrupted a mutex and tripped the xQueueSemaphoreTake assert).
  if (xTaskCreate(syncTask, "sift-sync", 16384, nullptr, 1, &handle) != pdPASS) {
    g_taskRunning.store(false);
    LOG_ERR("SIFT", "failed to start background sync task");
  } else {
    LOG_INF("SIFT", "docked — background sync started");
  }
}

}  // namespace sift
