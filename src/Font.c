#include "Font.h"

#include "util.h"

Font xfzero()
{
    static Font font;
    return font;
}

Font xfbuild(const char* const path, const int size, const uint32_t color, const int outlined)
{
    if(!TTF_WasInit())
        TTF_Init();
    Font f = xfzero();
    f.type = TTF_OpenFont(path, size);
    if(f.type == NULL)
        xbomb("Could not open %s\n", path);
    // Inside color of font.
    f.color.r = (color >> 0x10) & 0xFF;
    f.color.g = (color >> 0x08) & 0xFF;
    f.color.b = (color >> 0x00) & 0xFF;
    // Font Outlining.
    printf("%d\n", outlined);
    TTF_SetFontOutline(f.type, outlined);
    return f;
}

static SDL_Rect tmid(const Font f, const int x, const int y, const char* const text)
{
    int w = 0;
    int h = 0;
    TTF_SizeText(f.type, text, &w, &h);
    const SDL_Rect target = {
        x - w / 2,
        y - h / 2,
        w,
        h,
    };
    return target;
}

static SDL_Texture* tget(const Font f, const Sdl sdl, const char* text)
{
    SDL_Surface* const surface = TTF_RenderText_Solid(f.type, text, f.color);
    SDL_Texture* const texture = SDL_CreateTextureFromSurface(sdl.renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

static void xfput(const Font f, const Sdl sdl, const int x, const int y, const char* const text, const int alpha)
{
    SDL_Texture* texture = tget(f, sdl, text);
    //SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, alpha < 0 ? 0 : alpha);
    const SDL_Rect where = tmid(f, x, y, text);
    SDL_RenderCopy(sdl.renderer, texture, NULL, &where);
    SDL_DestroyTexture(texture);
}

void xfwrt(const Font fill, const Font line, const Sdl sdl, const int x, const int y, const char* const text, const int alpha)
{
    xfput(fill, sdl, x, y, text, alpha);
    xfput(line, sdl, x, y, text, alpha);
}