#include "SiftCache.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

#include <cstdlib>

namespace sift {
namespace cache {

namespace {
const char* const BASE = "/.crosspoint/sift";
const char* const IMG_DIR = "/.crosspoint/sift/img";

std::string feedsPath() { return std::string(BASE) + "/feeds.json"; }
std::string articlesPath(const std::string& feed) { return std::string(BASE) + "/list_" + feed + ".json"; }
std::string articlePath(int id) { return std::string(BASE) + "/article_" + std::to_string(id) + ".json"; }
std::string syncedPath() { return std::string(BASE) + "/synced"; }

std::string readAll(const std::string& path) {
  HalFile f;
  if (!Storage.openFileForRead("SIFTCACHE", path, f)) return "";
  const size_t n = f.size();
  if (n == 0) return "";
  std::string s;
  s.resize(n);
  if (f.read(&s[0], n) != static_cast<int>(n)) return "";
  return s;
}

bool writeAll(const std::string& path, const String& content) {
  ensureDirs();
  return Storage.writeFile(path.c_str(), content);
}
}  // namespace

void ensureDirs() {
  Storage.ensureDirectoryExists(BASE);
  Storage.ensureDirectoryExists(IMG_DIR);
}

bool saveFeeds(int totalUnread, const std::vector<Feed>& feeds) {
  JsonDocument doc;
  doc["totalUnread"] = totalUnread;
  JsonArray arr = doc["feeds"].to<JsonArray>();
  for (const auto& f : feeds) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = f.id;
    o["title"] = f.title;
    o["unread"] = f.unread;
  }
  String out;
  serializeJson(doc, out);
  return writeAll(feedsPath(), out);
}

bool loadFeeds(int& totalUnread, std::vector<Feed>& out) {
  const std::string body = readAll(feedsPath());
  if (body.empty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) return false;
  totalUnread = doc["totalUnread"] | 0;
  for (JsonObject f : doc["feeds"].as<JsonArray>()) {
    out.push_back({f["id"] | 0, std::string(f["title"] | ""), f["unread"] | 0});
  }
  return true;
}

bool saveArticles(const std::string& feed, const std::vector<ArticleMeta>& articles) {
  JsonDocument doc;
  JsonArray arr = doc["articles"].to<JsonArray>();
  for (const auto& a : articles) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = a.id;
    o["title"] = a.title;
    o["feed"] = a.feed;
    o["date"] = a.date;
    o["excerpt"] = a.excerpt;
  }
  String out;
  serializeJson(doc, out);
  return writeAll(articlesPath(feed), out);
}

bool loadArticles(const std::string& feed, std::vector<ArticleMeta>& out) {
  const std::string body = readAll(articlesPath(feed));
  if (body.empty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) return false;
  for (JsonObject a : doc["articles"].as<JsonArray>()) {
    out.push_back({a["id"] | 0, std::string(a["title"] | ""), std::string(a["feed"] | ""),
                   std::string(a["date"] | ""), std::string(a["excerpt"] | "")});
  }
  return true;
}

bool saveArticle(const ArticleFull& a) {
  JsonDocument doc;
  doc["id"] = a.id;
  doc["title"] = a.title;
  doc["feed"] = a.feed;
  doc["date"] = a.date;
  doc["text"] = a.text;
  doc["image"] = a.image;
  doc["hasImage"] = a.hasImage;
  String out;
  serializeJson(doc, out);
  return writeAll(articlePath(a.id), out);
}

bool loadArticle(int id, ArticleFull& out) {
  const std::string body = readAll(articlePath(id));
  if (body.empty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) return false;
  out.id = id;
  out.title = std::string(doc["title"] | "");
  out.feed = std::string(doc["feed"] | "");
  out.date = std::string(doc["date"] | "");
  out.text = std::string(doc["text"] | "");
  out.image = std::string(doc["image"] | "");
  out.hasImage = doc["hasImage"] | false;
  return true;
}

std::string imagePath(int id) { return std::string(IMG_DIR) + "/" + std::to_string(id) + ".png"; }

bool hasImage(int id) {
  const std::string p = imagePath(id);
  return Storage.exists(p.c_str());
}

uint32_t lastSyncEpoch() {
  const std::string s = readAll(syncedPath());
  if (s.empty()) return 0;
  return static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 10));
}

void setLastSyncEpoch(uint32_t epoch) { writeAll(syncedPath(), String(static_cast<unsigned long>(epoch))); }

}  // namespace cache
}  // namespace sift
