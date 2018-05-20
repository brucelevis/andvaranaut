#pragma once

#include "Sdl.h"

typedef struct
{
    Sdl sdl;
    uint32_t* pixels;
    int width;
    // Scanlines are rotated 90 degrees. An x argument would be passed here.
    int y;
}
Scanline;

// Renders the map to the display from the position of the hero. Animates water current.
Point xraster(const Scanline, const Hits, const Hero, const Flow current, const Flow clouds, const Map);
