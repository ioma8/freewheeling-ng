/* 
   Music holds us together
   "it becomes a movement"
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

#include "fweelin_config.h"
#include "fweelin_core_dsp.h"

const float Processor::MIN_VOL = 0.01;
const nframes_t Processor:: DEFAULT_SMOOTH_LENGTH = 64;
// Length of metronome strike sound in samples
const nframes_t Pulse::METRONOME_HIT_LEN = 800;
// Length of metronome tone sound in samples
const nframes_t Pulse::METRONOME_TONE_LEN = 4400;
// Initial metronome volume
const float Pulse::METRONOME_INIT_VOL = 0.1;

// AutoLimitProcessor
const float AutoLimitProcessor::LIMITER_ATTACK_LENGTH = 1024,
  AutoLimitProcessor::LIMITER_START_AMP = 1.0;
const nframes_t AutoLimitProcessor::LIMITER_ADJUST_PERIOD = 64;
//

const float RecordProcessor::OVERDUB_DEFAULT_FEEDBACK = 0.5;

// DB FADER CODE
#define DB_FLOOR -1000.f
#define FADER_MIN_DB -70.f

int math_gcd (int a, int b) {
  return (b == 0 ? a : math_gcd(b, a % b));
};

int math_lcm (int a, int b) {
  return a*b/math_gcd(a,b);
};

// IEC 60-268-18 fader levels.  Thanks to Steve Harris. 
// Thanks to Chris Cannam & Sonic Visualiser code

float iec_dB_to_fader (float db)
{
  float def = 0.0f; // Meter deflection %age

  if (db < -70.0f) {
    def = 0.0f;
  } else if (db < -60.0f) {
    def = (db + 70.0f) * 0.25f;
  } else if (db < -50.0f) {
    def = (db + 60.0f) * 0.5f + 2.5f; // used to be +5.0f, but this caused a discontinuity in low dB deflections
  } else if (db < -40.0f) {
    def = (db + 50.0f) * 0.75f + 7.5f;
  } else if (db < -30.0f) {
    def = (db + 40.0f) * 1.5f + 15.0f;
  } else if (db < -20.0f) {
    def = (db + 30.0f) * 2.0f + 30.0f;
  } else {
    def = (db + 20.0f) * 2.5f + 50.0f;
  }

  return def;
}

static float iec_fader_to_dB(float def)  // Meter deflection %age
{
  float db = 0.0f;
  
  if (def >= 50.0f) {
    db = (def - 50.0f) / 2.5f - 20.0f;
  } else if (def >= 30.0f) {
    db = (def - 30.0f) / 2.0f - 30.0f;
  } else if (def >= 15.0f) {
    db = (def - 15.0f) / 1.5f - 40.0f;
  } else if (def >= 7.5f) {
    db = (def - 7.5f) / 0.75f - 50.0f;
  } else if (def >= 2.5f) {           // used to be 0.5, ...
    db = (def - 2.5f) / 0.5f - 60.0f; // used to be (def - 5.0f), but this caused a discontinuity in low dB deflections
  } else {
    db = (def / 0.25f) - 70.0f;
  }

  return db;
}

float AudioLevel::fader_to_dB (float level, float maxDb)
{
  if (level == 0.) 
    return DB_FLOOR;

  float maxPercent = iec_dB_to_fader(maxDb);
  float percent = level * maxPercent;
  float dB = iec_fader_to_dB(percent);
  return dB;
}

float AudioLevel::dB_to_fader (float dB, float maxDb)
{
  if (dB == DB_FLOOR) 
    return 0.;

  // The IEC scale gives a "percentage travel" for a given dB
  // level, but it reaches 100% at 0dB.  So we want to treat the
  // result not as a percentage, but as a scale between 0 and
  // whatever the "percentage" for our (possibly >0dB) max dB is.
    
  float maxPercent = iec_dB_to_fader(maxDb);
  float percent = iec_dB_to_fader(dB);
  float faderLevel = percent / maxPercent;
    
  if (faderLevel < 0.) faderLevel = 0.;
  if (faderLevel > 1.0) faderLevel = 1.0;
    
  return faderLevel;
}
//

// *********** CORE SIGNAL PROCESSING

Processor::Processor(Fweelin *app) : 
  app(app), prelen(DEFAULT_SMOOTH_LENGTH), prewritten(0), prewriting(0) {
  // Create preprocess audio buffers (for smoothing)
  preab = new AudioBuffers(app);
  for (int i = 0; i < preab->numouts; i++) {
    preab->outs[0][i] = new sample_t[prelen];
    preab->outs[1][i] = (preab->IsStereoMaster() ? new sample_t[prelen] : 0);
  }
};

Processor::~Processor() {
  // Delete preprocess audio buffers
  for (int i = 0; i < preab->numouts; i++) {
    delete[] preab->outs[0][i];
    if (preab->outs[1][i] != 0)
      delete[] preab->outs[1][i];
  }

  delete preab;
};

void Processor::dopreprocess() {
  if (prewriting) {
    printf("Caught ourselves writing PREPROCESS!\n");
    return;
  }
  
  if (!prewritten && !prewriting) {
    // Do a preprocess to smooth abrupt changes in flow
    prewriting = 1;
    process(1,prelen,preab); // Process but write to preprocess buffers
    prewriting = 0;
    prewritten = 1;
  }
}

// Fade together current with preprocessed to create a smoothed output during
// control changes
void Processor::fadepreandcurrent(AudioBuffers *ab) {
  if (prewriting) {
    // This does happen
  }
  
  if (prewritten) {
    float dr = 1.0/prelen;
    // For each output
    for (int i = 0; i < ab->numouts; i++) {
      int stereo = (ab->outs[1][i] != 0 ? 1 : 0);
      for (int j = 0; j <= stereo; j++) {
        // Fade between preprocessed and current output
        sample_t *out = ab->outs[j][i];
        if (out != 0) {
          // Do the pre-buffers match the outputs in channels?
          sample_t *pre = preab->outs[j][i];
          if (pre == 0) {
            printf("DSP: ERROR: Pre buffers are null for active output.\n");
          } else {
            float ramp = 0.0;
            for (nframes_t l = 0; l < prelen; l++, ramp += dr)
              out[l] = (out[l]*ramp) + (pre[l]*(1.0-ramp));
          }
        }
      }
    }

    prewritten = 0;
  }
}


// TODO
//
// Please note that all sync is done with a granularity of the audio period 
// size. A potential bug exists when loops recorded with one period size
// are played on a system with another period size... the syncronization
// may not line up, causing a click in the audio.
//
// Not sure how this might be resolved
// For loops whose syncronized length is not a multiple of the period size,
// those loops may need to be resized so that the loop point falls on a
// period boundary
//


AutoLimitProcessor::AutoLimitProcessor(Fweelin *app) :
  Processor(app) {
  ResetLimiter();
};

AutoLimitProcessor::~AutoLimitProcessor() {
};

void AutoLimitProcessor::ResetLimiter() {
  dlimitvol = app->getCFG()->GetLimiterReleaseRate();
  limitvol = LIMITER_START_AMP;
  curlimitvol = LIMITER_START_AMP;
  maxvol = app->getCFG()->GetLimiterThreshhold();
  limiterfreeze = 0;
}

void AutoLimitProcessor::process(char /*pre*/, nframes_t len, AudioBuffers *ab) {
  // Autolimit treats stereo channels as linked
  float localmaxvol = 0.0,
    maxamp = app->getCFG()->GetMaxLimiterGain(),
    limitthresh = app->getCFG()->GetLimiterThreshhold(),
    limitrr = app->getCFG()->GetLimiterReleaseRate();
  int clipcnt = 0;

  sample_t *s_left = ab->outs[0][0],  // Process in place on the output
    *s_right = ab->outs[1][0];
  int stereo = (s_right != 0 ? 1 : 0);

  if (stereo) {
    // Stereo limit
    for (nframes_t l = 0; l < len; l++, s_left++, s_right++) {
      float abssamp_l = fabs(*s_left),
        abssamp_r = fabs(*s_right);

      // Volume scale
      *s_left *= curlimitvol;
      *s_right *= curlimitvol;

      float abssamp2_l = fabs(*s_left),
        abssamp2_r = fabs(*s_right);

      if (!limiterfreeze) {
        // Find local maxima
        if (abssamp_l > localmaxvol)
          localmaxvol = abssamp_l;
        if (abssamp_r > localmaxvol)
          localmaxvol = abssamp_r;

        // Find clipping after limit
        if (abssamp2_l > limitthresh)
          clipcnt++; // Force limit
        if (abssamp2_r > limitthresh)
          clipcnt++; // Force limit

        // Adjust volume scale
        curlimitvol += dlimitvol;
      }

      // Clipping kludge--
      // Some output formats clip near 1.0.. brickwall limit to 1.0
      if (abssamp2_l > 0.99) {
        // Absolute clip! Truncate
        if (*s_left > 0.0)
          *s_left = 0.99;
        else
          *s_left = -0.99;
      }
      if (abssamp2_r > 0.99) {
        // Absolute clip! Truncate
        if (*s_right > 0.0)
          *s_right = 0.99;
        else
          *s_right = -0.99;
      }

      if (l+1 == len || l % LIMITER_ADJUST_PERIOD == 0) {
        // Time to check & adjust limit parameters

        if (clipcnt > 0 || localmaxvol > maxvol) {
          // We got more clipping!
          clipcnt = 0;

          float newlimitvol = limitthresh/localmaxvol;

          // Compute volume reduction necessary to bring in peak
          limitvol = newlimitvol;

          // Compute delta (speed) to get to that reduction (attack time)
          dlimitvol = (limitvol-curlimitvol)/LIMITER_ATTACK_LENGTH;

          // Store new max
          maxvol = localmaxvol;
        }

        if (dlimitvol < 0.0 && curlimitvol <= limitvol) {
          // We are done the attack phase- switch to release!
          dlimitvol = limitrr;
        }

        if (dlimitvol > 0.0 && curlimitvol > maxamp) {
          // Too much amp!
          dlimitvol = 0.0;
        }
      }
    }
  } else {
    // Mono limit
    for (nframes_t l = 0; l < len; l++, s_left++) {
      float abssamp_l = fabs(*s_left);

      // Volume scale
      *s_left *= curlimitvol;

      float abssamp2_l = fabs(*s_left);

      if (!limiterfreeze) {
        // Find local maxima
        if (abssamp_l > localmaxvol)
          localmaxvol = abssamp_l;

        // Find clipping after limit
        if (abssamp2_l > limitthresh)
          clipcnt++; // Force limit

        // Adjust volume scale
        curlimitvol += dlimitvol;
      }

      // Vorbis clipping kludge--
      // Vorbis clips near 1.0.. 1st step is brickwall at 1.0
      // then scale down.. (scaling happens in FileStreamer)
      if (abssamp2_l > 0.99) {
        // Absolute clip! Truncate
        if (*s_left > 0.0)
          *s_left = 0.99;
        else
          *s_left = -0.99;
      }

      if (l+1 == len || l % LIMITER_ADJUST_PERIOD == 0) {
        // Time to check & adjust limit parameters

        if (clipcnt > 0 || localmaxvol > maxvol) {
          // We got more clipping!
          clipcnt = 0;

          float newlimitvol = limitthresh/localmaxvol;

          // Compute volume reduction necessary to bring in peak
          limitvol = newlimitvol;

          // Compute delta (speed) to get to that reduction (attack time)
          dlimitvol = (limitvol-curlimitvol)/LIMITER_ATTACK_LENGTH;

          // Store new max
          maxvol = localmaxvol;
        }

        if (dlimitvol < 0.0 && curlimitvol <= limitvol) {
          // We are done the attack phase- switch to release!
          dlimitvol = limitrr;
        }

        if (dlimitvol > 0.0 && curlimitvol > maxamp) {
          // Too much amp!
          dlimitvol = 0.0;
        }
      }
    }
  }
}


// Overdubbing version of record into existing loop
RecordProcessor::RecordProcessor(Fweelin *app,
                                 InputSettings *iset, 
                                 float *inputvol,
                                 Loop *od_loop,
                                 float od_playvol,
                                 nframes_t od_startofs,
                                 float *od_feedback) : 
  Processor(app), sync_state(SS_NONE), 
  iset(iset), inputvol(inputvol), nbeats(0), endsyncidx(-2), endsyncwait(0), 
  sync_idx(-1), sync_add_pos(0), sync_add(0),
  stopped(0), pa_mgr(0), od_loop(od_loop), od_playvol(od_playvol), 
  od_feedback(od_feedback), od_curbeat(0), od_fadein(1), od_fadeout(0), 
  od_stop(0), od_prefadeout(0), od_lastofs(0) {
  // Store initial value for overdub feedback
  if (od_feedback != 0)
    od_feedback_lastval = *od_feedback;

  // Use the block supplied-- fixed length
  growchain = 0;
  compute_stats = 0;
  recblk = od_loop->blocks;
  
  // Clear the hash for the loop, since overdubbing will change it
  od_loop->SetSaveStatus(NO_SAVE);
  
  mbuf[0] = new sample_t[app->getBUFSZ()];
  od_last_mbuf[0] = new sample_t[app->getBUFSZ()];
  od_last_lpbuf[0] = new sample_t[app->getBUFSZ()];
  // Is the loop we are overdubbing into a stereo loop? If so, run in stereo
  if (recblk->IsStereo()) {
    stereo = 1;
    mbuf[1] = new sample_t[app->getBUFSZ()];
    od_last_mbuf[1] = new sample_t[app->getBUFSZ()];
    od_last_lpbuf[1] = new sample_t[app->getBUFSZ()];
  } else {
    stereo = 0;
    mbuf[1] = 0;
    od_last_mbuf[1] = 0;
    od_last_lpbuf[1] = 0;
  }

  // Create an audioblock iterator to move through that memory
  i = new AudioBlockIterator(recblk,app->getBUFSZ(),
                             app->getPRE_EXTRACHANNEL());
  // And a temporary iterator for overdub jumps
  tmpi = new AudioBlockIterator(recblk,app->getBUFSZ(),
                                app->getPRE_EXTRACHANNEL());

  // Check for quantize to pulse
  sync = od_loop->pulse;
  if (sync != 0) {
    // Behavior is that in overdub we always start immediately 
    // .. at the right place in the loop
    if (od_startofs == 0)
      i->Jump(sync->GetPos());
    else {
      // Calculate correct current beat based on startofs
      od_curbeat = od_startofs/sync->GetLength();
    }

    // Notify Pulse to call us every beat
    sync_state = SS_BEAT;
    sync_add_pos = 0;
    sync_add = 1;
  }

  if (od_startofs != 0) {
    // Start at specified position
    i->Jump(od_startofs);
  }

  // And update peaks&avgs for displays (no grow!)
  pa_mgr = app->getBMG()->PeakAvgOn(recblk,i);
};

// Recording into preexisting fixed size block
// We can optionally specify whether we want the block to be stereo
// Don't try to switch from stereo->mono->stereo for a block, though
// If we don't specify stereo/mono, RecordProcessor decides automatically
// based on the existing 'dest'
RecordProcessor::RecordProcessor(Fweelin *app,
                                 InputSettings *iset, 
                                 float *inputvol,
                                 AudioBlock *dest, int suggest_stereo) :
  Processor(app), sync_state(SS_NONE),
  iset(iset), inputvol(inputvol), sync(0), tmpi(0), nbeats(0), 
  sync_idx(-1), sync_add_pos(0), sync_add(0),
  stopped(0), pa_mgr(0), od_loop(0), od_feedback(0), od_fadeout(0), od_stop(0), 
  od_prefadeout(0) {
  // Use the block supplied-- fixed length
  growchain = 0;
  compute_stats = 1;
  recblk = dest;
  
  // Stereo or mono?
  mbuf[0] = new sample_t[app->getBUFSZ()];
  stereo = (suggest_stereo == -1 ? recblk->IsStereo() : suggest_stereo);
  if (stereo)
    mbuf[1] = new sample_t[app->getBUFSZ()];
  else
    mbuf[1] = 0;
  
  // Create an audioblock iterator to move through that memory
  i = new AudioBlockIterator(recblk,app->getBUFSZ(),
                             app->getPRE_EXTRACHANNEL());
};

// ISSUES-
//
// Loop is straddled right now
// between RecordProcessor & LoopManager-- icky
// make separation more clear
//
// Tricky stuff with those block managers
//

// Recording new blocks, growing size as necessary             
RecordProcessor::RecordProcessor(Fweelin *app,
                                 InputSettings *iset, 
                                 float *inputvol,
                                 Pulse *sync, 
                                 AudioBlock *audiomem,
                                 AudioBlockIterator *audiomemi,
                                 nframes_t peaksavgs_chunksize) : 
  Processor(app), sync_state(SS_NONE),
  iset(iset), inputvol(inputvol), sync(sync), tmpi(0), 
  nbeats(0), endsyncidx(-2), endsyncwait(0), 
  sync_idx(-1), sync_add_pos(0), sync_add(0),
  stopped(0), pa_mgr(0), od_loop(0), od_feedback(0), od_fadeout(0), od_stop(0), 
  od_prefadeout(0) {
  // Grow the length of record as necessary
  growchain = 1;
  compute_stats = 0;
  
  // Get the next available block so we can begin recording now!
  recblk = (AudioBlock *) app->getPRE_AUDIOBLOCK()->RTNew();
  if (recblk == 0) {
    printf("RecordProcessor: ERROR: No free blocks for record\n");
    stopped = 1;
    i = 0;
    return;
  }

  // Stereo or mono?
  mbuf[0] = new sample_t[app->getBUFSZ()];
  if (iset->IsSelectedStereo()) {
    stereo = 1;
    mbuf[1] = new sample_t[app->getBUFSZ()];
  } else {
    stereo = 0;
    mbuf[1] = 0;
  }

  // printf("REC: New loop: %s\n",(stereo ? "stereo" : "mono"));

  // Create an audioblock iterator to move through that memory
  i = new AudioBlockIterator(recblk,app->getBUFSZ(),
                             app->getPRE_EXTRACHANNEL());
  
  // Check for sync to an pulse
  if (sync != 0) {
    if (sync->GetPct() >= 0.5) {
      // Close to next downbeat- so delay record start til then
      stopped = 1; 

      sync_state = SS_START;
      sync_add_pos = 0;
      sync_add = 1;
    } else {
      // Close to previous downbeat- start record now & grab missed 
      // chunk from previous downbeat til now
      
      // Use BED_MarkerPoints in audiomem to get chunk
      BED_MarkerPoints *mp = (BED_MarkerPoints *)
        (audiomem->GetExtendedData(T_BED_MarkerPoints));
      if (mp == 0) {
        // No marker block
        printf("Err (SYNCREC): No markers striped in Audio Mem!\n");
        exit(1);
      } else {
        // Get a sub block from the previous downbeat
        nframes_t curofs = audiomemi->GetTotalLength2Cur(),
          prevofs;
        TimeMarker *prevm = mp->GetMarkerNBeforeCur(1,curofs);
        if (prevm != 0) {
          prevofs = prevm->markofs;
          
          AudioBlock *beginblk = 
            audiomem->GenerateSubChain(prevofs,curofs,stereo);
          if (beginblk == 0) {
            printf("Err: Problem generating subchain from audiomemory.\n");
            exit(1);
          }
          
          // Now reorganize the block chain to put beginblk first
          recblk = recblk->InsertFirst(beginblk);
          
          // Recalculate iterator constants-- iterator should
          // start recording at end of block ripped from audiomemory
          i->GenConstants();
        }
        else {
          // Bad markers (not fatal!)- just don't copy from audiomem
          printf("SYNCREC: Previous markers in Audio Mem unknown!\n");
        }
      }
      
      // Notify Pulse to call us every beat
      sync_state = SS_BEAT;
      sync_add_pos = 0;
      sync_add = 1;
    }
  }
  
  // Concurrently compute peaks & averages during recording
  if (peaksavgs_chunksize != 0) {
    AudioBlock *peaks = (AudioBlock *) recblk->RTNew(),
      *avgs = (AudioBlock *) recblk->RTNew();
    if (peaks == 0 || avgs == 0) {
      printf("RecordProcessor: ERROR: No free blocks for peaks/avgs\n");
      stopped = 1;
      return;
    }
    
    recblk->AddExtendedData(new BED_PeaksAvgs(peaks,avgs,
                                              peaksavgs_chunksize));
    pa_mgr = app->getBMG()->PeakAvgOn(recblk,i,1);
  }
  
  // Tell the block manager to auto-grow this block chain for recording
  app->getBMG()->GrowChainOn(recblk,i);
};

RecordProcessor::~RecordProcessor() 
{
  if (!stopped) {
    // This shouldn't be a problem, it happens in overdub and direct record
    AbortRecording();
  }

  // Notify BlockManager we are dying in case any mgrs depend on us
  app->getBMG()->RefDeleted(this);
  app->getBMG()->RefDeleted(i);

  // Redundant check- stop pulse sync callback
  if (sync != 0 && sync_idx != -1) {
    if (CRITTERS)
      printf("CORE: Stray PulseSync Callback!\n");
    sync->DelPulseSync(sync_idx);
  }
  
  if (i != 0)
    delete i;
  if (tmpi != 0)
    delete tmpi;

  delete[] mbuf[0];
  if (mbuf[1] != 0)
    delete[] mbuf[1];

  if (od_loop != 0) {
    delete[] od_last_mbuf[0];
    if (od_last_mbuf[1] != 0)
      delete[] od_last_mbuf[1];

    delete[] od_last_lpbuf[0];
    if (od_last_lpbuf[1] != 0)
      delete[] od_last_lpbuf[1];
  }
};

void RecordProcessor::SyncUp() {
  // Sync loop to pulse
  if (od_loop != 0) {
    sync_state = SS_START;
    sync = od_loop->pulse;
    sync_add_pos = 0;
    sync_add = 1;
  }
}

nframes_t RecordProcessor::GetRecordedLength() {
  return i->GetTotalLength2Cur();
}

void RecordProcessor::EndNow() {
  // End record now
  if (od_loop == 0)
    stopped = 1; // For straight record, stop all processing
  else 
    od_stop = 1; // For overdub, keep playing going

  EndRecordEvent *endrec = 
    (EndRecordEvent *) Event::GetEventByType(T_EV_EndRecord);
  if (endrec == 0) {
    printf("RecordProcessor: ERROR: No free event memory\n");
    exit(1);
  }

  if (od_loop == 0 && i->GetTotalLength2Cur() == 0) {
    // Zero length record! Forget it!
    endrec->keeprecord = 0;
    AbortRecording();
  }
  else {
    endrec->keeprecord = 1;

    // Not overdubbing..
    if (od_loop == 0) {
      // Chop recorded block to current position (end record)
      i->EndChain();
      
      if (growchain) {
        // Stop expanding this recording
        app->getBMG()->GrowChainOff(recblk);
      }

      // For new non syncronized loops--
      // Bend our strip of audio into a loop
      // Smooth beginning into end, shorten loop-
      if (sync == 0)
        recblk->Smooth();
      /* else
        printf("reclen: %d, synclen: %d\n",recblk->GetTotalLen(),
        sync->GetLength()); */
    }

    // End peaks avgs compute now
    if (pa_mgr != 0) {
      pa_mgr->End();
      app->getBMG()->PeakAvgOff(recblk);
      pa_mgr = 0;
    }
  }

  // Broadcast a message that recording has finished
  // Broadcast nonRT, because EndRecord does non-RT safe operations
  app->getEMG()->BroadcastEvent(endrec, this);
}

// May be called during EndNow() (RT)
// RT safe
void RecordProcessor::AbortRecording() {
  // Stop iterator- this stops GrowChain as well as other
  // managers attached to iterator
  i->Stop();

  // Stop pulse sync
  if (sync != 0) {
    // AbortRecording is RT safe, so reset sync_state to stop further
    // sync processing
    sync_state = SS_ENDED;
  }

  // No more process(), just in case race condition
  // causes process() while we are still cleaning up
  stopped = 1;
 
  // End peaks avgs compute now
  if (pa_mgr != 0) {
    pa_mgr->End();
    app->getBMG()->PeakAvgOff(recblk);
    pa_mgr = 0;
  }

  // Blocks are deleted nonRT in DeleteLoop

  if (growchain)
    // Stop expanding this recording
    app->getBMG()->GrowChainOff(recblk);
}

void RecordProcessor::PulseSync (int syncidx, nframes_t /*actualpos*/) {
  // printf("RecSync: %d ESIdx: %d Sync_Idx: %d ActualPos: %d\n",syncidx,endsyncidx,sync_idx,actualpos);
  
  if (syncidx == endsyncidx && endsyncwait) {
    // End record msg
    
    // printf("PulseSync CB: %d %d\n",syncidx,actualpos);
    
    // End record now
    EndNow();
    // Remove user-defined sync point
    endsyncwait = 0;
    sync->DelPulseSync(endsyncidx);
    // Stop calling use!
    sync_state = SS_ENDED;
  } else if (syncidx == sync_idx) {
    // Regular pulse msg
    switch (sync_state) {
    case SS_START:
      if (od_loop != 0) {
        // Overdub record, fade to start since we are syncing with pulse
        Jump(od_loop->pulse->GetPos());
      }
      else {
        // Start record now
        stopped = 0;
      }
      
      sync_state = SS_BEAT;
      break;
      
    case SS_END:
      nbeats++;
      endsyncwait = 1;
      break;
      
    case SS_BEAT:
      if (od_loop != 0) {
        // Overdub record
        od_curbeat++;
        if (od_curbeat >= od_loop->nbeats) {
          // Quantize loop by restarting
          Jump(od_loop->pulse->GetPos());
          od_curbeat = 0;
        }
      } else {
        // Regular grow record
        nbeats++;
      }
      
      // Call us on further beats
      sync_state = SS_BEAT;
      break;
      
    case SS_ENDED:
      break;
      
    default:
      break;
    }
  }
};

void RecordProcessor::End() {
  if (od_loop != 0) {
    // Overdub ends after short smooth fadeout (see process())
    od_fadeout = 1;
  } else {
    if (sync != 0) {
      // Sync, so check timing
      if (sync->GetPct() >= 0.5 || sync->GetPos() < REC_TAIL_LEN) {
        // Delay record til slightly after downbeat-- we 
        // want to capture an extra 'tail' for crossfading.
        if (sync->GetPos() < REC_TAIL_LEN)
          endsyncwait = 1; // Past the downbeat
        // printf("CORE: Add EndSync\n");
        endsyncidx = sync->AddPulseSync(this,REC_TAIL_LEN);
        sync_state = SS_END;
      } else {
        // Close to previous downbeat- end record now & crop extra chunk
        EndNow();
        
        // Now hack off end of recording to downbeat
        // *** To be tested ***
        /*if (recblk->HackTotalLengthBy(sync->GetPos()))
          printf("Err (SYNCREC): Problem cutting length of record\n");*/
      }
    } else {
      // No sync, stop recording now!
      EndNow();
    }
  }
}

// Fades input samples out of mix- writing to 'dest'
void RecordProcessor::FadeOut_Input(nframes_t len, 
                                    sample_t *input_l, sample_t *input_r,
                                    sample_t *loop_l, sample_t *loop_r, 
                                    float old_fb, float /*new_fb*/, float fb_delta,
                                    sample_t *dest_l, sample_t *dest_r) {
  // Fade out input samples
  // (feedback to 1.0)

  // Left
  float ramp = 0.0, negramp = 1.0,
    dr = 1.0/prelen;
  nframes_t l = 0;
  float cur_fb = old_fb;
  if (fb_delta == 0.0) {
    for (; l < prelen; l++, ramp += dr, negramp -= dr) {
      dest_l[l] = (input_l[l]*negramp) +
        (loop_l[l]*(ramp+negramp*cur_fb));
    }
  } else {
    for (; l < prelen; l++, ramp += dr, negramp -= dr, cur_fb += fb_delta) {
      dest_l[l] = (input_l[l]*negramp) +
        (loop_l[l]*(ramp+negramp*cur_fb));
    }
  }
  
  // Copy remainder of fragment directly from loop
  if (len-l > 0)
    memcpy(&(dest_l[l]),&(loop_l[l]),sizeof(sample_t) * (len-l));

  if (dest_r != 0) {
    // Right
    ramp = 0.0;
    negramp = 1.0;
    cur_fb = old_fb;
    if (fb_delta == 0.0) {
      for (l = 0; l < prelen; l++, ramp += dr, negramp -= dr) {
        dest_r[l] = (input_r[l]*negramp) +
          (loop_r[l]*(ramp+negramp*cur_fb));
      }
    } else {
      for (l = 0; l < prelen; l++, ramp += dr, negramp -= dr, cur_fb += fb_delta) {
        dest_r[l] = (input_r[l]*negramp) +
          (loop_r[l]*(ramp+negramp*cur_fb));
      }
    }
    
    // Copy remainder of fragment directly from loop
    if (len-l > 0)
      memcpy(&(dest_r[l]),&(loop_r[l]),sizeof(sample_t) * (len-l));
  }
};

// Fades input samples into mix- writing to 'dest'
void RecordProcessor::FadeIn_Input(nframes_t len, 
                                   sample_t *input_l, sample_t *input_r,
                                   sample_t *loop_l, sample_t *loop_r, 
                                   float old_fb, float /*new_fb*/, float fb_delta,
                                   sample_t *dest_l, sample_t *dest_r) {
  // Left

  // Fade in input samples
  float ramp = 0.0, negramp = 1.0,
    dr = 1.0/prelen;
  nframes_t l = 0;
  float cur_fb = old_fb;
  if (fb_delta == 0.0) {
    for (; l < prelen; l++, ramp += dr, negramp -= dr) {
      dest_l[l] = (input_l[l]*ramp) +
        (loop_l[l]*(negramp+ramp*cur_fb));
    }
    
    // Proceed as regular mix
    for (; l < len; l++)
      dest_l[l] = input_l[l] + loop_l[l] * cur_fb;         
  } else {
    for (; l < prelen; l++, ramp += dr, negramp -= dr, cur_fb += fb_delta) {
      dest_l[l] = (input_l[l]*ramp) +
        (loop_l[l]*(negramp+ramp*cur_fb));
    }
    
    // Proceed as regular mix
    for (; l < len; l++, cur_fb += fb_delta)
      dest_l[l] = input_l[l] + loop_l[l] * cur_fb;     
  }
  
  if (dest_r != 0) {
    // Right

    // Fade in input samples
    ramp = 0.0;
    negramp = 1.0;
    cur_fb = old_fb;
    if (fb_delta == 0.0) {
      for (l = 0; l < prelen; l++, ramp += dr, negramp -= dr) {
        dest_r[l] = (input_r[l]*ramp) +
          (loop_r[l]*(negramp+ramp*cur_fb));
      }

      // Proceed as regular mix
      for (; l < len; l++)
        dest_r[l] = input_r[l] + loop_r[l] * cur_fb; 
    } else {
      for (l = 0; l < prelen; l++, ramp += dr, negramp -= dr, cur_fb += fb_delta) {
        dest_r[l] = (input_r[l]*ramp) +
        (loop_r[l]*(negramp+ramp*cur_fb));
      }
      
      // Proceed as regular mix
      for (; l < len; l++, cur_fb += fb_delta)
        dest_r[l] = input_r[l] + loop_r[l] * cur_fb; 
    }
  }  
};

// Jumps to a position within an overdubbing loop- fade of input & output
void RecordProcessor::Jump(nframes_t ofs) {
  //dopreprocess();
  //i->Jump(ofs);
  
  nframes_t curofs = i->GetTotalLength2Cur();
  if (curofs != ofs) {
    if (od_loop != 0) {
#if 0
      printf("Overdub jump: %d -> %d\n",curofs,ofs);
      printf("looplen: %d pulselen: %d\n",od_loop->blocks->GetTotalLen(),
             od_loop->pulse->GetLength());

      // Since we are jumping in overdub, we need to fade out/in input

      // We need to fadeout input on preprocess
      // and fadein input on new position

      // So go back to the last processed fragment and fade out input samples
      FadeOut_Input(app->getBUFSZ(), 
                    od_last_mbuf[0], od_last_mbuf[1],
                    od_last_lpbuf[0], od_last_lpbuf[1],
                    od_feedback, 
                    od_last_mbuf[0], od_last_mbuf[1]);

      // Store in last processed fragment
      tmpi->Jump(od_lastofs);
      tmpi->PutFragment(od_last_mbuf[0],od_last_mbuf[1]);
      
      od_fadein = 1;
#endif

      od_lastofs = curofs;
      od_prefadeout = 1;
      od_fadein = 1;
    }

    dopreprocess();
    i->Jump(ofs);
  }
};

void RecordProcessor::process(char pre, nframes_t len, AudioBuffers *ab) {
  nframes_t fragmentsize = app->getBUFSZ();
  if (len > fragmentsize)
    len = fragmentsize;

  if (!pre && sync != 0 && sync_add) {
    // RT thread- add pulse sync callback
    sync_idx = sync->AddPulseSync(this,sync_add_pos);
    sync_add = 0;
    
    // rintf("CORE: Record AddSync: %d @ idx %d\n",sync_add_pos,sync_idx);
  }

  // If we have sync points defined with a pulse,
  // and if sync_state is SS_ENDED, this is the last run through process()
  // so remove the pulse sync(s)
  
  if (!pre && sync_state == SS_ENDED && sync_idx != -1) {
    sync->DelPulseSync(sync_idx);
    sync_idx = -1;
  }

  if (!pre && sync_state == SS_ENDED && endsyncwait) {
    endsyncwait = 0;
    sync->DelPulseSync(endsyncidx);
  }
  
  // Some key behavior:
  // stopped means no processing occurs
  // od_fadein means fade in input samples
  // od_fadeout means fade out input samples (it is the end of overdub)
  // od_stop means overdub has ended but playing from loop is retained

  sample_t *out[2] = {ab->outs[0][0], ab->outs[1][0]};
  if (!stopped) {
    sample_t *lpbuf[2] = {0,0}; // Loop buffer

    // Compute feedback delta
    float new_fb,
      old_fb,
      fb_delta = 0.0;
    if (od_feedback != 0) {
      new_fb = *od_feedback;
      old_fb = od_feedback_lastval;
      fb_delta = (new_fb-old_fb)/len;
    } else
      new_fb = old_fb = OVERDUB_DEFAULT_FEEDBACK;
    if (!pre)
      od_feedback_lastval = new_fb;
    /* if (od_feedback != 0) 
      printf("OLD FB: %f NEW FB: %f DELTA: %f\n",old_fb,new_fb,fb_delta); */
    
    // ** Play part
    if (od_loop != 0) {
      // Overdub- playing & recording

      // Adjust volume
      if (!pre)
        od_loop->UpdateVolume();

      // Check if we need to fade-out at a previous position
      if (!pre && od_prefadeout) {
        // We jumped to a new position in overdub.. fadeout at 
        // old position
        tmpi->Jump(od_lastofs);
        
        // Get audio from the block
        if (stereo)
          tmpi->GetFragment(&lpbuf[0],&lpbuf[1]);
        else 
          tmpi->GetFragment(&lpbuf[0],0);
        
        // Mix selected inputs
        ab->MixInputs(len,mbuf,iset,*inputvol,compute_stats);
        
        FadeOut_Input(len, mbuf[0], mbuf[1],
                      lpbuf[0], lpbuf[1], 
                      old_fb, new_fb, fb_delta,
                      mbuf[0], mbuf[1]);

        if (stereo) 
          tmpi->PutFragment(mbuf[0],mbuf[1]);
        else
          tmpi->PutFragment(mbuf[0],0);

        od_prefadeout = 0;
      }
      
      // Get audio from the block
      if (stereo)
        i->GetFragment(&lpbuf[0],&lpbuf[1]);
      else 
        i->GetFragment(&lpbuf[0],0);

      // Scale volume
      float vol = od_loop->vol * od_playvol;
      // Protect our ears!
      float maxvol = app->getCFG()->GetMaxPlayVol();
      if (maxvol > 0.0 && vol > maxvol)
        vol = maxvol;
      if (vol < 0)
        vol = 0;
      
      // Play to output
      if (stereo) {
        sample_t *o_l = out[0],
          *o_r = out[1],
          *lpb_l = lpbuf[0],
          *lpb_r = lpbuf[1];
        for (nframes_t l = 0; l < len; l++, o_l++, o_r++, lpb_l++, lpb_r++) {
          *o_l = *lpb_l * vol;
          *o_r = *lpb_r * vol;
        }
      } else {
        sample_t *o_l = out[0],
          *lpb_l = lpbuf[0];
        for (nframes_t l = 0; l < len; l++, o_l++, lpb_l++)
          *o_l = *lpb_l * vol;
        if (out[1] != 0)
          // Mono loop into stereo outs- duplicate
          memcpy(out[1],out[0],sizeof(sample_t) * len);
      }

      if (!pre) {
        // Fade together current with preprocessed to create smoothed
        fadepreandcurrent(ab);
      }
    } else {
      // No overdub- not playing- zero outputs
      memset(out[0],0,sizeof(sample_t) * len);
      if (out[1] != 0)
        memset(out[1],0,sizeof(sample_t) * len);
    }

    // ** Record part
    if (!pre && !od_stop) {
      // Mix selected inputs to record loop
      ab->MixInputs(len,mbuf,iset,*inputvol,compute_stats);
      
      if (od_loop != 0) {
        // Overdub- mix new input with loop
        
        // First, make copies of buffers incase we have an overdub jump
        memcpy(od_last_lpbuf[0],lpbuf[0],sizeof(sample_t) * len);
        memcpy(od_last_mbuf[0],mbuf[0],sizeof(sample_t) * len);
        if (stereo) {
          memcpy(od_last_lpbuf[1],lpbuf[1],sizeof(sample_t) * len);
          memcpy(od_last_mbuf[1],mbuf[1],sizeof(sample_t) * len);
        }
        // od_lastofs = i->GetTotalLength2Cur();
                
        if (!od_fadeout && !od_fadein) {
          // Regular case mix
          if (stereo) {
            sample_t *m_l = mbuf[0],
              *m_r = mbuf[1],
              *lpb_l = lpbuf[0],
              *lpb_r = lpbuf[1];
            if (fb_delta == 0.0) {
              for (nframes_t l = 0; l < len; l++, m_l++, m_r++, lpb_l++, 
                   lpb_r++) {
                *m_l += *lpb_l * old_fb;
                *m_r += *lpb_r * old_fb;
              }
            } else {
              for (nframes_t l = 0; l < len; l++, m_l++, m_r++, lpb_l++, 
                   lpb_r++, old_fb += fb_delta) {
                *m_l += *lpb_l * old_fb;
                *m_r += *lpb_r * old_fb;
              }
            }
          } else {
            sample_t *m_l = mbuf[0],
              *lpb_l = lpbuf[0];
            if (fb_delta == 0.0) {
              for (nframes_t l = 0; l < len; l++, m_l++, lpb_l++)
                *m_l += *lpb_l * old_fb;
            } else {
              for (nframes_t l = 0; l < len; l++, m_l++, lpb_l++, old_fb += fb_delta)
                *m_l += *lpb_l * old_fb;
            }
          }
        } else if (od_fadein) {
          // Fade in input samples
          FadeIn_Input(len, mbuf[0], mbuf[1],
                       lpbuf[0], lpbuf[1], 
                       old_fb, new_fb, fb_delta,
                       mbuf[0], mbuf[1]);

          // Proceed as regular overdub
          od_fadein = 0;
        } else if (od_fadeout) {
          // Fade out input samples
          FadeOut_Input(len, mbuf[0], mbuf[1],
                        lpbuf[0], lpbuf[1], 
                        old_fb, new_fb, fb_delta,
                        mbuf[0], mbuf[1]);
        }
      }
      
      // Record
      if (stereo) 
        i->PutFragment(mbuf[0],mbuf[1]);
      else
        i->PutFragment(mbuf[0],0);

      if (!pre) {
        if (od_fadeout) {
          // Now end!
          EndNow();
        }
        
        // Move along
        i->NextFragment();
      }
    } else if (!pre && od_stop) {
      // Ended, no record but just advance
      i->NextFragment();
    }
  } else {
    memset(out[0],0,sizeof(sample_t) * len);
    if (out[1] != 0)
      memset(out[1],0,sizeof(sample_t) * len);
  }
}

PlayProcessor::PlayProcessor(Fweelin *app, Loop *playloop, float playvol,
                             nframes_t startofs) :
  Processor(app), sync_state(SS_NONE), 
  sync_idx(-1), sync_add_pos(0), sync_add(0),
  stopped(0), playloop(playloop), playvol(playvol), curbeat(0) {
  // Stereo?
  stereo = playloop->blocks->IsStereo();

  // Setup iterator to move through loop
  i = new AudioBlockIterator(playloop->blocks,app->getBUFSZ());

  // Check for quantize to pulse
  sync = playloop->pulse;
  if (sync != 0) {
    // Compute start position based on long count of pulse
    if (playloop->nbeats == 0) {
      printf("CORE: Zero beats in loop- adjusting\n");
      playloop->nbeats++;
    }
    
    int startcnt = sync->GetLongCount_Cur() % playloop->nbeats;
    nframes_t startofs_lc = startcnt * sync->GetLength() + sync->GetPos();
    
    // Start using long count
    curbeat = startcnt;
    i->Jump(startofs_lc);

    // Notify Pulse to call us every beat
    sync_state = SS_BEAT;
    sync_add_pos = 0;
    sync_add = 1;
  }
  
#ifdef FWEELIN_EXPERIMENTAL_NOTIFY_PULSE
    // OK, we are synced to a pulse, so notify on every beat of that pulse
    if (startofs == 0 && sync->GetPct() >= 0.5) {
      // Close to next downbeat- so delay play start til then
      stopped = 1; 
      sync_state = SS_START;
      sync_add_pos = 0;
      sync_add = 1;
    } else {
      // Either we are close to a previous downbeat,
      // or we've been set to start at a specific place.
      
      // Start play now at the right place
      if (startofs == 0)
        i->Jump(sync->GetPos());
      else {
        // Calculate correct current beat based on startofs
        curbeat = startofs/sync->GetLength();
      }

      // Notify Pulse to call us every beat
      sync_state = SS_BEAT;
      sync_add_pos = 0;
      sync_add = 1;
    }
  } 

  if (startofs != 0) {
    // Start at specified position
    i->Jump(startofs);
  }
#else // FWEELIN_EXPERIMENTAL_NOTIFY_PULSE
  if (startofs) {}
#endif // FWEELIN_EXPERIMENTAL_NOTIFY_PULSE
};

PlayProcessor::~PlayProcessor() {
  // Notify BlockManager we are dying in case any mgrs depend on us
  app->getBMG()->RefDeleted(this);
  app->getBMG()->RefDeleted(i);

  // Redundant check- stop pulse sync callback
  if (sync != 0 && sync_idx != -1) {
    if (CRITTERS)
      printf("CORE: Stray PulseSync Callback!\n");
    sync->DelPulseSync(sync_idx);
  }

  delete i;
}

void PlayProcessor::SyncUp() {
  // Sync loop to pulse
  sync_state = SS_START;
  sync = playloop->pulse;
  sync_add_pos = 0;
  sync_add = 1;
}

nframes_t PlayProcessor::GetPlayedLength() {
  return i->GetTotalLength2Cur();
}

void PlayProcessor::PulseSync (int /*syncidx*/, nframes_t /*actualpos*/) {
  switch (sync_state) { 
  case SS_START:
    // Start play now
    dopreprocess(); // Fade to start
    i->Jump(sync->GetPos());
    stopped = 0;
    
    // Call us every beat
    sync_state = SS_BEAT;
    break;
    
  case SS_BEAT:
    curbeat++;
    if (curbeat >= playloop->nbeats) {
      // Quantize loop by restarting
      dopreprocess(); // Fade to loop point
      i->Jump(sync->GetPos());
      curbeat = 0;
    }
    
    // Call us on further beats
    sync_state = SS_BEAT;
    break;
    
  case SS_ENDED:
    break;
    
  default:
    break;
  }
};

void PlayProcessor::process(char pre, nframes_t len, AudioBuffers *ab) {
  nframes_t fragmentsize = app->getBUFSZ();
  if (len > fragmentsize) 
    len = fragmentsize;

  if (!pre && sync != 0 && sync_add) {
    // RT thread- add pulse sync callback
    sync_idx = sync->AddPulseSync(this,sync_add_pos);
    sync_add = 0;
  }
  
  if (!pre && sync_state == SS_ENDED && sync_idx != -1) {
    sync->DelPulseSync(sync_idx);
    sync_idx = -1;
  }

  sample_t *out[2] = {ab->outs[0][0], ab->outs[1][0]};
  if (!stopped) {
    if (!pre)
      playloop->UpdateVolume();
    
    // Get audio from the block and put it in out
    sample_t *buf[2];
    if (stereo)
      i->GetFragment(&buf[0],&buf[1]);
    else
      i->GetFragment(&buf[0],0);

    // Scale volume
    float vol = playloop->vol * playvol;
    // Protect our ears!
    float maxvol = app->getCFG()->GetMaxPlayVol();
    if (maxvol > 0.0 && vol > maxvol)
      vol = maxvol;
    if (vol < 0)
      vol = 0;

    // Play
    if (stereo) {
      sample_t *o_l = out[0],
        *o_r = out[1],
        *b_l = buf[0],
        *b_r = buf[1];
      for (nframes_t l = 0; l < len; l++, o_l++, o_r++, b_l++, b_r++) {
        *o_l = *b_l * vol;
        *o_r = *b_r * vol;
      }
    } else {
      sample_t *o_l = out[0],
        *b_l = buf[0];
      for (nframes_t l = 0; l < len; l++, o_l++, b_l++)
        *o_l = *b_l * vol;
      if (out[1] != 0)
        // Mono loop into stereo outs- duplicate
        memcpy(out[1],out[0],sizeof(sample_t) * len);
    }

    if (!pre) {
      // Fade together current with preprocessed to create smoothed
      fadepreandcurrent(ab);
      
      // Advance to next fragment!
      i->NextFragment();
    }
  } else {
    memset(out[0],0,sizeof(sample_t) * len);
    if (out[1] != 0)
      memset(out[1],0,sizeof(sample_t) * len);
  }
}

FileStreamer::FileStreamer(Fweelin *app, int input_idx, char stereo, nframes_t outbuflen) :
  Processor(app), writerstatus(STATUS_STOPPED), input_idx(input_idx), stereo(stereo),
  outname(""), timingname(""), write_timing(0), nbeats(0), outbuflen(outbuflen), threadgo(1) {
  const static size_t STACKSIZE = 1024*128;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr,STACKSIZE);
  printf("FILESTREAMER: Stacksize: %zd.\n",STACKSIZE);

  // Start encoding thread
  int ret = pthread_create(&encode_thread,
                           &attr,
                           run_encode_thread,
                           static_cast<void *>(this));
  if (ret != 0) {
    printf("(FILESTREAMER) pthread_create failed, exiting\n");
    exit(1);
  }
  RT_RWThreads::RegisterReaderOrWriter(encode_thread);

  // Allocate space for time markers
  marks = ::new TimeMarker[MARKERBUFLEN];
}

FileStreamer::~FileStreamer() {
  // Stop any writing
  if (writerstatus != STATUS_STOPPED)
    writerstatus = STATUS_STOP_PENDING;

  // Terminate the management thread
  threadgo = 0;
  pthread_join(encode_thread,0);

  // Erase space for time markers
  ::delete[] marks;

  // printf(" :: Processor: VorbisStreamer end\n");
};

void FileStreamer::InitStreamer() {
  // Allocate output buffer 
  outbuf[0] = new sample_t[outbuflen];
  if (stereo)
    outbuf[1] = new sample_t[outbuflen];
  else 
    outbuf[1] = 0;

  // Setup encoder
  switch (filetype) {
  case VORBIS: // OGG VORBIS
    enc = new VorbisEncoder(app,stereo);
    break;
  case WAV: //WAV 
    enc = new SndFileEncoder(app, outbuflen, stereo, WAV);
    break;
  case FLAC: //FLAC
    enc = new SndFileEncoder(app, outbuflen, stereo, FLAC);
    break;
  case AU: //AU
    enc = new SndFileEncoder(app, outbuflen, stereo, AU);
    break;
  default: 
    enc = new VorbisEncoder(app,stereo);
    break;
  }
};

void FileStreamer::EndStreamer() {
  // Free output buffer
  delete[] outbuf[0];
  if (outbuf[1] != 0)
    delete[] outbuf[1];

  // Cleanup encoder
  delete enc;
};

int FileStreamer::StartWriting(const std::string &filename_stub, const char *stream_type_name, char write_timing, codec type) {
  if (writerstatus != STATUS_STOPPED)
    return -1; // Already writing!

  outname = filename_stub + stream_type_name + app->getCFG()->GetAudioFileExt(type);
  printf("DISK: Open %s for writing\n",outname.c_str());
  outputsize = 0;
  filetype = type;
  this->write_timing = write_timing;
  if (write_timing)
    this->timingname = filename_stub + FWEELIN_OUTPUT_TIMING_EXT;
  else
    this->timingname = "";
  writerstatus = STATUS_START_PENDING;

  return 0;
};

void FileStreamer::process(char pre, nframes_t len, AudioBuffers *ab) {
  // Stream a given input to disk
  sample_t *in[2] = {ab->ins[0][input_idx], ab->ins[1][input_idx]};
  // sample_t *out = ab->outs[0];

  if (!pre && writerstatus == STATUS_RUNNING) {
    // Streamer is active-- dump the realtime data into our buffer
    // for later encoding

    // Vorbis clips near 1.0
    sample_t *b_l = in[0],
      *b_r = in[1];
    if (stereo && b_r != 0) {
      // Stereo
      enc->Preprocess(b_l,b_r,len);

      if (outpos < encodepos && outpos + len > encodepos)
        // Encode thread is not keeping up. We are about to wrap around the ring buffer
        printf("DISK: FileStreamer encoder thread is behind. CPU use too high / too many streams.\n");

      nframes_t writepos = outpos;
      if (writepos + len < outbuflen) {
        memcpy(&outbuf[0][writepos],in[0],sizeof(sample_t) * len);
        memcpy(&outbuf[1][writepos],in[1],sizeof(sample_t) * len);
        writepos += len;
      }
      else {
        // Wrap case- byte-by-byte
        nframes_t n;
        sample_t *o_l = outbuf[0],
          *o_r = outbuf[1];
        for (n = 0; writepos < outbuflen; writepos++, n++) {
          o_l[writepos] = b_l[n];
          o_r[writepos] = b_r[n];
        }
        writepos = 0;
        wrap = 1; 
        for (; n < len; writepos++, n++) {
          o_l[writepos] = b_l[n];
          o_r[writepos] = b_r[n];
        }
      }
      outpos = writepos;
    } else {
      // Mono
      enc->Preprocess(b_l,0,len);

      nframes_t writepos = outpos;
      if (writepos + len < outbuflen) {
        memcpy(&outbuf[0][writepos],in[0],sizeof(sample_t) * len);
        writepos += len;
      }
      else {
        // Wrap case- byte-by-byte
        nframes_t n;
        sample_t *o_l = outbuf[0];
        for (n = 0; writepos < outbuflen; writepos++, n++)
          o_l[writepos] = b_l[n];
        writepos = 0;
        wrap = 1; 
        for (; n < len; writepos++, n++)
          o_l[writepos] = b_l[n];
      }
      outpos = writepos;
    }
  }
};

void FileStreamer::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_PulseSync :
    // Called because of downbeat
    
    // Record the beat
    if (timingfd != 0) {
      nbeats++;
      int writeidx = mkwriteidx;
      marks[writeidx].markofs = app->getRP()->GetSampleCnt()-startcnt;
      marks[writeidx].data = nbeats;
      writeidx++;
      if (writeidx >= MARKERBUFLEN)
        writeidx = 0;
      mkwriteidx = writeidx;
    }
    break;

  default:
    break;
  }
};

void *FileStreamer::run_encode_thread (void *ptr) {
  FileStreamer *inst = static_cast<FileStreamer *>(ptr);
  nframes_t curpos;

  // *** Could FileStreamer be rewritten as a BlockManager, using the
  // BMG manage thread? It could still be a core DSP processor, no?
  while (inst->threadgo || inst->writerstatus != STATUS_STOPPED) {
    switch (inst->writerstatus) {
    case STATUS_RUNNING: {
      // We're running, so feed data to the chosen audio encoder.
      // Make a local copy of outpos-- because it is liable to change
      // during this loop!-- RT thread
      curpos = inst->outpos;
      if (curpos != inst->encodepos) {
        nframes_t num;
        if (inst->wrap) {
          // Wrap case- so take from the end & beginning of circular buffer 
          nframes_t numend = inst->outbuflen - inst->encodepos;
          num = curpos + numend;

          if (numend > 0) {
            inst->outputsize += inst->enc->WriteSamplesToDisk (inst->outbuf, inst->encodepos, numend);
          }
          if (curpos > 0) {
             inst->outputsize += inst->enc->WriteSamplesToDisk (inst->outbuf, 0, curpos);
          }

          inst->wrap = 0;
        } else {
          if (curpos > inst->encodepos) {           
            num = curpos - inst->encodepos;
            inst->outputsize += inst->enc->WriteSamplesToDisk (inst->outbuf, inst->encodepos, num);
          } else {
            // Wrap not set but buffer seems wrapped- this could be caused
            // by extreme processor load and this thread not keeping up with
            // RT- print warning
            printf("DISK: WARNING: FileStreamer Buffer wrap possible?\n");
          }
        }

        inst->encodepos = curpos;
      }
      
      // Now dump markers to gnusound USX for later edit points
      int readidx = inst->mkreadidx;
      while (readidx != inst->mkwriteidx) {
        if (inst->timingfd != 0)
          fprintf(inst->timingfd,"%ld=4 0 0.000000 lb%d\n",
                  (long int) inst->marks[readidx].markofs,
                  (int) inst->marks[readidx].data);
        readidx++;
        if (readidx >= MARKERBUFLEN)
          readidx = 0;
      }
      inst->mkreadidx = readidx;

      break;
    }

    case STATUS_START_PENDING:
      // Open output file
      // printf("DISK: Open output file.\n");
      inst->outfd = fopen(inst->outname.c_str(),"wb");
      if (inst->write_timing)
        inst->timingfd = fopen(inst->timingname.c_str(),"wb");
      else
        inst->timingfd = 0;
      if (inst->outfd != 0) {
        // Setup vorbis lib
        printf("DISK: Initialize streamer.\n");
        inst->InitStreamer();
        
        if (inst->timingfd) {
          // Write header block for GNUSound
          fprintf(inst->timingfd,
                  "[Mixer]\n"
                  "0=1.000000\n"
                  "1=1.000000\n"
                  "Source Is Mute=0\n"
                  "Source Is Solo=0\n\n"
                  "[Markers for track 0]\n");
        }
        
        // Setup beat counting
        inst->nbeats = 0;
        inst->mkwriteidx = 0;
        inst->mkreadidx = 0;
        
        // Notify us on ALL pulse beats... we will stripe them all!
        inst->app->getEMG()->ListenEvent(inst,0,T_EV_PulseSync);
        
        // Set encoder to dump to file we have opened
        inst->enc->SetupFileForWriting(inst->outfd);
        
        inst->outputsize = 0;
        inst->outpos = 0;
        inst->encodepos = 0;
        inst->wrap = 0;
        inst->startcnt = inst->app->getRP()->GetSampleCnt();
        inst->writerstatus = STATUS_RUNNING;
        
        // printf("DISK: Streamer encoding.\n");
      } else {
        printf("DISK: Error writing output file.\n");
        if (inst->outfd != 0)
          fclose(inst->outfd);
        if (inst->timingfd != 0)
          fclose(inst->timingfd);
        inst->writerstatus = STATUS_STOPPED;
        inst->timingfd = 0;
        inst->outname = "";
        inst->timingname = "";
        inst->app->FlushStreamOutName();
      }
      break; 

    case STATUS_STOP_PENDING:
      printf("DISK: Closing streamer.\n");

      // Tell encoder we are stopping
      inst->enc->PrepareFileForClosing();

      // Now end
      inst->EndStreamer();

      // Stop notify on ALL pulse beats.
      inst->app->getEMG()->UnlistenEvent(inst,0,T_EV_PulseSync);

      fclose(inst->outfd);
      if (inst->timingfd != 0) {
        fclose(inst->timingfd);
        inst->timingfd = 0;
      }
      printf("DISK: ..done\n");

      inst->writerstatus = STATUS_STOPPED;
      inst->outname = "";
      inst->timingname = "";
      break; 
    }

    // 100 ms delay between encoding cycles- give other processes a go!
    usleep(100000);
  }

  return 0;
};

PassthroughProcessor::PassthroughProcessor(Fweelin *app, InputSettings *iset,
                                           float *inputvol) : 
  Processor(app), iset(iset), inputvol(inputvol) {
  // Create input settings for all inputs set
  alliset = new InputSettings(app,app->getABUFS()->numins);
};

PassthroughProcessor::~PassthroughProcessor() {
  delete alliset;
  // printf(" :: Processor: PassthroughProcessor end\n");
};

void PassthroughProcessor::process(char pre, nframes_t len, AudioBuffers *ab) {
  nframes_t fragmentsize = app->getBUFSZ();
  if (len > fragmentsize) 
    len = fragmentsize;

  // Single output hack
  sample_t *out[2] = {ab->outs[0][0], ab->outs[1][0]}; 
  if (!pre) {
    // ***
    // This part could be optimized out of RT
   
    // Copy current input settings
    *alliset = *iset;

    // And only mix inputs that are set to monitor
    for (int i = 0; i < alliset->numins; i++)
      alliset->selins[i] = app->getCFG()->IsInputMonitoring(i);
    // ***

    // Mix all inputs together into single output- this is a monitor mix
    ab->MixInputs(len,out,alliset,*inputvol,0);
  }
  else {
    memset(out[0],0,sizeof(sample_t) * len);
    if (out[1] != 0)
      memset(out[1],0,sizeof(sample_t) * len);
  }
}

void InputSettings::SetInputVol(int n, float vol, float logvol) { 
  if (HasInput(n)) {
    if (vol >= 0.) 
      invols[n] = vol; 
    else if (logvol >= 0.)
      invols[n] = DB2LIN(AudioLevel::fader_to_dB(logvol, app->getCFG()->GetFaderMaxDB())); 
    dinvols[n] = 1.0;
  } else {
    printf("CORE: InputSettings- input number %d not in range.\n",n);
  }
}
