#pragma once

#include "Point.h"

typedef struct
{
    float acceleration;
    float speed;
    Point direction;
    Point velocity;
    Point where;
    float height;
}
Flow;

Flow xzflow();

Flow xstart(const float height);

Flow xstream(Flow);
