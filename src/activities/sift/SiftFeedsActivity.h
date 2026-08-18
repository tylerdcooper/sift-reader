#pragma once

#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * Top level of the Sift reader: "All articles" plus the user's feeds with unread
 * counts, fetched from the Sift server. Selecting a row opens its article list.
 */
class SiftFeedsActivity final : public UiListActivity {
 public:
  SiftFeedsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return count; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Sift"; }

  // Fetch feeds and (re)populate the list rows, including the "Download for
  // offline" action row.
  void buildRows();
  // Pull the whole catalog to the on-device cache, drawing progress.
  void runSync();
  void drawSyncProgress(int done, int total, const char* label);
  int lastShownPct = -1;

  static constexpr int kMax = 64;
  freeink::ui::ListItem rowItems[kMax]{};
  int count = 0;
  // Backing storage for ListItem pointers; feedSelectors[i] is "all" or a feed
  // id string (empty for a non-actionable error row).
  std::vector<std::string> labels;
  std::vector<std::string> values;
  std::vector<std::string> feedSelectors;
};
