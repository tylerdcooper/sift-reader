#include "SiftCache.h"

#include <ArduinoJson.h>
#include <HalStorage.h>
#include <Logging.h>

namespace sift {
namespace cache {

namespace {
const char* const BASE = "/.crosspoint/sift";
const char* const IMG_DIR = "/.crosspoint/sift/img";

std::string listPath() { return std::string(BASE) + "/queue.json"; }
std::string articlePath(int id) { return std::string(BASE) + "/a_" + std::to_string(id) + ".json"; }

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

std::string imagePath(int id, int n) {
  return std::string(IMG_DIR) + "/" + std::to_string(id) + "_" + std::to_string(n) + ".jpg";
}

bool hasImage(int id, int n) {
  const std::string p = imagePath(id, n);
  return Storage.exists(p.c_str());
}

bool saveList(const std::vector<QueueItem>& items) {
  JsonDocument doc;
  JsonArray arr = doc["items"].to<JsonArray>();
  for (const auto& it : items) {
    JsonObject o = arr.add<JsonObject>();
    o["id"] = it.id;
    o["title"] = it.title;
    o["feed"] = it.feed;
    o["date"] = it.date;
    o["images"] = it.images;
  }
  String out;
  serializeJson(doc, out);
  return writeAll(listPath(), out);
}

bool loadList(std::vector<ListItem>& out) {
  const std::string body = readAll(listPath());
  if (body.empty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) return false;
  for (JsonObject o : doc["items"].as<JsonArray>()) {
    ListItem it;
    it.id = o["id"] | 0;
    it.title = std::string(o["title"] | "");
    it.feed = std::string(o["feed"] | "");
    it.date = std::string(o["date"] | "");
    it.images = o["images"] | 0;
    out.push_back(std::move(it));
  }
  return true;
}

bool saveArticle(const QueueItem& item) {
  JsonDocument doc;
  doc["id"] = item.id;
  doc["title"] = item.title;
  doc["feed"] = item.feed;
  doc["date"] = item.date;
  doc["images"] = item.images;
  JsonArray arr = doc["blocks"].to<JsonArray>();
  for (const auto& b : item.blocks) {
    JsonObject o = arr.add<JsonObject>();
    if (b.image) {
      o["image"] = true;
      o["n"] = b.n;
    } else {
      o["image"] = false;
      o["text"] = b.text;
    }
  }
  String out;
  serializeJson(doc, out);
  return writeAll(articlePath(item.id), out);
}

bool loadArticle(int id, QueueItem& out) {
  const std::string body = readAll(articlePath(id));
  if (body.empty()) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body.c_str())) return false;
  out.id = id;
  out.title = std::string(doc["title"] | "");
  out.feed = std::string(doc["feed"] | "");
  out.date = std::string(doc["date"] | "");
  out.images = doc["images"] | 0;
  for (JsonObject o : doc["blocks"].as<JsonArray>()) {
    Block b;
    b.image = o["image"] | false;
    if (b.image) {
      b.n = o["n"] | 0;
    } else {
      b.text = std::string(o["text"] | "");
    }
    out.blocks.push_back(std::move(b));
  }
  return true;
}

void removeArticle(int id, int imageCount) {
  const std::string ap = articlePath(id);
  if (Storage.exists(ap.c_str())) Storage.remove(ap.c_str());
  // Remove images (plus a couple past the count in case it shrank).
  for (int n = 0; n < imageCount + 2; ++n) {
    const std::string ip = imagePath(id, n);
    const std::string pxc = ip.substr(0, ip.rfind('.')) + ".pxc";
    if (Storage.exists(ip.c_str())) Storage.remove(ip.c_str());
    if (Storage.exists(pxc.c_str())) Storage.remove(pxc.c_str());
  }
  // Drop it from the cached list too, so the saved screen is correct even before
  // the next sync. (syncQueue overwrites the list afterward, so this is a no-op
  // there.)
  std::vector<ListItem> list;
  if (loadList(list)) {
    std::vector<QueueItem> kept;
    for (const auto& it : list) {
      if (it.id == id) continue;
      QueueItem q;
      q.id = it.id;
      q.title = it.title;
      q.feed = it.feed;
      q.date = it.date;
      q.images = it.images;
      kept.push_back(std::move(q));
    }
    saveList(kept);
  }
}

}  // namespace cache
}  // namespace sift
