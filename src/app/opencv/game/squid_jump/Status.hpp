void drawlivesandlevel() {
  for (int i = 0; i < lives; i++) {  // Draw lives
    // ((WIDTH - LIFE_SIZE - (lives-1) * LIFE_SPACING) / 2) + LIFE_SPACING * i)
    Sprites::drawOverwrite((WIDTH - LIFE_SIZE) / 2 - (lives - 1) * (LIFE_SPACING / 2) + LIFE_SPACING * i, (HEIGHT - LIFE_SIZE) / 2, Life, 0);
  }

  setcursor((WIDTH - (GetNumberOfDigits(stage.num) + 7) * CHAR_WIDTH)/2, HEIGHT / 4 * 3 - CHAR_HEIGHT / 2);
  squidprint("LEVEL: ");
  squidprint(stage.num);
}

void drawpoints() {
  setcursor(WIDTH / 2 - 24 - (GetNumberOfDigits(stagebonus) + 1) * CHAR_WIDTH / 2, HEIGHT / 2 - CHAR_HEIGHT / 2);
  squidprint("+");
  squidprint(stagebonus);
  setcursor(WIDTH / 2 + 24 - (GetNumberOfDigits(timebonus) + 1) * CHAR_WIDTH / 2, HEIGHT / 2 - CHAR_HEIGHT / 2);
  squidprint("+");
  squidprint(timebonus);

  setcursor((WIDTH - CHAR_WIDTH * (7 + GetNumberOfDigits(score))) / 2, HEIGHT / 4 * 3 - CHAR_HEIGHT / 2);
  squidprint("SCORE: ");
  scoresquidprint(score);
}

void drawhighscore() {
  setcursor((WIDTH - CHAR_WIDTH * (7 + GetNumberOfDigits(score))) / 2, HEIGHT / 2 - CHAR_HEIGHT / 2);
  squidprint("SCORE: ");
  scoresquidprint(score);

  setcursor((WIDTH - CHAR_WIDTH * (9 + GetNumberOfDigits(topscore))) / 2, HEIGHT / 4 * 3 - CHAR_HEIGHT / 2);
  squidprint("HISCORE: ");
  scoresquidprint(topscore);
}


void setupstatus(int event) {
  if (event == GAMEOVER || event == CONGRATS) {
    proceedwithA = true;
  } else {
    proceedwithA = false;
  }

  status = event;
  statustimer = 180;
  statustimermax = 180;
  gamestate = GAME_STATUS;
}

void gamestatus() {
  if (status == MISS) {
    if (led) {
      arduboy.digitalWriteRGB(RGB_ON, RGB_OFF, RGB_OFF);
    }
    setcursor(WIDTH / 2 - 2 * CHAR_WIDTH, HEIGHT / 4 - CHAR_HEIGHT / 2);
    squidprint("MISS");
  } else if (status == GOAL) {
    if (led) {
      arduboy.digitalWriteRGB(RGB_OFF, RGB_ON, RGB_OFF);
    }
    setcursor(WIDTH / 2 - 2 * CHAR_WIDTH, HEIGHT / 4 - CHAR_HEIGHT / 2);
    squidprint("GOAL");
  } else if (status == START) {
    if (led) {
      arduboy.digitalWriteRGB(RGB_OFF, RGB_ON, RGB_OFF);
    }
    setcursor(WIDTH / 2 - 3 * CHAR_WIDTH, HEIGHT / 4 - CHAR_HEIGHT / 2);
    squidprint("START!");
  } else if (status == GAMEOVER) {
    if (led) {
      arduboy.digitalWriteRGB(RGB_ON, RGB_OFF, RGB_ON);
    }
    setcursor(WIDTH / 2 - 5 * CHAR_WIDTH, HEIGHT / 4 - CHAR_HEIGHT / 2);
    squidprint("GAME OVER!");
  } else if (status == CONGRATS) {
    if (led) {
      arduboy.digitalWriteRGB(RGB_ON, RGB_ON, RGB_OFF);
    }
    setcursor(WIDTH / 2 - 8 * CHAR_WIDTH, HEIGHT / 4 - CHAR_HEIGHT / 2);
    squidprint("CONGRATULATIONS!");
  }

  if (statustimer > statustimermax / 2) {  // 1st half
    if (status == START || status == MISS) {
      drawlivesandlevel();
    } else if (status == GOAL) {
      drawpoints();
    } else if (status == CONGRATS) {
      drawpoints();
    } else if (status == GAMEOVER) {
      drawhighscore();
    }
  } else {  // 2nd half
    if (status == START || status == MISS || status == GOAL) {
      drawlivesandlevel();
    } else if (status == GAMEOVER || status == CONGRATS) {
      drawhighscore();
    }
  }

  // if (arduboy.justPressed(A_BUTTON)) {  // Just for testing?
  //   statustimer = 0;
  // }

  if (proceedwithA) {
    if (arduboy.justPressed(A_BUTTON)) {
      arduboy.digitalWriteRGB(RGB_OFF, RGB_OFF, RGB_OFF);
      gamestate = GAME_TITLE;
    }
  } else if (statustimer <= 0) {
    arduboy.digitalWriteRGB(RGB_OFF, RGB_OFF, RGB_OFF);
    if (stage.num <= 25) {
      gamestate = GAME_PLAY;
    }
  }

  if (statustimer > 0) {
    statustimer--;
  }
}
