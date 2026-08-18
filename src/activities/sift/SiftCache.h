#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "SiftClient.h"

// On-SD store of the read-later queue, under /.crosspoint/sift/ — its own folder,
// never the books library. queue.json holds the list (for the saved screen);
// a_<id>.json holds each article's blocks; img/<id>_<n>.jpg the inline images.
namespace sift {
namespace cache {

void ensureDirs();

// The saved-list metadata (no blocks) for the list screen.
struct ListItem {
  int id = 0;
  std::string title;
  std::string feed;
  std::string date;
  int images = 0;
};

bool saveList(const std::vector<QueueItem>& items);
bool loadList(std::vector<ListItem>& out);

bool saveArticle(const QueueItem& item);  // writes a_<id>.json (blocks)
bool loadArticle(int id, QueueItem& out);

std::string imagePath(int id, int n);  // /.crosspoint/sift/img/<id>_<n>.jpg
bool hasImage(int id, int n);

// Remove one article's blocks + images (read on the device, or dropped from the
// server queue). imageCount bounds how many img files to try removing.
void removeArticle(int id, int imageCount);

}  // namespace cache
}  // namespace sift
