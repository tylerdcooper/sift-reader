#include "SiftSync.h"

#include <Logging.h>

#include <atomic>
#include <ctime>
#include <string>
#include <vector>

#include "SiftCache.h"
#include "SiftClient.h"
#include "network/HttpDownloader.h"

namespace sift {
namespace sync {

// Match the width the reader requests so a synced image is byte-identical to a
// live one (and its dither/pixel cache is reusable).
static constexpr int IMG_W = 440;

// One sync at a time: the dock background task and the manual "Download for
// offline" action both call syncAll; whoever is second gets kBusy.
static std::atomic<bool> g_busy{false};

bool isSyncing() { return g_busy.load(); }

int syncAll(ProgressFn progress, void* ctx) {
  bool expected = false;
  if (!g_busy.compare_exchange_strong(expected, true)) return kBusy;

  cache::ensureDirs();

  int result = -1;
  int totalUnread = 0;
  std::vector<Feed> feeds;
  std::vector<ArticleMeta> articles;
  if (fetchFeeds(totalUnread, feeds) && fetchArticles("all", articles)) {
    const int total = static_cast<int>(articles.size());
    int done = 0;
    for (const auto& m : articles) {
      if (progress) progress(ctx, done, total, m.title.c_str());
      ArticleFull full;
      if (fetchArticle(m.id, full) && full.hasImage && !cache::hasImage(m.id)) {
        const std::string url = sift::imageUrl(m.id, IMG_W);
        if (!url.empty()) {
          NetGuard guard;  // serialize with the UI's own fetches
          HttpDownloader::downloadToFile(url, cache::imagePath(m.id));
        }
      }
      done++;
      if (progress) progress(ctx, done, total, m.title.c_str());
    }
    cache::setLastSyncEpoch(static_cast<uint32_t>(time(nullptr)));
    LOG_INF("SIFT", "offline sync complete: %d articles", done);
    result = done;
  }

  g_busy.store(false);
  return result;
}

}  // namespace sync
}  // namespace sift
