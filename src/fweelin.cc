/* ************
   FreeWheeling
   ************

   What is music,
   if it is not shared in community,
   held in friendship,
   alive and breathing,
   soil and soul?

   THANKS & PRAISE
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

#include <signal.h>
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

#include "stacktrace.h"
#include "fweelin_signal.h"

#include "fweelin_midiio.h"
#include "fweelin_videoio.h"
#include "fweelin_sdlio.h"
#include "fweelin_audioio.h"

#include "fweelin_core.h"
#include "fweelin_core_dsp.h"

static void info_signal_handler(int sig) {
  fweelin_log_nonfatal_signal(sig);
}


#ifndef NO_COMPILE_MAIN
extern "C" int FweelinAppMain(int /*argc*/, char *argv[]) {
  // Initialize the stack trace mechanism 
  StackTraceInit(argv[0], -1);

#if !defined(WIN32)
  // Register signal handlers
  struct sigaction fatal_sact;
  sigemptyset(&fatal_sact.sa_mask);
  fatal_sact.sa_flags = 0;
  fatal_sact.sa_handler = fweelin_fatal_signal_handler;
  sigaction(SIGSEGV, &fatal_sact, NULL);
  sigaction(SIGBUS,  &fatal_sact, NULL);
  sigaction(SIGILL,  &fatal_sact, NULL);
  sigaction(SIGFPE,  &fatal_sact, NULL);

  struct sigaction info_sact;
  sigemptyset(&info_sact.sa_mask);
  info_sact.sa_flags = 0;
  info_sact.sa_handler = info_signal_handler;
  sigaction(SIGUSR1, &info_sact, NULL);
  sigaction(SIGUSR2, &info_sact, NULL);

  struct sigaction shutdown_sact;
  sigemptyset(&shutdown_sact.sa_mask);
  shutdown_sact.sa_flags = 0;
  shutdown_sact.sa_handler = fweelin_request_shutdown_signal_handler;
  sigaction(SIGINT, &shutdown_sact, NULL);
  sigaction(SIGTERM, &shutdown_sact, NULL);
#endif // WIN32

  fweelin_clear_shutdown_request();

  Fweelin flo;
  
  printf("FreeWheeling %s\n",VERSION);
  printf("May we return to the circle.\n\n");

  if (!flo.setup())
    flo.go();
  else
    printf("Error starting FreeWheeling!\n");
  
  return 0;
}

#ifndef __MACOSX__
int main (int argc, char *argv[]) {
  return FweelinAppMain(argc, argv);
}
#endif
#endif // NO_COMPILE_MAIN


// Improvisation is loving what is.
