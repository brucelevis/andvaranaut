#pragma once

typedef struct
{
    char** sentences;
    int count;
    int max;
}
Speech;

Speech xspappend(Speech, const char* sentence);
