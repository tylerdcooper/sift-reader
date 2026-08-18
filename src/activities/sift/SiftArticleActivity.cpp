#include "SiftArticleActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "I18nKeys.h"
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr size_t MAX_LINE_BYTES = 191;
constexpr int SIDE_PADDING = 20;
}  // namespace

SiftArticleActivity::SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title,
                                         std::string meta, std::string body)
    : Activity("SiftArticle", renderer, mappedInput),
      title(std::move(title)),
      meta(std::move(meta)),
      body(std::move(body)) {}

void SiftArticleActivity::onEnter() {
  Activity::onEnter();
  wrapText();
  requestUpdate();
}

int SiftArticleActivity::bodyStartY() const {
  const auto& metrics = UITheme::getInstance().getMetrics();
  return metrics.topPadding + 10 + renderer.getLineHeight(UI_12_FONT_ID) + renderer.getLineHeight(UI_10_FONT_ID) +
         metrics.verticalSpacing;
}

int SiftArticleActivity::bodyWidthPx() const { return renderer.getScreenWidth() - 2 * SIDE_PADDING; }

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

void SiftArticleActivity::wrapText() {
  lines.clear();
  lines.reserve(body.size() / 32 + 8);

  const int fontId = SETTINGS.getReaderFontId();
  renderer.ensureSdCardFontReady(fontId, body.c_str(), 0x01 /* REGULAR */);

  const int maxWidth = bodyWidthPx();
  const int spaceWidth = renderer.getSpaceWidth(fontId, EpdFontFamily::REGULAR);
  const int lineHeight = renderer.getLineHeight(fontId);
  linesPerPage = std::max(1, bodyHeightPx() / lineHeight);

  const char* text = body.c_str();
  const uint32_t n = static_cast<uint32_t>(body.size());
  uint32_t lineStart = 0, lineEnd = 0;
  int lineWidth = 0;

  const auto flushLine = [&](uint32_t nextStart) {
    lines.push_back({lineStart, static_cast<uint16_t>(lineEnd - lineStart)});
    lineStart = nextStart;
    lineEnd = nextStart;
    lineWidth = 0;
  };

  uint32_t i = 0;
  while (i < n) {
    const char c = text[i];
    if (c == '\n' || c == '\0') {
      flushLine(i + 1);
      i++;
      continue;
    }
    if (c == ' ' || c == '\t' || c == '\r') {
      i++;
      continue;
    }
    const uint32_t tokenStart = i;
    while (i < n && text[i] != ' ' && text[i] != '\t' && text[i] != '\r' && text[i] != '\n' && text[i] != '\0' &&
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
    } else if (lineWidth + spaceWidth + tokenWidth <= maxWidth && tokenStart + tokenLen - lineStart <= UINT16_MAX) {
      lineEnd = tokenStart + tokenLen;
      lineWidth += spaceWidth + tokenWidth;
    } else {
      flushLine(tokenStart);
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
      const uint32_t rest = lineStart + lastFit;
      lineEnd = rest;
      flushLine(rest);
      lineEnd = rest + (len - lastFit);
      lineWidth = measureSpan(fontId, text + lineStart, lineEnd - lineStart);
    }
  }
  if (lineEnd > lineStart) flushLine(n);
  while (!lines.empty() && lines.back().len == 0) lines.pop_back();

  totalPages = std::max(1, (static_cast<int>(lines.size()) + linesPerPage - 1) / linesPerPage);
  currentPage = 0;
}

void SiftArticleActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  int tx = 0, ty = 0;
  if (mappedInput.wasScreenTapped(tx, ty)) {
    if (tx < renderer.getScreenWidth() / 3) {
      if (currentPage > 0) {
        currentPage--;
        requestUpdate();
      }
    } else if (currentPage + 1 < totalPages) {
      currentPage++;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNext([this] {
    if (currentPage + 1 < totalPages) {
      currentPage++;
      requestUpdate();
    }
  });
  buttonNavigator.onPrevious([this] {
    if (currentPage > 0) {
      currentPage--;
      requestUpdate();
    }
  });
}

void SiftArticleActivity::drawBody(const int fontId, const int x, const int startY) const {
  const int lineHeight = renderer.getLineHeight(fontId);
  char buf[MAX_LINE_BYTES + 1];
  const int firstLine = currentPage * linesPerPage;
  const int lastLine = std::min(firstLine + linesPerPage, static_cast<int>(lines.size()));
  for (int i = firstLine; i < lastLine; i++) {
    if (lines[i].len == 0) continue;
    const size_t len = std::min(static_cast<size_t>(lines[i].len), MAX_LINE_BYTES);
    memcpy(buf, body.c_str() + lines[i].start, len);
    buf[len] = '\0';
    renderer.drawText(fontId, x, startY + (i - firstLine) * lineHeight, buf);
  }
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
    const int counterWidth = renderer.getTextWidth(UI_10_FONT_ID, counter);
    renderer.drawText(UI_10_FONT_ID, renderer.getScreenWidth() - SIDE_PADDING - counterWidth, metaY, counter);
  }

  const int fontId = SETTINGS.getReaderFontId();
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  drawBody(fontId, SIDE_PADDING, bodyStartY());
  scope.endScanAndPrewarm();
  drawBody(fontId, SIDE_PADDING, bodyStartY());

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), "", (currentPage > 0 ? "<" : ""), (currentPage + 1 < totalPages ? ">" : ""));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
