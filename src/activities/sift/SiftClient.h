#pragma once

#include <string>
#include <vector>

// Thin client for the Sift server's device JSON API. Blocking fetches (run from
// an activity's loop, with a Loading screen shown first). Base URL + token come
// from SiftConfigStore, or a compile-time simulator default.
namespace sift {

struct Feed {
  int id;
  std::string title;
  int unread;
};
struct ArticleMeta {
  int id;
  std::string title;
  std::string feed;
  std::string date;
  std::string excerpt;
};
struct ArticleFull {
  int id;
  std::string title;
  std::string feed;
  std::string date;
  std::string text;
  std::string image;
  bool hasImage = false;
};

std::string baseUrl();
std::string token();
bool configured();

// Direct URL (with token) for the article's lead image, normalized server-side
// to a panel-width grayscale baseline JPEG. Empty if not configured.
std::string imageUrl(int id, int width);

bool fetchFeeds(int& totalUnread, std::vector<Feed>& out);
bool fetchArticles(const std::string& feed, std::vector<ArticleMeta>& out);
bool fetchArticle(int id, ArticleFull& out);

}  // namespace sift
