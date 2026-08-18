#include "SiftClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "SiftConfigStore.h"
#include "network/HttpDownloader.h"

namespace sift {

static SemaphoreHandle_t siftNetMutex() {
  static SemaphoreHandle_t m = xSemaphoreCreateMutex();
  return m;
}
void netEnsureInit() { (void)siftNetMutex(); }
NetGuard::NetGuard() {
  SemaphoreHandle_t m = siftNetMutex();
  if (m) xSemaphoreTake(m, portMAX_DELAY);
}
NetGuard::~NetGuard() {
  SemaphoreHandle_t m = siftNetMutex();
  if (m) xSemaphoreGive(m);
}

std::string baseUrl() {
  if (SIFT_CONFIG.hasConfig()) return SIFT_CONFIG.getBaseUrl();
#ifdef SIFT_SIM_URL
  return SIFT_SIM_URL;
#else
  return "";
#endif
}

std::string token() {
  if (SIFT_CONFIG.hasConfig()) return SIFT_CONFIG.getToken();
#ifdef SIFT_SIM_TOKEN
  return SIFT_SIM_TOKEN;
#else
  return "";
#endif
}

bool configured() { return !baseUrl().empty() && !token().empty(); }

std::string imageUrl(int id, int n, int width) {
  if (!configured()) return "";
  return baseUrl() + "/api/device/image/" + std::to_string(id) + "?n=" + std::to_string(n) +
         "&w=" + std::to_string(width) + "&token=" + token();
}

// GET baseUrl+path (+token), Wi-Fi- and mutex-guarded, returning the body.
static bool getBody(const std::string& path, std::string& body) {
  if (!configured()) {
    LOG_ERR("SIFT", "not configured");
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    LOG_ERR("SIFT", "wifi not connected");
    return false;
  }
  std::string url = baseUrl() + path;
  url += (path.find('?') == std::string::npos) ? "?token=" : "&token=";
  url += token();
  NetGuard guard;
  if (!HttpDownloader::fetchUrl(url, body)) {
    LOG_ERR("SIFT", "fetch failed");
    return false;
  }
  return true;
}

bool fetchQueue(std::vector<QueueItem>& out) {
  std::string body;
  if (!getBody("/api/device/queue", body)) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) {
    LOG_ERR("SIFT", "queue parse failed");
    return false;
  }
  for (JsonObject a : doc["articles"].as<JsonArray>()) {
    QueueItem item;
    item.id = a["id"] | 0;
    item.title = std::string(a["title"] | "");
    item.feed = std::string(a["feed"] | "");
    item.date = std::string(a["date"] | "");
    item.images = a["images"] | 0;
    for (JsonObject b : a["blocks"].as<JsonArray>()) {
      Block blk;
      if (std::string(b["type"] | "") == "image") {
        blk.image = true;
        blk.n = b["n"] | 0;
      } else {
        blk.image = false;
        blk.text = std::string(b["text"] | "");
      }
      item.blocks.push_back(std::move(blk));
    }
    out.push_back(std::move(item));
  }
  return true;
}

bool markRead(int id) {
  std::string body;
  return getBody("/api/device/read?id=" + std::to_string(id), body);
}

}  // namespace sift
