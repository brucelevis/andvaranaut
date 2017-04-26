#include "Util.h"

int fl(const float x)
{
    return (int) x - (x < (int) x);
}

int cl(const float x)
{
    return (int) x + (x > (int) x);
}

float dec(const float x)
{
    return x - (int) x;
}

int rnd(const float x)
{
    return x + 0.5;
}

int lns(FILE* const file)
{
    int ch, lines = 0, pc = '\n';
    while((ch = getc(file)) != EOF)
    {
        if(ch == '\n')
            lines++;
        pc = ch;
    }
    if(pc != '\n')
        lines++;
    rewind(file);
    return lines;
}

char* readln(FILE* const file)
{
    int ch, reads = 0, size = 64;
    char* line = (char*) malloc(size * sizeof(char));
    while((ch = getc(file)) != '\n' && ch != EOF)
    {
        line[reads++] = ch;
        if(reads + 1 == size)
            line = (char*) realloc(line, size *= 2);
    }
    line[reads] = '\0';
    return line;
}

int illuminate(const float torch, const float distance)
{
    const int mod = torch / (distance * distance);
    return mod > 0xFF ? 0xFF : mod;
}
