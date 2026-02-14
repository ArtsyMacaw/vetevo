#ifndef FONTS_H
#define FONTS_H

/* Fonts are ASCII encoded to allow for easier character lookup */
typedef struct {
    int width;
    int height;
    const unsigned char *table;
} font_t;

extern font_t font16;
extern font_t font20;
extern font_t font24;
extern font_t font40;
extern font_t font60;

/* Font60 only has 0-9, :, A, P, M for time display to save space
 * Thus isn't ASCII encoded, and the offsets need to be defined manually */
enum {
    ONE = 0,
    TWO = 510,
    THREE = 1020,
    FOUR = 1530,
    FIVE = 2040,
    SIX = 2550,
    SEVEN = 3060,
    EIGHT = 3570,
    NINE = 4080,
    ZERO = 4590,
    COLON = 5100,
    LETTER_A = 5610,
    LETTER_P = 6120,
    LETTER_M = 6630,
};

#endif
