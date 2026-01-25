#pragma once

void sound(int pitch, unsigned char duration);
void clearserialbuffer();

void readcustommap(unsigned char l);
void savecustommap(unsigned char l);
void compressmap();
void clearcompressed();
void expandmap(unsigned char l, boolean iscustom);

void onenter();
void onleave();
void tostate(uint8_t button, unsigned char state);

void drawbox();
void editorinput();
void die();
void gameplayinput();

void drawnumber(unsigned char x, unsigned char y, unsigned char value);
void drawdude();
void draweditor();
void drawui();
void drawmap();

void credits();
void systems();
void title();
void menu();
void select();

void ending1();
void ending2();
void ending3();
void ending4();
void ending5();

void checkcompletion();
void change(unsigned char from, unsigned char to);
void swapall(unsigned char from, unsigned char to);
void loadlevel(unsigned char l, boolean iscustom);

void editor();
void gameplay();
void gameloop();

