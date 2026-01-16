#ifndef INTRO_H
#define INTRO_H

#include "global.h"
#include "assets.h"

#define INTRO_TEXT_FRAME_LIMIT 180

#define INTRO_LOGO_FRAMES 5
#define INTRO_LOGO_FRAME_UPDATE 30

#define INTRO_CRAWL_FRAME_UPDATE 3
#define INTRO_CRAWL_FRAME_LIMIT -180

void stateIntroText() {
  // Skip the static "A long time ago" page in UVE5 integration.
  gameState = STATE_INTRO_CRAWL;
  introTextStartMs = 0;
  introCrawlLastStepMs = 0;
  crawlFrameCount = 64;
}

void stateIntroCrawl() {
  ab.clear();
  if (introCrawlLastStepMs == 0) {
    introCrawlLastStepMs = ab.getMillis();
  }

  if(crawlFrameCount <= INTRO_CRAWL_FRAME_LIMIT) {
    gameState = STATE_MENU;
    introTextStartMs = 0;
    introCrawlLastStepMs = 0;
  } else {
    Sprites::drawOverwrite(24, crawlFrameCount, StarWarsLogo, 0);
    ab.setCursor(24, crawlFrameCount + 50);
    for (unsigned char i = 0; i < CRAWL_LINES; i++) {
      ab.println(strcpy_P(tBuffer, (char*)pgm_read_dword(&(crawlText[i]))));
    }

    // Original behavior: move 1px every 3 frames at 60fps => ~20px/s.
    const uint32_t now = ab.getMillis();
    while (crawlFrameCount > INTRO_CRAWL_FRAME_LIMIT && (uint32_t)(now - introCrawlLastStepMs) >= 50U) {
      crawlFrameCount--;
      introCrawlLastStepMs += 50U;
    }
  }
}

#endif
