#include "SiftArticleActivity.h"

#include <Epub/blocks/ImageBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/ImageToFramebufferDecoder.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "SiftCache.h"
#include "SiftClient.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t MAX_LINE_BYTES = 191;
constexpr int SIDE_PADDING = 20;
constexpr int IMG_GAP = 14;                    // space around an inline image
constexpr float IMG_MAX_BODY_FRACTION = 0.6f;  // an image can use up to this of the body height
}  // namespace

SiftArticleActivity::SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int articleId)
    : Activity("SiftArticle", renderer, mappedInput), articleId(articleId) {}

int SiftArticleActivity::imageAreaWidth() const {
  return std::min(renderer.getScreenWidth(), renderer.getScreenHeight()) - 2 * SIDE_PADDING;
}

int SiftArticleActivity::bodyStartY() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + 10 + renderer.getLineHeight(UI_12_FONT_ID) + renderer.getLineHeight(UI_10_FONT_ID) +
         metrics.verticalSpacing;
}

int SiftArticleActivity::bodyHeightPx() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int bottom = metrics.buttonHintsHeight + metrics.verticalSpacing;
  return renderer.getScreenHeight() - bodyStartY() - bottom;
}

int SiftArticleActivity::measureSpan(const int fontId, const char* text, size_t len) const {
  char buf[MAX_LINE_BYTES + 1];
  len = std::min(len, MAX_LINE_BYTES);
  memcpy(buf, text, len);
  buf[len] = '\0';
  return renderer.getTextAdvanceX(fontId, buf, EpdFontFamily::REGULAR);
}

// Word-wrap one text block into line Elems (UTF-8 aware, long-word splitting).
void SiftArticleActivity::wrapInto(const std::string& body, const int fontId, const int maxWidth, const int lineHeight) {
  const int spaceWidth = renderer.getSpaceWidth(fontId, EpdFontFamily::REGULAR);
  const char* text = body.c_str();
  const uint32_t n = static_cast<uint32_t>(body.size());

  auto pushLine = [&](uint32_t start, uint32_t len) {
    Elem e;
    e.image = false;
    e.text.assign(text + start, len);
    e.height = lineHeight;
    elems.push_back(std::move(e));
  };

  uint32_t lineStart = 0, lineEnd = 0;
  int lineWidth = 0;
  uint32_t i = 0;
  while (i < n) {
    const char c = text[i];
    if (c == '\n') {
      pushLine(lineStart, lineEnd - lineStart);
      i++;
      lineStart = lineEnd = i;
      lineWidth = 0;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      i++;
      continue;
    }
    const uint32_t tokenStart = i;
    while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' &&
           i - tokenStart < MAX_LINE_BYTES) {
      i++;
    }
    while (i - tokenStart > 1 && (text[i] & 0xC0) == 0x80) i--;
    const uint32_t tokenLen = i - tokenStart;
    const int tokenWidth = measureSpan(fontId, text + tokenStart, tokenLen);

    if (lineEnd == lineStart) {
      lineStart = tokenStart;
      lineEnd = tokenStart + tokenLen;
      lineWidth = tokenWidth;
    } else if (lineWidth + spaceWidth + tokenWidth <= maxWidth) {
      lineEnd = tokenStart + tokenLen;
      lineWidth += spaceWidth + tokenWidth;
    } else {
      pushLine(lineStart, lineEnd - lineStart);
      lineStart = tokenStart;
      lineEnd = tokenStart + tokenLen;
      lineWidth = tokenWidth;
    }

    while (lineWidth > maxWidth && lineEnd - lineStart > 1) {
      const uint32_t len = lineEnd - lineStart;
      uint32_t lastFit = 0;
      for (uint32_t f = 1; f <= len; f++) {
        if (f == len || (text[lineStart + f] & 0xC0) != 0x80) {
          if (measureSpan(fontId, text + lineStart, f) > maxWidth) break;
          lastFit = f;
        }
      }
      if (lastFit == 0) {
        lastFit = 1;
        while (lastFit < len && (text[lineStart + lastFit] & 0xC0) == 0x80) lastFit++;
      }
      pushLine(lineStart, lastFit);
      lineStart += lastFit;
      lineWidth = measureSpan(fontId, text + lineStart, lineEnd - lineStart);
    }
  }
  if (lineEnd > lineStart) pushLine(lineStart, lineEnd - lineStart);
}

void SiftArticleActivity::buildLayout() {
  elems.clear();
  sift::QueueItem item;
  if (!sift::cache::loadArticle(articleId, item)) {
    Elem e;
    e.text = "Couldn't load this article.";
    e.height = renderer.getLineHeight(SETTINGS.getReaderFontId());
    elems.push_back(std::move(e));
    return;
  }
  title = item.title;
  meta = item.feed;
  if (!item.date.empty()) meta += (meta.empty() ? "" : "  \xC2\xB7  ") + item.date;
  imageCount = item.images;

  const int fontId = SETTINGS.getReaderFontId();
  const int lineHeight = renderer.getLineHeight(fontId);
  const int maxW = imageAreaWidth();
  const int maxImgH = std::max(1, static_cast<int>(bodyHeightPx() * IMG_MAX_BODY_FRACTION));

  renderer.ensureSdCardFontReady(fontId, title.c_str(), 0x01);

  for (const auto& b : item.blocks) {
    if (b.image) {
      const std::string path = sift::cache::imagePath(articleId, b.n);
      ImageToFramebufferDecoder* dec = ImageDecoderFactory::getDecoder(path);
      ImageDimensions dims;
      if (!dec || !dec->getDimensions(path, dims) || dims.width <= 0 || dims.height <= 0) continue;  // not downloaded
      const float scale = std::min({maxW / static_cast<float>(dims.width),
                                    maxImgH / static_cast<float>(dims.height), 1.0f});
      Elem e;
      e.image = true;
      e.n = b.n;
      e.w = std::max(1, static_cast<int>(dims.width * scale));
      e.h = std::max(1, static_cast<int>(dims.height * scale));
      e.height = e.h + IMG_GAP * 2;
      elems.push_back(std::move(e));
    } else {
      renderer.ensureSdCardFontReady(fontId, b.text.c_str(), 0x01);
      wrapInto(b.text, fontId, maxW, lineHeight);
      // paragraph gap
      Elem gap;
      gap.text = "";
      gap.height = lineHeight / 2;
      elems.push_back(std::move(gap));
    }
  }
  loaded = true;
}

void SiftArticleActivity::paginate() {
  pageStart.clear();
  pageStart.push_back(0);
  const int bodyH = bodyHeightPx();
  int y = 0;
  for (int i = 0; i < static_cast<int>(elems.size()); ++i) {
    if (y > 0 && y + elems[i].height > bodyH) {
      pageStart.push_back(i);
      y = 0;
    }
    y += elems[i].height;
  }
  totalPages = std::max(1, static_cast<int>(pageStart.size()));
  currentPage = 0;
}

void SiftArticleActivity::onEnter() {
  Activity::onEnter();
  buildLayout();
  paginate();
  requestUpdate();
}

void SiftArticleActivity::markReadAndClose() {
  sift::markRead(articleId);  // best-effort; syncs read state back to the web
  sift::cache::removeArticle(articleId, imageCount);
  finish();
}

void SiftArticleActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }
  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    markReadAndClose();
    return;
  }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (tx < renderer.getScreenWidth() / 3) {
      if (currentPage > 0) { currentPage--; requestUpdate(); }
    } else if (currentPage + 1 < totalPages) {
      currentPage++;
      requestUpdate();
    }
    return;
  }
  buttonNavigator.onNext([this] {
    if (currentPage + 1 < totalPages) { currentPage++; requestUpdate(); }
  });
  buttonNavigator.onPrevious([this] {
    if (currentPage > 0) { currentPage--; requestUpdate(); }
  });
}

void SiftArticleActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();

  const int titleY = metrics.topPadding + 10;
  renderer.drawText(UI_12_FONT_ID, SIDE_PADDING, titleY, title.c_str(), true, EpdFontFamily::BOLD);
  const int metaY = titleY + renderer.getLineHeight(UI_12_FONT_ID);
  renderer.drawText(UI_10_FONT_ID, SIDE_PADDING, metaY, meta.c_str());
  if (totalPages > 1) {
    char counter[16];
    snprintf(counter, sizeof(counter), "%d/%d", currentPage + 1, totalPages);
    const int cw = renderer.getTextWidth(UI_10_FONT_ID, counter);
    renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - SIDE_PADDING - cw, metaY, counter);
  }

  const int first = pageStart[currentPage];
  const int last = (currentPage + 1 < totalPages) ? pageStart[currentPage + 1] : static_cast<int>(elems.size());
  const int fontId = SETTINGS.getReaderFontId();
  const int startY = bodyStartY();

  // Draw images first (once), tracking each element's y.
  int y = startY;
  std::vector<int> ys;
  ys.reserve(last - first);
  for (int i = first; i < last; ++i) {
    ys.push_back(y);
    if (elems[i].image) {
      const int imgX = SIDE_PADDING + std::max(0, (imageAreaWidth() - elems[i].w) / 2);
      ImageBlock block(sift::cache::imagePath(articleId, elems[i].n), std::string(),
                       static_cast<int16_t>(elems[i].w), static_cast<int16_t>(elems[i].h));
      block.render(renderer, imgX, y + IMG_GAP, /*dither=*/true);
      ImageBlock::releaseRenderCache();
    }
    y += elems[i].height;
  }

  // Text with the font prewarm scan/real double pass.
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = first; i < last; ++i) {
      if (elems[i].image || elems[i].text.empty()) continue;
      renderer.drawText(fontId, SIDE_PADDING, ys[i - first], elems[i].text.c_str());
    }
    if (pass == 0) scope.endScanAndPrewarm();
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "Read", (currentPage > 0 ? "<" : ""),
                                            (currentPage + 1 < totalPages ? ">" : ""));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
