#include "SiftFeedsActivity.h"

#include <GfxRenderer.h>

#include <memory>

#include "MappedInputManager.h"
#include "SiftReaderActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
struct SampleFeed {
  const char* name;
  const char* unread;  // shown right-aligned; "" hides it
};
constexpr SampleFeed kFeeds[] = {
    {"All articles", "71"}, {"Hacker News", "48"},   {"The Verge", "12"}, {"Craft", "5"},
    {"Database Weekly", "3"}, {"Longreads", "2"},     {"Type Digest", "2"}, {"Indie Hackers", "1"},
    {"Mind & Machine", ""},  {"The Prepared", ""},
};
constexpr int kFeedCount = static_cast<int>(sizeof(kFeeds) / sizeof(kFeeds[0]));
}  // namespace

SiftFeedsActivity::SiftFeedsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("SiftFeeds", renderer, mappedInput) {}

void SiftFeedsActivity::onEnter() {
  UiListActivity::onEnter();
  count = kFeedCount < kMax ? kFeedCount : kMax;
  for (int i = 0; i < count; ++i) {
    fui::ListItem item;
    item.label = kFeeds[i].name;
    if (kFeeds[i].unread[0] != '\0') item.value = kFeeds[i].unread;
    item.actionValue = static_cast<int16_t>(i);
    rowItems[i] = item;
  }
  nav.selected = 0;
}

void SiftFeedsActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= count) return;
  // v1: every feed opens the same sample article list; per-feed filtering wires
  // in with real data.
  startActivityForResult(std::make_unique<SiftReaderActivity>(renderer, mappedInput),
                         [](const ActivityResult&) {});
}

void SiftFeedsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.items = rowItems;
  props.count = static_cast<uint16_t>(count);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
