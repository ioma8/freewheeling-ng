#include <algorithm>
#include <cmath>

#include "SDL_gfx/SDL_gfxPrimitives.h"

static bool LockSurfaceIfNeeded(SDL_Surface *surface) {
  if (!SDL_MUSTLOCK(surface))
    return true;
  return SDL_LockSurface(surface) == 0;
}

static void UnlockSurfaceIfNeeded(SDL_Surface *surface) {
  if (SDL_MUSTLOCK(surface))
    SDL_UnlockSurface(surface);
}

static void PutPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
  if (surface == 0 || x < 0 || y < 0 || x >= surface->w || y >= surface->h)
    return;

  Uint8 *ptr = static_cast<Uint8 *>(surface->pixels) + y * surface->pitch +
               x * surface->format->BytesPerPixel;
  switch (surface->format->BytesPerPixel) {
  case 1:
    *ptr = static_cast<Uint8>(color);
    break;
  case 2:
    *reinterpret_cast<Uint16 *>(ptr) = static_cast<Uint16>(color);
    break;
  case 3:
    if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
      ptr[0] = static_cast<Uint8>((color >> 16) & 0xFF);
      ptr[1] = static_cast<Uint8>((color >> 8) & 0xFF);
      ptr[2] = static_cast<Uint8>(color & 0xFF);
    } else {
      ptr[0] = static_cast<Uint8>(color & 0xFF);
      ptr[1] = static_cast<Uint8>((color >> 8) & 0xFF);
      ptr[2] = static_cast<Uint8>((color >> 16) & 0xFF);
    }
    break;
  case 4:
    *reinterpret_cast<Uint32 *>(ptr) = color;
    break;
  }
}

static Uint32 MapRGBA(SDL_Surface *surface, Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  return SDL_MapRGBA(surface->format, r, g, b, a);
}

int hlineRGBA(SDL_Surface *surface, Sint16 x1, Sint16 x2, Sint16 y,
              Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0)
    return -1;
  if (x2 < x1)
    std::swap(x1, x2);
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;
  for (int x = x1; x <= x2; ++x)
    PutPixel(surface, x, y, color);
  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int vlineRGBA(SDL_Surface *surface, Sint16 x, Sint16 y1, Sint16 y2,
              Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0)
    return -1;
  if (y2 < y1)
    std::swap(y1, y2);
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;
  for (int y = y1; y <= y2; ++y)
    PutPixel(surface, x, y, color);
  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int boxRGBA(SDL_Surface *surface, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0)
    return -1;
  if (x2 < x1)
    std::swap(x1, x2);
  if (y2 < y1)
    std::swap(y1, y2);
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;
  for (int y = y1; y <= y2; ++y) {
    for (int x = x1; x <= x2; ++x)
      PutPixel(surface, x, y, color);
  }
  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int lineRGBA(SDL_Surface *surface, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
             Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0)
    return -1;
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;

  int dx = std::abs(x2 - x1);
  int dy = std::abs(y2 - y1);
  int sx = (x1 < x2) ? 1 : -1;
  int sy = (y1 < y2) ? 1 : -1;
  int err = dx - dy;

  while (true) {
    PutPixel(surface, x1, y1, color);
    if (x1 == x2 && y1 == y2)
      break;
    const int e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x1 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y1 += sy;
    }
  }

  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int circleRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0 || rad < 0)
    return -1;
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;

  int dx = rad;
  int dy = 0;
  int err = 1 - dx;
  while (dx >= dy) {
    PutPixel(surface, x + dx, y + dy, color);
    PutPixel(surface, x + dy, y + dx, color);
    PutPixel(surface, x - dy, y + dx, color);
    PutPixel(surface, x - dx, y + dy, color);
    PutPixel(surface, x - dx, y - dy, color);
    PutPixel(surface, x - dy, y - dx, color);
    PutPixel(surface, x + dy, y - dx, color);
    PutPixel(surface, x + dx, y - dy, color);
    ++dy;
    if (err < 0) {
      err += 2 * dy + 1;
    } else {
      --dx;
      err += 2 * (dy - dx) + 1;
    }
  }

  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int filledCircleRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0 || rad < 0)
    return -1;
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;

  for (int dy = -rad; dy <= rad; ++dy) {
    const int span = static_cast<int>(std::sqrt(static_cast<double>(rad * rad - dy * dy)));
    for (int dx = -span; dx <= span; ++dx)
      PutPixel(surface, x + dx, y + dy, color);
  }

  UnlockSurfaceIfNeeded(surface);
  return 0;
}

static bool AngleWithinRange(double angle, double start, double end) {
  while (angle < 0.0)
    angle += 360.0;
  while (start < 0.0)
    start += 360.0;
  while (end < 0.0)
    end += 360.0;
  angle = std::fmod(angle, 360.0);
  start = std::fmod(start, 360.0);
  end = std::fmod(end, 360.0);
  if (start <= end)
    return angle >= start && angle <= end;
  return angle >= start || angle <= end;
}

int filledPieRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                  Sint16 start, Sint16 end,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  if (surface == 0 || rad < 0)
    return -1;
  const Uint32 color = MapRGBA(surface, r, g, b, a);
  if (!LockSurfaceIfNeeded(surface))
    return -1;

  for (int dy = -rad; dy <= rad; ++dy) {
    for (int dx = -rad; dx <= rad; ++dx) {
      if (dx * dx + dy * dy > rad * rad)
        continue;
      const double angle = std::atan2(-dy, dx) * 180.0 / M_PI;
      if (AngleWithinRange(angle, start, end))
        PutPixel(surface, x + dx, y + dy, color);
    }
  }

  UnlockSurfaceIfNeeded(surface);
  return 0;
}

int filledpieRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                  Sint16 start, Sint16 end,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
  return filledPieRGBA(surface, x, y, rad, start, end, r, g, b, a);
}
