#include "Sdl.h"

#include "Bar.h"
#include "Frame.h"
#include "Scanline.h"
#include "Bundle.h"
#include "util.h"

Sdl xzsdl()
{
    static Sdl sdl;
    return sdl;
}

// Rotates renderer 90 degrees and copies to backbuffer.
static void churn(const Sdl sdl)
{
    const SDL_Rect dst = {
        (sdl.xres - sdl.yres) / 2,
        (sdl.yres - sdl.xres) / 2,
        sdl.yres,
        sdl.xres,
    };
    SDL_RenderCopyEx(sdl.renderer, sdl.canvas, NULL, &dst, -90, NULL, SDL_FLIP_NONE);
}

// Presents backbuffer to the screen.
void xpresent(const Sdl sdl)
{
    SDL_RenderPresent(sdl.renderer);
}

// Clips a sprite, left and right, based on zbuffer ray caster.
static SDL_Rect clip(const Sdl sdl, const SDL_Rect frame, const Point where, Point* const zbuff)
{
    SDL_Rect seen = frame;

    // Left clip.
    for(; seen.w > 0; seen.w--, seen.x++)
    {
        const int x = seen.x;

        if(x < 0 || x >= sdl.xres)
            continue;

        if(where.x < zbuff[x].x)
            break;
    }

    // Right clip.
    for(; seen.w > 0; seen.w--)
    {
        const int x = seen.x + seen.w;

        if(x < 0 || x >= sdl.xres)
            continue;

        if(where.x < zbuff[x].x)
        {
            seen.w = seen.w + 1;
            break;
        }
    }

    return seen;
}

// Draws a rectangle the slow way.
static void dbox(const Sdl sdl, const int x, const int y, const int width, const uint32_t color, const int filled)
{
    const int a = (color >> 0x18) & 0xFF;
    const int r = (color >> 0x10) & 0xFF;
    const int g = (color >> 0x08) & 0xFF;
    const int b = (color >> 0x00) & 0xFF;

    const SDL_Rect square = { x, y, width, width };
    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);

    filled ?
        SDL_RenderFillRect(sdl.renderer, &square):
        SDL_RenderDrawRect(sdl.renderer, &square);
}

// Renders character speech.
static void rspeech(Sprite* const sprite, const Sdl sdl, const Ttf ttf, const SDL_Rect target, const Timer tm)
{
    const int index = (tm.ticks / 6) % sprite->speech.count;
    const char* const sentence = sprite->speech.sentences[index];

    SDL_Texture* const fill = xtget(ttf.fill, sdl.renderer, 0xFF, sentence);
    SDL_Texture* const line = xtget(ttf.line, sdl.renderer, 0xFF, sentence);

    // Get font dimensions.
    int w = 0;
    int h = 0;
    TTF_SizeText(ttf.fill.type, sentence, &w, &h);

    // Calculate where sentence will be placed on screen.
    const SDL_Rect to = {
        target.x + target.w / 2 - w / 2,
        target.y + target.h / 3, // TODO: Maybe tune the offset per sprite?
        w,
        h,
    };

    // Transfer and cleanup.
    SDL_RenderCopy(sdl.renderer, fill, NULL, &to);
    SDL_RenderCopy(sdl.renderer, line, NULL, &to);
    SDL_DestroyTexture(fill);
    SDL_DestroyTexture(line);
}

// Pastes all visible sprites on screen.
static void paste(const Sdl sdl, const Ttf ttf, const Sprites sprites, Point* const zbuff, const Hero hero, const Timer tm)
{
    for(int which = 0; which < sprites.count; which++)
    {
        Sprite* const sprite = &sprites.sprite[which];

        // Move onto the next sprite if this sprite is behind the player.
        if(sprite->where.x < 0)
            continue;

        // Calculate sprite size - the sprite must be an even integer else the sprite will jitter.
        const float focal = hero.fov.a.x;
        const int size = (focal * sdl.xres / 2.0f) / sprite->where.x;
        const int osize = xodd(size) ? size + 1 : size;

        // Calculate sprite location on screen. Account for hero yaw and height.
        const int my = sdl.yres / 2 * (sprite->state == LIFTED ? 1.0f : (2.0f - hero.yaw));
        const int mx = sdl.xres / 2;
        const int l = mx - osize / 2;
        const int t = my - osize * (sprite->state == LIFTED ? 0.5f : (1.0f - hero.height));
        const int s = hero.fov.a.x * (sdl.xres / 2) * xslp(sprite->where);
        const SDL_Rect target = { l + s, t, osize, osize };

        // Move onto the next sprite if this sprite is off screen.
        if(target.x + target.w < 0 || target.x >= sdl.xres)
            continue;

        // Get sprite surface and texture.
        const int selected = sprite->ascii - ' ';
        SDL_Surface* const surface = sdl.surfaces.surface[selected];
        SDL_Texture* const texture = sdl.textures.texture[selected];
        const int w = surface->w / FRAMES;
        const int h = surface->h / STATES;

        // Determine sprite animation based on ticks.
        const SDL_Rect image = { w * (tm.ticks % FRAMES), h * sprite->state, w, h };

        // Calculate how much of the sprite is seen.
        // The sprite's latest seen rect is then saved to the sprite.
        // This will come in handy for ranged attacks or just general mouse targeting.
        sprite->seen = clip(sdl, target, sprite->where, zbuff);

        // Move onto the next sprite if this totally behind a wall and cannot be seen.
        if(sprite->seen.w <= 0)
            continue;

        // Apply lighting to the sprite.
        const int modding = xilluminate(hero.torch, sprite->where.x);
        SDL_SetTextureColorMod(texture, modding, modding, modding);

        // Apply transparency to the sprite, if required.
        if(sprite->transparent)
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_ADD);

        // Render the sprite.
        SDL_RenderSetClipRect(sdl.renderer, &sprite->seen);
        SDL_RenderCopy(sdl.renderer, texture, &image, &target);
        SDL_RenderSetClipRect(sdl.renderer, NULL);

        // Remove transperancy from the sprite.
        if(sprite->transparent)
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        // Revert lighting to the sprite for the map editor panel.
        SDL_SetTextureColorMod(texture, 0xFF, 0xFF, 0xFF);

        // If the sprite is within earshot of hero then render speech sentences.
        if(!xisdead(sprite->state) && !xismute(sprite))
        {
            // NOTE: Sprites where oriented to players gaze.
            // Their relative position to the player is recalculated.
            if(xeql(xadd(hero.where, sprite->where), hero.where, hero.aura))
                rspeech(sprite, sdl, ttf, target, tm);
        }
    }
}

Sdl xsetup(const Args args)
{
    SDL_Init(SDL_INIT_VIDEO);
    Sdl sdl = xzsdl();

    sdl.window = SDL_CreateWindow(
        "Andvaranaut",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        args.xres,
        args.yres,
        SDL_WINDOW_SHOWN);

    if(sdl.window == NULL)
        xbomb("error: could not open window\n");

    sdl.renderer = SDL_CreateRenderer(
        sdl.window,
        -1,
        // Hardware acceleration.
        SDL_RENDERER_ACCELERATED |
        // Screen Vertical Sync on / off.
        (args.vsync ? SDL_RENDERER_PRESENTVSYNC : 0x0));

    // The canvas texture will be used for per pixel drawings for the walls, floors, and ceiling.
    // Notice the flip between yres and xres for the sdl canvas texture.
    // This was done for fast caching. Upon presenting, the canvas will be rotated upwards by 90 degrees.
    // Notice how ARGB8888 is used for the hardware. This is the fastest option for the CPU / GPU translations via SDL2.
    sdl.canvas = SDL_CreateTexture(
        sdl.renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        args.yres,
        args.xres);

    sdl.xres = args.xres;
    sdl.yres = args.yres;
    sdl.fps = args.fps;
    sdl.threads = args.threads;
    sdl.surfaces = xpull();
    sdl.textures = xcache(sdl.surfaces, sdl.renderer);

    // GUI surfaces start at this index of the surfaces.
    sdl.gui = '~' - ' ' + 25;
    sdl.wht = 0xFFDFEFD7;
    sdl.blk = 0xFF000000;
    sdl.red = 0xFFD34549;
    sdl.yel = 0xFFDBD75D;

    return sdl;
}

void xrender(const Sdl sdl, const Ttf ttf, const Hero hero, const Sprites sprites, const Map map, const Flow current, const Flow clouds, const Timer tm)
{
    // Z-buffer will be populated once the map renderering is finished.
    Point* const zbuff = xtoss(Point, sdl.xres);

    // The display must be locked for per-pixel writes.
    void* screen;
    int pitch;
    SDL_LockTexture(sdl.canvas, NULL, &screen, &pitch);
    const int width = pitch / sizeof(uint32_t);
    uint32_t* pixels = (uint32_t*) screen;

    // The camera is orientated to the players gaze.
    const Line camera = xrotate(hero.fov, hero.theta);

    // Rendering bundles are used for rendering a
    // portion of the map (ceiling, walls, and flooring) to the backbuffer.
    // One thread per CPU is allocated.
    Bundle* const b = xtoss(Bundle, sdl.threads);
    for(int i = 0; i < sdl.threads; i++)
    {
        b[i].a = (i + 0) * sdl.xres / sdl.threads;
        b[i].b = (i + 1) * sdl.xres / sdl.threads;
        b[i].zbuff = zbuff;
        b[i].camera = camera;
        b[i].pixels = pixels;
        b[i].width = width;
        b[i].sdl = sdl;
        b[i].hero = hero;
        b[i].current = current;
        b[i].clouds = clouds;
        b[i].map = map;
    };

    // Launch all threads.
    SDL_Thread** const threads = xtoss(SDL_Thread*, sdl.threads);
    for(int i = 0; i < sdl.threads; i++)
        threads[i] = SDL_CreateThread(xbraster, "n/a", &b[i]);

    // Wait for thread completion.
    for(int i = 0; i < sdl.threads; i++)
    {
        int status; /* Ignored */
        SDL_WaitThread(threads[i], &status);
    }

    // Per-pixel writes for the floor, wall, and ceiling are now
    // complete for the entire backbuffer; unlock the display.
    SDL_UnlockTexture(sdl.canvas);

    // The map was rendered on its side for cache efficiency. Rotate the map upwards.
    churn(sdl);

    // For sprites to be pasted to the screen they must first be orientated
    // to the player's gaze. Afterwards they must be placed back.
    xorient(sprites, hero);
    paste(sdl, ttf, sprites, zbuff, hero, tm);
    xplback(sprites, hero);

    // Cleanup.
    free(zbuff);
    free(b);
    free(threads);
}

// Draws melee gauge and calculates attack.
static Attack dgmelee(const Sdl sdl, const Gauge g, const Item it, const float sens)
{
    // Animate attack.
    for(int i = 0; i < g.count; i++)
    {
        const float growth = i / (float) g.count;
        const int size = 16; // Whatever feels best.
        const int width = growth * size;
        const int green = 0xFF * growth;
        const uint32_t color = (0xFF << 0x10) | (green << 0x08);
        const Point mid = {
            (width - sdl.xres) / 2.0f,
            (width - sdl.yres) / 2.0f,
        };
        const Point where = xsub(xmul(g.points[i], sens), mid);
        dbox(sdl, where.x, where.y, width, color, true);
    }

    const int tail = 10;
    if(g.count < tail)
        return xzattack();

    const float mag = xgmag(g, it.damage);

    // Hurts is a melee property. For instance, more than one enemy can be hurt when a warhammer is used.
    const int last = g.count - 1;
    const Point dir = xunt(xsub(g.points[last], g.points[last - tail]));
    const Attack melee = { mag, dir, it.hurts, MELEE, 0, xzpoint() };
    return melee;
}

// Draws ranged gauge.
static Attack dgrange(const Sdl sdl, const Gauge g, const Item it, const float sens)
{
    if(g.count > 0)
    {
        // Animate attack. Both amplitude and attack are bow properties.
        const int state = it.amplitude / 2.0f;
        const int width = it.amplitude * xsinc(g.count, it.period) + state;

        // <Hurts> is also a bow property. Longbows, for instance, can hurt more than one sprite.
        const int x = g.points[g.count - 1].x * sens - (width - sdl.xres) / 2;
        const int y = g.points[g.count - 1].y * sens - (width - sdl.yres) / 2;
        dbox(sdl, x, y, width, sdl.wht, false);

        // Calculate range attack based on sinc steady state.
        // TODO: Fix this.
        const float mag = 100.0f;

        // A random point in the target reticule is chosen.
        const Point reticule = {
            (float) (x + rand() % (width < 1 ? 1 : width)), // Divide by zero check.
            (float) (y + rand() % (width < 1 ? 1 : width)),
        };

        // Range attacks will just have south facing hurt animation drawn.
        const Point dir = { 0.0f, -1.0f };

        const Attack range = { mag, dir, it.hurts, RANGE, 0, reticule };

        return range;
    }
    else return xzattack();
}

// Draws magic gauge.
static Attack dgmagic(const Sdl sdl, const Gauge g, const Item it, const float sens, const Inventory inv, const Scroll sc)
{
    // Casting scroll is cleared as it will be fully drawn here.
    xsclear(sc);
    if(g.count > 0)
    {
        const int size = (sc.width - 1) / 2;

        // Pixels for the grid size.
        const int grid = sdl.yres / (sc.width / 0.8f);

        // Middle of the screen.
        const Point middle = { (float) sdl.xres / 2, (float) sdl.yres / 2 };

        // Half a grid for centering grid squares.
        const Point shift = { (float) grid / 2, (float) grid / 2 };

        // Animate attack (inside square for mouse cursor).
        for(int i = 0; i < g.count; i++)
        {
            const Point point = xadd(xmul(g.points[i], sens), shift);
            const Point corner = xsnap(point, grid);
            const Point center = xsub(xadd(corner, middle), shift);

            dbox(sdl, center.x, center.y, grid, sdl.wht, true);

            // Populate Scroll int array. Was cleared earlier.
            const int x = size + corner.x / grid;
            const int y = size + corner.y / grid;

            // Clamp if over boundry.
            const int xc = x < 0 ? 0 : x >= sc.width ? sc.width - 1 : x;
            const int yc = y < 0 ? 0 : y >= sc.width ? sc.width - 1 : y;
            sc.casting[xc + yc * size] = 1;
        }

        // Animate attack (grid squares).
        for(int x = -size; x <= size; x++)
        for(int y = -size; y <= size; y++)
        {
            const Point which = {
                (float) x,
                (float) y,
            };
            const Point corner = xmul(which, grid);
            const Point center = xsub(xadd(corner, middle), shift);
            dbox(sdl, center.x, center.y, grid, sdl.red, false);
        }

        // Draw the cursor.
        const Point point = xadd(xmul(g.points[g.count - 1], sens), shift);
        const Point center = xsub(xadd(point, middle), shift);
        dbox(sdl, center.x, center.y, 6, sdl.red, true);
    }

    // Calculate attack.
    // Runs through scroll int array and checks for error with all scroll int array shapes.
    // The magic scroll closest to the drawn gauge shape is placed as a scroll index.
    const int scindex = xsindex(sc);

    // Maybe accuracy to scroll gives better attack + item base attack?
    (void) it;
    const float mag = 0.0f;

    // If the scroll index is not found in the inventory the attack will go from MAGIC to NOATTACK.
    (void) inv;

    // Direction won't be needed for magic attacks as magic will spawn new sprites, be it food sprites,
    // attack sprites (fire / ice), etc.
    const Point dir = { 0.0f, 0.0f };
    const Attack magic = { mag, dir, 0, MAGIC, scindex, xzpoint() };
    return magic;
}

// Draws all power gauge squares.
Attack xdgauge(const Sdl sdl, const Gauge g, const Inventory inv, const Scroll sc)
{
    const Item it = inv.items.item[inv.selected];
    const float sens = 2.0f;
    return
        xismelee(it.c) ? dgmelee(sdl, g, it, sens) :
        xisrange(it.c) ? dgrange(sdl, g, it, sens) :
        // Magic wand needs inventory access for checking against scrolls.
        xismagic(it.c) ? dgmagic(sdl, g, it, sens, inv, sc) :
        xzattack();
}

// Returns true if a tile is clipped off the screen.
static int clipping(const Sdl sdl, const Overview ov, const SDL_Rect to)
{
    return (to.x > sdl.xres || to.x < -ov.w)
        && (to.y > sdl.yres || to.y < -ov.h);
}

// Draw tiles for the grid layout.
static void dgridl(const Sdl sdl, const Overview ov, const Sprites sprites, const Map map, const Timer tm)
{
    // Clear renderer and draw overview tiles.
    SDL_SetRenderDrawColor(sdl.renderer, 0x00, 0x00, 0x00, 0x00);
    SDL_RenderClear(sdl.renderer);
    for(int j = 0; j < map.rows; j++)
    for(int i = 0; i < map.cols; i++)
    {
        // Walling will default if anything other 1, 2, or 3 is selected.
        const int ascii =
            ov.party == FLORING ? map.floring[j][i] :
            ov.party == CEILING ? map.ceiling[j][i] : map.walling[j][i];

        // If empty space then skip the tile.
        const int ch = ascii - ' ';

        if(ch == 0)
            continue;

        // Otherwise render the tile.
        const SDL_Rect to = { ov.w * i + ov.px, ov.h * j + ov.py, ov.w, ov.h };

        if(clipping(sdl, ov, to))
            continue;

        SDL_RenderCopy(sdl.renderer, sdl.textures.texture[ch], NULL, &to);
    }

    // Put down sprites. Sprites will not snap to the grid.
    for(int s = 0; s < sprites.count; s++)
    {
        Sprite* const sprite = &sprites.sprite[s];
        const int index = sprite->ascii - ' ';
        const int w = sdl.surfaces.surface[index]->w / FRAMES;
        const int h = sdl.surfaces.surface[index]->h / STATES;

        const SDL_Rect from = { w * (tm.ticks % FRAMES), h * sprite->state, w, h };

        // Right above cursor.
        const SDL_Rect to = {
            (int) ((ov.w * sprite->where.x - ov.w / 2) + ov.px),
            (int) ((ov.h * sprite->where.y - ov.h / 1) + ov.py),
            ov.w, ov.h,
        };

        if(clipping(sdl, ov, to))
            continue;

        SDL_RenderCopy(sdl.renderer, sdl.textures.texture[index], &from, &to);
    }
}

// Draw the selection panel.
static void dpanel(const Sdl sdl, const Overview ov, const Timer tm)
{
    for(int i = ov.wheel; i <= '~' - ' '; i++)
    {
        const SDL_Rect to = { ov.w * (i - ov.wheel), 0, ov.w, ov.h };

        // Draw sprite on panel.
        if(xsissprite(i + ' '))
        {
            const int w = sdl.surfaces.surface[i]->w / FRAMES;
            const int h = sdl.surfaces.surface[i]->h / STATES;

            const SDL_Rect from = { w * (tm.ticks % FRAMES), h * IDLE, w, h };

            if(clipping(sdl, ov, to))
                continue;

            SDL_RenderCopy(sdl.renderer, sdl.textures.texture[i], &from, &to);
        }
        // Draw tile block on [anel.
        else SDL_RenderCopy(sdl.renderer, sdl.textures.texture[i], NULL, &to);
    }
}

// View the map editor.
void xview(const Sdl sdl, const Overview ov, const Sprites sprites, const Map map, const Timer tm)
{
    dgridl(sdl, ov, sprites, map, tm);
    dpanel(sdl, ov, tm);
}

// Draws status bar (one of health, magic, or fatigue).
void xdbar(const Sdl sdl, const Hero hero, const int position, const Timer tm, const int size, const Bar bar)
{
    const int max =
        bar == HPS ? hero.hpsmax :
        bar == FTG ? hero.ftgmax : hero.mnamax;

    const float lvl =
        bar == HPS ? hero.hps :
        bar == FTG ? hero.ftg : hero.mna;

    const float threshold = hero.warning * max;

    SDL_Texture* const texture = sdl.textures.texture[sdl.gui];
    SDL_Surface* const surface = sdl.surfaces.surface[sdl.gui];

    const int w = surface->w;

    const SDL_Rect gleft = { 0,  0, w, w };
    const SDL_Rect glass = { 0, 32, w, w };
    const SDL_Rect grite = { 0, 64, w, w };

    // Will animate bar to flicker if below threshold.
    const int frame = tm.ticks % 2 == 0;
    const SDL_Rect fluid = { 0, (int) bar + (lvl < threshold ? w * frame : 0), w, w };
    const SDL_Rect empty[] = {
        { 0, fluid.y + 2 * w, w, w }, // 75%.
        { 0, fluid.y + 4 * w, w, w }, // 50%.
        { 0, fluid.y + 6 * w, w, w }, // 25%.
    };

    for(int i = 0; i < max; i++)
    {
        const int ww = size * w;
        const int yy = sdl.yres - ww * (1 + position);
        const SDL_Rect to = { ww * i, yy, ww, ww };

        // Full fluid level.
        if(xfl(lvl) > i)
            SDL_RenderCopy(sdl.renderer, texture, &fluid, &to);

        // Partial fluid level.
        if(xfl(lvl) == i)
            xdec(lvl) > 0.75f ? SDL_RenderCopy(sdl.renderer, texture, &empty[0], &to) :
            xdec(lvl) > 0.50f ? SDL_RenderCopy(sdl.renderer, texture, &empty[1], &to) :
            xdec(lvl) > 0.25f ? SDL_RenderCopy(sdl.renderer, texture, &empty[2], &to) : 0;

        // Glass around fluid.
        SDL_RenderCopy(sdl.renderer, texture, i == 0 ? &gleft : i == max - 1 ? &grite : &glass, &to);
    }
}

// Draws all hero status bars (health, mana, and fatigue).
void xdbars(const Sdl sdl, const Hero hero, const Timer tm)
{
    const int size = 1;
    xdbar(sdl, hero, 2, tm, size, HPS);
    xdbar(sdl, hero, 1, tm, size, MNA);
    xdbar(sdl, hero, 0, tm, size, FTG);
}

// Draws the inventory backpanel. Selected inventory item is highlighted.
static void dinvbp(const Sdl sdl, const Inventory inv)
{
    const Point wht = { 0.0, 512.0 };
    const Point red = { 0.0, 528.0 };
    const Point grn = { 0.0, 544.0 };
    for(int i = 0; i < inv.items.max; i++)
    {
        SDL_Texture* const texture = sdl.textures.texture[sdl.gui];
        SDL_Surface* const surface = sdl.surfaces.surface[sdl.gui];

        const int w = surface->w;
        const int xx = sdl.xres - inv.width;

        const SDL_Rect from = {
            (int) (i == inv.hilited ? grn.x : i == inv.selected ? red.x : wht.x),
            (int) (i == inv.hilited ? grn.y : i == inv.selected ? red.y : wht.y),
            w, w
        };

        const SDL_Rect to = { xx, inv.width * i, inv.width, inv.width };
        SDL_RenderCopy(sdl.renderer, texture, &from, &to);
    }
}

// Draws the inventory items.
static void dinvits(const Sdl sdl, const Inventory inv)
{
    for(int i = 0; i < inv.items.max; i++)
    {
        const Item item = inv.items.item[i];

        if(item.c == NONE)
            continue;

        const int index = xcindex(item.c);
        SDL_Texture* const texture = sdl.textures.texture[index];
        SDL_Surface* const surface = sdl.surfaces.surface[index];

        const int w = surface->w;
        const int xx = sdl.xres - inv.width;
        const SDL_Rect from = { 0, w * item.index, w, w }, to = { xx, inv.width * i, inv.width, inv.width };

        SDL_RenderCopy(sdl.renderer, texture, &from, &to);
    }
}

// Draws the inventory.
void xdinv(const Sdl sdl, const Inventory inv)
{
    dinvbp(sdl, inv);
    dinvits(sdl, inv);
}

// Draws the map rooms.
static void drooms(uint32_t* pixels, const int width, const Map map, const uint32_t in, const uint32_t out)
{
    for(int y = 1; y < map.rows - 1; y++)
    for(int x = 1; x < map.cols - 1; x++)
    {
        // Paint the walls.
        if(map.walling[y][x] != ' ' && map.walling[y][x + 1] == ' ') pixels[x + y * width] = out;
        if(map.walling[y][x] != ' ' && map.walling[y][x - 1] == ' ') pixels[x + y * width] = out;
        if(map.walling[y][x] != ' ' && map.walling[y + 1][x] == ' ') pixels[x + y * width] = out;
        if(map.walling[y][x] != ' ' && map.walling[y - 1][x] == ' ') pixels[x + y * width] = out;

        // Paint the free space.
        if(map.walling[y][x] == ' ') pixels[x + y * width] = in;

        // Doors must not be drawn.
        if(map.walling[y][x] == '!') pixels[x + y * width] = in;
    }
}

// Like dbox, but with per pixel access for streaming targets. Does not use SDL_Rect.
static void ddot(uint32_t* pixels, const int width, const Point where, const int size, const uint32_t in, const uint32_t out)
{
    for(int y = -size; y <= size; y++)
    for(int x = -size; x <= size; x++)
    {
        const int xx = x + where.x;
        const int yy = y + where.y;

        pixels[xx + yy * width] = in;

        if(x == -size) pixels[xx + yy * width] = out;
        if(x == +size) pixels[xx + yy * width] = out;
        if(y == -size) pixels[xx + yy * width] = out;
        if(y == +size) pixels[xx + yy * width] = out;
    }
}

// Draws the map.
void xdmap(const Sdl sdl, const Map map, const Point where)
{
    // This map texture is not apart of the Sdl struct since it must be refreshed each frame via creation and destruction.
    SDL_Texture* const texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, map.cols, map.rows);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    // Lock.
    void* screen;
    int pitch;
    SDL_LockTexture(texture, NULL, &screen, &pitch);
    const int width = pitch / sizeof(uint32_t);
    uint32_t* pixels = (uint32_t*) screen;

    // Draw rooms and hero dot.
    drooms(pixels, width, map, sdl.wht, sdl.blk);
    ddot(pixels, width, where, 3, sdl.red, sdl.blk);

    // Unlock and send.
    SDL_UnlockTexture(texture);
    const SDL_Rect dst = { 0, 0, map.cols, map.rows };
    SDL_RenderCopy(sdl.renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}
