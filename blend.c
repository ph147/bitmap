#include <stdio.h>
#include <inttypes.h>
#include "bitmap.h"

/*
 * http://www.simplefilter.de/en/basics/mixmods.html
 * http://blog.deepskycolors.com/archivo/2010/04/21/formulas-for-Photoshop-blending-modes.html
 * http://www.pegtop.net/delphi/articles/blendmodes/
 * http://en.wikipedia.org/wiki/Blend_modes
 */

void
blend_mode(image_t *dest, image_t *source, uint8_t (*pt2blend)(uint8_t, uint8_t))
{
    size_t i, j;

    for (i = 0; i < source->height; ++i)
    {
        for (j = 0; j < source->width; ++j)
        {
            dest->matrix[i][j].red = pt2blend(dest->matrix[i][j].red,
                    source->matrix[i][j].red);
            dest->matrix[i][j].green = pt2blend(dest->matrix[i][j].green,
                    source->matrix[i][j].green);
            dest->matrix[i][j].blue = pt2blend(dest->matrix[i][j].blue,
                    source->matrix[i][j].blue);
        }
    }
}

uint8_t
xor(uint8_t bottom, uint8_t top)
{
    return bottom ^ top;
}

uint8_t
overlay(uint8_t bottom, uint8_t top)
{
    if (bottom < 128)
        return (2*top*bottom/255);
    else
        return (255-2*(255-top)*(255-bottom)/255);
}

uint8_t
screen(uint8_t bottom, uint8_t top)
{
    return 255 - (((255-top)*(255-bottom))/255);
}

uint8_t
multiply(uint8_t bottom, uint8_t top)
{
    return (top * bottom) / 255;
}

uint8_t
add(uint8_t bottom, uint8_t top)
{
    if (top + bottom < 256)
        return top + bottom;
    else
        return 255;
}

uint8_t
subtract(uint8_t bottom, uint8_t top)
{
    if (top > bottom)
        return 0;
    else
        return bottom - top;
}

uint8_t
difference(uint8_t bottom, uint8_t top)
{
    if (top > bottom)
        return top - bottom;
    else
        return bottom - top;
}

uint8_t
darken_only(uint8_t bottom, uint8_t top)
{
    if (top < bottom)
        return top;
    else
        return bottom;
}

uint8_t
lighten_only(uint8_t bottom, uint8_t top)
{
    if (top < bottom)
        return bottom;
    else
        return top;
}

uint8_t
color_dodge(uint8_t bottom, uint8_t top)
{
    if (top == 255 || bottom/(255-top) > 255)
        return 255;
    return bottom/(255-top);
}

uint8_t
color_burn(uint8_t bottom, uint8_t top)
{
    if (top == 0 || (255-bottom)/top > 255)
        return 0;
    return 255 - (255-bottom)/top;
}

uint8_t
soft_light(uint8_t bottom, uint8_t top)
{
    if (top < 128)
        return multiply(bottom, top + 128);
    else
        return 255 - multiply(255 - bottom, 255 - (top - 128));
}
