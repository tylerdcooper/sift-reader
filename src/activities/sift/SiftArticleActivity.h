#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <GfxRenderer.h>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

class MappedInputManager;

/**
 * Reads one saved Sift article from the on-SD cache: title + "feed · date", then
 * the body laid out as a flow of wrapped text lines and inline images, paged.
 * Confirm marks it read (deletes it from the device and syncs that back).
 */
class SiftArticleActivity final : public Activity {
 public:
  SiftArticleActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int articleId);

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // One laid-out unit: a single wrapped text line, or an inline image.
  struct Elem {
    bool image = false;
    std::string text;         // text line (when !image)
    int n = 0, w = 0, h = 0;  // inline image index + display size
    int height = 0;           // vertical space this unit occupies
  };

  void buildLayout();
  void paginate();
  int bodyStartY() const;
  int bodyHeightPx() const;
  int imageAreaWidth() const;
  int measureSpan(int fontId, const char* text, size_t len) const;
  void wrapInto(const std::string& text, int fontId, int maxWidth, int lineHeight);
  void markReadAndClose();

  int articleId;
  int imageCount = 0;
  std::string title;
  std::string meta;
  bool loaded = false;

  std::vector<Elem> elems;
  std::vector<int> pageStart;  // index into elems where each page begins
  int currentPage = 0;
  int totalPages = 1;
  ButtonNavigator buttonNavigator;
};
