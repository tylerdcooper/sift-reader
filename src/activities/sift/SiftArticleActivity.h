#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;
class ImageBlock;

/**
 * Reads one Sift article: title + "feed · date" header, then the body word-
 * wrapped into pages (buttons / side-taps turn pages, Back returns). When the
 * article has a lead image, it is downloaded, decoded and dithered on-device
 * and shown at the top of the first page, with the text flowing beneath it.
 */
class SiftArticleActivity final : public Activity {
 public:
  SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int articleId, std::string titleHint);
  ~SiftArticleActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  struct Line {
    uint32_t start;
    uint16_t len;
  };

  int bodyStartY() const;
  int bodyWidthPx() const;
  int bodyHeightPx() const;
  void wrapText();
  int measureSpan(int fontId, const char* text, size_t len) const;
  void drawBody(int fontId, int x, int startY, int firstLine, int lastLine) const;

  // Lead image: fetch the normalized JPEG, probe it, size it to fit the top of
  // page 0. imageBandHeight() is the vertical space it reserves there.
  void prepareImage(int id);
  int imageBandHeight() const;
  void pageLineRange(int page, int& firstLine, int& lastLine) const;

  int articleId;
  std::string title;
  std::string meta;
  std::string body;
  std::vector<Line> lines;
  int currentPage = 0;
  int totalPages = 1;
  int linesPerPage = 1;  // text lines on pages after the first
  int linesPage0 = 1;    // text lines on the first page (reduced when an image is shown)
  ButtonNavigator buttonNavigator;

  std::string imagePath;
  std::unique_ptr<ImageBlock> imageBlock;
  bool imageReady = false;
  int imgW = 0;
  int imgH = 0;
};
