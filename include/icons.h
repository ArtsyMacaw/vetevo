#include <stdint.h>

#ifndef ICONS_H
#define ICONS_H

typedef struct
{
    int width;
    int height;
    uint8_t *data;
} icon_t;

extern const icon_t cloud;
extern const icon_t drizzle;
extern const icon_t rain;
extern const icon_t thunder;
extern const icon_t haze;
extern const icon_t snow;
extern const icon_t sun;
extern const icon_t moon;
extern const icon_t wind;
extern const icon_t cloud_small;
extern const icon_t drizzle_small;
extern const icon_t rain_small;
extern const icon_t thunder_small;
extern const icon_t haze_small;
extern const icon_t snow_small;
extern const icon_t sun_small;
extern const icon_t moon_small;
extern const icon_t raindrop;

#endif
