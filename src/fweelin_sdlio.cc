/*
   With my hands, I weave the bridge's design
   feeling as the sides combine,
   reality remade and realigned,
   I fell thru death, now it's my time.
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

#include <SDL2/SDL.h>

#ifdef __MACOSX__
#include <SDL_gfx/SDL_gfxPrimitives.h>
#include <SDL_ttf/SDL_ttf.h>
#else
#include <SDL/SDL_gfxPrimitives.h>
#include <SDL/SDL_ttf.h>
#endif

#include "fweelin_sdlio.h"
#include "fweelin_core.h"

static SDLKey TranslateSDLKeycode(SDL_Keycode keycode) {
  if ((keycode >= 32 && keycode <= 126) ||
      (keycode >= 160 && keycode <= 255))
    return static_cast<SDLKey>(keycode);

  switch (keycode) {
  case SDLK_KP_0: return FWL_SDLK_KP0;
  case SDLK_KP_1: return FWL_SDLK_KP1;
  case SDLK_KP_2: return FWL_SDLK_KP2;
  case SDLK_KP_3: return FWL_SDLK_KP3;
  case SDLK_KP_4: return FWL_SDLK_KP4;
  case SDLK_KP_5: return FWL_SDLK_KP5;
  case SDLK_KP_6: return FWL_SDLK_KP6;
  case SDLK_KP_7: return FWL_SDLK_KP7;
  case SDLK_KP_8: return FWL_SDLK_KP8;
  case SDLK_KP_9: return FWL_SDLK_KP9;
  case SDLK_KP_PERIOD: return FWL_SDLK_KP_PERIOD;
  case SDLK_KP_DIVIDE: return FWL_SDLK_KP_DIVIDE;
  case SDLK_KP_MULTIPLY: return FWL_SDLK_KP_MULTIPLY;
  case SDLK_KP_MINUS: return FWL_SDLK_KP_MINUS;
  case SDLK_KP_PLUS: return FWL_SDLK_KP_PLUS;
  case SDLK_KP_ENTER: return FWL_SDLK_KP_ENTER;
  case SDLK_KP_EQUALS: return FWL_SDLK_KP_EQUALS;
  case SDLK_UP: return FWL_SDLK_UP;
  case SDLK_DOWN: return FWL_SDLK_DOWN;
  case SDLK_RIGHT: return FWL_SDLK_RIGHT;
  case SDLK_LEFT: return FWL_SDLK_LEFT;
  case SDLK_INSERT: return FWL_SDLK_INSERT;
  case SDLK_HOME: return FWL_SDLK_HOME;
  case SDLK_END: return FWL_SDLK_END;
  case SDLK_PAGEUP: return FWL_SDLK_PAGEUP;
  case SDLK_PAGEDOWN: return FWL_SDLK_PAGEDOWN;
  case SDLK_F1: return FWL_SDLK_F1;
  case SDLK_F2: return FWL_SDLK_F2;
  case SDLK_F3: return FWL_SDLK_F3;
  case SDLK_F4: return FWL_SDLK_F4;
  case SDLK_F5: return FWL_SDLK_F5;
  case SDLK_F6: return FWL_SDLK_F6;
  case SDLK_F7: return FWL_SDLK_F7;
  case SDLK_F8: return FWL_SDLK_F8;
  case SDLK_F9: return FWL_SDLK_F9;
  case SDLK_F10: return FWL_SDLK_F10;
  case SDLK_F11: return FWL_SDLK_F11;
  case SDLK_F12: return FWL_SDLK_F12;
  case SDLK_F13: return FWL_SDLK_F13;
  case SDLK_F14: return FWL_SDLK_F14;
  case SDLK_F15: return FWL_SDLK_F15;
  case SDLK_NUMLOCKCLEAR: return FWL_SDLK_NUMLOCK;
  case SDLK_CAPSLOCK: return FWL_SDLK_CAPSLOCK;
  case SDLK_SCROLLLOCK: return FWL_SDLK_SCROLLOCK;
  case SDLK_RSHIFT: return FWL_SDLK_RSHIFT;
  case SDLK_LSHIFT: return FWL_SDLK_LSHIFT;
  case SDLK_RCTRL: return FWL_SDLK_RCTRL;
  case SDLK_LCTRL: return FWL_SDLK_LCTRL;
  case SDLK_RALT: return FWL_SDLK_RALT;
  case SDLK_LALT: return FWL_SDLK_LALT;
  case SDLK_RGUI: return FWL_SDLK_RMETA;
  case SDLK_LGUI: return FWL_SDLK_LMETA;
  case SDLK_MODE: return FWL_SDLK_MODE;
  case SDLK_HELP: return FWL_SDLK_HELP;
  case SDLK_PRINTSCREEN: return FWL_SDLK_PRINT;
  case SDLK_SYSREQ: return FWL_SDLK_SYSREQ;
  case SDLK_MENU: return FWL_SDLK_MENU;
  case SDLK_POWER: return FWL_SDLK_POWER;
  case SDLK_UNDO: return FWL_SDLK_UNDO;
  default:
    return FWL_SDLK_UNKNOWN;
  }
}

static int DecodeTextInputUnicode(const char *text) {
  const unsigned char *ptr = reinterpret_cast<const unsigned char *>(text);
  if (ptr == 0 || ptr[0] == '\0')
    return 0;
  if (ptr[0] < 0x80)
    return ptr[0];
  if ((ptr[0] & 0xE0) == 0xC0 && ptr[1] != '\0') {
    const int codepoint = ((ptr[0] & 0x1F) << 6) | (ptr[1] & 0x3F);
    if (codepoint <= 255)
      return codepoint;
  }
  return 0;
}

// ******** KEYBOARD / MOUSE / JOYSTICK HANDLER

static const char *SDL_names[] = {
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "backspace", 
  "tab", 
  "", 
  "", 
  "clear", 
  "return", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "pause", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "escape", 
  "", 
  "", 
  "", 
  "", 
  "space", 
  "exclamation", 
  "dblquote", 
  "numbersign", 
  "dollarsign", 
  "", 
  "ampersand", 
  "backquote", 
  "openparen", 
  "closeparen", 
  "asterisk", 
  "plus", 
  "comma", 
  "minus", 
  "period", 
  "slash", 
  "zero", 
  "one", 
  "two", 
  "three", 
  "four", 
  "five", 
  "six", 
  "seven", 
  "eight", 
  "nine", 
  "colon", 
  "semicolon", 
  "lessthan", 
  "equal", 
  "greaterthan", 
  "questionmark", 
  "at", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "squarebracketopen", 
  "backslash", 
  "squarebracketclose", 
  "caret", 
  "underscore", 
  "tilde", 
  "a", 
  "b", 
  "c", 
  "d", 
  "e", 
  "f", 
  "g", 
  "h", 
  "i", 
  "j", 
  "k", 
  "l", 
  "m", 
  "n", 
  "o", 
  "p", 
  "q", 
  "r", 
  "s", 
  "t", 
  "u", 
  "v", 
  "w", 
  "x", 
  "y", 
  "z", 
  "", 
  "", 
  "", 
  "", 
  "delete", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "", 
  "world0", 
  "world1", 
  "world2", 
  "world3", 
  "world4", 
  "world5", 
  "world6", 
  "world7", 
  "world8", 
  "world9", 
  "world10", 
  "world11", 
  "world12", 
  "world13", 
  "world14", 
  "world15", 
  "world16", 
  "world17", 
  "world18", 
  "world19", 
  "world20", 
  "world21", 
  "world22", 
  "world23", 
  "world24", 
  "world25", 
  "world26", 
  "world27", 
  "world28", 
  "world29", 
  "world30", 
  "world31", 
  "world32", 
  "world33", 
  "world34", 
  "world35", 
  "world36", 
  "world37", 
  "world38", 
  "world39", 
  "world40", 
  "world41", 
  "world42", 
  "world43", 
  "world44", 
  "world45", 
  "world46", 
  "world47", 
  "world48", 
  "world49", 
  "world50", 
  "world51", 
  "world52", 
  "world53", 
  "world54", 
  "world55", 
  "world56", 
  "world57", 
  "world58", 
  "world59", 
  "world60", 
  "world61", 
  "world62", 
  "world63", 
  "world64", 
  "world65", 
  "world66", 
  "world67", 
  "world68", 
  "world69", 
  "world70", 
  "world71", 
  "world72", 
  "world73", 
  "world74", 
  "world75", 
  "world76", 
  "world77", 
  "world78", 
  "world79", 
  "world80", 
  "world81", 
  "world82", 
  "world83", 
  "world84", 
  "world85", 
  "world86", 
  "world87", 
  "world88", 
  "world89", 
  "world90", 
  "world91", 
  "world92", 
  "world93", 
  "world94", 
  "world95", 
  "KP0", 
  "KP1", 
  "KP2", 
  "KP3", 
  "KP4", 
  "KP5", 
  "KP6", 
  "KP7", 
  "KP8", 
  "KP9", 
  "KPperiod", 
  "KPslash", 
  "KPasterisk", 
  "KPminus", 
  "KPplus", 
  "enter", 
  "equals", 
  "up", 
  "down", 
  "right", 
  "left", 
  "insert", 
  "home", 
  "end", 
  "pageup", 
  "pagedown", 
  "f1", 
  "f2", 
  "f3", 
  "f4", 
  "f5", 
  "f6", 
  "f7", 
  "f8", 
  "f9", 
  "f10", 
  "f11", 
  "f12", 
  "f13", 
  "f14", 
  "f15", 
  "", 
  "", 
  "", 
  "numlock", 
  "capslock", 
  "scrolllock", 
  "rightshift", 
  "leftshift", 
  "rightctrl", 
  "leftctrl", 
  "rightalt", 
  "leftalt", 
  "rightmeta", 
  "leftmeta", 
  "leftsuper", 
  "rightsuper", 
  "altgr", 
  "compose", 
  "help", 
  "printscreen", 
  "sysreq", 
  "break", 
  "menu", 
  "power", 
  "euro", 
  "undo"};

SDLKey SDLIO::GetSDLKey(char *keyname) {
  SDLKey ky;
  for (ky = FWL_SDLK_FIRST; ky < FWL_SDLK_LAST; ky = (SDLKey)(ky+1)) {
    /*char *nm = SDL_GetKeyName(ky);
      printf("\"%s\", \n",(!strcmp(nm,"unknown key") ? "" : nm));*/
    /*if (SDL_names[ky][0] != '\0')
      printf("%03d - %s\n",ky,SDL_names[ky]);*/

    if (!strcmp(SDL_names[ky],keyname))
      return ky;
  }

  return FWL_SDLK_UNKNOWN;
};

const char *SDLIO::GetSDLName(SDLKey sym) {
  if (sym < FWL_SDLK_FIRST || sym >= FWL_SDLK_LAST)
    return "";
  return SDL_names[sym];
};

int SDLIO::activate() {
  sdlthreadgo = 1;
  
#if 0
  printf("SDLIO: Starting SDL input thread.\n");

  const static size_t STACKSIZE = 1024*64;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("SDL: Stacksize: %d.\n",STACKSIZE);

  sdlthreadgo = 1;
  int ret = pthread_create(&sdl_thread,
                           &attr,
                           run_sdl_thread,
                           this);
  if (ret != 0) {
    printf("SDLIO: (SDL input) pthread_create failed, exiting");
    return 1;
  }
  RT_RWThreads::RegisterReaderOrWriter(sdl_thread);

  // Setup high priority threads
  struct sched_param schp;
  memset(&schp, 0, sizeof(schp));
  schp.sched_priority = sched_get_priority_max(SCHED_OTHER);
  printf("SDLIO: thread priority %d\n",schp.sched_priority);
  if (pthread_setschedparam(sdl_thread, SCHED_OTHER, &schp) != 0) {
    printf("SDLIO: Can't set hi priority thread, will use regular!\n");
  }
#endif
  
  return 0;
}

void SDLIO::close() {
  sdlthreadgo = 0;
  // *** SDL IO now running on main thread
  // pthread_join(sdl_thread,0);
  printf("SDLIO: end\n");
}

void SDLIO::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_ExitSession :
    sdlthreadgo = 0;
    if (CRITTERS)
      printf("\n** Event: Exit session\n");
    break;

  default:
    break;
  }
}

// *********** Old hardcoded key handler
// *********** Still handles a few things directly
void SDLIO::handle_key(int keycode, char press) {
  //printf("SDLIO: %d\n",keycode);
  
  static int prevkeycode = 0,
    prevcnt = 0;

  if (press) {
    if (keycode == prevkeycode) 
      prevcnt++;
    else
      prevcnt = 0;
    prevkeycode = keycode;

#if 0
    if (keycode == 51) {
      // Backslash freezes & resets limiter
      if (sets.leftshift || sets.rightshift) {
        // Reset limiter
        app->getRP()->ResetLimiter();
      } else {
        // Toggle limiter freeze
        char limiterfreeze = app->getRP()->GetLimiterFreeze();
        limiterfreeze = (limiterfreeze == 0 ? 1 : 0); 
        app->getRP()->SetLimiterFreeze(limiterfreeze);
      }
    }
#endif

    if (keycode == FWL_SDLK_KP_ENTER) {
      // keypad enter
      app->getCFG()->status_report = 1;
    }
    else if (keycode == FWL_SDLK_SPACE) {
      // spacebar
      sets.spacekey = 1;
    }
    else if (keycode == FWL_SDLK_LALT) {
      // left alt
      sets.leftalt = 1;
    }
    else if (keycode == FWL_SDLK_RALT) {
      // right alt
      sets.rightalt = 1;
    }
    else if (keycode == FWL_SDLK_LSHIFT) {
      // left shift
      sets.leftshift = 1;
    }
    else if (keycode == FWL_SDLK_RSHIFT) {
      // right shift
      sets.rightshift = 1;
    } 
    else if (keycode == FWL_SDLK_LCTRL) {
      // left ctrl
      sets.leftctrl = 1;
    }
    else if (keycode == FWL_SDLK_RCTRL) {
      // right ctrl
      sets.rightctrl = 1;
    }
    else if (keycode == FWL_SDLK_UP) {
      // up arrow, hold for adjusting individual loop volumes
      sets.upkey = 1;
    }
    else if (keycode == FWL_SDLK_DOWN) {
      // down arrow, hold for adjusting individual loop volumes
      sets.downkey = 1;
    }
    else if (keycode == FWL_SDLK_ESCAPE) {
      // escape key- exit!
      //app->getEMG()->BroadcastEvent(::new ExitSessionEvent(), this);
      //sdlthreadgo = 0;
      //printf("end\n");
    } else {
      // This keyrange (F1-F10) is set to select pulses
      if ((keycode >= FWL_SDLK_F1 &&
           keycode <= FWL_SDLK_F10)) {
        if (sets.leftshift || sets.rightshift) {
          // Set subdivide

          // Old method (F1=1, F2=2, F3=4, F4=8, F5=16..)
          // app->getLOOPMGR()->SetSubdivide((int) pow(2,keycode-67));

          // New method- Fn is translated to subdivide directly
          // And amplified by repeat presses of Fn
          int newsub = (prevcnt+1) * (keycode-FWL_SDLK_F1+1);
          app->getLOOPMGR()->SetSubdivide(newsub);
        }
      }

#if 0
      if (keycode == 95) {
        // Toggle metronome on current pulse (F11)
        Pulse *a = app->getLOOPMGR()->GetCurPulse();
        if (a != 0)
          a->SwitchMetronome((a->IsMetronomeActive() ? 0 : 1));
      }
#endif
    }
  } else {
    // Release
    if (keycode == FWL_SDLK_LALT) {
      // left alt
      sets.leftalt = 0;
    }
    else if (keycode == FWL_SDLK_RALT) {
      // right alt
      sets.rightalt = 0;
    }
    else if (keycode == FWL_SDLK_SPACE) {
      // spacebar
      sets.spacekey = 0;
    }
    else if (keycode == FWL_SDLK_LSHIFT) {
      // left shift
      sets.leftshift = 0;
    }
    else if (keycode == FWL_SDLK_RSHIFT) {
      // right shift
      sets.rightshift = 0;
    }
    else if (keycode == FWL_SDLK_LCTRL) {
      // left ctrl
      sets.leftctrl = 0;
    }
    else if (keycode == FWL_SDLK_RCTRL) {
      // right ctrl
      sets.rightctrl = 0;
    }
    else if (keycode == FWL_SDLK_UP) {
      // up arrow
      sets.upkey = 0;
    }
    else if (keycode == FWL_SDLK_DOWN) {
      // down arrow
      sets.downkey = 0;
    }
  }
}

// This is the SDL input event loop
void *SDLIO::run_sdl_thread(void *ptr)
{
  SDLIO *inst = static_cast<SDLIO *>(ptr);
  SDL_Event event;

  printf("SDLIO: SDL Input thread start.\n");

#ifdef __MACOSX__
  // inst->cocoa.SetupCocoaThread();
#endif
  
  // Setup joysticks
  inst->numjoy = SDL_NumJoysticks();
  if (inst->numjoy > 0) 
    inst->joys = new SDL_Joystick *[inst->numjoy];
  printf("SDLIO: Detected %d joysticks..\n", inst->numjoy);
  for (int i = 0; i < inst->numjoy; i++) {
    printf("  Joystick #%d: %s\n",i+1,SDL_JoystickNameForIndex(i));
    inst->joys[i] = SDL_JoystickOpen(i);
  }
  
  // Listen for pertinent events
  if (inst->app->getEMG() == 0) {
    printf("INIT: Error: Event Manager not yet active!\n");
    exit(1);
  }
  inst->app->getEMG()->ListenEvent(inst,0,T_EV_ExitSession);

  while (inst->sdlthreadgo) {
    if (SDL_WaitEvent(&event)) {
      switch (event.type) {
      case SDL_JOYBUTTONDOWN :
      case SDL_JOYBUTTONUP :
        {
          JoystickButtonInputEvent *jevt = (JoystickButtonInputEvent *) 
              Event::GetEventByType(T_EV_Input_JoystickButton);
            
          jevt->joystick = event.jbutton.which;
          jevt->button = event.jbutton.button;
          jevt->down = (event.type == SDL_JOYBUTTONUP ? 0 : 1);
          inst->app->getEMG()->BroadcastEventNow(jevt, inst);
            
          if (inst->app->getCFG()->IsDebugInfo())
            printf("JOYSTICK: Joystick #%d, button #%d %s\n",
                   jevt->joystick,jevt->button,
                   (jevt->down ? "pressed" : "released"));
        }
        break;

      case SDL_MOUSEMOTION :
        {
          MouseMotionInputEvent *mevt = (MouseMotionInputEvent *) 
              Event::GetEventByType(T_EV_Input_MouseMotion);
            
          mevt->x = event.motion.x;
          mevt->y = event.motion.y;
          inst->app->getEMG()->BroadcastEventNow(mevt, inst);
            
          // No debug info for mouse motion
#if 0
          if (inst->app->getCFG()->IsDebugInfo())
            printf("MOUSE: Motion: (%d,%d)\n",
                   mevt->x, mevt->y);
#endif
        }

        //      printf("x: %d y: %d\n",
        //      event.motion.x,event.motion.y);
        break;

      case SDL_MOUSEBUTTONDOWN :
      case SDL_MOUSEBUTTONUP :
        {
          MouseButtonInputEvent *mevt = (MouseButtonInputEvent *) 
              Event::GetEventByType(T_EV_Input_MouseButton);
            
          mevt->button = event.button.button;
          mevt->down = (event.type == SDL_MOUSEBUTTONUP ? 0 : 1);
          mevt->x = event.button.x;
          mevt->y = event.button.y;
          inst->app->getEMG()->BroadcastEventNow(mevt, inst);
            
          if (inst->app->getCFG()->IsDebugInfo())
            printf("MOUSE: Button #%d %s @ (%d,%d)\n",
                   mevt->button,
                   (mevt->down ? "pressed" : "released"),
                   mevt->x, mevt->y);
        }

        //printf("button: %d x: %d y: %d\n",
        //        event.button.button, 
        //        event.button.x,event.button.y);
        break;

      case SDL_KEYDOWN : 
        {
          SDLKey sym = TranslateSDLKeycode(event.key.keysym.sym);
          if (sym >= FWL_SDLK_FIRST && sym < FWL_SDLK_LAST) {
            // Mark the key as held down
            inst->keyheld[sym] = 1;
            
            // Now generate an input event..
            KeyInputEvent *kevt = (KeyInputEvent *) 
              Event::GetEventByType(T_EV_Input_Key);
            
            kevt->down = 1;
            kevt->keysym = sym;
            kevt->unicode = 0;
            inst->app->getEMG()->BroadcastEventNow(kevt, inst);
            
            if (inst->app->getCFG()->IsDebugInfo())
              printf("KEYBOARD: Key pressed: %d (%s)\n",
                     kevt->keysym, GetSDLName(sym));
            inst->handle_key(sym,1);
          } else {
            printf("KEYBOARD: Invalid key\n");
          }
        }
        break;
      case SDL_KEYUP :
        {
          SDLKey sym = TranslateSDLKeycode(event.key.keysym.sym);
          if (sym >= FWL_SDLK_FIRST && sym < FWL_SDLK_LAST) {
            // Mark the key as unheld
            inst->keyheld[sym] = 0;
            
            // Now generate an input event..
            KeyInputEvent *kevt = (KeyInputEvent *) 
              Event::GetEventByType(T_EV_Input_Key);
            kevt->down = 0;
            kevt->keysym = sym;
            kevt->unicode = 0;
            inst->app->getEMG()->BroadcastEventNow(kevt, inst);
            
            if (inst->app->getCFG()->IsDebugInfo())
              printf("KEYBOARD: Key released: %d (%s)\n",
                     kevt->keysym, GetSDLName(sym));
            inst->handle_key(sym,0);
            break;
          } else
            printf("KEYBOARD: Invalid key\n");
        }
        break;
      case SDL_TEXTINPUT :
        {
          const int unicode = DecodeTextInputUnicode(event.text.text);
          if (unicode != 0) {
            KeyInputEvent *kevt = (KeyInputEvent *)
              Event::GetEventByType(T_EV_Input_Key);

            kevt->down = 1;
            kevt->keysym = unicode;
            kevt->unicode = unicode;
            inst->app->getEMG()->BroadcastEventNow(kevt, inst);
          }
        }
        break;
      }
    }
    /*else {
      printf("SDL Error Waiting for Event!\n");
      sdlthreadgo = 0;
      }*/

    // usleep(100); // Give up the processor briefly!
  }

  inst->app->getEMG()->UnlistenEvent(inst,0,T_EV_ExitSession);

  printf("Closing joysticks.\n");
  for (int i = 0; i < inst->numjoy; i++)
    SDL_JoystickClose(inst->joys[i]);

#ifdef __MACOSX__
  // inst->cocoa.TakedownCocoaThread();
#endif
  
  printf("SDLIO: SDL Input thread done.\n");
  return 0;
}
