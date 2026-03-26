#ifndef FWEELIN_SDL_TTF_H
#define FWEELIN_SDL_TTF_H

#include <SDL/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _TTF_Font TTF_Font;

#define TTF_STYLE_NORMAL 0x00

int TTF_Init(void);
void TTF_Quit(void);
TTF_Font *TTF_OpenFont(const char *file, int ptsize);
void TTF_CloseFont(TTF_Font *font);
void TTF_SetFontStyle(TTF_Font *font, int style);
int TTF_FontHeight(TTF_Font *font);
int TTF_SizeText(TTF_Font *font, const char *text, int *w, int *h);
SDL_Surface *TTF_RenderText_Shaded(TTF_Font *font, const char *text, SDL_Color fg, SDL_Color bg);

#ifdef __cplusplus
}
#endif

#endif
