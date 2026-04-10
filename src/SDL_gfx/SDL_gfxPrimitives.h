#ifndef FWEELIN_SDL_GFX_PRIMITIVES_H
#define FWEELIN_SDL_GFX_PRIMITIVES_H

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

int boxRGBA(SDL_Surface *surface, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
            Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int hlineRGBA(SDL_Surface *surface, Sint16 x1, Sint16 x2, Sint16 y,
              Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int vlineRGBA(SDL_Surface *surface, Sint16 x, Sint16 y1, Sint16 y2,
              Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int lineRGBA(SDL_Surface *surface, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2,
             Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int circleRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
               Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int filledCircleRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                     Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int filledPieRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                  Sint16 start, Sint16 end,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a);
int filledpieRGBA(SDL_Surface *surface, Sint16 x, Sint16 y, Sint16 rad,
                  Sint16 start, Sint16 end,
                  Uint8 r, Uint8 g, Uint8 b, Uint8 a);

#ifdef __cplusplus
}
#endif

#endif
