// Copyright (C) 2016-2017 Gustav Louw
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.

#include "Point.h"

#include <math.h>

static inline bool
fn(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    // Testing for floating 0.f equality is okay since point was derived from floor and ceil
    return Point_Decimal(point.y) == 0.f && party[y + 0][x + 0] && party[y - 1][x + 0] == 0;
}

static inline bool
fe(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    // Testing for floating 0.f equality is okay since point was derived from floor and ceil
    return Point_Decimal(point.x) == 0.f && party[y + 0][x + 0] == 0 && party[y + 0][x - 1];
}

static inline bool
fs(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    // Testing for floating 0.f equality is okay since point was derived from floor and ceil
    return Point_Decimal(point.y) == 0.f && party[y + 0][x + 0] == 0 && party[y - 1][x + 0];
}

static inline bool
fw(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    // Testing for floating 0.f equality is okay since point was derived from floor and ceil
    return Point_Decimal(point.x) == 0.f && party[y + 0][x + 0] && party[y + 0][x - 1] == 0;
}

static inline bool
Hor(const Point point, uint8_t** const party)
{
    return fn(point, party) || fs(point, party);
}

static inline bool
Ver(const Point point, uint8_t** const party)
{
    return fe(point, party) || fw(point, party);
}

static inline bool
Enclosure(const Point point, uint8_t** const party)
{
    return Hor(point, party) || Ver(point, party);
}

// Fast floor to replace math floor
static inline float
ffloor(const float a)
{
    return (int)a - (a < (int)a);
}

// Fast ceil to replace math ceil
static inline float
fceil(const float a)
{
    return (int)a + (a > (int)a);
}

static inline Point
StepNorth(const Point where, const float m, const float b)
{
    const float y = fceil(where.y - 1.f);
    const float x = (y - b) / m;
    return (Point){ x, y };
}

static inline Point
StepSouth(const Point where, const float m, const float b)
{
    const float y = (int)(where.y + 1.f); // Faster than ffloor
    const float x = (y - b) / m;
    return (Point){ x, y };
}

static inline Point
StepEast(const Point where, const float m, const float b)
{
    const float x = (int)(where.x + 1.f); // Faster than fflor
    const float y = m * x + b;
    return (Point){ x, y };
}

static inline Point
StepWest(const Point where, const float m, const float b)
{
    const float x = fceil(where.x - 1.f);
    const float y = m * x + b;
    return (Point){ x, y };
}

static inline Point
Closest(const Point where, const Point i, const Point j)
{
    const float im = Point_Magnitude(Point_Sub(i, where));
    const float jm = Point_Magnitude(Point_Sub(j, where));
    return im < jm ? i : j;
}

static inline Point
Step(const Point where, const float m, const float b, const int q, uint8_t** const party)
{
    Point point;
    switch(q)
    {
        case 0: point = Closest(where, StepEast(where, m, b), StepSouth(where, m, b)); break;
        case 1: point = Closest(where, StepWest(where, m, b), StepSouth(where, m, b)); break;
        case 2: point = Closest(where, StepWest(where, m, b), StepNorth(where, m, b)); break;
        case 3: point = Closest(where, StepEast(where, m, b), StepNorth(where, m, b)); break;
    }
    // Out of bounds check (Caveat: requires a closed map)
    if(Point_Collision(point, party)) return point;
    // Not out of bounds? Step next if an enclosure is not found
    return Enclosure(point, party) ? point : Step(point, m, b, q, party);
}

static inline int
Quadrant(const float radians)
{

    const float x = cosf(radians);
    const float y = sinf(radians);
    if(x > 0.f && y > 0.f) return 0;
    if(x < 0.f && y > 0.f) return 1;
    if(x < 0.f && y < 0.f) return 2;
    if(x > 0.f && y < 0.f) return 3;
    return -1;
}

Point
Point_Cast(const Point where, const float radians, uint8_t** const party)
{
    const float m = tanf(radians);
    const float b = where.y - m * where.x;
    const float q = Quadrant(radians);
    return Step(where, m, b, q, party);
}

Point
Point_Sub(const Point i, const Point j)
{
    return (Point){ i.x - j.x, i.y - j.y };
}

Point
Point_Add(const Point i, const Point j)
{
    return (Point){ i.x + j.x, i.y + j.y };
}

Point
Point_Mul(const Point i, const float n)
{
    return (Point){ i.x * n, i.y * n };
}

uint8_t
Point_TileEnclosure(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    if(fe(point, party)) return party[y + 0][x - 1];
    if(fs(point, party)) return party[y - 1][x + 0];
    return party[y][x];
}

uint8_t
Point_TileParty(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    return party[y][x];
}

bool
Point_Collision(const Point point, uint8_t** const party)
{
    const int x = point.x;
    const int y = point.y;
    return party[y][x];
}

float
Point_Magnitude(const Point point)
{
    const float x = point.x * point.x;
    const float y = point.y * point.y;
    return sqrtf(x + y);
}

float
Point_Percent(const Point point, uint8_t** const party)
{
    if(fe(point, party)) return 1.f - Point_Decimal(point.y);
    if(fs(point, party)) return 1.f - Point_Decimal(point.x);
    return fn(point, party) ?
        Point_Decimal(point.x):
        Point_Decimal(point.y); // West
}

float
Point_Decimal(const float d)
{
    return d - (int)d;
}
