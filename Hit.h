#pragma once

#include "Point.h"
#include "Compass.h"

typedef struct
{
    int surface;
    float offset;
    Point where;
    Compass facing;
}
Hit;

Hit march(const Point where, const Point direction, char** const walling);
