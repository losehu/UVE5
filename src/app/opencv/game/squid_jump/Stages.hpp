// Stage Builder

void build_platforms() {                                                                                    // Platforms (+ Zapfish)
  struct Platform ground = { 0, HEIGHT - 2 * BLOCK_SIZE, WIDTH / BLOCK_SIZE, REGULAR_PLATFORM, false, 0 };  // Ground always the same
  stage.platforms[0] = ground;
  int lastheight = HEIGHT - 2 * BLOCK_SIZE;

  for (int i = 1; i < stage.totalplatforms; i++) {
    int len;
    int type;
    int typerand;

    switch (stage.num) {
      case 1:  // Wide regular platforms
        len = random(5, 9);
        type = 0;
        break;
      case 2:  // Slightly slimmer
        len = random(4, 8);
        type = 0;
        break;
      case 3:  // With red jump fish
        len = random(4, 8);
        type = 0;
        break;
      case 4:  // With half ice platforms
        len = random(4, 8);
        type = random(2);
        break;
      case 5:  // More ice
        len = random(4, 8);
        typerand = random(3);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = ICE_PLATFORM;
        }
        break;
      case 6:  // Jellies + red
        len = random(4, 8);
        typerand = random(2);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = JELLYFISH_PLATFORM;
        }
        break;
      case 7:  // Smaller jellies
        len = random(4, 7);
        typerand = random(2);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = JELLYFISH_PLATFORM;
        }
        break;
      case 8:  // Blue jellies + jellies + ice + regular
        len = random(4, 7);
        type = random(3);
        break;
      case 9:                // Smaller
        len = random(4, 6);  // 3?
        type = random(3);
        break;
      case 10:  // Regular + starfish
        len = random(4, 6);
        type = REGULAR_PLATFORM;
        break;
      case 11:  // Conveyors + reds
        len = random(5, 7);
        typerand = random(4);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = CONVEYOR_PLATFORM;
        }
        break;
      case 12:  // Same?
        len = random(4, 7);
        typerand = random(4);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = CONVEYOR_PLATFORM;
        }
        break;
      case 13:  // All platforms + starfish
        len = random(4, 7);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 14:  // Reds + starfish + blue jellyfish
        len = random(4, 7);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 15:  // Tiny platforms + starfish
        len = random(2, 5);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 16:  // Small regulars + reds + starfish
        len = random(2, 5);
        type = REGULAR_PLATFORM;
        break;
      case 17:  // Jellies + jellies + regulars + ice
        len = random(2, 8);
        typerand = random(4);
        if (typerand == 3) {
          type = 2;
        } else {
          type = typerand;
        }
        break;
      case 18:  // All (Faster 1.5x speed)
        len = random(4, 7);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 19:  // Regular + ice + starfish
        len = random(4, 7);
        typerand = random(3);
        if (typerand == 0) {
          type = REGULAR_PLATFORM;
        } else {
          type = ICE_PLATFORM;
        }
        break;
      case 20:  // All thin
        len = random(2, 5);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 21:  // All + starfish
        len = random(3, 6);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 22:  // All platforms
        len = random(3, 6);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 23:  // Thin + blues + reds
        len = random(3, 6);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      case 24:  // Fast jellies + conveyors + starfish
        len = random(4, 7);
        typerand = random(2);
        if (typerand == 0) {
          type = JELLYFISH_PLATFORM;
        } else {
          type = CONVEYOR_PLATFORM;
        }
        break;
      case 25:  // Thin + fast + all + starfish + jellies
        len = random(2, 5);
        typerand = random(4);
        if (typerand == 3) {
          type = 4;
        } else {
          type = typerand;
        }
        break;
      default:
        len = random(5, 9);
        type = 0;
        break;
    }

    // Offset 2 and 3 in so that is possible to jump between opposite platforms
    int xlow = (4 - len <= 0) ? 0 : 4 - len;
    int xhigh = ((WIDTH / BLOCK_SIZE - 4 + len >= WIDTH / BLOCK_SIZE) ? WIDTH / BLOCK_SIZE : (WIDTH / BLOCK_SIZE - 4 + len)) - len + 1;

    int x = random(xlow, xhigh) * BLOCK_SIZE;
    int y = lastheight - random(5, 10) * BLOCK_SIZE;
    bool facingright = (type == JELLYFISH_PLATFORM || type == CONVEYOR_PLATFORM) ? random(0, 2) : false;
    byte sprite = 0;

    stage.platforms[i] = { (float)x, y, len, type, facingright, sprite };
    lastheight = y;
  }

  zapfish = { lastheight - ZAP_UP - ZAP_SIZE };
}

void totalpowerups() {
  if ((stage.num >= 3 && stage.num <= 13) || stage.num == 17 || stage.num == 19) {
    stage.totalpowerups = random(1, 3);
  } else if (stage.num == 14 || stage.num == 20) {
    stage.totalpowerups = random(1, 4);
  } else if ((stage.num >= 15 && stage.num <= 18) || stage.num == 21 || (stage.num >= 23 && stage.num <= 25)) {
    stage.totalpowerups = random(3, 7);
  } else {
    stage.totalpowerups = 0;
  }

  // Cap just in case
  if (stage.totalpowerups > MAX_POWERUPS) {
    stage.totalpowerups = MAX_POWERUPS;
  }
}

void build_powerups() {  //Requires platforms to be built first
  // Chooses space between platforms
  for (int i = 0; i < stage.totalpowerups; i++) {
    int aboveplatform = random(1, stage.totalplatforms - 1);  // Won't choose the top or bottom one
    int y = random(stage.platforms[aboveplatform + 1].y + BLOCK_SIZE, stage.platforms[aboveplatform].y - POWERUP_SIZE);

    int x = random(WIDTH - POWERUP_SIZE);
    byte type;  // = random(3);
    int randtype;

    switch (stage.num) {
      case 3:
        type = POWERUP_FISH;
        break;
      case 4:
        type = POWERUP_FISH;
        break;
      case 5:
        type = POWERUP_FISH;
        break;
      case 6:
        type = POWERUP_FISH;
        break;
      case 7:
        type = POWERUP_FISH;
        break;
      case 8:
        type = POWERUP_JELLY;
        break;
      case 9:
        type = POWERUP_JELLY;
        break;
      case 10:
        type = POWERUP_STAR;
        break;
      case 11:
        type = POWERUP_FISH;
        break;
      case 12:
        type = POWERUP_FISH;
        break;
      case 13:
        type = POWERUP_STAR;
        break;
      case 14:
        type = random(3);
        break;
      case 15:
        type = POWERUP_STAR;
        break;
      case 16:
        randtype = random(2);
        if (randtype == 0) {
          type = POWERUP_FISH;
        } else {
          type = POWERUP_STAR;
        }
        break;
      case 17:
        type = POWERUP_JELLY;
        break;
      case 18:
        type = random(3);
        break;
      case 19:
        type = POWERUP_STAR;
        break;
      case 20:
        type = POWERUP_JELLY;
        break;
      case 21:
        type = random(1, 3);
        break;
      case 23:
        type = random(2);
        break;
      case 24:
        type = random(3);
        break;
      case 25:
        type = random(3);
        break;
      default:
        type = 0;
        break;
    }

    // Reroll overlapping powerups
    bool nocollisions = true;
    for (int k = 0; k < i; k++) {
      if (x + POWERUP_SIZE >= stage.powerups[k].x && x <= stage.powerups[k].x + POWERUP_SIZE && y + POWERUP_SIZE >= stage.powerups[k].y && y <= stage.powerups[k].y + POWERUP_SIZE) {
        nocollisions = false;
      }
    }

    if (nocollisions) {
      stage.powerups[i] = { x, y, type };
    } else {
      i--;  // Reroll
    }
  }
}


void resetstage() {  // Either decrease powerups, or increase len
  if (stage.num > 25) {
    stage.num = 25;
  }

  stage.staroffset = random(STAR_WRAP);

  if (stage.num >= 18) {
    stage.speed = 4;
  } else if (stage.num >= 18) {
    stage.speed = 3;
  } else {
    stage.speed = 2;
  }

  totalpowerups();
  stage.totalplatforms = 22 + stage.totalpowerups - (stage.speed - 2);  // Min 22 max 28
  build_platforms();
  build_powerups();

  // Reset states
  player = { (WIDTH - PLAYER_SIZE) / 2, HEIGHT - 2 * BLOCK_SIZE - PLAYER_SIZE, 0, 0, true, 0, 0, 0, 0, false, 1 };
  camerapos = 0;
  poisonheight = 128;
  currentstagetimer = 0;
}