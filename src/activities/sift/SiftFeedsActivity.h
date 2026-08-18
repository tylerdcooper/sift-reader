#pragma once

#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * The Sift screen on the device: your saved read-later queue (articles you sent
 * from the web). Connects Wi-Fi and silently syncs on entry, then lists the
 * saved articles from the on-SD cache; selecting one opens the reader.
 */
class SiftFeedsActivity final : public UiListActivity {
 public:
  SiftFeedsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void loop() override;

 private:
  int listCount() const override { return count; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Sift"; }

  void buildRows();  // from the on-SD cache (shows "Sync in progress" if empty + syncing)

  bool wasBusy = false;  // to refresh the list when a background sync completes

  static constexpr int kMax = 128;
  freeink::ui::ListItem rowItems[kMax]{};
  int count = 0;
  std::vector<std::string> labels;
  std::vector<std::string> subtitles;
  std::vector<int> ids;  // parallel; -1 for a non-actionable placeholder row
};
