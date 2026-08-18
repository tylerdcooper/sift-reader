#include "SiftSync.h"

#include <Logging.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "SiftCache.h"
#include "SiftClient.h"
#include "network/HttpDownloader.h"

namespace sift {
namespace sync {

// The X4 Pro's short side minus margins; the server normalizes each image to
// this width so the device dithers a panel-fit picture.
static constexpr int IMG_W = 440;

int syncQueue() {
  if (!sift::configured() || WiFi.status() != WL_CONNECTED) return -1;
  cache::ensureDirs();

  // Remember what we had, to prune anything dropped from the queue.
  std::vector<cache::ListItem> previous;
  cache::loadList(previous);

  std::vector<QueueItem> items;
  if (!fetchQueue(items)) return -1;

  for (const auto& item : items) {
    cache::saveArticle(item);
    for (const auto& b : item.blocks) {
      if (!b.image || cache::hasImage(item.id, b.n)) continue;
      const std::string url = sift::imageUrl(item.id, b.n, IMG_W);
      if (url.empty()) continue;
      NetGuard guard;
      HttpDownloader::downloadToFile(url, cache::imagePath(item.id, b.n));
    }
  }

  cache::saveList(items);

  // Prune articles that are no longer in the queue (read/removed on the web).
  for (const auto& old : previous) {
    const bool stillHere = std::any_of(items.begin(), items.end(),
                                       [&](const QueueItem& q) { return q.id == old.id; });
    if (!stillHere) cache::removeArticle(old.id, old.images);
  }

  LOG_INF("SIFT", "queue sync complete: %d articles", static_cast<int>(items.size()));
  return static_cast<int>(items.size());
}

}  // namespace sync
}  // namespace sift
