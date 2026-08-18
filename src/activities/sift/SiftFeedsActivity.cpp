#include "SiftFeedsActivity.h"

#include <GfxRenderer.h>
#include <WiFi.h>

#include <algorithm>
#include <memory>

#include "MappedInputManager.h"
#include "SiftArticleActivity.h"
#include "SiftBackgroundSync.h"
#include "SiftCache.h"
#include "SiftClient.h"
#include "SiftSync.h"
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
    if (!sift::configured()) {
      labels.push_back("Sift not set up");
      subtitles.push_back("");
    } else if (sift::bgBusy()) {
      labels.push_back("Sync in progress\xE2\x80\xA6");
      subtitles.push_back("Downloading your saved articles");
    } else {
      labels.push_back("No saved articles yet");
      subtitles.push_back("Send articles from Sift on the web");
    }
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

void SiftFeedsActivity::onEnter() {
  UiListActivity::onEnter();
  // Show the cached queue immediately — never a connect screen. Kick a
  // background connect+sync; buildRows() shows "Sync in progress" if we have
  // nothing cached yet, and loop() refreshes when it completes.
  wasBusy = sift::bgBusy();
  buildRows();

#ifdef SIMULATOR
  // The sim has no saved Wi-Fi creds; sync inline so the list works headlessly.
  if (sift::configured()) {
    WiFi.begin();
    sift::sync::syncQueue();
    buildRows();
    requestUpdate();
  }
#else
  sift::requestSync();
#endif
}

void SiftFeedsActivity::onExit() {
  UiListActivity::onExit();
  sift::endSyncSession();  // tear Wi-Fi down; it's only up while in Sift
}

void SiftFeedsActivity::loop() {
  UiListActivity::loop();
  // When a background sync finishes (or starts), refresh the list live.
  const bool busy = sift::bgBusy();
  if (busy != wasBusy) {
    wasBusy = busy;
    buildRows();
    requestUpdate();
  }
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
