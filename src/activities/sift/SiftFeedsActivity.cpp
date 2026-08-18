#include "SiftFeedsActivity.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <WiFi.h>

#include <algorithm>
#include <cstdio>
#include <memory>

#include "MappedInputManager.h"
#include "SiftClient.h"
#include "SiftReaderActivity.h"
#include "SiftSync.h"
#include "SilentRestart.h"
#include "activities/network/WifiSelectionActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace fui = freeink::ui;

namespace {
const char* const SYNC_SELECTOR = "__sync__";
constexpr int SIDE_PADDING = 20;
}  // namespace

SiftFeedsActivity::SiftFeedsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("SiftFeeds", renderer, mappedInput) {}

void SiftFeedsActivity::buildRows() {
  labels.clear();
  values.clear();
  feedSelectors.clear();

  int totalUnread = 0;
  std::vector<sift::Feed> feeds;
  const bool isConfigured = sift::configured();
  if (isConfigured && sift::fetchFeeds(totalUnread, feeds)) {
    labels.push_back("All articles");
    values.push_back(totalUnread > 0 ? std::to_string(totalUnread) : "");
    feedSelectors.push_back("all");
    for (const auto& f : feeds) {
      labels.push_back(f.title.empty() ? "(untitled feed)" : f.title);
      values.push_back(f.unread > 0 ? std::to_string(f.unread) : "");
      feedSelectors.push_back(std::to_string(f.id));
    }
    labels.push_back("Download all for offline");
    values.push_back("");
    feedSelectors.push_back(SYNC_SELECTOR);
  } else if (!isConfigured) {
    labels.push_back("Sift not set up");
    values.push_back("");
    feedSelectors.push_back("");
  } else {
    labels.push_back("Couldn't reach Sift");
    values.push_back("");
    feedSelectors.push_back("");
  }

  count = std::min(static_cast<int>(labels.size()), kMax);
  for (int i = 0; i < count; ++i) {
    fui::ListItem item;
    item.label = labels[i].c_str();
    if (!values[i].empty()) item.value = values[i].c_str();
    item.actionValue = static_cast<int16_t>(i);
    rowItems[i] = item;
  }
  nav.selected = 0;
}

void SiftFeedsActivity::onEnter() {
  UiListActivity::onEnter();
  // CrossPoint keeps Wi-Fi down outside network screens, so bring it up (auto-
  // connecting to the saved network) before any fetch — otherwise the TLS stack
  // asserts on a null mutex. Child reader/article activities inherit it.
  if (sift::configured() && WiFi.status() != WL_CONNECTED) {
    showConnecting();
    startActivityForResult(std::make_unique<WifiSelectionActivity>(renderer, mappedInput),
                           [this](const ActivityResult&) {
                             buildRows();
                             requestUpdate();
                           });
    return;
  }
  buildRows();
}

void SiftFeedsActivity::onExit() {
  // CrossPoint convention: network screens reboot to home on exit to tear Wi-Fi
  // down cleanly (see OPDS/Calibre/OTA). Must be last — it restarts the device.
  UiListActivity::onExit();
  if (WiFi.getMode() != WIFI_MODE_NULL) silentRestart();
}

void SiftFeedsActivity::showConnecting() {
  labels.clear();
  values.clear();
  feedSelectors.clear();
  labels.push_back("Connecting to Wi-Fi\xE2\x80\xA6");  // …
  values.push_back("");
  feedSelectors.push_back("");
  count = 1;
  freeink::ui::ListItem item;
  item.label = labels[0].c_str();
  item.actionValue = 0;
  rowItems[0] = item;
  nav.selected = 0;
  requestUpdate();
}

void SiftFeedsActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= count || feedSelectors[index].empty()) return;
  if (feedSelectors[index] == SYNC_SELECTOR) {
    runSync();
    return;
  }
  startActivityForResult(std::make_unique<SiftReaderActivity>(renderer, mappedInput, feedSelectors[index]),
                         [](const ActivityResult&) {});
}

void SiftFeedsActivity::runSync() {
  lastShownPct = -1;
  drawSyncProgress(0, 0, "Starting");
  const int n = sift::sync::syncAll(
      [](void* ctx, int done, int total, const char* label) {
        static_cast<SiftFeedsActivity*>(ctx)->drawSyncProgress(done, total, label);
      },
      this);
  const char* doneLabel = "Done";
  if (n == sift::sync::kBusy) {
    doneLabel = "Sync already running";
  } else if (n < 0) {
    doneLabel = "Couldn't reach Sift";
  }
  const int shown = n < 0 ? 0 : n;
  lastShownPct = -1;
  drawSyncProgress(shown, shown, doneLabel);

  // Repopulate the list and return to it.
  buildRows();
  requestUpdate();
}

void SiftFeedsActivity::drawSyncProgress(int done, int total, const char* label) {
  const int pct = total > 0 ? (done * 100 / total) : (done > 0 ? 100 : 0);
  // Limit e-ink refreshes: only repaint when the percentage actually moves.
  if (pct == lastShownPct && total > 0 && done != total) return;
  lastShownPct = pct;

  renderer.clearScreen();
  const int w = renderer.getScreenWidth();
  renderer.drawText(UI_12_FONT_ID, SIDE_PADDING, 70, "Downloading for offline", true, EpdFontFamily::BOLD);

  char line[48];
  if (total > 0) {
    snprintf(line, sizeof(line), "%d / %d articles", done, total);
  } else {
    snprintf(line, sizeof(line), "%s", label ? label : "");
  }
  renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, 110, line);

  const int barX = SIDE_PADDING, barY = 138, barW = w - 2 * SIDE_PADDING, barH = 18;
  renderer.fillRect(barX, barY, barW, barH, true);
  renderer.fillRect(barX + 2, barY + 2, barW - 4, barH - 4, false);
  const int fillW = (barW - 4) * pct / 100;
  if (fillW > 0) renderer.fillRect(barX + 2, barY + 2, fillW, barH - 4, true);

  if (label && total > 0) {
    renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, 178, label);
  }
  renderer.displayBuffer();
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
