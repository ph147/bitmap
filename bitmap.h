#ifndef _BITMAP_H
#define _BITMAP_H

typedef struct
{
    uint8_t blue;
    uint8_t green;
    uint8_t red;
} __attribute__ ((packed)) pixel_24bit_t;

typedef struct
{
    float h, s, l;
} hsl_t;

typedef struct
{
    uint32_t width;
    uint32_t height;
    pixel_24bit_t **matrix;
} image_t;

typedef struct
{
    uint8_t identifier[2];
    uint32_t size_in_bytes;
    uint16_t reserved[2];
    uint32_t offset;
} __attribute__((packed)) file_header_t;

typedef struct
{
    uint32_t header;
    int32_t width;
    int32_t height;
    uint16_t color_planes;
    uint16_t bits_per_pixel;
    uint32_t compression_method;
    uint32_t image_size;
    int32_t horizontal_res;
    int32_t vertical_res;
    uint32_t colors_in_palette;
    uint32_t important_colors;
} __attribute__((packed)) dib_header_t;

static void init_image(image_t **image, uint32_t width, uint32_t height);
static void free_image(image_t **image);
static void read_headers(FILE *in, FILE *out, file_header_t *header, dib_header_t *dib);
static void print_pixel(pixel_24bit_t *pixel);
static void pixel_invert(pixel_24bit_t *pixel);
static void pixel_bw(pixel_24bit_t *pixel);
static void pixel_contrast(pixel_24bit_t *pixel, float contrast);
static void pixel_red(pixel_24bit_t *pixel);
static void pixel_2001(pixel_24bit_t *pixel);
static hsl_t rgb_to_hsl(pixel_24bit_t *pixel);
static pixel_24bit_t hsl_to_rgb(hsl_t *hsl);
static void pixel_test(pixel_24bit_t *pixel);
static void read_pixels(FILE *in, file_header_t *header, dib_header_t *dib, image_t **img);
static void write_pixels(FILE *out, file_header_t *header, dib_header_t *dib, image_t **img);
static void edit_pixels(image_t *img);
static void gaussian_blur(image_t *img);
static void duplicate_layer(image_t **dest, image_t *source);
static void overlay(image_t *dest, image_t *source, float opacity);
static void xor_images(image_t *dest, image_t *source);

#endif
