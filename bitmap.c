#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include "bitmap.h"
#include "llist.h"
#include "blend.h"

// TODO image_t → layer_t's, opacity → write_data → auto combine
// TODO does layer exist?
// TODO is 1st layer at top or bottom?

#define DEBUG printf

#define INPUT "lenna.bmp"
#define OUTPUT "Lenna.bmp"

static image_t
*new_image(uint32_t width, uint32_t height)
{
    image_t *ret = malloc(sizeof(image_t));
    ret->layers = new_list();
    ret->width = width;
    ret->height = width;

    // TODO
    ret->header = NULL;
    ret->dib = NULL;
    return ret;
}

layer_t *layer(image_t *img, size_t n)
{
    size_t i;
    node *tmp = img->layers->head;
    for (i = 0; i < n-1; ++i)
    {
        if (tmp == NULL)
            return NULL;
        tmp = tmp->next;
    }
    return tmp == NULL ? NULL : tmp->value;
}

static void
init_layer(layer_t **layer, uint32_t width, uint32_t height)
{
    size_t i;

    *layer = malloc(sizeof(layer_t));
    (*layer)->matrix = (pixel_24bit_t **) malloc(height*sizeof(pixel_24bit_t *));
    for (i = 0; i < height; ++i)
        (*layer)->matrix[i] = (pixel_24bit_t *) malloc(width*sizeof(pixel_24bit_t));

    (*layer)->height = height;
    (*layer)->width = width;
}

static void
free_layer(layer_t **layer)
{
    size_t i;
    for (i = 0; i < (*layer)->height; ++i)
        free((*layer)->matrix[i]);
    free((*layer)->matrix);
    free(*layer);
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
read_pixels(FILE *in, file_header_t *header, dib_header_t *dib, layer_t **layer)
{
    size_t col, row;
    int width = dib->width;
    int height = dib->height;
    size_t size = sizeof(pixel_24bit_t);
    size_t rem;
    pixel_24bit_t pixel;

    init_layer(layer, dib->width, dib->height);

    fseek(in, header->offset, SEEK_SET);

    for (row = 0; row < height; ++row)
    {
        for (col = 0; col < width; ++col)
            fread(&((*layer)->matrix[row][col]), sizeof(pixel_24bit_t), 1, in);
        rem = (width*size) % 4;
        if (rem)
            fread(&((*layer)->matrix[row][col]), 4-rem, 1, in);
    }
}

static image_t
*read_data(FILE *in) //, file_header_t *header, dib_header_t *dib, layer_t **layer)
{
    image_t *ret;
    file_header_t *header = malloc(sizeof(file_header_t));
    dib_header_t *dib = malloc(sizeof(dib_header_t));
    layer_t *layer;

    read_headers(in, header, dib);
    read_pixels(in, header, dib, &layer);

    ret = new_image(dib->width, dib->height);
    ret->header = header;
    ret->dib = dib;
    add_node(ret->layers, layer);
    return ret;
}

static void
write_data(FILE *out, image_t *img) //, file_header_t *header, dib_header_t *dib, layer_t **layer)
{
    pixel_24bit_t pixel;
    size_t width, height;
    size_t col, row;
    size_t rem;
    size_t size = sizeof(pixel_24bit_t);
    dib_header_t *dib = img->dib;
    file_header_t *header = img->header;
    // TODO combine layers
    layer_t *layer = img->layers->head->value;

    dib->width = layer->width;
    dib->height = layer->height;

    width = dib->width;
    height = dib->height;

    fwrite(header, sizeof(*header), 1, out);
    fwrite(dib, sizeof(*dib), 1, out);
    fseek(out, header->offset, SEEK_SET);

    for (row = 0; row < height; ++row)
    {
        for (col = 0; col < width; ++col)
            fwrite(&(layer->matrix[row][col]), sizeof(pixel_24bit_t), 1, out);
        rem = (width*size) % 4;
        if (rem)
            fwrite(&(layer->matrix[row][col]), 4-rem, 1, out);
    }
}

static void
filter(layer_t *layer, void (*pt2filter)(pixel_24bit_t *))
{
    size_t i, j;
    for (i = 0; i < layer->height; ++i)
        for (j = 0; j < layer->width; ++j)
            pt2filter(&(layer->matrix[i][j]));
}

static void
edit_pixels(layer_t *layer)
{
    size_t i, j;
    for (i = 0; i < layer->height; ++i)
        for (j = 0; j < layer->width; ++j)
            pixel_2001(&(layer->matrix[i][j]));
}

static void
gaussian_blur(layer_t *layer)
{
    //const float gauss[] = { 0.006, 0.061, 0.242, 0.383, 0.242, 0.061, 0.006 };
    const float gauss[] = { 0.0089, 0.0123, 0.0165, 0.0215, 0.0273, 0.0336, 0.0403, 0.0469, 0.0532, 0.0586, 0.0628, 0.0655, 0.0664, 0.0655, 0.0628, 0.0586, 0.0532, 0.0469, 0.0403, 0.0336, 0.0273, 0.0215, 0.0165, 0.0123, 0.0089 };
    size_t len = sizeof(gauss)/sizeof(float);
    size_t i, j, k, offset;
    pixel_24bit_t sum;
    layer_t *tmp;

    init_layer(&tmp, layer->width, layer->height);

    for (i = 0; i < layer->height; ++i)
    {
        for (j = 0; j < layer->width; ++j)
        {
            sum.red = sum.blue = sum.green = 0;
            /* Only advance one pixel every second cycle
             * if we are near the borders: limitation
             * of gaussian blur. */
            if (j < len)
                offset = -(int)j/2;
            if (j > layer->width - len)
                offset = (int)(-j+layer->width-2*len)/2;

            for (k = 0; k < len; ++k)
            {
                sum.red += gauss[k]*layer->matrix[i][j+k+offset].red;
                sum.blue += gauss[k]*layer->matrix[i][j+k+offset].blue;
                sum.green += gauss[k]*layer->matrix[i][j+k+offset].green;
            }
            tmp->matrix[i][j].red = sum.red;
            tmp->matrix[i][j].green = sum.green;
            tmp->matrix[i][j].blue = sum.blue;
        }
    }
    for (i = 0; i < layer->height; ++i)
        for (j = 0; j < layer->width; ++j)
            layer->matrix[i][j] = tmp->matrix[i][j];
    for (i = 0; i < layer->width; ++i)
    {
        for (j = 0; j < layer->height; ++j)
        {
            if (j < len)
                offset = -(int)j/2;
            if (j > layer->height - len)
                offset = (int)(-j+layer->height-2*len)/2;
            sum.red = sum.blue = sum.green = 0;
            for (k = 0; k < len; ++k)
            {
                sum.red += gauss[k]*layer->matrix[j+k+offset][i].red;
                sum.blue += gauss[k]*layer->matrix[j+k+offset][i].blue;
                sum.green += gauss[k]*layer->matrix[j+k+offset][i].green;
            }
            tmp->matrix[j][i].red = sum.red;
            tmp->matrix[j][i].green = sum.green;
            tmp->matrix[j][i].blue = sum.blue;
        }
    }
    for (i = 0; i < layer->height; ++i)
        for (j = 0; j < layer->width; ++j)
            layer->matrix[i][j] = tmp->matrix[i][j];

    free_layer(&tmp);
}

static void
pixel_max(pixel_t *cur, pixel_t *new)
{
    if (new->red > cur->red)
        cur->red = new->red;
    if (new->blue > cur->blue)
        cur->blue = new->blue;
    if (new->green > cur->green)
        cur->green = new->green;
}

static void
pixel_min(pixel_t *cur, pixel_t *new)
{
    if (new->red < cur->red)
        cur->red = new->red;
    if (new->blue < cur->blue)
        cur->blue = new->blue;
    if (new->green < cur->green)
        cur->green = new->green;
}

static void
sharpen(layer_t *layer)
{
    const float sharp[3][3] = {
        { -0.0625, -0.125, -0.0625 },
        {  -0.125,    1.0,  -0.125 },
        { -0.0625, -0.125, -0.0625 }
    };
    size_t i, j, k, l;
    pixel_t sum;
    pixel_t min = {0, 0, 0};
    pixel_t max = {0, 0, 0};
    layer_t *tmp;

    init_layer(&tmp, layer->width, layer->height);

    for (i = 0; i < layer->height-3; ++i)
    {
        for (j = 0; j < layer->width-3; ++j)
        {
            sum.red = sum.blue = sum.green = 0;
            for (k = 0; k < 3; ++k)
            {
                for (l = 0; l < 3; ++l)
                {
                    sum.red += sharp[k][l]*layer->matrix[i+k][j+l].red;
                    sum.blue += sharp[k][l]*layer->matrix[i+k][j+l].blue;
                    sum.green += sharp[k][l]*layer->matrix[i+k][j+l].green;
                }
            }
            pixel_max(&max, &sum);
            pixel_min(&min, &sum);
            tmp->matrix[i][j].red = (uint8_t) (sum.red+80)*0.2;
            tmp->matrix[i][j].blue = (uint8_t) (sum.blue+80)*0.2;
            tmp->matrix[i][j].green = (uint8_t) (sum.green+80)*0.2;
        }
    }
    printf("min: %d, %d, %d\n", min.red, min.green, min.blue);
    printf("max %d, %d, %d\n", max.red, max.green, max.blue);

    for (i = 0; i < layer->height; ++i)
    {
        for (j = 0; j < layer->width; ++j)
        {
            layer->matrix[i][j].red += tmp->matrix[i][j].red;
            layer->matrix[i][j].blue += tmp->matrix[i][j].blue;
            layer->matrix[i][j].green += tmp->matrix[i][j].green;
        }
    }

    free_layer(&tmp);
}

static void
noise(layer_t *layer)
{
    size_t i, j;
    uint8_t value;

    srand(time(NULL));

    for (i = 0; i < layer->height; ++i)
    {
        for (j = 0; j < layer->width; ++j)
        {
            value = (rand()%2) * 255;
            layer->matrix[i][j].red = value;
            layer->matrix[i][j].blue = value;
            layer->matrix[i][j].green = value;
        }
    }
}

static void
duplicate_layer(layer_t **dest, layer_t *source)
{
    size_t i, j;

    init_layer(dest, source->width, source->height);

    for (i = 0; i < source->height; ++i)
        for (j = 0; j < source->width; ++j)
            (*dest)->matrix[i][j] = source->matrix[i][j];
}

static void
combine(layer_t *dest, layer_t *source, float opacity)
{
    size_t i, j;
    size_t height = MIN(source->height, dest->height);
    size_t width = MIN(source->width, dest->width);

    for (i = 0; i < height; ++i)
    {
        for (j = 0; j < width; ++j)
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

static void
scale(layer_t *dest, layer_t *source, float factor)
{
    size_t i, j;

    for (i = 0; i < source->height; ++i)
        for (j = 0; j < source->width; ++j)
            dest->matrix[(int)(i*factor)][(int)(j*factor)] = source->matrix[i][j];
}

static void
rotate_180(layer_t *layer)
{
    size_t i, j;
    layer_t *tmp;

    duplicate_layer(&tmp, layer);

    for (i = 0; i < layer->height; ++i)
        for (j = 0; j < layer->width; ++j)
            layer->matrix[i][j] = tmp->matrix[layer->height-i-1][layer->width-j-1];

    free_layer(&tmp);
}

static void
rotate_90(layer_t **layer)
{
    size_t i, j;
    layer_t *dupl;
    layer_t *ret;
    layer_t *tmp;

    duplicate_layer(&dupl, *layer);
    init_layer(&ret, dupl->height, dupl->width);

    for (i = 0; i < dupl->width; ++i)
        for (j = 0; j < dupl->height; ++j)
            ret->matrix[i][j] = dupl->matrix[j][dupl->width-i-1];

    tmp = *layer;
    *layer = ret;
    free_layer(&tmp);

    free_layer(&dupl);
}

static void
rotate_270(layer_t **layer)
{
    size_t i;
    for (i = 0; i < 3; ++i)
        rotate_90(layer);
}

int
main()
{
    FILE *infile = fopen(INPUT, "r");
    FILE *outfile = fopen(OUTPUT, "w");

    file_header_t header;
    dib_header_t dib;

    image_t *img = read_data(infile);
    //duplicate_layer(img, layer(img, 1));
    gaussian_blur(layer(img, 1));

    /*
    layer_t *layer1;
    layer_t *layer2;

    // TODO read from/to layer or image?
    read_data(infile, &header, &dib, &layer1);

    //filter(layer1, pixel_bw);
    duplicate_layer(&layer2, layer1);
    noise(layer2);
    gaussian_blur(layer2);
    blend_mode(layer2, layer1, overlay);
    sharpen(layer1);
    duplicate_layer(&layer2, layer1);
    gaussian_blur(layer2);
    //filter(layer2, pixel_bw);
    //filter(layer2, pixel_invert);
    blend_mode(layer2, layer1, soft_light);
    combine(layer1, layer2, 0.6);
    filter(layer1, pixel_bw);
    filter(layer1, pixel_invert);
    //rotate_90(&layer1);

    */
    //write_data(outfile, &header, &dib, &(img->layers->head->value));
    write_data(outfile, img);

    /*
    free_layer(&layer1);
    free_layer(&layer2);
    */

    fclose(infile);
    fclose(outfile);
    return 0;
}
