#include "Projection.h"

#include "util.h"

static Clamped clamp(const int yres, const float bot, const float top)
{
    const Clamped clamp = {
        (int) bot < 0 ? 0 : xcl(bot), (int) top > yres ? yres : xfl(top)
    };
    return clamp;
}

Projection xproject(const int yres, const float focal, const float yaw, const Point corrected, const float height)
{
    // The corrected x distance must be clamped to a value small enough otherwise the size will
    // exceed the limitations of single precision floating point. The clamp value is arbitrary.
    const float shift = 0.0;
    const float size = focal * yres / (corrected.x < 1e-5 ? 1e-5 : corrected.x);
    const float mid = yaw * yres / 2.0;
    const float bot = mid + (0.0 - height) * size;
    const float top = mid + (1.0 - height) * size;
    const int level = 0;
    const Projection projection = {
        bot, top, clamp(yres, bot, top), size, height, yres, mid, shift, level
    };
    return projection;
}

Projection xstack(const Projection p, const float shift)
{
    const float bot = p.top - 1.0;
    const float top = p.top - 1.0 + p.size * shift;
    const Projection projection = {
        bot, top, clamp(p.yres, bot, top), p.size, p.height, p.yres, p.mid, p.shift + shift, p.level + 1
    };
    return projection;
}

Projection xdrop(const Projection p, const float shift)
{
    const float top = p.bot + 2.0;
    const float bot = p.bot + 2.0 + p.size * shift;
    const Projection projection = {
        bot, top, clamp(p.yres, bot, top), p.size, p.height, p.yres, p.mid, p.shift + shift, p.level - 1
    };
    return projection;
}

float xccast(const Projection p, const int x)
{
    return (1.0 - p.height + p.shift) * p.size / (x + 1 + p.level - p.mid);
}

float xfcast(const Projection p, const int x)
{
    return (0.0 - p.height + p.shift) * p.size / (x - 1 + p.level - p.mid);
}
