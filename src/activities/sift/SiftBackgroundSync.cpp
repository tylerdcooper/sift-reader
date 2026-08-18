#include "SiftBackgroundSync.h"

#include <Arduino.h>  // millis
#include <Logging.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <string>

#include "SiftClient.h"
#include "SiftSync.h"
#include "WifiCredentialStore.h"

namespace sift {

namespace {
enum class St { Idle, Connecting, Syncing, Done };
St g_state = St::Idle;
std::atomic<bool> g_taskRunning{false};
bool g_requested = false;   // true while the user is in Sift
bool g_weConnected = false;
bool g_credsLoaded = false;
bool g_syncStarted = false;
uint32_t g_connectStart = 0;
uint32_t g_lastSync = 0;

constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t RESYNC_MS = 5u * 60u * 1000u;  // re-sync every 5 min while open

void syncTask(void*) {
  sift::sync::syncQueue();  // network + SD only; safe off the UI thread
  g_taskRunning.store(false);
  vTaskDelete(nullptr);
}
}  // namespace

bool bgBusy() { return g_requested && (g_state == St::Connecting || g_state == St::Syncing); }

void requestSync() {
  g_requested = true;
  if (g_state == St::Done) g_state = St::Idle;  // allow an immediate re-sync on re-entry
}

void endSyncSession() {
  g_requested = false;
  if (g_weConnected) {
    WiFi.disconnect(true);  // save battery; Wi-Fi is only up while in Sift
    g_weConnected = false;
  }
  g_state = St::Idle;
  g_syncStarted = false;
}

void backgroundTick() {
  if (!g_requested || !sift::configured()) return;

  switch (g_state) {
    case St::Idle: {
      if (!g_credsLoaded) {
        WIFI_STORE.loadFromFile();
        g_credsLoaded = true;
      }
      if (g_lastSync != 0 && millis() - g_lastSync < RESYNC_MS) return;
      if (WiFi.status() == WL_CONNECTED) {
        g_state = St::Syncing;
        break;
      }
      const std::string ssid = WIFI_STORE.getLastConnectedSsid();
      if (ssid.empty()) return;
      const auto cred = WIFI_STORE.findCredential(ssid);
      if (!cred) return;
      sift::netEnsureInit();
      WiFi.mode(WIFI_STA);
      WiFi.begin(cred->ssid.c_str(), cred->password.c_str());
      g_weConnected = true;
      g_connectStart = millis();
      g_state = St::Connecting;
      LOG_INF("SIFT", "connecting Wi-Fi to sync the queue");
      break;
    }
    case St::Connecting: {
      if (WiFi.status() == WL_CONNECTED) {
        g_state = St::Syncing;
      } else if (millis() - g_connectStart > CONNECT_TIMEOUT_MS) {
        g_lastSync = millis();  // back off before retrying
        g_state = St::Done;
      }
      break;
    }
    case St::Syncing: {
      if (!g_syncStarted) {
        g_syncStarted = true;
        g_taskRunning.store(true);
        TaskHandle_t h = nullptr;
        if (xTaskCreate(syncTask, "sift-sync", 16384, nullptr, 1, &h) != pdPASS) {
          g_taskRunning.store(false);
          g_syncStarted = false;
          g_lastSync = millis();
          g_state = St::Done;
        }
      } else if (!g_taskRunning.load()) {
        g_syncStarted = false;
        g_lastSync = millis();
        g_state = St::Done;  // stay connected while in Sift so mark-read works
      }
      break;
    }
    case St::Done:
      break;  // requestSync() resets to Idle for the periodic re-sync
  }
}

}  // namespace sift
