#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SiftClient.h"

// On-SD cache of the Sift device API, under /.crosspoint/sift/. Written by the
// sync routine (and opportunistically on every successful live fetch) so the
// reader keeps working when the device is offline. JSON text mirrors the server
// payloads; images are the same normalized JPEGs the reader renders.
namespace sift {
namespace cache {

void ensureDirs();

bool saveFeeds(int totalUnread, const std::vector<Feed>& feeds);
bool loadFeeds(int& totalUnread, std::vector<Feed>& out);

bool saveArticles(const std::string& feed, const std::vector<ArticleMeta>& articles);
bool loadArticles(const std::string& feed, std::vector<ArticleMeta>& out);

bool saveArticle(const ArticleFull& article);
bool loadArticle(int id, ArticleFull& out);

// Cached, panel-normalized JPEG for one article (may not exist).
std::string imagePath(int id);
bool hasImage(int id);

// Whole-catalog freshness: unix seconds of the last completed sync, 0 if never.
uint32_t lastSyncEpoch();
void setLastSyncEpoch(uint32_t epoch);

}  // namespace cache
}  // namespace sift
