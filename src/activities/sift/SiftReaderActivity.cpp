#include "SiftReaderActivity.h"

#include <GfxRenderer.h>

#include <memory>

#include "MappedInputManager.h"
#include "SiftArticleActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
struct SampleArticle {
  const char* title;
  const char* meta;
  const char* body;
};
constexpr const char* kSampleBody =
    "There is a quiet argument, made mostly by people who build things for a living, that software should be "
    "slow to change and fast to use. Not slow in the sense of sluggish, but slow in the sense of considered.\n\n"
    "The fastest software is the software that does less. Every feature you add is a feature someone has to "
    "understand, maintain, and eventually work around. The best tools feel small even when they do a great "
    "deal, because their authors resisted the urge to make everything configurable.\n\n"
    "On a device like this one, that discipline is not optional. There is no room for a hundred settings, no "
    "budget for a dozen background services. What remains is the reading, and the reading is the point.\n\n"
    "So we build for constraint, and we let the constraint do the editing for us.";
// Sample content for designing the layout in the simulator. Real articles
// (fetched from the Sift server) replace this next.
constexpr SampleArticle kArticles[] = {
    {"The case for slow software", "Craft \xC2\xB7 2d", kSampleBody},
    {"How SQLite scales to millions of reads", "Database Weekly \xC2\xB7 3d", kSampleBody},
    {"Designing for e-ink: contrast over color", "Type Digest \xC2\xB7 4d", kSampleBody},
    {"The quiet return of RSS", "The Verge \xC2\xB7 5d", kSampleBody},
    {"Why your side project should stay small", "Indie Hackers \xC2\xB7 6d", kSampleBody},
    {"A field guide to focus", "Mind & Machine \xC2\xB7 1w", kSampleBody},
    {"What we lost when feeds became algorithms", "Longreads \xC2\xB7 1w", kSampleBody},
    {"Notes on building for constraint", "The Prepared \xC2\xB7 2w", kSampleBody},
};
constexpr int kArticleCount = static_cast<int>(sizeof(kArticles) / sizeof(kArticles[0]));
}  // namespace

SiftReaderActivity::SiftReaderActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : UiListActivity("SiftReader", renderer, mappedInput) {}

void SiftReaderActivity::onEnter() {
  UiListActivity::onEnter();
  count = kArticleCount < kMax ? kArticleCount : kMax;
  for (int i = 0; i < count; ++i) {
    fui::ListItem item;
    item.label = kArticles[i].title;
    item.subtitle = kArticles[i].meta;
    item.actionValue = static_cast<int16_t>(i);
    rowItems[i] = item;
  }
  nav.selected = 0;
}

void SiftReaderActivity::activateIndex(int index) {
  app.clearTapFlash();
  if (index < 0 || index >= count) return;
  const auto& a = kArticles[index];
  startActivityForResult(std::make_unique<SiftArticleActivity>(renderer, mappedInput, a.title, a.meta, a.body),
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
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  syncListViewport(screen, props);
  screen.list(props);
}
