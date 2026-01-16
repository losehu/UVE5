#pragma once

// Forward declarations for the Squid Jump sketch.
// When building as a real Arduino `.ino`, the build system auto-generates these,
// but when we include the sketch into a `.cpp` we need them explicitly.

void titleinput();
void titlescreen();
void startgame();
void gameinput();
bool powerupcollision(struct Powerup powerup);
void physics();
void poison();
void zap();
void powerups();
void nextlevel();
void movecamera();
bool cull(struct Platform platform);
bool cullpup(struct Powerup powerup);
bool cullzap(struct Zapfish zapfish);
bool cullstar(int y);
void drawgame();
void gameplay();
void updatetops();
void pause();
void gameloop();
void setup();
void loop();
