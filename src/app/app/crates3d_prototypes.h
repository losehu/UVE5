#pragma once

void read_map(int stop_level);
void load();
void save();

void draw_block_slow(int x, int y);
void draw_block(int x, int y);
void draw_black_slow(int x, int y);
void draw_black(int x, int y);
void draw_map(int xmap, int ymap, int zuser, int angle);

int read_keys();
int check_block(int x, int y, int z);
int check_move(int newx, int newy, int newz);
void move_block(int oldx, int oldy, int oldz, int newx, int newy);
void move(int dir);

void print(int x, int y, char *s);
void title_screen();
int gameloop();

