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
bool g_requested = false;      // a sync is wanted
bool g_keepConnected = false;  // true = Sift session (stay up for mark-read); false = one-shot wake
bool g_weConnected = false;
bool g_credsLoaded = false;
bool g_syncStarted = false;
uint32_t g_connectStart = 0;
uint32_t g_lastSync = 0;

constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t RESYNC_MS = 5u * 60u * 1000u;  // don't re-sync more often than this

void syncTask(void*) {
  sift::sync::syncQueue();  // network + SD only; safe off the UI thread
  g_taskRunning.store(false);
  vTaskDelete(nullptr);
}

bool isDue() { return g_lastSync == 0 || millis() - g_lastSync >= RESYNC_MS; }

void teardown() {
  if (g_weConnected) {
    WiFi.disconnect(true);
    g_weConnected = false;
  }
  g_requested = false;
  g_keepConnected = false;
  g_syncStarted = false;
  g_state = St::Idle;
}
}  // namespace

bool bgBusy() { return g_requested && (g_state == St::Connecting || g_state == St::Syncing); }

void syncOnWake() {
  g_requested = true;
  g_keepConnected = false;  // one-shot: disconnect after
  if (g_state == St::Done) g_state = St::Idle;
}

void requestSync() {
  g_requested = true;
  g_keepConnected = true;  // Sift session: stay connected for mark-read
  if (g_state == St::Done) g_state = St::Idle;
}

void endSyncSession() { teardown(); }

void backgroundTick() {
  if (!g_requested || !sift::configured()) return;

  switch (g_state) {
    case St::Idle: {
      if (!g_credsLoaded) {
        WIFI_STORE.loadFromFile();
        g_credsLoaded = true;
      }
      if (WiFi.status() == WL_CONNECTED) {
        g_state = isDue() ? St::Syncing : St::Done;
        break;
      }
      // One-shot with nothing due to do: stop. (A session still connects so
      // mark-read works.)
      if (!g_keepConnected && !isDue()) {
        g_requested = false;
        return;
      }
      const std::string ssid = WIFI_STORE.getLastConnectedSsid();
      if (ssid.empty()) { g_requested = false; return; }
      const auto cred = WIFI_STORE.findCredential(ssid);
      if (!cred) { g_requested = false; return; }
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
        g_state = isDue() ? St::Syncing : St::Done;
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
        g_state = St::Done;
      }
      break;
    }
    case St::Done: {
      // One-shot wake sync: disconnect and stop. A Sift session stays connected
      // (for mark-read) until endSyncSession().
      if (!g_keepConnected) teardown();
      break;
    }
  }
}

}  // namespace sift
