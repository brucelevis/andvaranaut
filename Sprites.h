#pragma once

#include "Point.h"

typedef struct
{
    Point where;
    int ascii;
}
Sprite;

typedef struct
{
    int count;
    Sprite* sprite;
}
Sprites;

Sprites wake(const char* const name);
void sleep(const Sprites sprites);
Sprites swap(const Sprites sprites, const char* const name);
