#pragma once

#ifdef ESP8266
#include <avr/pgmspace.h>
//#define pgm_read_ptr pgm_read_word
#else
#include <stdint.h>
#include <string.h>
#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(x) (*((const uint8_t*)(x)))
#endif
#ifndef pgm_read_word
#define pgm_read_word(x) (*((const uint16_t*)(x)))
#endif
#ifndef pgm_read_ptr
#define pgm_read_ptr(x) (*((const uintptr_t*)(x)))
#endif
#ifndef strlen_P
#define strlen_P(x) strlen(x)
#endif
#endif

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64

#define TARGET_FRAMERATE 30
#define DEV_MODE 0

#define INPUT_LEFT 1
#define INPUT_RIGHT 2
#define INPUT_UP 4
#define INPUT_DOWN 8
#define INPUT_A 16
#define INPUT_B 32

#define COLOUR_WHITE 1
#define COLOUR_BLACK 0

#define FIXED_ANGLE_MAX 256

#define CAMERA_SCALE 1
#define CLIP_PLANE 32
#define CLIP_ANGLE 32
#define NEAR_PLANE_MULTIPLIER 130
#define NEAR_PLANE (DISPLAY_WIDTH * NEAR_PLANE_MULTIPLIER / 256)
#define HORIZON (DISPLAY_HEIGHT / 2)

#define CELL_SIZE 256
#define PARTICLES_PER_SYSTEM 8
#define BASE_SPRITE_SIZE 16
#define MAX_SPRITE_SIZE (DISPLAY_HEIGHT / 2)
#define MIN_TEXTURE_DISTANCE 4
#define MAX_QUEUED_DRAWABLES 12

#define TURN_SPEED 3
