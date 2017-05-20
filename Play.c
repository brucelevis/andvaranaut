#include "Play.h"

#include "Hero.h"
#include "Sdl.h"
#include "Map.h"
#include "Sprites.h"
#include "World.h"

static bool done(const uint8_t* const key)
{
    return key[SDL_SCANCODE_F1];
}

void play(const char* argv[])
{
    const int res = strtol(argv[1], NULL, 0);
    const int fps = 60;
    Hero hero = spawn();
    Map map = open(hero.zone);
    Sprites sprites = wake(hero.zone);
    Sdl sdl = setup(res, fps);
    const uint8_t* const key = SDL_GetKeyboardState(NULL);
    for(int renders = 0; res == 128 ? renders < fps : !done(key); renders++)
    {
        const int t0 = SDL_GetTicks();
        SDL_PumpEvents();
        hero = sustain(hero, sprites, map, key);
        map = edit(hero, map, key);
        sprites = place(hero, sprites, key);
        render(sdl, hero, sprites, map);
        sdl = tick(sdl, renders);
        const int t1 = SDL_GetTicks();
        const int ms = 1000.0 / sdl.fps - (t1 - t0);
        SDL_Delay(ms < 0 ? 0 : ms);
    }
    release(sdl);
    close(map);
    kill(sprites);
}
