#pragma once

#include "Field.h"

typedef struct
{
    int y; // Notice y and x are in reverse
    int x; // here, contrary to the Point type.
    float val;
}
Atom;

Atom a_materialize(const Field, const int y, const int x);
