#pragma once

#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "SiftClient.h"
#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * In-place article list for one feed selector ("all", "saved", "favorites", or a
 * numeric feed id). Fetches metadata from the Sift server; selecting a row opens
 * the article reader.
 */
class SiftReaderActivity final : public UiListActivity {
 public:
  SiftReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string feed = "all");

  void onEnter() override;

 private:
  int listCount() const override { return count; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Sift"; }

  std::string feed;
  static constexpr int kMax = 128;
  freeink::ui::ListItem rowItems[kMax]{};
  int count = 0;
  std::vector<sift::ArticleMeta> articles;
  // Backing storage for the ListItem const char* pointers.
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
};
