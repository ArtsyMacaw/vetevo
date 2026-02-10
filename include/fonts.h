#ifndef FONTS_H
#define FONTS_H

/* Fonts are ASCII encoded to allow for easier character lookup */
typedef struct {
    int width;
    int height;
    const unsigned char *table;
} font_t;

extern font_t font16;

#endif
