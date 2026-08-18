#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

/**
 * Reads one Sift article: title + "feed · date" header, then the body word-
 * wrapped into pages (buttons / side-taps turn pages, Back returns). Portrait
 * layout for now; plain text (images render next).
 */
class SiftArticleActivity final : public Activity {
 public:
  SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title, std::string meta,
                      std::string body);

  void onEnter() override;
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
  void drawBody(int fontId, int x, int startY) const;

  std::string title;
  std::string meta;
  std::string body;
  std::vector<Line> lines;
  int currentPage = 0;
  int totalPages = 1;
  int linesPerPage = 1;
  ButtonNavigator buttonNavigator;
};
