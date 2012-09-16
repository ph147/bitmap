#ifndef _BLEND_H
#define _BLEND_H

#include <inttypes.h>
#include "bitmap.h"

void blend_mode(image_t *dest, image_t *source, uint8_t (*pt2blend)(uint8_t, uint8_t));
uint8_t xor(uint8_t bottom, uint8_t top);
uint8_t overlay(uint8_t bottom, uint8_t top);
uint8_t screen(uint8_t bottom, uint8_t top);
uint8_t multiply(uint8_t bottom, uint8_t top);
uint8_t add(uint8_t bottom, uint8_t top);
uint8_t subtract(uint8_t bottom, uint8_t top);
uint8_t difference(uint8_t bottom, uint8_t top);
uint8_t darken_only(uint8_t bottom, uint8_t top);
uint8_t lighten_only(uint8_t bottom, uint8_t top);
uint8_t color_dodge(uint8_t bottom, uint8_t top);
uint8_t color_burn(uint8_t bottom, uint8_t top);
uint8_t soft_light(uint8_t bottom, uint8_t top);

#endif
