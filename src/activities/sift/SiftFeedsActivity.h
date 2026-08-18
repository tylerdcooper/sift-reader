#pragma once

#include <GfxRenderer.h>

#include "activities/UiListActivity.h"

class MappedInputManager;

/**
 * Top level of the Sift reader: the user's feeds with unread counts. Selecting
 * a feed opens its article list. First row is "All articles". Sample data for
 * now; real feeds wire in with the Sift server fetch.
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

  static constexpr int kMax = 24;
  freeink::ui::ListItem rowItems[kMax]{};
  int count = 0;
};
