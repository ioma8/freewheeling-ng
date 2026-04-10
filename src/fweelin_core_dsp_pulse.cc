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

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "fweelin_config.h"
#include "fweelin_core_dsp.h"
#include "fweelin_dsp_profile.h"

Pulse::Pulse(Fweelin *app, nframes_t len, nframes_t startpos)
    : Processor(app), len(len), curpos(startpos), lc_len(1), lc_cur(0),
      wrapped(0), stopped(0), prev_sync_bb(0), sync_cnt(0),
      prev_sync_speed(-1), prev_sync_type(0), prevbpm(0.0), prevtap(0),
      metroofs(metrolen), metrohiofs(metrolen), metroloofs(metrolen),
      metrolen(METRONOME_HIT_LEN), metrotonelen(METRONOME_TONE_LEN),
      metroactive(0), metrovol(METRONOME_INIT_VOL), numsyncpos(0),
      clockrun(SS_NONE) {
#define METRO_HI_FREQ 880
#define METRO_HI_AMP 1.5
#define METRO_LO_FREQ 440
#define METRO_LO_AMP 1.0

  metro = new sample_t[metrolen];
  metrohitone = new sample_t[metrotonelen];
  metrolotone = new sample_t[metrotonelen];
  for (nframes_t i = 0; i < metrolen; i++)
    metro[i] = ((sample_t)rand() / RAND_MAX - 0.5f) *
               (1.0f - static_cast<float>(i) / metrolen);
  for (nframes_t i = 0; i < metrotonelen; i++) {
    metrohitone[i] =
        METRO_HI_AMP *
        sin(METRO_HI_FREQ * i * 2 * M_PI / app->getAUDIO()->get_srate()) *
        (1.0f - static_cast<float>(i) / metrotonelen);
    metrolotone[i] =
        METRO_LO_AMP *
        sin(METRO_LO_FREQ * i * 2 * M_PI / app->getAUDIO()->get_srate()) *
        (1.0f - static_cast<float>(i) / metrotonelen);
  }
}

Pulse::~Pulse() {
  app->getBMG()->RefDeleted(this);
  delete[] metro;
  delete[] metrohitone;
  delete[] metrolotone;
}

nframes_t Pulse::QuantizeLength(nframes_t src) {
  float frac = static_cast<float>(src) / len;
  if (frac < 0.5f)
    frac = 1.0f;
  return static_cast<nframes_t>(round(frac) * len);
}

void Pulse::SetMIDIClock(char start) {
  if (app->getMIDI()->GetMIDISyncTransmit()) {
    if (start) {
      clockrun = SS_START;
    } else {
      clockrun = SS_NONE;

      MIDIStartStopInputEvent *ssevt =
          (MIDIStartStopInputEvent *)Event::GetEventByType(
              T_EV_Input_MIDIStartStop);
      if (ssevt != 0) {
        ssevt->start = 0;
        app->getEMG()->BroadcastEvent(ssevt, this);
      }
    }
  }
}

int Pulse::ExtendLongCount(long nbeats, char endjustify) {
  if (nbeats > 0) {
    int lc_new_len = math_lcm(lc_len, (int)nbeats);

    if (endjustify && lc_new_len > lc_len) {
      int lc_end_delta = lc_len - lc_cur;
      lc_cur = lc_new_len - lc_end_delta;
    }

    lc_len = lc_new_len;
  }

  return lc_len;
}

void Pulse::process(char pre, nframes_t l, AudioBuffers *ab) {
  static int midi_clock_count = 0;
  static int midi_beat_count = 0;
  uint64_t profile_start_ticks = 0;

  if (!pre && DspProfile::Enabled())
    profile_start_ticks = DspProfile::NowTicks();

  if (app->getAUDIO()->IsTransportRolling() &&
      !app->getAUDIO()->IsTimebaseMaster()) {
    char sync_type = app->GetSyncType();
    int sync_speed = app->GetSyncSpeed();
    if (sync_type != prev_sync_type || sync_speed != prev_sync_speed) {
      prevbpm = 0;
      prev_sync_bb = -1;

      prev_sync_type = sync_type;
      prev_sync_speed = sync_speed;
    }

    double bpm = app->getAUDIO()->GetTransport_BPM();
    if (bpm != prevbpm) {
      float mult =
          (sync_type ? sync_speed
                     : app->getAUDIO()->GetTransport_BPB() * sync_speed);
      len = (nframes_t)((double)60.0 * app->getAUDIO()->get_srate() * mult /
                        bpm);
      prevbpm = bpm;
    }

    int sync_bb = (sync_type ? app->getAUDIO()->GetTransport_Beat()
                             : app->getAUDIO()->GetTransport_Bar());
    if (sync_bb != prev_sync_bb) {
      sync_cnt++;
      if (sync_cnt >= sync_speed) {
        sync_cnt = 0;
        Wrap();
      }

      prev_sync_bb = sync_bb;
    }
  }

  nframes_t fragmentsize = app->getBUFSZ();
  if (l > fragmentsize)
    l = fragmentsize;

  sample_t *out = ab->outs[0][0];
  nframes_t ofs = 0;
  if (!pre && !stopped) {
    nframes_t remaining = len - curpos;

    wrapped = 0;
    nframes_t oldpos = curpos;
    curpos += MIN(l, remaining);

    if (clockrun != SS_NONE && app->getMIDI()->GetMIDISyncTransmit()) {
      char sync_type = app->GetSyncType();
      int sync_speed = app->GetSyncSpeed();

      int clocksperpulse = MIDI_CLOCK_FREQUENCY * sync_speed;
      if (!sync_type)
        clocksperpulse *= SYNC_BEATS_PER_BAR;

      float framesperclock = (float)len / clocksperpulse;
      int oldclock = (int)((float)oldpos / framesperclock);
      int newclock = (int)((float)curpos / framesperclock);
      if ((clockrun == SS_BEAT && newclock != oldclock) || curpos >= len) {
        if (clockrun == SS_START) {
          metrohiofs = 0;
          midi_clock_count = 0;
          midi_beat_count = 0;

          MIDIStartStopInputEvent *ssevt =
              (MIDIStartStopInputEvent *)Event::GetEventByType(
                  T_EV_Input_MIDIStartStop);
          if (ssevt != 0) {
            ssevt->start = 1;
            app->getEMG()->BroadcastEvent(ssevt, this);
          }

          clockrun = SS_BEAT;
        } else {
          midi_clock_count++;
          if (midi_clock_count >= MIDI_CLOCK_FREQUENCY) {
            midi_clock_count = 0;

            midi_beat_count++;
            if (midi_beat_count >= clocksperpulse / MIDI_CLOCK_FREQUENCY) {
              midi_beat_count = 0;
              metrohiofs = 0;
            } else {
              metroloofs = 0;
            }
          }

          MIDIClockInputEvent *clkevt =
              (MIDIClockInputEvent *)Event::GetEventByType(
                  T_EV_Input_MIDIClock);
          if (clkevt != 0)
            app->getEMG()->BroadcastEvent(clkevt, this);
        }
      }
    }

    if (curpos >= len) {
      wrapped = 1;
      curpos = 0;

      lc_cur++;
      if (lc_cur >= lc_len)
        lc_cur = 0;

      PulseSyncEvent *pevt =
          (PulseSyncEvent *)Event::GetEventByType(T_EV_PulseSync);
      if (pevt != 0)
        app->getEMG()->BroadcastEvent(pevt, this);

      app->getBMG()->HiPriTrigger(this);
      metroofs = 0;

      l -= remaining;
      memset(out, 0, sizeof(sample_t) * remaining);
      ofs += remaining;
    }

    for (int i = 0; i < numsyncpos; i++) {
      if (syncpos[i].cb != 0 &&
          ((oldpos < syncpos[i].syncpos && curpos >= syncpos[i].syncpos) ||
           (wrapped && syncpos[i].syncpos <= curpos))) {
        syncpos[i].cb->PulseSync(i, curpos);
      }
    }
  }

  if (metroofs < metrolen) {
    if (metroactive && metrovol > 0.0) {
      nframes_t i = 0;
      for (; i < l && metroofs + i < metrolen; i++)
        out[ofs + i] = metro[metroofs + i] * metrovol;
      for (; i < l; i++)
        out[ofs + i] = 0;
    } else {
      memset(&out[ofs], 0, sizeof(sample_t) * l);
    }

    if (!pre)
      metroofs += l;
  } else {
    memset(&out[ofs], 0, sizeof(sample_t) * l);
  }

  if (metrohiofs < metrotonelen) {
    if (metroactive && metrovol > 0.0) {
      nframes_t i = 0;
      for (; i < l && metrohiofs + i < metrotonelen; i++)
        out[ofs + i] += metrohitone[metrohiofs + i] * metrovol;
    }

    if (!pre)
      metrohiofs += l;
  }

  if (metroloofs < metrotonelen) {
    if (metroactive && metrovol > 0.0) {
      nframes_t i = 0;
      for (; i < l && metroloofs + i < metrotonelen; i++)
        out[ofs + i] += metrolotone[metroloofs + i] * metrovol;
    }

    if (!pre)
      metroloofs += l;
  }

  if (ab->outs[1][0] != 0)
    memcpy(ab->outs[1][0], ab->outs[0][0], sizeof(sample_t) * l);

  if (profile_start_ticks != 0)
    DspProfile::RecordPulseProcess(DspProfile::NowTicks() - profile_start_ticks,
                                   l);
}
