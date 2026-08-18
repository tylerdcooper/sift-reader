#pragma once

namespace sift {
namespace sync {

// Progress callback: (ctx, articles done, total, current title). Called before
// and after each article. Function pointer + ctx (no std::function) so it's
// cheap to pass around.
using ProgressFn = void (*)(void* ctx, int done, int total, const char* label);

// syncAll return sentinels (otherwise the count of articles synced).
constexpr int kUnreachable = -1;  // server couldn't be reached
constexpr int kBusy = -2;         // another sync is already running

// Pull the whole catalog to the on-SD cache: feeds, the full article list, and
// every article's body + lead image. Network-first (populating the cache).
// Serialized: only one sync runs at a time (see kBusy).
int syncAll(ProgressFn progress = nullptr, void* ctx = nullptr);

// True while a sync is in progress (background or manual).
bool isSyncing();

}  // namespace sync
}  // namespace sift
