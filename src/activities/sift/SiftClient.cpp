#include "SiftClient.h"

#include <ArduinoJson.h>
#include <Logging.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "SiftCache.h"
#include "SiftConfigStore.h"
#include "network/HttpDownloader.h"

namespace sift {

// Function-local static: C++11 guarantees the mutex is created exactly once,
// even under concurrent first-use. netEnsureInit() lets the main thread force
// that creation up front.
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

std::string imageUrl(int id, int width) {
  if (!configured()) return "";
  // fmt=png: server pre-dithers to the native gray codes; the device renders it
  // without re-dithering.
  return baseUrl() + "/api/device/image/" + std::to_string(id) + "?w=" + std::to_string(width) +
         "&fmt=png&token=" + token();
}

// GET baseUrl + path (+ token) and parse the JSON body. path may already carry a
// query string; the token is appended with the right separator.
static bool getJson(const std::string& path, JsonDocument& doc) {
  if (!configured()) {
    LOG_ERR("SIFT", "not configured");
    return false;
  }
  std::string url = baseUrl() + path;
  url += (path.find('?') == std::string::npos) ? "?token=" : "&token=";
  url += token();

  std::string body;
  {
    NetGuard guard;  // serialize with the background sync task
    if (!HttpDownloader::fetchUrl(url, body)) {
      LOG_ERR("SIFT", "fetch failed");
      return false;
    }
  }
  const DeserializationError err = deserializeJson(doc, body.c_str());
  if (err) {
    LOG_ERR("SIFT", "json parse: %s", err.c_str());
    return false;
  }
  return true;
}

// Each fetch is cache-through: try the network, write the cache on success, and
// fall back to the cache when the device is offline. So the reader keeps working
// once a sync (or a prior visit) has populated the cache.

bool fetchFeeds(int& totalUnread, std::vector<Feed>& out) {
  JsonDocument doc;
  if (getJson("/api/device/feeds", doc)) {
    totalUnread = doc["totalUnread"] | 0;
    for (JsonObject f : doc["feeds"].as<JsonArray>()) {
      out.push_back({f["id"] | 0, std::string(f["title"] | ""), f["unread"] | 0});
    }
    cache::saveFeeds(totalUnread, out);
    return true;
  }
  out.clear();
  return cache::loadFeeds(totalUnread, out);
}

bool fetchArticles(const std::string& feed, std::vector<ArticleMeta>& out) {
  JsonDocument doc;
  if (getJson("/api/device/articles?feed=" + feed + "&limit=100", doc)) {
    for (JsonObject a : doc["articles"].as<JsonArray>()) {
      out.push_back({a["id"] | 0, std::string(a["title"] | ""), std::string(a["feed"] | ""),
                     std::string(a["date"] | ""), std::string(a["excerpt"] | "")});
    }
    cache::saveArticles(feed, out);
    return true;
  }
  out.clear();
  return cache::loadArticles(feed, out);
}

bool fetchArticle(int id, ArticleFull& out) {
  JsonDocument doc;
  if (getJson("/api/device/article/" + std::to_string(id), doc)) {
    out.id = id;
    out.title = std::string(doc["title"] | "");
    out.feed = std::string(doc["feed"] | "");
    out.date = std::string(doc["date"] | "");
    out.text = std::string(doc["text"] | "");
    out.image = std::string(doc["image"] | "");
    out.hasImage = doc["hasImage"] | false;
    cache::saveArticle(out);
    return true;
  }
  return cache::loadArticle(id, out);
}

}  // namespace sift
