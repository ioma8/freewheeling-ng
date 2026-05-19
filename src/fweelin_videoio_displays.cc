/*
   Art is the beginning.
*/

/* Copyright 2004-2011 Jan Pekau

   This file is part of Freewheeling.

   Freewheeling is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   Freewheeling is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Freewheeling.  If not, see <http://www.gnu.org/licenses/>. */

#include <sys/stat.h>
#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include "fweelin_string_utils.h"
#include "fweelin_videoio.h"
#include "fweelin_core.h"
#include "fweelin_paramset.h"

void FloDisplayPanel::Draw(SDL_Surface *screen,
                           const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_sx = metrics.ScaleX(sx);
  int draw_sy = metrics.ScaleY(sy);
  int draw_margin = metrics.ScaleX(margin);

  boxRGBA(screen,
          draw_x,draw_y,draw_x+draw_sx,draw_y+draw_sy,
          0,0,0,190);
  vlineRGBA(screen,draw_x,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,draw_x+draw_sx,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font == 0 || font->font == 0) {
    printf("VIDEO: WARNING: No font specified for parameter set.\n");
    return;
  }

  int textheight = 0;
  TTF_SizeText(font->font,VERSION,0,&textheight);

  // Draw title
  if (title != 0)
    VideoIO::draw_text(screen,font->font,
                       title,draw_x+draw_sx-draw_margin,draw_y,titleclr,2,0);
}

void LoopTray::Draw(SDL_Surface *screen,
                    const FweelinRenderMetrics &metrics) {
  const static SDL_Color borderclr = {100, 100, 90, 0};

  LockBrowser();

  // **** Add event to change placenames when 'video-show-loop' is called

  VideoIO *vid = app->getVIDEO();

  // Generate circular map for loop tray
  if (loopmap == 0)
    loopmap = vid->CreateMap(vid->getLSCOPEPIC(), metrics.ScaleX(loopsize));

  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_iconsize = metrics.ScaleX(iconsize);
  int draw_xpand_x1 = metrics.ScaleX(xpand_x1);
  int draw_xpand_y1 = metrics.ScaleY(xpand_y1);
  int draw_xpand_x2 = metrics.ScaleX(xpand_x2);
  int draw_xpand_y2 = metrics.ScaleY(xpand_y2);
  int draw_basepos = metrics.ScaleX(basepos);

  // Draw iconified version
  boxRGBA(screen,draw_x,draw_y,draw_x+draw_iconsize,draw_y+draw_iconsize,
          borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_iconsize,draw_y,40,40,40,255);
  hlineRGBA(screen,draw_x,draw_x+draw_iconsize,draw_y+draw_iconsize,40,40,40,255);
  vlineRGBA(screen,draw_x,draw_y,draw_y+draw_iconsize,40,40,40,255);
  vlineRGBA(screen,draw_x+draw_iconsize,draw_y,draw_y+draw_iconsize,40,40,40,255);
  FILLED_PIE(screen,draw_x+draw_iconsize/2,draw_y+draw_iconsize/2,
             draw_iconsize*3/8,
             30,359,
             0xF9,0xE6,0x13,255);
  circleRGBA(screen,draw_x+draw_iconsize/2,draw_y+draw_iconsize/2,
             draw_iconsize*3/8,
             40,40,40, 255); // Outline

  if (xpanded) {
    // Draw background
    {
      int xpand_yb1 = draw_xpand_y1+draw_basepos,
        xpand_yb2 = draw_xpand_y2-draw_basepos,
        xpand_xb1 = draw_xpand_x1+draw_basepos,
        xpand_xb2 = draw_xpand_x2-draw_basepos;

      boxRGBA(screen,draw_xpand_x1,draw_xpand_y1,xpand_xb1,draw_xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_xb1,draw_xpand_y1,draw_xpand_x2,draw_xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,draw_xpand_x1,draw_xpand_y1,draw_xpand_x2,xpand_yb1,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,draw_xpand_x1,xpand_yb2,draw_xpand_x2,draw_xpand_y2,
              borderclr.r,borderclr.g,borderclr.b,255);
      boxRGBA(screen,xpand_xb1,xpand_yb1,xpand_xb2,xpand_yb2,0,0,0,255);
    }

    hlineRGBA(screen,draw_xpand_x1,draw_xpand_x2,draw_xpand_y1,40,40,40,255);
    hlineRGBA(screen,draw_xpand_x1,draw_xpand_x2,draw_xpand_y2,40,40,40,255);
    vlineRGBA(screen,draw_xpand_x1,draw_xpand_y1,draw_xpand_y2,40,40,40,255);
    vlineRGBA(screen,draw_xpand_x2,draw_xpand_y1,draw_xpand_y2,40,40,40,255);

    LoopTrayItem *curl = (LoopTrayItem *) first;

    // If necessary, recalc positions for loops
    if (touchtray) {
      char go = 1;
      int curx = basepos, cury = basepos;
      int loopjump = loopsize+basepos;

      // Space for loops?
      if (curx >= xpand_x2-xpand_x1-loopjump ||
          cury >= xpand_y2-xpand_y1-loopjump)
        go = 0;

      while (curl != 0) {
        if (go) {
          curl->xpos = curx;
          curl->ypos = cury;

          // Move to next spot
          curx += loopjump;
          if (curx >= xpand_x2-xpand_x1-loopjump) {
            curx = basepos;
            cury += loopjump;
            if (cury >= xpand_y2-xpand_y1-loopjump)
              go = 0;
          }
        } else {
          curl->xpos = -1;
          curl->ypos = -1;
        }

        curl = (LoopTrayItem *) curl->next;
      }

      touchtray = 0;
    }

    // Draw items
    curl = (LoopTrayItem *) first;
    char go = 1;
    while (curl != 0 && go) {
      if (curl->xpos != -1)
        Draw_Item(screen,curl,xpand_x1+curl->xpos,xpand_y1+curl->ypos,metrics);
      else
        go = 0;

      curl = (LoopTrayItem *) curl->next;
    }
  }

  UnlockBrowser();
};

void LoopTray::Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y,
                         const FweelinRenderMetrics &metrics) {
  const static float loop_colorbase = 0.5;
  const static SDL_Color white = { 0xEF, 0xAF, 0xFF, 0 };
  const static SDL_Color cursorclr = { 0xEF, 0x11, 0x11, 0 };
  static SDL_Color loop_color[4] = { { 0x62, 0x62, 0x62, 0 },
                                     { 0xF9, 0xE6, 0x13, 0 },
                                     { 0xFF, 0xFF, 0xFF, 0 },
                                     { 0xE0, 0xDA, 0xD5, 0 } };

  LoopManager *loopmgr = app->getLOOPMGR();
  LoopTrayItem *li = (LoopTrayItem *) i;
  int draw_x = metrics.ScaleX(x);
  int draw_y = metrics.ScaleY(y);
  int draw_loopsize = metrics.ScaleX(loopsize);

  float colormag;
  char loopexists;
  if (loopmgr->GetSlot(li->loopid) || loopmgr->IsActive(li->loopid)) {
    loopexists = 1;
    colormag = loop_colorbase + loopmgr->GetTriggerVol(li->loopid);
    if (colormag > 1.0)
      colormag = 1.0;
  } else {
    loopexists = 0;
    colormag = loop_colorbase;
  }

  // Draw loop
  if (loopexists) {
    if (!app->getVIDEO()->
        DrawLoop(loopmgr,li->loopid,screen,app->getVIDEO()->getLSCOPEPIC(),
                 loop_color,colormag,app->getCFG(),0,loopmap,x,y,
                 app->getMASTERLIMITER()->GetLimiterVolume(),0)) {
      // Place name
      if (li->placename != 0)
        VideoIO::draw_text(screen,font->font,li->placename,draw_x,draw_y,white);

      // Loop name
      if (xpand_liney == -1)
        TTF_SizeText(font->font,VERSION,0,&xpand_liney);

      // printf("cur: %p\n",cur);
      if (i == cur && renamer != 0) {
        RenameUIVars *rui = renamer->UpdateUIVars();

        // Draw text with cursor
        int sx, sy;
        int txty = draw_y+draw_loopsize-xpand_liney;
        const char *curn = renamer->GetCurName();
        if (*curn != '\0')
          VideoIO::draw_text(screen,font->font,curn,
                             draw_x,txty,white,0,0,&sx,&sy);
        else {
          sx = 0;
          sy = xpand_liney;
        }

        if (rui->rename_cursor_toggle)
          boxRGBA(screen,
                  draw_x+sx,txty,
                  draw_x+sx+sy/2,txty+sy,
                  cursorclr.r,cursorclr.g,cursorclr.b,255);
      } else if (li->name != 0)
        VideoIO::draw_text(screen,font->font,li->name,
                           draw_x,draw_y+draw_loopsize-xpand_liney,
                           white);
    }
  }

  // Browser::Draw_Item(screen,i,x,y);
};

// Draw browser display
void Browser::Draw_Item(SDL_Surface *screen, BrowserItem *i, int x, int y,
                        const FweelinRenderMetrics &metrics) {
  const static SDL_Color white = { 0xEF, 0xAF, 0xFF, 0 };
  const static SDL_Color cursorclr = { 0x77, 0x77, 0x77, 0 };
  const static unsigned int tmp_size = 256;
  static char tmp[tmp_size];
  int draw_x = metrics.ScaleX(x);
  int draw_y = metrics.ScaleY(y);

  if (font != 0 && font->font != 0 && i != 0) {
    switch (i->GetType()) {
    case B_Patch :
      {
        PatchItem *p = (PatchItem *) i;

        // Current patch
        snprintf(tmp,tmp_size,"%02d: %s",p->id,p->name);
        VideoIO::draw_text(screen,font->font,tmp,draw_x,draw_y,white);
      }
      break;

    case B_Division :
      break;

    default :
      {
        if (i->GetType() == B_Loop)
          snprintf(tmp,tmp_size,"%s-",FWEELIN_OUTPUT_LOOP_NAME);
        else if (i->GetType() == B_Scene)
          snprintf(tmp,tmp_size,"%s-",FWEELIN_OUTPUT_SCENE_NAME);
        else
          tmp[0] = '\0';

        if (i == cur && renamer != 0) {
          RenameUIVars *rui = renamer->UpdateUIVars();

          fweelin_append_truncate(tmp, tmp_size, renamer->GetCurName());

          // Draw text with cursor
          int sx, sy;
          VideoIO::draw_text(screen,font->font,tmp,draw_x,draw_y,white,0,0,
                             &sx,&sy);

          if (rui->rename_cursor_toggle)
            boxRGBA(screen,
                    draw_x+sx,draw_y,
                    draw_x+sx+sy/2,draw_y+sy,
                    cursorclr.r,cursorclr.g,cursorclr.b,255);
        } else if (i->name != 0) {
          fweelin_append_truncate(tmp, tmp_size, i->name);
          VideoIO::draw_text(screen,font->font,tmp,draw_x,draw_y,white);
        }
      }
      break;
    }
  }
};

// Draw browser display
void Browser::Draw(SDL_Surface *screen,
                   const FweelinRenderMetrics &metrics) {
  LockBrowser();

  if (xpanded) {
    int draw_xpand_x1 = metrics.ScaleX(xpand_x1);
    int draw_xpand_y1 = metrics.ScaleY(xpand_y1);
    int draw_xpand_x2 = metrics.ScaleX(xpand_x2);
    int draw_xpand_y2 = metrics.ScaleY(xpand_y2);

    // Draw expanded view

    // Dim the background
    for (int i = draw_xpand_y1; i <= draw_xpand_y2; i++)
      hlineRGBA(screen,draw_xpand_x1,draw_xpand_x2,i,0,0,0,200);
    hlineRGBA(screen,draw_xpand_x1,draw_xpand_x2,draw_xpand_y1,127,127,127,255);
    hlineRGBA(screen,draw_xpand_x1,draw_xpand_x2,draw_xpand_y2,127,127,127,255);
    vlineRGBA(screen,draw_xpand_x1,draw_xpand_y1,draw_xpand_y2,127,127,127,255);
    vlineRGBA(screen,draw_xpand_x2,draw_xpand_y1,draw_xpand_y2,127,127,127,255);

    if (cur != 0) {
      BrowserItem *sp_1 = cur,
        *sp_2 = cur;

      // Compute text height and center of expanded window-- once!
      int liney = xpand_liney;
      if (liney == -1) {
        TTF_SizeText(font->font,VERSION,0,&xpand_liney);
        liney = xpand_liney;
      }
      int xpand_centery = (draw_xpand_y1 + draw_xpand_y2) / 2;
      int xpand_spread = MIN(xpand_centery - draw_xpand_y1,
                             draw_xpand_y2 - xpand_centery);
      xpand_spread /= liney;

      int ofs_1 = 0;
      boxRGBA(screen,
              draw_xpand_x1,xpand_centery,
              draw_xpand_x2,xpand_centery+liney,
              127,0,0,255);
      Draw_Item(screen,sp_1,xpand_x1,
                xpand_centery-liney*ofs_1,metrics);
      while (ofs_1 < xpand_spread && sp_1->prev != 0) {
        sp_1 = sp_1->prev;
        ofs_1++;
        Draw_Item(screen,sp_1,xpand_x1,
                  xpand_centery-liney*ofs_1,metrics);
      }
      int ofs_2 = 0;
      while (ofs_2 < xpand_spread-1 && sp_2->next != 0) {
        sp_2 = sp_2->next;
        ofs_2++;
        Draw_Item(screen,sp_2,xpand_x1,
                  xpand_centery+liney*ofs_2,metrics);
      }
    }

    if (mygettime()-xpand_lastactivity >= xpand_delay) {
      // No activity, close expanded window
      xpanded = 0;
    }
  }

  // Draw single-line view
  Draw_Item(screen,cur,xpos,ypos,metrics);

  UnlockBrowser();
}

void FloDisplayParamSet::Draw(SDL_Surface *screen,
                              const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color barclr = { 0xFF, 0x50, 0x20, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_sx = metrics.ScaleX(sx);
  int draw_sy = metrics.ScaleY(sy);
  int draw_margin = metrics.ScaleX(margin);

  boxRGBA(screen,
          draw_x,draw_y,draw_x+draw_sx,draw_y+draw_sy,
          0,0,0,190);
  vlineRGBA(screen,draw_x,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,draw_x+draw_sx,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font == 0 || font->font == 0) {
    printf("VIDEO: WARNING: No font specified for parameter set.\n");
    return;
  }

  int textheight = 0;
  TTF_SizeText(font->font,VERSION,0,&textheight);

  // Draw title
  if (title != 0)
    VideoIO::draw_text(screen,font->font,
                       title,draw_x+draw_sx-draw_margin,draw_y,titleclr,2,0);

  if (curbank < numbanks) {
    ParamSetBank *b = &banks[curbank];

    if (b->name != 0)
      VideoIO::draw_text(screen,font->font,banks[curbank].name,
          draw_x+draw_margin,draw_y,titleclr,0,0);

    // Draw bars for all active parameters in this bank
    int spacing = (draw_sx - draw_margin*2) / numactiveparams,
        cury = draw_y + draw_sy - draw_margin,
        curbary = cury - textheight - draw_margin;

    int maxheight = draw_sy - draw_margin*3 - textheight*3;
    float barscale = maxheight / b->maxvalue;
    int thickness = spacing / 4;

    int curx = draw_x + thickness*2 + draw_margin;

    for (int i = b->firstparamidx;
        i < b->numparams && i < b->firstparamidx + numactiveparams; i++, curx += spacing) {
      // Param name
      if (b->params[i].name != 0)
        VideoIO::draw_text(screen,font->font,b->params[i].name,
            curx,cury,titleclr,1,2);

      float lvl = b->params[i].value * barscale;

      // Show max value
      boxRGBA(screen,
              curx-thickness/2,curbary,
              curx+thickness/2,curbary-maxheight,
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
          curx-thickness,curbary,
          curx+thickness,curbary-lvl,
          barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
          curx-thickness/2,curbary,
          curx+thickness/2,curbary-lvl,
          barclr.r,barclr.g,barclr.b,255);
    }
  }
};

// Draw text display
void FloDisplayText::Draw(SDL_Surface *screen,
                          const FweelinRenderMetrics &metrics) {
  static char tmp[255];
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color valclr = { 0xDF, 0xEF, 0x20, 0 };

  if (font != 0 && font->font != 0) {
    int xofs = 0, yofs = 0;

    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,metrics.ScaleX(xpos),metrics.ScaleY(ypos),
                         titleclr,0,1,
                         &xofs,&yofs);

    // Draw value
    UserVariable val = exp->Evaluate(0);
    val.Print(tmp,255);
    VideoIO::draw_text(screen,font->font,
                       tmp,metrics.ScaleX(xpos)+xofs,metrics.ScaleY(ypos),
                       valclr,0,1);
  }
};

// Draw switch display
void FloDisplaySwitch::Draw(SDL_Surface *screen,
                            const FweelinRenderMetrics &metrics) {
  const static SDL_Color title1clr = { 0xDF, 0xEF, 0x20, 0 };
  const static SDL_Color title0clr = { 0x11, 0x22, 0x33, 0 };

  if (font != 0 && font->font != 0 && title != 0) {
    // Evaluate exp
    UserVariable val = exp->Evaluate(0);
    char nonz = (char) val;

    // Draw title
    VideoIO::draw_text(screen,font->font,
                       title,metrics.ScaleX(xpos),metrics.ScaleY(ypos),
                       (nonz ? title1clr : title0clr),0,1);
  }
};

// Draw circular switch display
void FloDisplayCircleSwitch::Draw(SDL_Surface *screen,
                                  const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  SDL_Color c1clr = { 0xDF, 0x20, 0x20, 0 };
  SDL_Color c0clr = { 0x11, 0x22, 0x33, 0 };
  const static float flashspd = 4.0;

  double dt = mygettime()-nonztime;
  char flashon = 1;
  if (flash)
    flashon = !((char) (((long int) (dt*flashspd)) % 2));

  // Evaluate exp
  UserVariable val = exp->Evaluate(0);
  char nonz = (char) val;
  if (nonz && !prevnonz)
    // Value is switching on-- store time
    nonztime = mygettime();
  prevnonz = nonz;

  // Draw circle
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_rad1 = metrics.ScaleX(rad1);
  int draw_rad0 = metrics.ScaleX(rad0);
  SDL_Color *c = (nonz && flashon ? &c1clr : &c0clr);
  filledCircleRGBA(screen,draw_x,draw_y,(nonz && flashon ? draw_rad1 : draw_rad0),
                   c->r, c->g, c->b, 255);

  if (font != 0 && font->font != 0 && title != 0) {
    // Draw title
    VideoIO::draw_text(screen,font->font,
                       title,draw_x+2*draw_rad0,draw_y,titleclr,0,1);
  }
};

// Draw text switch display
void FloDisplayTextSwitch::Draw(SDL_Surface *screen,
                                const FweelinRenderMetrics &metrics) {
  // No title displayed
  SDL_Color c1clr = { 0x77, 0x88, 0x99, 0 };
  SDL_Color c0clr = { 0x99, 0x88, 0x77, 0 };

  // Evaluate exp
  UserVariable val = exp->Evaluate(0);
  char nonz = (char) val;

  // Draw appropriate text
  char *dtxt = (nonz ? text1 : text0);
  if (dtxt != 0)
    VideoIO::draw_text(screen,font->font,dtxt,
                       metrics.ScaleX(xpos),metrics.ScaleY(ypos),
                       (nonz ? c1clr : c0clr),0,1);
};

// Draw text display
void FloDisplayBar::Draw(SDL_Surface *screen,
                         const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color barclr = { 0xFF, 0x50, 0x20, 0 };
  const static float calwidth = 1.1;
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_thickness =
    FweelinScaleExtent(thickness,
                       orient == O_Horizontal ? metrics.scale_y : metrics.scale_x);
  float draw_barscale =
    barscale * (orient == O_Horizontal ? metrics.scale_x : metrics.scale_y);

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,draw_x,draw_y,titleclr,
                         (orient == O_Vertical ? 1 : 2),
                         (orient == O_Horizontal ? 1 : 0));
  }

  // Get value of expression
  UserVariable val = exp->Evaluate(0);
  float fval = (float) val;

  if (dbscale) {
    // dB

    // Convert linear value to dB and then to fader level:
    int lvl = (int) (AudioLevel::dB_to_fader(LIN2DB(fval), maxdb) * draw_barscale);

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          // printf("%f > %f def\n",i,AudioLevel::dB_to_fader(i, maxdb));

          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * draw_barscale);
          hlineRGBA(screen,
                    draw_x-(int) (calwidth*draw_thickness),
                    draw_x-draw_thickness,
                    draw_y-clvl,
                    clr,clr,clr,255);
          hlineRGBA(screen,
                    draw_x+draw_thickness,
                    draw_x+(int) (calwidth*draw_thickness),
                    draw_y-clvl,
                    clr,clr,clr,255);
        }
      }

      // Bar
      boxRGBA(screen,
              draw_x-draw_thickness,draw_y,
              draw_x+draw_thickness,draw_y-lvl,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              draw_x-draw_thickness/2,draw_y,
              draw_x+draw_thickness/2,draw_y-lvl,
              barclr.r,barclr.g,barclr.b,255);
    } else {
      // Horizontal

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * draw_barscale);
          vlineRGBA(screen,
                    draw_x+clvl,
                    draw_y-(int) (calwidth*draw_thickness),
                    draw_y-draw_thickness,
                    clr,clr,clr,255);
          vlineRGBA(screen,
                    draw_x+clvl,
                    draw_y+draw_thickness,
                    draw_y+(int) (calwidth*draw_thickness),
                    clr,clr,clr,255);
        }
      }

      // Bar
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness,
              draw_x+lvl,draw_y+draw_thickness,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness/2,
              draw_x+lvl,draw_y+draw_thickness/2,
              barclr.r,barclr.g,barclr.b,255);
    }
  } else {
    // Linear

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      // Show calibration
      boxRGBA(screen,
              draw_x-draw_thickness/2,draw_y,
              draw_x+draw_thickness/2,(int) (draw_y-draw_barscale),
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
              draw_x-draw_thickness,draw_y,
              draw_x+draw_thickness,(int) (draw_y-fval*draw_barscale),
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              draw_x-draw_thickness/2,draw_y,
              draw_x+draw_thickness/2,(int) (draw_y-fval*draw_barscale),
              barclr.r,barclr.g,barclr.b,255);
    } else {
      // Horizontal

      // Show calibration
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness/2,
              (int) (draw_x+draw_barscale),draw_y+draw_thickness/2,
              barclr.r/2,barclr.g/2,barclr.b/2,255);

      // Bar
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness,
              (int) (draw_x+fval*draw_barscale),draw_y+draw_thickness,
              barclr.r/2,barclr.g/2,barclr.b/2,255);
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness/2,
              (int) (draw_x+fval*draw_barscale),draw_y+draw_thickness/2,
              barclr.r,barclr.g,barclr.b,255);
    }
  }
};

// Draw text display
void FloDisplayBarSwitch::Draw(SDL_Surface *screen,
                               const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 },
    warnclr = { 0xFF, 0, 0, 0 };
  const static SDL_Color barclr[2] = { { 0xEF, 0xAF, 0xFF, 0 },
                                       { 0xCF, 0x4F, 0xFC, 0 } };
  const static float calwidth = 1.1;
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_thickness =
    FweelinScaleExtent(thickness,
                       orient == O_Horizontal ? metrics.scale_y : metrics.scale_x);
  float draw_barscale =
    barscale * (orient == O_Horizontal ? metrics.scale_x : metrics.scale_y);

  const SDL_Color *bc = (color == 2 ? &barclr[1] : &barclr[0]);

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,draw_x,draw_y,titleclr,
                         (orient == O_Vertical ? 1 : 2),
                         (orient == O_Horizontal ? 1 : 0));
  }

  // Get value of expression
  UserVariable val = exp->Evaluate(0);
  float fval = (float) val;
  UserVariable sval = switchexp->Evaluate(0);
  char sw = (char) sval;

  if (calibrate && fval >= cval)
    bc = &warnclr;

  if (dbscale) {
    // dB

    // Convert linear value to dB and then to fader level:
    int lvl = (int) (AudioLevel::dB_to_fader(LIN2DB(fval), maxdb) * draw_barscale);

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * draw_barscale);
          hlineRGBA(screen,
                    draw_x-(int) (calwidth*draw_thickness/2),
                    draw_x-draw_thickness/2,
                    draw_y-clvl,
                    clr,clr,clr,(sw ? 255 : 127));
          hlineRGBA(screen,
                    draw_x+draw_thickness/2,
                    draw_x+(int) (calwidth*draw_thickness/2),
                    draw_y-clvl,
                    clr,clr,clr,(sw ? 255 : 127));
        }
      }

      // Bar
      boxRGBA(screen,
              draw_x-draw_thickness/2,draw_y,
              draw_x+draw_thickness/2,draw_y-lvl,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
    } else {
      // Horizontal

      if (marks) {
        // Show calibration
        const static float mindb = -60.,
          dbstep = 6.0;
        int clrstep = (int) (255/((maxdb-mindb)/dbstep)),
          clr = 0;

        for (float i = mindb; i <= maxdb; i += dbstep, clr += clrstep) {
          int clvl = (int) (AudioLevel::dB_to_fader(i, maxdb) * draw_barscale);
          vlineRGBA(screen,
                    draw_x+clvl,
                    draw_y-(int) (calwidth*draw_thickness/2),
                    draw_y-draw_thickness/2,
                    clr,clr,clr,(sw ? 255 : 127));
          vlineRGBA(screen,
                    draw_x+clvl,
                    draw_y+draw_thickness/2,
                    draw_y+(int) (calwidth*draw_thickness/2),
                    clr,clr,clr,(sw ? 255 : 127));
        }
      }

      // Bar
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness/2,
              draw_x+lvl,draw_y+draw_thickness/2,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
    }
  } else {
    // Linear

    // Draw bar
    if (orient == O_Vertical) {
      // Vertical

      // Bar
      boxRGBA(screen,
              draw_x-draw_thickness/2,draw_y,
              draw_x+draw_thickness/2,(int) (draw_y-fval*draw_barscale),
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
      // Calibrate
      if (calibrate)
        hlineRGBA(screen,
                  draw_x-draw_thickness/2,
                  draw_x+draw_thickness/2,
                  (int) (draw_y-cval*draw_barscale),
                  255,255,255,(sw ? 255 : 127));
    } else {
      // Horizontal

      // Bar
      boxRGBA(screen,
              draw_x,draw_y-draw_thickness/2,
              (int) (draw_x+fval*draw_barscale),draw_y+draw_thickness/2,
              bc->r,bc->g,bc->b,(sw ? 255 : 127));
      // Calibrate
      if (calibrate)
        vlineRGBA(screen,
                  (int) (draw_x+cval*draw_barscale),
                  draw_y-draw_thickness/2,
                  draw_y+draw_thickness/2,
                  255,255,255,(sw ? 255 : 127));
    }
  }
};

// Draw this element to the given screen-
// implementation given in videoio.cc
void FloLayoutBox::Draw(SDL_Surface *screen, SDL_Color clr,
                        const FweelinRenderMetrics &metrics) {
  int draw_left = metrics.ScaleX(left);
  int draw_top = metrics.ScaleY(top);
  int draw_right = metrics.ScaleX(right);
  int draw_bottom = metrics.ScaleY(bottom);
  // Solid box
  boxRGBA(screen,
          draw_left,draw_top,draw_right,draw_bottom,
          clr.r,clr.g,clr.b,255);
  // Outline
  if (lineleft)
    vlineRGBA(screen,draw_left,draw_top,draw_bottom,0,0,0,255);
  if (lineright)
    vlineRGBA(screen,draw_right,draw_top,draw_bottom,0,0,0,255);
  if (linetop)
    hlineRGBA(screen,draw_left,draw_right,draw_top,0,0,0,255);
  if (linebottom)
    hlineRGBA(screen,draw_left,draw_right,draw_bottom,0,0,0,255);
};

// Draw snapshots display
void FloDisplaySnapshots::Draw(SDL_Surface *screen,
                               const FweelinRenderMetrics &metrics) {
  const static SDL_Color titleclr = { 0x77, 0x88, 0x99, 0 };
  const static SDL_Color borderclr = { 0xFF, 0x50, 0x20, 0 };
  const static SDL_Color cursorclr = { 0xEF, 0x11, 0x11, 0 };
  int draw_x = metrics.ScaleX(xpos);
  int draw_y = metrics.ScaleY(ypos);
  int draw_sx = metrics.ScaleX(sx);
  int draw_sy = metrics.ScaleY(sy);
  int draw_margin = metrics.ScaleX(margin);

  LockSnaps();

  if (numdisp == -1) {
    int height = TTF_FontHeight(font->font);
    numdisp = draw_sy/height;
  }

  boxRGBA(screen,
          draw_x,draw_y,draw_x+draw_sx,draw_y+draw_sy,
          0,0,0,190);
  vlineRGBA(screen,draw_x,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  vlineRGBA(screen,draw_x+draw_sx,draw_y,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y,
            borderclr.r,borderclr.g,borderclr.b,255);
  hlineRGBA(screen,draw_x,draw_x+draw_sx,draw_y+draw_sy,
            borderclr.r,borderclr.g,borderclr.b,255);

  if (font != 0 && font->font != 0) {
    // Draw title
    if (title != 0)
      VideoIO::draw_text(screen,font->font,
                         title,draw_x+draw_sx/2,draw_y,titleclr,1,2);
  }

  // Draw items
  int cury = draw_y+draw_margin;
  int height = TTF_FontHeight(font->font);
  for (int i = firstidx; i < firstidx + numdisp; i++, cury += height) {
    const static int SNAP_NAME_LEN = 512;

    char buf[SNAP_NAME_LEN];
    Snapshot *sn = app->getSNAP(i);
    if (sn != 0) {
      RenameUIVars *rui = 0;
      const char *nm = sn->name;

      if (renamer != 0 && i == rename_idx) {
        // Use current name from renamer
        rui = renamer->UpdateUIVars();
        nm = renamer->GetCurName();
      }

      if (nm != 0)
        snprintf(buf,SNAP_NAME_LEN,"%2d %s",i+1,nm);
      else if (sn->exists != 0)
        snprintf(buf,SNAP_NAME_LEN,"%2d **",i+1);
      else
        snprintf(buf,SNAP_NAME_LEN,"%2d",i+1);

      int sx, sy;
      VideoIO::draw_text(screen,font->font,
                         buf,draw_x+draw_margin,cury,titleclr,0,0,&sx,&sy);

      if (rui != 0 && rui->rename_cursor_toggle)
          boxRGBA(screen,
                  draw_x+draw_margin+sx,cury,
                  draw_x+draw_margin+sx+sy/2,cury+sy,
                  cursorclr.r,cursorclr.g,cursorclr.b,255);
    }
  }

  UnlockSnaps();
};
