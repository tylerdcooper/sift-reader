#include "SiftFeedsActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>
#include <memory>

#include "MappedInputManager.h"
#include "SiftArticleActivity.h"
#include "SiftCache.h"
#include "SiftClient.h"
#include "SiftSync.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

SiftFeedsActivity::SiftFeedsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("SiftFeeds", renderer, mappedInput) {}

void SiftFeedsActivity::buildRows() {
  labels.clear();
  subtitles.clear();
  ids.clear();

  std::vector<sift::cache::ListItem> list;
  sift::cache::loadList(list);

  if (list.empty()) {
    labels.push_back(sift::configured() ? "No saved articles yet" : "Sift not set up");
    subtitles.push_back(sift::configured() ? "Send articles from Sift on the web" : "");
    ids.push_back(-1);
  } else {
    for (const auto& it : list) {
      labels.push_back(it.title.empty() ? "(untitled)" : it.title);
      std::string sub = it.feed;
      if (!it.date.empty()) sub += (sub.empty() ? "" : "  \xC2\xB7  ") + it.date;
      subtitles.push_back(sub);
      ids.push_back(it.id);
    }
  }

  count = std::min(static_cast<int>(labels.size()), kMax);
  for (int i = 0; i < count; ++i) {
    fui::ListItem item;
    item.label = labels[i].c_str();
    if (!subtitles[i].empty()) item.subtitle = subtitles[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems[i] = item;
  }
  nav.selected = 0;
}

void SiftFeedsActivity::showConnecting() {
  labels.clear();
  subtitles.clear();
  ids.clear();
  labels.push_back("Connecting to Wi-Fi\xE2\x80\xA6");
  subtitles.push_back("");
  ids.push_back(-1);
  count = 1;
  fui::ListItem item;
  item.label = labels[0].c_str();
  item.actionValue = 0;
  rowItems[0] = item;
  nav.selected = 0;
  requestUpdate();
}

void SiftFeedsActivity::syncAndRefresh() {
  sift::sync::syncQueue();  // silent — no progress screen
  buildRows();
  requestUpdate();
}

void SiftFeedsActivity::onEnter() {
  UiListActivity::onEnter();
  buildRows();  // show the cached queue immediately (offline-first)

  if (!sift::configured()) return;
#ifdef SIMULATOR
  WiFi.begin();  // sim connects immediately; skip the connect UI for headless testing
  syncAndRefresh();
  return;
#endif
  if (WiFi.status() == WL_CONNECTED) {
    syncAndRefresh();
    return;
  }
  // Bring Wi-Fi up (auto-connect saved network), then sync silently.
  showConnecting();
  startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                         [this](const ActivityResult&) { syncAndRefresh(); });
}

void SiftFeedsActivity::onExit() {
  UiListActivity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL) silentRestart();  // CrossPoint network-screen teardown
}

void SiftFeedsActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= count || ids[index] < 0) return;
  startActivityForResult(std::make_unique<SiftArticleActivity>(renderer, mappedInput, ids[index]),
                         [this](const ActivityResult&) {
                           // An article marked read removes itself; refresh from cache.
                           buildRows();
                           requestUpdate();
                         });
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
