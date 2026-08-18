#include "SiftArticleActivity.h"

#include <Epub/blocks/ImageBlock.h>
#include <Epub/converters/ImageDecoderFactory.h>
#include <Epub/converters/ImageToFramebufferDecoder.h>
#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>

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
#include "network/HttpDownloader.h"

namespace {
constexpr size_t MAX_LINE_BYTES = 191;
constexpr int SIDE_PADDING = 20;
constexpr int IMG_GAP = 12;                    // space between the lead image and the text
constexpr float IMG_MAX_BODY_FRACTION = 0.5f;  // image never eats more than this of the body height
const char* const SIFT_IMG_PATH = "/.crosspoint/sift-img.png";
const char* const SIFT_IMG_CACHE = "/.crosspoint/sift-img.pxc";
}  // namespace

SiftArticleActivity::SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int articleId,
                                         std::string titleHint)
    : Activity("SiftArticle", renderer, mappedInput), articleId(articleId), title(std::move(titleHint)) {}

SiftArticleActivity::~SiftArticleActivity() = default;

void SiftArticleActivity::onEnter() {
  Activity::onEnter();

  // A prior article reused the same temp path; drop any remembered failure and
  // stale pixel cache so this image is decoded fresh.
  ImageBlock::clearSessionRenderFailures();
  imageReady = false;
  imageBlock.reset();
  imgW = imgH = 0;

  sift::ArticleFull art;
  if (sift::fetchArticle(articleId, art)) {
    title = art.title;
    meta = art.feed;
    if (!art.date.empty()) meta += (meta.empty() ? "" : "  \xC2\xB7  ") + art.date;
    body = art.text;
    if (art.hasImage) prepareImage(articleId);
  } else {
    body = "Couldn't load this article. Check the reader's Wi-Fi connection and try again.";
  }
  wrapText();
  requestUpdate();
}

void SiftArticleActivity::onExit() {
  imageBlock.reset();
  ImageBlock::releaseRenderCache();
  if (Storage.exists(SIFT_IMG_PATH)) Storage.remove(SIFT_IMG_PATH);
  if (Storage.exists(SIFT_IMG_CACHE)) Storage.remove(SIFT_IMG_CACHE);
  Activity::onExit();
}

// Get the server-normalized lead image (preferring the offline-synced copy,
// else downloading to a temp slot), probe its size, and fit it to the top of
// the first page. Sets imageReady only when there's a usable image.
void SiftArticleActivity::prepareImage(int id) {
  if (sift::cache::hasImage(id)) {
    imagePath = sift::cache::imagePath(id);  // synced for offline — reuse it
  } else {
    imagePath = SIFT_IMG_PATH;
    if (Storage.exists(SIFT_IMG_PATH)) Storage.remove(SIFT_IMG_PATH);
    if (Storage.exists(SIFT_IMG_CACHE)) Storage.remove(SIFT_IMG_CACHE);
    const std::string url = sift::imageUrl(id, bodyWidthPx());
    if (url.empty()) return;
    sift::NetGuard guard;  // serialize with the background sync task
    if (HttpDownloader::downloadToFile(url, imagePath) != HttpDownloader::OK) {
      LOG_ERR("SIFT", "image download failed");
      return;
    }
  }

  ImageToFramebufferDecoder* decoder = ImageDecoderFactory::getDecoder(imagePath);
  ImageDimensions dims;
  if (!decoder || !decoder->getDimensions(imagePath, dims) || dims.width <= 0 || dims.height <= 0) {
    LOG_ERR("SIFT", "image probe failed");
    return;
  }

  // The server already fit the width; only the height cap can force a shrink.
  const int maxW = bodyWidthPx();
  const int maxH = std::max(1, static_cast<int>(bodyHeightPx() * IMG_MAX_BODY_FRACTION));
  float scale = std::min({maxW / static_cast<float>(dims.width), maxH / static_cast<float>(dims.height), 1.0f});
  imgW = std::max(1, static_cast<int>(dims.width * scale));
  imgH = std::max(1, static_cast<int>(dims.height * scale));

  imageBlock = makeUniqueNoThrow<ImageBlock>(imagePath, std::string(), static_cast<int16_t>(imgW),
                                             static_cast<int16_t>(imgH));
  if (!imageBlock) {
    imgW = imgH = 0;
    return;
  }
  imageReady = true;
}

int SiftArticleActivity::imageBandHeight() const { return imageReady ? imgH + IMG_GAP : 0; }

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
  linesPage0 = std::max(1, (bodyHeightPx() - imageBandHeight()) / lineHeight);

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

  const int total = static_cast<int>(lines.size());
  totalPages = (total <= linesPage0) ? 1 : 1 + (total - linesPage0 + linesPerPage - 1) / linesPerPage;
  currentPage = 0;
}

void SiftArticleActivity::pageLineRange(int page, int& firstLine, int& lastLine) const {
  const int total = static_cast<int>(lines.size());
  if (page <= 0) {
    firstLine = 0;
    lastLine = std::min(linesPage0, total);
  } else {
    firstLine = std::min(linesPage0 + (page - 1) * linesPerPage, total);
    lastLine = std::min(firstLine + linesPerPage, total);
  }
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

void SiftArticleActivity::drawBody(const int fontId, const int x, const int startY, const int firstLine,
                                   const int lastLine) const {
  const int lineHeight = renderer.getLineHeight(fontId);
  char buf[MAX_LINE_BYTES + 1];
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

  int firstLine, lastLine;
  pageLineRange(currentPage, firstLine, lastLine);
  const int textStartY = bodyStartY() + (currentPage == 0 ? imageBandHeight() : 0);

  // The lead image sits at the top of the first page; text flows beneath it.
  // Decoded + dithered on first draw, then served from its RAM/SD cache; freed
  // once this page render completes.
  if (currentPage == 0 && imageReady && imageBlock) {
    const int imgX = SIDE_PADDING + std::max(0, (bodyWidthPx() - imgW) / 2);
    // Server already dithered to the native gray codes — render without a second
    // dithering pass.
    imageBlock->render(renderer, imgX, bodyStartY(), /*dither=*/false);
    ImageBlock::releaseRenderCache();
  }

  const int fontId = SETTINGS.getReaderFontId();
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  drawBody(fontId, SIDE_PADDING, textStartY, firstLine, lastLine);
  scope.endScanAndPrewarm();
  drawBody(fontId, SIDE_PADDING, textStartY, firstLine, lastLine);

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), "", (currentPage > 0 ? "<" : ""), (currentPage + 1 < totalPages ? ">" : ""));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
