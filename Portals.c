#include "Portals.h"

#include "util.h"

#include <stdlib.h>
#include <string.h>

Portals populate(const char* const name)
{
    char* const path = strcon("config/", name);
    FILE* const file = fopen(path, "r");
    const int count = lns(file);
    Portal* const portal = (Portal*) calloc(count, sizeof(*portal));
    for(int i = 0; i < count; i++)
    {
        Point where = { 0.0, 0.0 };
        char* const line = readln(file);
        char* const location = strtok(line, " ");
        sscanf(location, "%f,%f", &where.x, &where.y);
        portal[i].where = where;
        portal[i].name = strnew(strtok(NULL, " #"));
        free(line);
    }
    free(path);
    fclose(file);
    const Portals portals = { count, portal };
    return portals;
}

void destroy(const Portals portals)
{
    for(int i = 0; i < portals.count; i++)
        free(portals.portal[i].name);
    free(portals.portal);
}
