#pragma once

#include <string>
#include <vector>

// Client for the Sift "read-later" device API. The device shows the queue of
// articles you sent from the web, each as ordered text/image blocks. Blocking
// fetches (run behind the Wi-Fi connect + a Loading state).
namespace sift {

// One piece of an article: a paragraph of text, or an inline image referenced by
// its index n (fetched from /api/device/image/<id>?n=<n>).
struct Block {
  bool image = false;
  std::string text;  // when !image
  int n = 0;         // when image
};

struct QueueItem {
  int id = 0;
  std::string title;
  std::string feed;
  std::string date;
  int images = 0;
  std::vector<Block> blocks;
};

std::string baseUrl();
std::string token();
bool configured();

// URL (with token) for the n-th inline image of an article, normalized to
// `width` px grayscale so the device can dither it itself.
std::string imageUrl(int id, int n, int width);

// Serializes all Sift network access (the TLS/HTTP stack isn't reentrant).
class NetGuard {
 public:
  NetGuard();
  ~NetGuard();
};
void netEnsureInit();

// GET /api/device/queue → the full read-later queue with blocks. False on
// network/parse failure (caller falls back to cache).
bool fetchQueue(std::vector<QueueItem>& out);

// GET /api/device/read?id=NN → mark an article read on the server (drops it from
// the queue and marks it read in Sift on the web).
bool markRead(int id);

}  // namespace sift
