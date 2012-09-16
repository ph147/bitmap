#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include "bitmap.h"
#include "blend.h"

#define DEBUG printf

#define INPUT "lenna.bmp"
#define OUTPUT "Lenna.bmp"

static void
init_image(image_t **image, uint32_t width, uint32_t height)
{
    size_t i;

    *image = malloc(sizeof(image_t));
    (*image)->matrix = (pixel_24bit_t **) malloc(height*sizeof(pixel_24bit_t *));
    for (i = 0; i < height; ++i)
        (*image)->matrix[i] = (pixel_24bit_t *) malloc(width*sizeof(pixel_24bit_t));

    (*image)->height = height;
    (*image)->width = width;
}

static void
free_image(image_t **image)
{
    size_t i;
    for (i = 0; i < (*image)->height; ++i)
        free((*image)->matrix[i]);
    free((*image)->matrix);
    free(*image);
}

static void
read_headers(FILE *in, file_header_t *header, dib_header_t *dib)
{
    fread(header, sizeof(*header), 1, in);
    fread(dib, sizeof(*dib), 1, in);
    if (dib->compression_method > 0)
    {
        fprintf(stderr, "Unsupported compression method.\n");
        exit(1);
    }
    if (dib->bits_per_pixel != 24)
    {
        fprintf(stderr, "Unsupported depth.\n");
        exit(1);
    }
}

static void
print_pixel(pixel_24bit_t *pixel)
{
    printf("#%2x%2x%2x\n", pixel->red, pixel->green, pixel->blue);
}

static void
pixel_invert(pixel_24bit_t *pixel)
{
    pixel->red = 0xff - pixel->red;
    pixel->green = 0xff - pixel->green;
    pixel->blue = 0xff - pixel->blue;
}

static void
pixel_bw(pixel_24bit_t *pixel)
{
    uint8_t luma = 0.2126*pixel->red + 0.7152*pixel->green + 0.0722*pixel->blue;
    pixel->red = luma;
    pixel->green = luma;
    pixel->blue = luma;
}

static void
pixel_contrast(pixel_24bit_t *pixel, float contrast)
{
    int tmp;

    tmp = pixel->red * contrast;
    pixel->red = tmp > 0xff ? 0xff : tmp;

    tmp = pixel->blue * contrast;
    pixel->blue = tmp > 0xff ? 0xff : tmp;

    tmp = pixel->green * contrast;
    pixel->green = tmp > 0xff ? 0xff : tmp;
}

static void
pixel_red(pixel_24bit_t *pixel)
{
    pixel->blue = 0;
    pixel->green = 0;
}

static void
pixel_2001(pixel_24bit_t *pixel)
{
    pixel->red ^= 0xff;
    pixel->green &= 0x32;
    pixel->blue |= 0x22;
}

static hsl_t
rgb_to_hsl(pixel_24bit_t *pixel)
{
    hsl_t ret;
    float M, m, C, HH, H;
    float L, S;
    float R = pixel->red / 255.0;
    float G = pixel->green / 255.0;
    float B = pixel->blue / 255.0;

    M = R > G ? R : G;
    M = B > M ? B : M;

    m = R < G ? R : G;
    m = B < M ? B : m;

    C = M-m;

    if (!C)
        HH = 0;
    else if (M == R)
    {
        HH = (float)(G-B)/C;
        while (HH > 6)
            HH -= 6;
    }
    else if (M == G)
        HH = (float)(B-R)/C + 2;
    else if (M == B)
        HH = (float)(R-G)/C + 4;

    H = 60*HH;
    if (H < 0)
        H += 360;
    if (H > 360)
        H -= 360;
    L = (float) (M+m)/2.0;
    
    if (!C)
        S = 0;
    else
    {
        if (2*L > 1)
            S = (float)C/(2-2*L);
        else
            S = (float)C/(2*L);
    }

    ret.h = H;
    ret.s = S;
    ret.l = L;

    return ret;
}

static pixel_24bit_t
hsl_to_rgb(hsl_t *hsl)
{
    pixel_24bit_t ret;

    float C;
    float HH, X;

    float h = hsl->h;
    float s = hsl->s;
    float l = hsl->l;

    float m;

    float r, g, b;
    
    float tmp;

    if (2*hsl->l > 1)
        C = (2-2*hsl->l)*hsl->s;
    else
        C = (2*hsl->l)*hsl->s;
    HH = hsl->h/60.0;

    tmp = HH;
    while (tmp >= 2)
        tmp -= 2;
    tmp = tmp - 1;
    if (tmp > 0)
        X = C*(1-tmp);
    else
        X = C*(1+tmp);
    
    if (!h)
        r = g = b = 0;
    else if (HH >= 0 && HH < 1)
    {
        r = C; g = X; b = 0;
    }
    else if (HH >= 1 && HH < 2)
    {
        r = X; g = C; b = 0;
    }
    else if (HH >= 2 && HH < 3)
    {
        r = 0; g = C; b = X;
    }
    else if (HH >= 3 && HH < 4)
    {
        r = 0; g = X; b = C;
    }
    else if (HH >= 4 && HH < 5)
    {
        r = X; g = 0; b = C;
    }
    else if (HH >= 5 && HH < 6)
    {
        r = C; g = 0; b = X;
    }

    m = l - C/2;

    ret.red = (r+m)*255;
    ret.green = (g+m)*255;
    ret.blue = (b+m)*255;

    return ret;
}

static void
pixel_test(pixel_24bit_t *pixel)
{
    hsl_t hsl = rgb_to_hsl(pixel);
    hsl.h += 180;
    if (hsl.h > 360)
        hsl.h -= 360;
    *pixel = hsl_to_rgb(&hsl);
}

static void
read_pixels(FILE *in, file_header_t *header, dib_header_t *dib, image_t **img)
{
    size_t col, row;
    int width = dib->width;
    int height = dib->height;
    size_t size = sizeof(pixel_24bit_t);
    size_t rem;
    pixel_24bit_t pixel;

    init_image(img, dib->width, dib->height);

    fseek(in, header->offset, SEEK_SET);

    for (row = 0; row < height; ++row)
    {
        for (col = 0; col < width; ++col)
            fread(&((*img)->matrix[row][col]), sizeof(pixel_24bit_t), 1, in);
        rem = (width*size) % 4;
        if (rem)
            fread(&((*img)->matrix[row][col]), 4-rem, 1, in);
    }
}

static void
read_data(FILE *in, file_header_t *header, dib_header_t *dib, image_t **img)
{
    read_headers(in, header, dib);
    read_pixels(in, header, dib, img);
}

static void
write_data(FILE *out, file_header_t *header, dib_header_t *dib, image_t **img)
{
    size_t col, row;
    int width = dib->width;
    int height = dib->height;
    size_t size = sizeof(pixel_24bit_t);
    size_t rem;
    pixel_24bit_t pixel;

    fwrite(header, sizeof(*header), 1, out);
    fwrite(dib, sizeof(*dib), 1, out);
    fseek(out, header->offset, SEEK_SET);

    for (row = 0; row < height; ++row)
    {
        for (col = 0; col < width; ++col)
            fwrite(&((*img)->matrix[row][col]), sizeof(pixel_24bit_t), 1, out);
        rem = (width*size) % 4;
        if (rem)
            fwrite(&((*img)->matrix[row][col]), 4-rem, 1, out);
    }
}

static void
filter(image_t *img, void (*pt2filter)(pixel_24bit_t *))
{
    size_t i, j;
    for (i = 0; i < img->height; ++i)
    {
        for (j = 0; j < img->width; ++j)
        {
            pt2filter(&(img->matrix[i][j]));
        }
    }
}

static void
edit_pixels(image_t *img)
{
    size_t i, j;
    for (i = 0; i < img->height; ++i)
    {
        for (j = 0; j < img->width; ++j)
        {
            pixel_2001(&(img->matrix[i][j]));
        }
    }
}

static void
gaussian_blur(image_t *img)
{
    //const float gauss[] = { 0.006, 0.061, 0.242, 0.383, 0.242, 0.061, 0.006 };
    const float gauss[] = { 0.0089, 0.0123, 0.0165, 0.0215, 0.0273, 0.0336, 0.0403, 0.0469, 0.0532, 0.0586, 0.0628, 0.0655, 0.0664, 0.0655, 0.0628, 0.0586, 0.0532, 0.0469, 0.0403, 0.0336, 0.0273, 0.0215, 0.0165, 0.0123, 0.0089 };
    size_t len = sizeof(gauss)/sizeof(float);
    size_t i, j, k, offset;
    pixel_24bit_t sum;
    image_t *tmp;

    init_image(&tmp, img->width, img->height);

    for (i = 0; i < img->height; ++i)
    {
        for (j = 0; j < img->width; ++j)
        {
            sum.red = sum.blue = sum.green = 0;
            /* Only advance one pixel every second cycle
             * if we are near the borders: limitation
             * of gaussian blur. */
            if (j < len)
                offset = -(int)j/2;
            if (j > img->width - len)
                offset = (int)(-j+img->width-2*len)/2;

            for (k = 0; k < len; ++k)
            {
                sum.red += gauss[k]*img->matrix[i][j+k+offset].red;
                sum.blue += gauss[k]*img->matrix[i][j+k+offset].blue;
                sum.green += gauss[k]*img->matrix[i][j+k+offset].green;
            }
            tmp->matrix[i][j].red = sum.red;
            tmp->matrix[i][j].green = sum.green;
            tmp->matrix[i][j].blue = sum.blue;
        }
    }
    for (i = 0; i < img->height; ++i)
        for (j = 0; j < img->width; ++j)
            img->matrix[i][j] = tmp->matrix[i][j];
    for (i = 0; i < img->width; ++i)
    {
        for (j = 0; j < img->height; ++j)
        {
            if (j < len)
                offset = -(int)j/2;
            if (j > img->height - len)
                offset = (int)(-j+img->height-2*len)/2;
            sum.red = sum.blue = sum.green = 0;
            for (k = 0; k < len; ++k)
            {
                sum.red += gauss[k]*img->matrix[j+k+offset][i].red;
                sum.blue += gauss[k]*img->matrix[j+k+offset][i].blue;
                sum.green += gauss[k]*img->matrix[j+k+offset][i].green;
            }
            tmp->matrix[j][i].red = sum.red;
            tmp->matrix[j][i].green = sum.green;
            tmp->matrix[j][i].blue = sum.blue;
        }
    }
    for (i = 0; i < img->height; ++i)
        for (j = 0; j < img->width; ++j)
            img->matrix[i][j] = tmp->matrix[i][j];

    free_image(&tmp);
}

static void
duplicate_layer(image_t **dest, image_t *source)
{
    size_t i, j;

    init_image(dest, source->width, source->height);

    for (i = 0; i < source->height; ++i)
        for (j = 0; j < source->width; ++j)
            (*dest)->matrix[i][j] = source->matrix[i][j];
}

static void
combine(image_t *dest, image_t *source, float opacity)
{
    size_t i, j;

    for (i = 0; i < source->height; ++i)
    {
        for (j = 0; j < source->width; ++j)
        {
            dest->matrix[i][j].red +=
                (int)((source->matrix[i][j].red-dest->matrix[i][j].red)*opacity);
            dest->matrix[i][j].green +=
                (int)((source->matrix[i][j].green-dest->matrix[i][j].green)*opacity);
            dest->matrix[i][j].blue +=
                (int)((source->matrix[i][j].blue-dest->matrix[i][j].blue)*opacity);
        }
    }
}

int
main()
{
    FILE *infile = fopen(INPUT, "r");
    FILE *outfile = fopen(OUTPUT, "w");

    file_header_t header;
    dib_header_t dib;

    image_t *layer1;
    image_t *layer2;

    read_data(infile, &header, &dib, &layer1);

    duplicate_layer(&layer2, layer1);
    filter(layer2, pixel_bw);
    filter(layer2, pixel_invert);
    gaussian_blur(layer2);
    blend_mode(layer2, layer1, overlay);
    combine(layer1, layer2, 1.0);

    write_data(outfile, &header, &dib, &layer1);

    free_image(&layer1);
    free_image(&layer2);

    fclose(infile);
    fclose(outfile);
    return 0;
}
