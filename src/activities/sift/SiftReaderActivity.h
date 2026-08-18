#pragma once

#include <GfxRenderer.h>

#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * Native Sift reader: a scrollable list of articles (title + "feed · date"),
 * the in-place alternative to browsing OPDS. First iteration uses sample data so
 * the layout can be designed in the simulator; real feed data wires in next.
 */
class SiftReaderActivity final : public UiListActivity {
 public:
  SiftReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;

 private:
  int listCount() const override { return count; }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return "Sift"; }

  static constexpr int kMax = 24;
  freeink::ui::ListItem rowItems[kMax]{};
  int count = 0;
};
