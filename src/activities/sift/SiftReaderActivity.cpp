#include "SiftReaderActivity.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <memory>

#include "MappedInputManager.h"
#include "SiftArticleActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

SiftReaderActivity::SiftReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string feed)
    : UiListActivity("SiftReader", renderer, mappedInput), feed(std::move(feed)) {}

void SiftReaderActivity::onEnter() {
  UiListActivity::onEnter();
  articles.clear();
  labels.clear();
  subtitles.clear();

  if (!sift::fetchArticles(feed, articles)) {
    labels.push_back("Couldn't load articles");
    subtitles.push_back("Check the reader's Wi-Fi and try again");
  }

  const int n = std::min(static_cast<int>(articles.size()), kMax);
  labels.reserve(n + 1);
  subtitles.reserve(n + 1);
  for (int i = 0; i < n; ++i) {
    labels.push_back(articles[i].title);
    std::string sub = articles[i].feed;
    if (!articles[i].date.empty()) sub += (sub.empty() ? "" : "  \xC2\xB7  ") + articles[i].date;
    subtitles.push_back(sub);
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

void SiftReaderActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= static_cast<int>(articles.size())) return;  // error/empty row
  const auto& a = articles[index];
  startActivityForResult(std::make_unique<SiftArticleActivity>(renderer, mappedInput, a.id, a.title),
                         [](const ActivityResult&) {});
}

void SiftReaderActivity::buildScreen(UiScreen& screen) {
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
