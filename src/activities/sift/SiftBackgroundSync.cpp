#include "SiftBackgroundSync.h"

#include <Arduino.h>  // millis
#include <HalGPIO.h>
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
bool g_weConnected = false;   // this service brought Wi-Fi up
bool g_credsLoaded = false;
bool g_syncStarted = false;
uint32_t g_connectStart = 0;
uint32_t g_lastAttempt = 0;

constexpr uint32_t CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t RESYNC_MS = 15u * 60u * 1000u;  // re-sync every 15 min while docked
constexpr uint32_t RETRY_MS = 60u * 1000u;         // back off after a failed attempt

void syncTask(void*) {
  sift::sync::syncQueue();  // network + SD only; safe off the UI thread
  g_taskRunning.store(false);
  vTaskDelete(nullptr);
}
}  // namespace

bool bgBusy() { return g_state == St::Connecting || g_state == St::Syncing; }

void backgroundTick() {
  if (!gpio.isUsbConnected()) {
    if (g_weConnected) {
      WiFi.disconnect(true);
      g_weConnected = false;
    }
    g_state = St::Idle;
    g_syncStarted = false;
    return;
  }
  if (!sift::configured()) return;

  switch (g_state) {
    case St::Idle: {
      if (!g_credsLoaded) {
        WIFI_STORE.loadFromFile();
        g_credsLoaded = true;
      }
      if (g_lastAttempt != 0 && millis() - g_lastAttempt < RETRY_MS) return;
      if (WiFi.status() == WL_CONNECTED) {  // already up (e.g. user opened Sift)
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
      LOG_INF("SIFT", "docked — connecting Wi-Fi for background sync");
      break;
    }
    case St::Connecting: {
      if (WiFi.status() == WL_CONNECTED) {
        g_state = St::Syncing;
      } else if (millis() - g_connectStart > CONNECT_TIMEOUT_MS) {
        g_lastAttempt = millis();
        g_state = St::Idle;
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
          g_lastAttempt = millis();
          g_state = St::Idle;
        }
      } else if (!g_taskRunning.load()) {
        g_syncStarted = false;
        g_lastAttempt = millis();
        g_state = St::Done;  // Wi-Fi stays up while docked so mark-read works
      }
      break;
    }
    case St::Done: {
      if (millis() - g_lastAttempt > RESYNC_MS) g_state = St::Idle;
      break;
    }
  }
}

}  // namespace sift
