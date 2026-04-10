/* 
   Truth has a power like electricity
   When we dance in Truth,
   it is infectious,
   unmistakable. 
*/

/* Copyright 2004-2011 Jan Pekau
   
   This file is part of Freewheeling.
   
   Freewheelisng is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.
   
   Freewheeling is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with Freewheeling.  If not, see <http://www.gnu.org/licenses/>. */

#include <string>
#include <sstream>

#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include <math.h>
#include <string.h>

#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#include <X11/Xlib.h>

#include <glob.h>

#include "fweelin_core.h"
#include "fweelin_fluidsynth.h"
#include "fweelin_paramset.h"
#include "fweelin_looplibrary.h"
#include "fweelin_startup_guard.h"

const float Loop::MIN_VOL = 0.01;
PreallocatedType *Loop::loop_pretype = 0;

namespace {

UserVariable *AddIntConst(FloConfig *cfg, const char *name, int value) {
  UserVariable *var = cfg->AddEmptyVariable((char *) name);
  var->type = T_int;
  *var = value;
  return var;
}

void AddEmptyVariables(FloConfig *cfg, const char *const *names, int count) {
  for (int i = 0; i < count; i++)
    cfg->AddEmptyVariable((char *) names[i]);
}

struct SystemVarLink {
  const char *name;
  CoreDataType type;
  char *ptr;
};

void LinkSystemVars(FloConfig *cfg, const SystemVarLink *links, int count) {
  for (int i = 0; i < count; i++)
    cfg->LinkSystemVariable((char *) links[i].name, links[i].type,
                            links[i].ptr);
}

void RollbackSetupCallback(void *ctx, int /*tag*/) {
  ((Fweelin *) ctx)->RollbackSetup();
}

}

// *********** CORE

Snapshot *Fweelin::getSNAP (int idx) {
  if (idx >= 0 && idx < cfg->GetMaxSnapshots())
    return &snaps[idx];
  else
    return 0;
}

void Snapshot::CreateSnapshot (char *_name, LoopManager *lm, TriggerMap *tmap) {
  if (this->exists && this->name != 0 && _name == 0) {
    // Preserve name of snapshot
    DeleteSnapshot(0);
  } else  
    DeleteSnapshot();

  this->exists = 1;
  if (this->name == 0 && _name != 0) {
    this->name = new char[strlen(_name)+1];
    strcpy(this->name,_name);
  }

  if (lm != 0) {
    // Count all loops
    this->numls = tmap->CountLoops();
    if (this->numls > 0) {
      this->ls = new LoopSnapshot[this->numls];
      
      int idx = 0;
      for (int i = 0; i < tmap->GetMapSize(); i++) {
        if (tmap->GetMap(i) != 0) {
          Loop *loop = tmap->GetMap(i);
          if (idx >= this->numls) {
            printf("CORE: ERROR: Loop count mismatch creating snapshot!\n");
            return;
          }
          
          this->ls[idx++] = LoopSnapshot(i,lm->GetStatus(i),
                                         loop->vol,lm->GetTriggerVol(i));
        }
      }
    }
  }
};

// Trigger snapshot #idx - return nonzero on failure
char Fweelin::TriggerSnapshot (int idx) {
  Snapshot *s = getSNAP(idx);
  if (s != 0 && s->exists) {
    for (int i = 0; i < s->numls; i++) {
      LoopSnapshot *ls = &(s->ls[i]);

      loopmgr->SetLoopVolume(ls->l_idx,ls->l_vol);

      if (ls->status == T_LS_Off && loopmgr->IsActive(ls->l_idx)) {
        loopmgr->Deactivate(ls->l_idx);
      } else if (ls->status == T_LS_Playing || 
                 ls->status == T_LS_Overdubbing) {
        if (loopmgr->GetStatus(ls->l_idx) != T_LS_Playing) {
          // Loop not yet playing
          if (loopmgr->IsActive(ls->l_idx))
            loopmgr->Deactivate(ls->l_idx);
          loopmgr->Activate(ls->l_idx, 0, ls->t_vol);
        } else {
          // Loop already playing- adjust volume
          loopmgr->SetTriggerVol(ls->l_idx, ls->t_vol);
        }
      } 
    }

    return 0;
  } else
    return 1;
};

LoopManager::LoopManager (Fweelin *app) : 
  renamer(0), rename_loop(0), 
  savequeue(0), loadqueue(0), cursave(0), curload(0), numsave(0), numload(0),
  loadloopid(0), needs_saving_stamp(0),
  default_looprange(Range(0,app->getCFG()->GetNumTriggers())),

  autosave(0), app(app), newloopvol(1.0), subdivide(1), curpulseindex(-1) {
  pthread_mutex_init (&loops_lock,0);

  int mapsz = app->getTMAP()->GetMapSize();

  Loop::SetupLoopPreallocation(app->getMMG());

  plist = new Processor *[mapsz];
  status = new LoopStatus[mapsz];
  waitactivate = new int[mapsz];
  waitactivate_shot = new char[mapsz];
  waitactivate_vol = new float[mapsz];
  waitactivate_od = new char[mapsz];
  waitactivate_od_fb = new float *[mapsz];

  numloops = 0;
  numrecordingloops = 0;
  
  lastrecidx = new int[LAST_REC_COUNT];
  memset(lastrecidx, 0, sizeof(int) * LAST_REC_COUNT);
  
  memset(plist, 0, sizeof(Processor *) * mapsz);
  int lst = T_LS_Off;
  memset(status, lst, sizeof(LoopStatus) * mapsz);
  memset(waitactivate, 0, sizeof(int) * mapsz);
  memset(waitactivate_shot, 0, sizeof(char) * mapsz);
  memset(waitactivate_vol, 0, sizeof(float) * mapsz);
  memset(waitactivate_od, 0, sizeof(char) * mapsz);
  memset(waitactivate_od_fb, 0, sizeof(float) * mapsz);
  memset(pulses, 0, sizeof(Pulse *) * MAX_PULSES);

  // Turn on block read/write managers for loading & saving loops
  bread = ::new BlockReadManager(0,this,app->getBMG(),
                                 app->getCFG()->loop_peaksavgs_chunksize);
  bwrite = ::new BlockWriteManager(0,this,app->getBMG());
  app->getBMG()->AddManager(bread);
  app->getBMG()->AddManager(bwrite);

  // Listen for important events
  app->getEMG()->ListenEvent(this,0,T_EV_EndRecord);
  app->getEMG()->ListenEvent(this,0,T_EV_ToggleDiskOutput);
  app->getEMG()->ListenEvent(this,0,T_EV_ToggleSelectLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_SelectOnlyPlayingLoops);
  app->getEMG()->ListenEvent(this,0,T_EV_SelectAllLoops);
  app->getEMG()->ListenEvent(this,0,T_EV_InvertSelection);
  app->getEMG()->ListenEvent(this,0,T_EV_CreateSnapshot);
  app->getEMG()->ListenEvent(this,0,T_EV_RenameSnapshot);
  app->getEMG()->ListenEvent(this,0,T_EV_TriggerSnapshot);
  app->getEMG()->ListenEvent(this,0,T_EV_SwapSnapshots);
  app->getEMG()->ListenEvent(this,0,T_EV_SetAutoLoopSaving);
  app->getEMG()->ListenEvent(this,0,T_EV_SaveLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_SaveNewScene);
  app->getEMG()->ListenEvent(this,0,T_EV_SaveCurrentScene);
  app->getEMG()->ListenEvent(this,0,T_EV_SetLoadLoopId);
  app->getEMG()->ListenEvent(this,0,T_EV_SetDefaultLoopPlacement);

  app->getEMG()->ListenEvent(this,0,T_EV_SlideMasterInVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SlideMasterOutVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SlideInVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SetMasterInVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SetMasterOutVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SetInVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_ToggleInputRecord);

  app->getEMG()->ListenEvent(this,0,T_EV_DeletePulse);
  app->getEMG()->ListenEvent(this,0,T_EV_SelectPulse);
  app->getEMG()->ListenEvent(this,0,T_EV_TapPulse);
  app->getEMG()->ListenEvent(this,0,T_EV_SwitchMetronome);
  app->getEMG()->ListenEvent(this,0,T_EV_SetSyncType);
  app->getEMG()->ListenEvent(this,0,T_EV_SetSyncSpeed);
  app->getEMG()->ListenEvent(this,0,T_EV_SetMidiSync);

  app->getEMG()->ListenEvent(this,0,T_EV_SetTriggerVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_SlideLoopAmp);
  app->getEMG()->ListenEvent(this,0,T_EV_SetLoopAmp);
  app->getEMG()->ListenEvent(this,0,T_EV_AdjustLoopAmp);
  app->getEMG()->ListenEvent(this,0,T_EV_TriggerLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_TriggerSelectedLoops);
  app->getEMG()->ListenEvent(this,0,T_EV_SetSelectedLoopsTriggerVolume);
  app->getEMG()->ListenEvent(this,0,T_EV_AdjustSelectedLoopsAmp);

  app->getEMG()->ListenEvent(this,0,T_EV_MoveLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_RenameLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_EraseLoop);
  app->getEMG()->ListenEvent(this,0,T_EV_EraseAllLoops);
  app->getEMG()->ListenEvent(this,0,T_EV_EraseSelectedLoops);
  app->getEMG()->ListenEvent(this,0,T_EV_SlideLoopAmpStopAll);

  app->getEMG()->ListenEvent(this,0,T_EV_ALSAMixerControlSet);
};

LoopManager::~LoopManager() { 
  // Stop block read/write managers
  bread->End(0);
  bwrite->End();
  app->getBMG()->DelManager(bread);
  app->getBMG()->DelManager(bwrite);

  Loop::TakedownLoopPreallocation();

  // Stop listening
  app->getEMG()->UnlistenEvent(this,0,T_EV_EndRecord);
  app->getEMG()->UnlistenEvent(this,0,T_EV_ToggleDiskOutput);
  app->getEMG()->UnlistenEvent(this,0,T_EV_ToggleSelectLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SelectOnlyPlayingLoops);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SelectAllLoops);
  app->getEMG()->UnlistenEvent(this,0,T_EV_InvertSelection);
  app->getEMG()->UnlistenEvent(this,0,T_EV_CreateSnapshot);
  app->getEMG()->UnlistenEvent(this,0,T_EV_RenameSnapshot);
  app->getEMG()->UnlistenEvent(this,0,T_EV_TriggerSnapshot);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SwapSnapshots);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetAutoLoopSaving);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SaveLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SaveNewScene);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SaveCurrentScene);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetLoadLoopId);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetDefaultLoopPlacement);

  app->getEMG()->UnlistenEvent(this,0,T_EV_SlideMasterInVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SlideMasterOutVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SlideInVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMasterInVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMasterOutVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetInVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_ToggleInputRecord);

  app->getEMG()->UnlistenEvent(this,0,T_EV_DeletePulse);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SelectPulse);
  app->getEMG()->UnlistenEvent(this,0,T_EV_TapPulse);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SwitchMetronome);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetSyncType);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetSyncSpeed);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetMidiSync);

  app->getEMG()->UnlistenEvent(this,0,T_EV_SetTriggerVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SlideLoopAmp);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetLoopAmp);
  app->getEMG()->UnlistenEvent(this,0,T_EV_AdjustLoopAmp);
  app->getEMG()->UnlistenEvent(this,0,T_EV_TriggerLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_TriggerSelectedLoops);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SetSelectedLoopsTriggerVolume);
  app->getEMG()->UnlistenEvent(this,0,T_EV_AdjustSelectedLoopsAmp);

  app->getEMG()->UnlistenEvent(this,0,T_EV_MoveLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_RenameLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_EraseLoop);
  app->getEMG()->UnlistenEvent(this,0,T_EV_EraseAllLoops);
  app->getEMG()->UnlistenEvent(this,0,T_EV_EraseSelectedLoops);
  app->getEMG()->UnlistenEvent(this,0,T_EV_SlideLoopAmpStopAll);

  app->getEMG()->UnlistenEvent(this,0,T_EV_ALSAMixerControlSet);

  EventManager::DeleteQueue(savequeue);
  EventManager::DeleteQueue(loadqueue);

  // Let BMG know that we are ending
  app->getBMG()->RefDeleted((AutoWriteControl *) this);

  delete[] lastrecidx;

  delete[] plist; delete[] status; 
  delete[] waitactivate; delete[] waitactivate_shot; 
  delete[] waitactivate_vol; delete[] waitactivate_od;
  delete[] waitactivate_od_fb;

  pthread_mutex_destroy (&loops_lock);
};

// Get length returns the length of any loop on the specified index
nframes_t LoopManager::GetLength(int index) {
  if (status[index] == T_LS_Recording) {
    // Ooh, we are recording on this index. Get the current length
    return ((RecordProcessor *) plist[index])->GetRecordedLength();
  }
  else {
    Loop *cur = app->getTMAP()->GetMap(index);
    if (cur != 0)
      return cur->blocks->GetTotalLen();
  }

  return 0;
}

// Get length returns the length of any loop on the specified index
// Rounded to its currently quantized length
// Or 0 if the loop has no pulse
nframes_t LoopManager::GetRoundedLength(int index) {
  if (status[index] == T_LS_Recording) {
    // Return 0 when recording
    return 0;

    // Ooh, we are recording on this index. Get the current length
    // return ((RecordProcessor *) plist[index])->GetRecordedLength();
  }
  else {
    Loop *cur = app->getTMAP()->GetMap(index);
    if (cur != 0) {
      if (cur->pulse != 0)
        return cur->pulse->QuantizeLength(cur->blocks->GetTotalLen());
      else
        return 0; // cur->blocks->GetTotalLen();
    }
  }
  
  return 0;
}

float LoopManager::GetPos(int index) {
  if (status[index] == T_LS_Recording)
    return 0.0;
  else {
    Loop *cur = app->getTMAP()->GetMap(index);
    if (cur != 0 && plist[index] != 0) {
      nframes_t playedlen = 0;
      if (status[index] == T_LS_Playing)
        playedlen = ((PlayProcessor *) plist[index])->GetPlayedLength();
      else if (status[index] == T_LS_Overdubbing)
        playedlen = ((RecordProcessor *) plist[index])->GetRecordedLength();

      if (cur->pulse == 0) 
        return (float) playedlen / cur->blocks->GetTotalLen();
      else {
        if (cur->pulse->QuantizeLength(cur->blocks->GetTotalLen()) == 0) {
          printf("LoopManager: ERROR: Problem with quantize GetPos\n");
          exit(1);
        }
        return (float) playedlen / 
          cur->pulse->QuantizeLength(cur->blocks->GetTotalLen());
      }
    }
  }
  
  return 0.0;
}

// Get current # of samples into block chain with given index
nframes_t LoopManager::GetCurCnt(int index) {
  if (status[index] == T_LS_Recording)
    return 0;
  else {
    Loop *cur = app->getTMAP()->GetMap(index);
    if (cur != 0 && plist[index] != 0) {
      if (status[index] == T_LS_Playing)
        return ((PlayProcessor *) plist[index])->GetPlayedLength();
      else if (status[index] == T_LS_Overdubbing) 
        return ((RecordProcessor *) plist[index])->GetRecordedLength();
    }
  }
  
  return 0;
}

// Sets triggered volume on specified index
// If index is not playing, activates the index
void LoopManager::SetTriggerVol(int index, float vol) {
  if (status[index] == T_LS_Playing) 
    ((PlayProcessor *) plist[index])->SetPlayVol(vol);
  else if (status[index] == T_LS_Overdubbing) 
    ((RecordProcessor *) plist[index])->SetODPlayVol(vol);
}

// Gets trigger volume on specified index
// If index is not playing, returns 0
float LoopManager::GetTriggerVol(int index) {
  if (status[index] == T_LS_Playing)
    return ((PlayProcessor *) plist[index])->GetPlayVol();
  else if (status[index] == T_LS_Overdubbing) 
    return ((RecordProcessor *) plist[index])->GetODPlayVol();
  else
    return 0.0;
}

// Returns a loop with the specified index, if one exists
Loop *LoopManager::GetSlot(int index) {
  return app->getTMAP()->GetMap(index);
}

void LoopManager::AdjustOutputVolume(float adjust) {
  app->getRP()->AdjustOutputVolume(adjust);
}

void LoopManager::SetOutputVolume(float set, float logset) {
  if (set >= 0.)
    app->getRP()->SetOutputVolume(set);
  else if (logset >= 0.)
    app->getRP()->SetOutputVolume(DB2LIN(AudioLevel::fader_to_dB(logset, app->getCFG()->GetFaderMaxDB())));
}

float LoopManager::GetOutputVolume() {
  return app->getRP()->GetOutputVolume();
}

void LoopManager::AdjustInputVolume(float adjust) {
  app->getRP()->AdjustInputVolume(adjust);
}

void LoopManager::SetInputVolume(float set, float logset) {
  if (set >= 0.)
    app->getRP()->SetInputVolume(set);
  else if (logset >= 0.)
    app->getRP()->SetInputVolume(DB2LIN(AudioLevel::fader_to_dB(logset, app->getCFG()->GetFaderMaxDB())));
}

float LoopManager::GetInputVolume() {
  return app->getRP()->GetInputVolume();
}

void LoopManager::SetLoopVolume(int index, float val) {
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0) {
    // First, preprocess for smoothing
    Processor *p = GetProcessor(index);
    if (p != 0) 
      p->dopreprocess();

    if (val >= 0.0)
      lp->vol = val;
    else
      lp->vol = 0.0;
  }
}

float LoopManager::GetLoopVolume(int index) {
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0)
    return lp->vol;
  else
    return 1.0;
}

void LoopManager::AdjustLoopVolume(int index, float adjust) {
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0) {
    lp->dvol += adjust*app->getAUDIO()->GetTimeScale();
    if (lp->dvol < 0.0)
      lp->dvol = 0.0;
  }
}

void LoopManager::SetLoopdVolume(int index, float val) {
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0)
    lp->dvol = val;
}

float LoopManager::GetLoopdVolume(int index) {
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0)
    return lp->dvol;
  else
    return 1.0;
}

void LoopManager::SelectPulse (int pulseindex) {
  if (pulseindex == -1) {
    if (GetCurPulse() != 0) 
      GetCurPulse()->SetMIDIClock(0); // Stop MIDI clock
    
    //printf("**Select: No pulse\n");
    curpulseindex = -1;    
  } else if (pulseindex < 0 || pulseindex >= MAX_PULSES) {
    printf("CORE: Invalid pulse #%d, ignoring.\n",pulseindex);
    return;
  } else if (pulses[pulseindex] == 0) {
    //printf("New pulse[%d]: %d SUB: %d\n", pulseindex, lastindex, subdivide);
    CreatePulse(lastindex, pulseindex, subdivide);
  } else {
    //printf("Select pulse[%d]\n", pulseindex);
    curpulseindex = pulseindex;
    StripePulseOn(pulses[pulseindex]);
    
    // Select pulse, send MIDI start
    GetCurPulse()->SetMIDIClock(1);
  }
}

// Save a whole scene, with an optional filename-
// if none is given, saves a new scene
void TriggerMap::Save(Fweelin *app, char *filename) {
  if (GetSaveStatus() == NO_SAVE) {
    // Scene hash is generated from hash of all loops in the triggermap--
    // so start by saving all loops
    app->getLOOPMGR()->SetAutoLoopSaving(0);
    for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) 
      app->getLOOPMGR()->SaveLoop(i);

    // Now, we have to wait until all that saving is done.
    // Queue a scene marker event in the save queue
    SceneMarkerEvent *sEvt = (SceneMarkerEvent *) Event::GetEventByType(T_EV_SceneMarker,1);
    if (filename != 0)
      strncpy(sEvt->s_filename,filename,FWEELIN_OUTNAME_LEN);
    
    app->getLOOPMGR()->AddToSaveQueue(sEvt);
  }
};

void TriggerMap::SetMap (int index, Loop *smp) {
  if (index < 0 || index >= mapsize) {
    printf("SetMap: Invalid loop index!\n");
  }
  else {
    map[index] = smp;
    TouchMap();
    
    // Fire off a TriggerSet event
    TriggerSetEvent *tevt = (TriggerSetEvent *) 
      Event::GetEventByType(T_EV_TriggerSet,1);
    tevt->idx = index;
    tevt->nw = smp;
    app->getEMG()->BroadcastEventNow(tevt, this);
  }
};

void TriggerMap::GoSave(char *filename) {
  // All loops in the scene are now hashed and saved

  char newScene = (filename[0] == '\0');  // New scene or overwrite existing?
  if (newScene) {
    // Begin our save by generating a scene hash from the loop hashes
    // This will give us an appropriate scene filename
    md5_ctx md5gen;
    md5_init(&md5gen);
    for (int i = 0; i < this->mapsize; i++)
      if (this->map[i] != 0) {
        if (this->map[i]->GetSaveStatus() == SAVE_DONE)
        {
          // Update scene hash with hash from this loop
          const uint8_t* data = (uint8_t*)this->map[i]->GetSaveHash();
          md5_update(&md5gen,SAVEABLE_HASH_LENGTH,data);
        }
        else
          printf("DISK: WARNING: Loop %d not saved yet but scene about to be "
                 "saved!\n",i);
      }
    
    // Done- compute our final hash
    md5_digest(&md5gen,SAVEABLE_HASH_LENGTH,GetSaveHash());
  }
  
  SetSaveStatus(SAVE_DONE);
    
  // Compose filenames & start writing
  char tmp[FWEELIN_OUTNAME_LEN];
  if (newScene) {
    GET_SAVEABLE_HASH_TEXT(GetSaveHash());
    snprintf(tmp,FWEELIN_OUTNAME_LEN,"%s/%s-%s%s",
             app->getCFG()->GetLibraryPath(),FWEELIN_OUTPUT_SCENE_NAME,
             hashtext,FWEELIN_OUTPUT_DATA_EXT);
  } else
    snprintf(tmp,FWEELIN_OUTNAME_LEN,"%s%s",
             filename,FWEELIN_OUTPUT_DATA_EXT);
             
  if (!newScene) {
    // Back up existing scene data
    struct stat st;
    if (stat(tmp,&st) == 0) {
      // First available backup filename
      unsigned int tmp2_size = FWEELIN_OUTNAME_LEN + 20;
      char tmp2[tmp2_size];
      unsigned char bCnt = 1;
      char go = 1;
      do {
        snprintf(tmp2,tmp2_size,"%s.backup.%d",tmp,bCnt);
        if (stat(tmp2,&st) != 0)
          go = 0; // Free backup filename
        else
          bCnt++;
      } while (go && bCnt % 256) ;
      
      printf("INIT: Backup existing scene.\n");
      if (rename(tmp, tmp2) != 0) {
        printf("INIT: Error %d moving '%s' to '%s'.\n",errno,tmp,tmp2);
      }
    }
  }
  
  struct stat st;
  printf("DISK: Opening %s '%s' for saving.\n",
         (newScene ? "new scene" : "existing scene"),
         tmp);
  if (newScene && stat(tmp,&st) == 0) {
    printf("DISK: ERROR: MD5 collision while saving scene- file exists!\n");
  } else {
    // Save scene XML data
    xmlDocPtr ldat = xmlNewDoc((xmlChar *) "1.0");
    if (ldat != 0) {
      const static int XT_LEN = 11;
      char xmltmp[XT_LEN];
      
      // Scene
      ldat->children = xmlNewDocNode(ldat,0,
                                     (xmlChar *) FWEELIN_OUTPUT_SCENE_NAME,0);

      // Loops
      for (int i = 0; i < this->mapsize; i++)
        if (this->map[i] != 0 && this->map[i]->GetSaveStatus() == SAVE_DONE) {
          xmlNodePtr lp = xmlNewChild(ldat->children, 0, 
                                      (xmlChar *) FWEELIN_OUTPUT_LOOP_NAME, 0);

          // Loop index
          snprintf(xmltmp,XT_LEN,"%d",i);
          xmlSetProp(lp,(xmlChar *) "loopid",(xmlChar *) xmltmp);

          // Loop hash (used to find the loop on disk)
          unsigned char *sh = this->map[i]->GetSaveHash();
          GET_SAVEABLE_HASH_TEXT(sh);
          xmlSetProp(lp,(xmlChar *) "hash",(xmlChar *) hashtext);

          // Loop volume
          snprintf(xmltmp,XT_LEN,"%.5f",this->map[i]->vol);
          xmlSetProp(lp,(xmlChar *) "volume",(xmlChar *) xmltmp);
        }

      // Snapshots
      Snapshot *snaps = app->getSNAPS();
      for (int i = 0; i < app->getCFG()->GetMaxSnapshots(); i++) {
        if (snaps[i].exists) {
          Snapshot *s = &snaps[i];
          xmlNodePtr sp = xmlNewChild(ldat->children, 0, 
                                      (xmlChar *) FWEELIN_OUTPUT_SNAPSHOT_NAME, 0);

          // Snapshot index
          snprintf(xmltmp,XT_LEN,"%d",i);
          xmlSetProp(sp,(xmlChar *) "snapid",(xmlChar *) xmltmp);

          // Name
          if (s->name != 0)
            xmlSetProp(sp,(xmlChar *) "name",(xmlChar *) s->name);

          for (int j = 0; j < s->numls; j++) {
            LoopSnapshot *ls = &(s->ls[j]);
            xmlNodePtr slp = xmlNewChild(sp, 0, 
                                         (xmlChar *) FWEELIN_OUTPUT_LOOPSNAPSHOT_NAME, 0);
            
            // Loop index
            snprintf(xmltmp,XT_LEN,"%d",ls->l_idx);
            xmlSetProp(slp,(xmlChar *) "loopid",(xmlChar *) xmltmp);

            // Loop status
            snprintf(xmltmp,XT_LEN,"%d",ls->status);
            xmlSetProp(slp,(xmlChar *) "status",(xmlChar *) xmltmp);

            // Loop volume
            snprintf(xmltmp,XT_LEN,"%.5f",ls->l_vol);
            xmlSetProp(slp,(xmlChar *) "loopvol",(xmlChar *) xmltmp);

            // Trigger volume
            snprintf(xmltmp,XT_LEN,"%.5f",ls->t_vol);
            xmlSetProp(slp,(xmlChar *) "triggervol",(xmlChar *) xmltmp);
          }
        }
      }

      xmlSaveFormatFile(tmp,ldat,1);
      xmlFreeDoc(ldat);

      if (newScene) {
        // Add scene to browser so we can load it
        Browser *br = app->getBROWSER(B_Scene);
        if (br != 0) {
          app->setCURSCENE(app->getLOOPMGR()->AddSceneToBrowser(br,tmp));
          br->AddDivisions(FWEELIN_FILE_BROWSER_DIVISION_TIME);
        }
      }

      printf("DISK: Close output.\n");
    }
  }
};

// If we are autosaving, we have to maintain a list of new loops to be saved
void LoopManager::CheckSaveMap() {
  if (needs_saving_stamp != app->getTMAP()->GetLastUpdate()) {
    //printf("Rebuild save map.\n");

    // No, rebuild!
    numsave = 0;
    savequeue = EventManager::DeleteQueue(savequeue);

    // Scan for loops that haven't yet been saved, add them to our list
    TriggerMap *tmap = app->getTMAP();
    int mapsz = tmap->GetMapSize();
    for (int i = 0; i < mapsz; i++) {
      Loop *l = tmap->GetMap(i);
      if (l != 0 && l->GetSaveStatus() == NO_SAVE) {
        // Loop exists but not saved-- add to our map
        LoopListEvent *ll = (LoopListEvent *) 
          Event::GetEventByType(T_EV_LoopList,1);
        ll->l = l;
        
        numsave++;
        EventManager::QueueEvent(&savequeue,ll);
      }
    }

    // Now we've updated map
    needs_saving_stamp = tmap->GetLastUpdate();
  } else {
    //printf("Stamp match: %lf\n",needs_saving_stamp);
  }
}

void LoopManager::StripePulseOn(Pulse *pulse) {
  app->getBMG()->StripeBlockOn(pulse,app->getAMPEAKS(),
                               app->getAMPEAKSI());
  app->getBMG()->StripeBlockOn(pulse,app->getAUDIOMEM(),
                               app->getAUDIOMEMI());
}

void LoopManager::StripePulseOff(Pulse *pulse) {
  app->getBMG()->StripeBlockOff(pulse,app->getAMPEAKS());
  app->getBMG()->StripeBlockOff(pulse,app->getAUDIOMEM());
}

// Creates a pulse of the given length in the first available slot,
// if none already exists of the right length
Pulse *LoopManager::CreatePulse(nframes_t len) {
  // First, check to see if we have a pulse of the right length-
  int i;
  for (i = 0; i < MAX_PULSES && 
         (pulses[i] == 0 || pulses[i]->GetLength() != len); i++);
  if (i < MAX_PULSES)
    // Found it, use this pulse
    return pulses[i];
  else {
    // No pulse found of right length-- create a new one
    for (i = 0; i < MAX_PULSES && pulses[i] != 0; i++);
    if (i < MAX_PULSES) {
      app->getRP()->AddChild(pulses[i] = 
                             new Pulse(app,len,0),
                             ProcessorItem::TYPE_HIPRIORITY);
      StripePulseOn(pulses[i]);
      curpulseindex = i;

      // Send MIDI start for pulse
      GetCurPulse()->SetMIDIClock(1);
      
      return pulses[i];
    } else
      // No space for a new pulse!
      return 0;
  }
};

// Create a time pulse around the specified index
// The length of the loop on the specified index becomes
// a time constant around which other loops center themselves
// subdivide the length of the loop by subdivide to get the core pulse
void LoopManager::CreatePulse(int index, int pulseindex, int sub) {
  Loop *cur = app->getTMAP()->GetMap(index);
  if (cur != 0 && (status[index] == T_LS_Off ||
                   status[index] == T_LS_Playing ||
                   status[index] == T_LS_Overdubbing)) {
    // Set pulse length based on loop length
    nframes_t len = GetLength(index);
    if (len != 0) {
      // Create iterator
      len /= sub; // Length subdivide
      nframes_t startpos = 0;
      // So set the starting pulse position based on where loop is playing
      if (status[index] == T_LS_Playing)
        startpos = ((PlayProcessor *) plist[index])->GetPlayedLength() % len;
      else if (status[index] == T_LS_Overdubbing)
        startpos = ((RecordProcessor *) plist[index])->
          GetRecordedLength() % len;
      
      app->getRP()->AddChild(cur->pulse = pulses[pulseindex] = 
                             new Pulse(app,len,startpos),
                             ProcessorItem::TYPE_HIPRIORITY);
      StripePulseOn(cur->pulse);
      curpulseindex = pulseindex;
      cur->nbeats = sub; // Set # of beats in the loop

      // Send MIDI start for pulse
      GetCurPulse()->SetMIDIClock(1);
      
      // Now reconfigure processor on this index to be synced to the new pulse
      if (status[index] == T_LS_Playing) 
        ((PlayProcessor *) plist[index])->SyncUp();
      else if (status[index] == T_LS_Overdubbing)
        ((RecordProcessor *) plist[index])->SyncUp();
    }
  }
}

// Taps a pulse- starting at the downbeat- if newlen is nonzero, the pulse's
// length is adjusted to reflect the length between taps- and a new pulse
// is created if none exists
void LoopManager::TapPulse(int pulseindex, char newlen) {
  // If more than TIMEOUT_RATIO * the current pulse length
  // frames have passed since the last tap, a new length is not defined
  const static float TAP_NEWLEN_TIMEOUT_RATIO = 5.0, //2.0,
    // Higher graduation makes tempo more stable against changes
    TAP_NEWLEN_GRADUATION = 0.0, //0.5,
    // Tolerance for rejecting tap tempo changes- as fraction of current length
    TAP_NEWLEN_REJECT_TOLERANCE = 1.0; //0.3;

  if (pulseindex >= 0 || pulseindex < MAX_PULSES) {
    Pulse *cur = pulses[pulseindex];
    if (cur == 0) {
      if (newlen) {
        // New pulse- tap now!- set zero length for now
        cur = pulses[pulseindex] = new Pulse(app,0,0);
        cur->stopped = 1;
        cur->prevtap = app->getRP()->GetSampleCnt();
        //cur->SwitchMetronome(1);
        app->getRP()->AddChild(cur,ProcessorItem::TYPE_HIPRIORITY);
        
        StripePulseOn(cur);
        curpulseindex = pulseindex;
      }
    } else {
      // Refresh sync
      SelectPulse(-1);
      SelectPulse(pulseindex);

      // Test position of axis
      char nextdownbeat = 0;
      if (cur->GetPct() >= 0.5)
        nextdownbeat = 1;

      // Old pulse- tap now!
      if (newlen) {
        // Redefine length from tap
        nframes_t oldlen = cur->GetLength(),
          newtap = app->getRP()->GetSampleCnt(),
          newlen = newtap - cur->prevtap;

        // .. only if the new length isn't outrageous
        if (oldlen < 64)
          cur->SetLength(newlen);
        else if (newlen < oldlen * TAP_NEWLEN_TIMEOUT_RATIO) {
          // 2nd outrageous length check
          float ratio = (float) MIN(newlen,oldlen)/MAX(newlen,oldlen);
          if (ratio > 1.0-TAP_NEWLEN_REJECT_TOLERANCE)
            cur->SetLength((nframes_t) (oldlen*TAP_NEWLEN_GRADUATION +
                                        newlen*(1-TAP_NEWLEN_GRADUATION)));
        }

        cur->prevtap = newtap;
        //cur->SwitchMetronome(1);
        cur->stopped = 0;
      }

      if (nextdownbeat)
        // Tap to beginning- with wrap
        cur->Wrap();
      else
        // Tap to beginning- no wrap
        cur->SetPos(0);

      // Notify external transport that we have moved
      app->getAUDIO()->RelocateTransport(0);
    }
  } else
    printf("CORE: Invalid pulse #%d, ignoring.\n",pulseindex);
}

void LoopManager::SwitchMetronome(int pulseindex, char active) {
  if (pulseindex >= 0 || pulseindex < MAX_PULSES) {
    Pulse *cur = pulses[pulseindex];
    if (cur != 0)
      cur->SwitchMetronome(active);
    else
      printf("CORE: No pulse at #%d, ignoring.\n",pulseindex);
  } else
    printf("CORE: Invalid pulse #%d, ignoring.\n",pulseindex);
}

void LoopManager::DeletePulse(int pulseindex) {
  if (pulseindex < 0 || pulseindex >= MAX_PULSES) {
    printf("CORE: Invalid pulse #%d, ignoring.\n",pulseindex);
    return;
  }

  if (pulses[pulseindex] != 0) {
    // Stop striping beats from this pulse
    StripePulseOff(pulses[pulseindex]);

    // Erase all loops which are attached to this pulse-- or we'll have
    // references pointing to the deleted pulse
    int nt = app->getCFG()->GetNumTriggers();
    for (int i = 0; i < nt; i++)
      if (GetPulse(i) == pulses[pulseindex]) 
        DeleteLoop(i);

    // Erase this pulse
    Processor *p = pulses[pulseindex];
    pulses[pulseindex] = 0;
    app->getRP()->DelChild(p);
  }
}

Pulse *LoopManager::GetPulse(int index) {
  Loop *lp = GetSlot(index);
  if (lp != 0)
    return lp->pulse;
  else
    return 0;
}

// Move the loop at specified index to another index
// only works if target index is empty
// returns 1 if success
int LoopManager::MoveLoop (int src, int tgt) {
  Loop *srloop = app->getTMAP()->GetMap(src);
  if (srloop != 0) {
    Loop *tgtloop = app->getTMAP()->GetMap(tgt);
    if (tgtloop == 0) {
      app->getTMAP()->SetMap(tgt,srloop);
      app->getTMAP()->SetMap(src,0);
      plist[tgt] = plist[src];
      plist[src] = 0;
      status[tgt] = status[src];
      status[src] = T_LS_Off;
      waitactivate[tgt] = waitactivate[src];
      waitactivate[src] = 0;
      waitactivate_shot[tgt] = waitactivate_shot[src];
      waitactivate_shot[src] = 0;
      waitactivate_vol[tgt] = waitactivate_vol[src];
      waitactivate_vol[src] = 0.;
      waitactivate_od[tgt] = waitactivate_od[src];
      waitactivate_od[src] = 0;
      waitactivate_od_fb[tgt] = waitactivate_od_fb[src];
      waitactivate_od_fb[src] = 0;
      for (int i = 0; i < LAST_REC_COUNT; i++)  
        if (lastrecidx[i] == src)
          lastrecidx[i] = tgt;

      if (lastindex == src)
        lastindex = tgt;
      
      UpdateLoopLists_ItemMoved(src,tgt);
    }
    else 
      return 0;
  }
  else
    return 0;
  
  return 1;
}

// Delete the loop at the specified index..
// Not RT safe!
// Threadsafe
void LoopManager::DeleteLoop (int index) {
  LockLoops();

  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp != 0) {
    // First, zero the map at the given index
    // To prevent anybody from attaching to the loop as we delete it
    app->getTMAP()->SetMap(index, 0);
  }

  if (plist[index] != 0) {
    // We have a processor on this loop! Stop it!
    if (status[index] == T_LS_Recording) {
      RecordProcessor *recp = (RecordProcessor *) plist[index];
      recp->AbortRecording();
      AudioBlock *recblk = recp->GetFirstRecordedBlock();
      if (recblk != 0) {
        app->getBMG()->RefDeleted(recblk);
        recblk->DeleteChain(); // *** Not RT Safe
        if (lp != 0)
          lp->blocks = 0;
      }

      numrecordingloops--;
    } else if (status[index] == T_LS_Overdubbing)
      numrecordingloops--;

    // Remove the record/play processor
    Processor *p = plist[index];
    plist[index] = 0;
    app->getRP()->DelChild(p);
    status[index] = T_LS_Off;
    waitactivate[index] = 0;
    waitactivate_shot[index] = 0;
    waitactivate_vol[index] = 0.;
    waitactivate_od[index] = 0;
    waitactivate_od_fb[index] = 0;
  }
    
  if (lp != 0) {
    if (lp->blocks != 0) {
      // Notify any blockmanagers working on this loop's audio to end!
      app->getBMG()->RefDeleted(lp->blocks);
      lp->blocks->DeleteChain(); // *** Not RT Safe
    }

    lp->RTDelete();
    numloops--;
    
    // Update looplists/scenes to ensure that loop is removed from them
    UpdateLoopLists_ItemRemoved(index);
  }

  UnlockLoops();
}

void LoopManager::UpdateLoopLists_ItemAdded (int l_idx) {
  // Update...

  // Snapshots
  Snapshot *snaps = app->getSNAPS();
  for (int i = 0; i < app->getCFG()->GetMaxSnapshots(); i++) {
    if (snaps[i].exists) {
      Snapshot *s = &snaps[i];
      char go = 1;
      for (int j = 0; go && j < s->numls; j++) 
        if (s->ls[j].l_idx == l_idx)
          go = 0;

      if (go) {
        // Loop index not present in snapshot- add, defaulting to loop off
        LoopSnapshot *newls = 0;
        newls = new LoopSnapshot[s->numls+1];

        memcpy(newls,s->ls,sizeof(LoopSnapshot) * s->numls);
        LoopSnapshot *n = &newls[s->numls];

        n->l_idx = l_idx;
        n->status = T_LS_Off;
        n->l_vol = GetLoopVolume(l_idx);
        n->t_vol = 0.;

        delete[] s->ls;
        s->ls = newls;
        s->numls = s->numls+1;
      }
    }
  }
};

void LoopManager::UpdateLoopLists_ItemRemoved (int l_idx) {
  // Update...

  // Selection sets
  for (int i = 0; i < NUM_LOOP_SELECTION_SETS; i++) {
    LoopList **ll = app->getLOOPSEL(i);
    *ll = LoopList::Remove(*ll,l_idx);
  }

  // Snapshots
  Snapshot *snaps = app->getSNAPS();
  for (int i = 0; i < app->getCFG()->GetMaxSnapshots(); i++) {
    if (snaps[i].exists) {
      Snapshot *s = &snaps[i];
      char go = 1;
      for (int j = 0; go && j < s->numls; j++) 
        if (s->ls[j].l_idx == l_idx) {
          // Remove loop from list
          LoopSnapshot *newls = 0;
          if (s->numls > 1) {
            newls = new LoopSnapshot[s->numls-1];

            // All elements preceding j
            memcpy(newls,s->ls,sizeof(LoopSnapshot) * j);
            // & following
            memcpy(&newls[j],&(s->ls[j+1]),
                   sizeof(LoopSnapshot) * (s->numls-j-1));
          }

          delete[] s->ls;
          s->ls = newls;
          s->numls = s->numls-1;
          go = 0; // No more checking in this snapshot
        }
    }
  }
};

void LoopManager::UpdateLoopLists_ItemMoved (int l_idx_old, int l_idx_new) {
  // Update...

  // Selection sets
  for (int i = 0; i < NUM_LOOP_SELECTION_SETS; i++) {
    LoopList **ll = app->getLOOPSEL(i),
      *prev,
      *found = LoopList::Scan(*ll,l_idx_old,&prev);
    
    // Update index
    if (found != 0)
      found->l_idx = l_idx_new;
  }

  // Snapshots
  Snapshot *snaps = app->getSNAPS();
  for (int i = 0; i < app->getCFG()->GetMaxSnapshots(); i++) {
    if (snaps[i].exists) {
      Snapshot *s = &snaps[i];
      for (int j = 0; j < s->numls; j++) 
        if (s->ls[j].l_idx == l_idx_old)
          s->ls[j].l_idx = l_idx_new;
    }
  }
};

int LoopManager::GetLongCountForAllPlayingLoops(Pulse *&p) {
  int lc = 1;
  p = 0;
  for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) {
    if (app->getLOOPMGR()->GetStatus(i) == T_LS_Playing) {
      Loop *l = app->getTMAP()->GetMap(i);
      // printf("Playing loop %d - nbeats %d - pulse %p\n",i,l->nbeats,l->pulse);
      if (l != 0)
        if (l->pulse != 0 && l->nbeats > 0) {
          if (p == 0) {
            p = l->pulse;
            lc = math_lcm(lc,(int) l->nbeats);
            // printf("First LCM: %d\n",lc);
          } else if (l->pulse == p) {
            lc = math_lcm(lc,(int) l->nbeats);
            // printf("LCM: %d\n",lc);
          } else
            printf("CORE: More than one pulse, can't compute long count for all playing loops.\n");
        }
    }
  }

  return lc;
}

// Trigger the loop at index within the map
// The exact behavior varies depending on what is already happening with
// this loop and the settings passed- see configuration documentation
// *** Not RT Safe
void LoopManager::Activate (int index, char shot, float vol, nframes_t ofs, 
                            char overdub, float *od_feedback) {
  // printf("ACTIVATE plist %p status %d\n",plist[index],status[index]);

  if (plist[index] != 0) {
    // We have a problem, we already have a processor on this index.
    // Queue the requested activate
    waitactivate[index] = 1;
    waitactivate_shot[index] = shot;
    waitactivate_vol[index] = vol;
    waitactivate_od[index] = overdub;
    waitactivate_od_fb[index] = od_feedback;
    return;
  }
  
  Loop *lp = app->getTMAP()->GetMap(index);
  if (lp == 0) {
    // Record a new loop
    float *inputvol = app->getRP()->GetInputVolumePtr(); // Where to get input vol from
    app->getRP()->AddChild(plist[index] =
                           new RecordProcessor(app,app->getISET(),inputvol,
                                               GetCurPulse(),
                                               app->getAUDIOMEM(),
                                               app->getAUDIOMEMI(),
                                               app->getCFG()->
                                               loop_peaksavgs_chunksize));
    
    numrecordingloops++;
    status[index] = T_LS_Recording;
    // Keep track of this index in our record of last recorded indexes
    for (int i = LAST_REC_COUNT-1; i > 0; i--)
      lastrecidx[i] = lastrecidx[i-1];
    lastrecidx[0] = index;
  } else {
    // A loop exists at that index
    
    if (lp->pulse != 0)
      lp->pulse->ExtendLongCount(lp->nbeats,1);

    if (overdub) {
      // Overdub
      float *inputvol = app->getRP()->GetInputVolumePtr(); // Get input vol from main
      app->getRP()->AddChild(plist[index] = 
                             new RecordProcessor(app,
                                                 app->getISET(),inputvol,
                                                 lp,vol,ofs,od_feedback));
      numrecordingloops++;
      status[index] = T_LS_Overdubbing;
    } else {
      // Play
      app->getRP()->AddChild(plist[index] = 
                             new PlayProcessor(app,lp,vol,ofs));
      status[index] = T_LS_Playing;
    }
          
    // **DEBUG** Show long count based on all playing loops
    /*
    int lcm = 1;
    printf("***\n");
    for (int i = 0; i < app->getTMAP()->GetMapSize(); i++) {
      if (status[i] == T_LS_Playing) {
        Loop *l = app->getTMAP()->GetMap(i);
        lcm = math_lcm(lcm,(int) l->nbeats);
        printf("Loop[%d] len: %d lcm: %d\n",i,(int) l->nbeats,lcm);
      }
    }*/
  }
}

// *** Not RT Safe
void LoopManager::Deactivate (int index) {
  if (plist[index] == 0) {
    // We have a problem, there is supposed to be a processor here!
    printf("Nothing happening on index %d to deactivate\n",index);
    return;
  }
  
  // If we recorded something new to this index, store it in the map
  if (status[index] == T_LS_Recording && 
      app->getTMAP()->GetMap(index) == 0) {
    // *** Perhaps make a function in RecordProcessor called
    // ** 'createloop'.. which does the encapsulation from the blocks
    Pulse *curpulse = 0;
    if (curpulseindex != -1)
      curpulse = pulses[curpulseindex];

    // Adjust newloop volume so that it will match the volume
    // it was heard as-- since output volume does not scale
    // the initial monitor but does scale loops, we need to adjust
    float adjustednewloopvol = newloopvol / GetOutputVolume(); 

    //printf("newlp from plist: %p\n",plist[index]);
    long nbeats = ((RecordProcessor *) plist[index])->GetNBeats();
    if (curpulse != 0) {
      if (curpulse->GetPct() >= 0.5) {
        nbeats++; // One more beat, since record will wait til next beat
        curpulse->ExtendLongCount(nbeats,1);
      } else {
        if (nbeats == 0)
          nbeats++; // Never set to zero beats, even if the loop is shorter than 1 pulse
          
        // End record after downbeat- don't justify to end of phrase
        curpulse->ExtendLongCount(nbeats,0);
      }
    }
    
    // Create a loop out of the recorded blocks
    Loop *newlp = Loop::GetNewLoop();
    newlp->InitLoop(((RecordProcessor *) plist[index])->GetFirstRecordedBlock(),
           curpulse,1.0,adjustednewloopvol,nbeats,
           app->getCFG()->GetLoopOutFormat());
    app->getTMAP()->SetMap(index, newlp);
    UpdateLoopLists_ItemAdded(index);
    numloops++;
    lastindex = index;

    // Record processor will broadcast when it is ready to end!
    ((RecordProcessor *) plist[index])->End();    
  } else if (status[index] == T_LS_Overdubbing) {
    // Overdubbing record processor will end immediately and broadcast 
    // EndRecord event
    ((RecordProcessor *) plist[index])->End();
  } else if (status[index] == T_LS_Playing) {
    // Stop playing/overdubbing
    Processor *p = plist[index];
    plist[index] = 0;
    app->getRP()->DelChild(p);
    status[index] = T_LS_Off;    
  }
}

void LoopManager::ReceiveEvent(Event *ev, EventProducer *from) {
  switch (ev->GetType()) {
  case T_EV_EndRecord :
    // Recording has ended on one of the RecordProcessors- find it!
    for (int i = 0; i < app->getTMAP()->GetMapSize(); i++) 
      if (plist[i] == from) {
        // Should we keep this recording
        if (!((EndRecordEvent *) ev)->keeprecord) {
          DeleteLoop(i); // No
        }
        else {
          nframes_t playofs = 0;
          if (status[i] == T_LS_Recording) {
            // Adjust number of beats in loop based on the recording
            // (this is now done immediately based on sync position)
            /* app->getTMAP()->GetMap(i)->nbeats = 
              ((RecordProcessor *) plist[i])->GetNBeats(); */

            Pulse *recsync = ((RecordProcessor *) plist[i])->GetPulse();
            // Sync recording may have ended late, so start play where we
            // left off
            if (recsync != 0)
              playofs = recsync->GetPos();
          } else if (status[i] == T_LS_Overdubbing) {
            // Start play at position where overdub left off
            playofs = ((RecordProcessor *) plist[i])->GetRecordedLength();
          }

          // Remove recordprocessor from chain
          Processor *p = plist[i];
          plist[i] = 0;
          app->getRP()->DelChild(p);
          numrecordingloops--;
          status[i] = T_LS_Off;

          // Check if we need to activate a playprocessor
          if (waitactivate[i]) {
            waitactivate[i] = 0;
            // Activate is not RT safe (new processor alloc)
            // So this event thread had better be nonRT!
            Activate(i,waitactivate_shot[i],waitactivate_vol[i],
                     playofs,
                     waitactivate_od[i],waitactivate_od_fb[i]);
          }
        }
      }
    break;

  case T_EV_ToggleDiskOutput :
    {
      // OK!
      if (CRITTERS)
        printf("CORE: Received ToggleDiskOutputEvent\n");
      app->ToggleDiskOutput();
    }
    break;

  case T_EV_ToggleSelectLoop :
    {
      ToggleSelectLoopEvent *sev = (ToggleSelectLoopEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received ToggleSelectLoopEvent: Set %d Loop ID %d\n",
               sev->setid,sev->loopid);
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        // Get loop with id
        Loop *l = app->getTMAP()->GetMap(sev->loopid);
        if (l != 0) {
          LoopList *prev;
          LoopList *exists = LoopList::Scan(*ll,sev->loopid,&prev);
          if (exists != 0) {
            // printf("REMOVE!\n");
            *ll = LoopList::Remove(*ll,exists,prev);
            l->ChangeSelectedCount(-1);
          } else {
            // printf("ADD!\n");
            *ll = LoopList::AddBegin(*ll,sev->loopid);
            l->ChangeSelectedCount(1);
          }
        }
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_SelectOnlyPlayingLoops :
    {
      SelectOnlyPlayingLoopsEvent *sev = (SelectOnlyPlayingLoopsEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SelectOnlyPlayingLoopsEvent: Set %d [%s]\n",
               sev->setid,(sev->playing ? "PLAYING" : "IDLE"));
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        // Scan all loops for playing loops
        for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) {
          Loop *l = app->getTMAP()->GetMap(i);
          if (GetStatus(i) == T_LS_Overdubbing ||
              GetStatus(i) == T_LS_Playing) {
            if (l != 0) {
              // Loop exists, and it's playing!
              LoopList *prev;
              LoopList *exists = LoopList::Scan(*ll,i,&prev);
              if (!sev->playing && exists != 0) {
                // printf("REMOVE!\n");
                *ll = LoopList::Remove(*ll,exists,prev);
                l->ChangeSelectedCount(-1);
              } else if (sev->playing && exists == 0) {
                // printf("ADD!\n");
                *ll = LoopList::AddBegin(*ll,i);
                l->ChangeSelectedCount(1);
              }
            }
          } else if (app->getTMAP()->GetMap(i) != 0) {
            // Loop exists, but not playing/overdubbing
            LoopList *prev;
            LoopList *exists = LoopList::Scan(*ll,i,&prev);
            if (sev->playing && exists != 0) {
              // printf("REMOVE!\n");
              *ll = LoopList::Remove(*ll,exists,prev);
              l->ChangeSelectedCount(-1);
            } else if (!sev->playing && exists == 0) {
              // printf("ADD!\n");
              *ll = LoopList::AddBegin(*ll,i);
              l->ChangeSelectedCount(1);
            }
          }
        }
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_SelectAllLoops :
    {
      SelectAllLoopsEvent *sev = (SelectAllLoopsEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SelectAllLoopsEvent: Set %d [%s]\n",
               sev->setid,(sev->select ? "SELECT" : "UNSELECT"));
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        // Scan all loops
        for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) {
          Loop *l = app->getTMAP()->GetMap(i);
          if (l != 0) {
            // Loop exists in map
            LoopList *prev;
            LoopList *exists = LoopList::Scan(*ll,i,&prev);
            if (!sev->select && exists != 0) {
              // Unselect loops- remove loop from list
              // printf("REMOVE!\n");
              *ll = LoopList::Remove(*ll,exists,prev);
              l->ChangeSelectedCount(-1);
            } else if (sev->select && exists == 0) {
              // Select loops- add loop to list
              // printf("ADD!\n");
              *ll = LoopList::AddBegin(*ll,i);
              l->ChangeSelectedCount(1);
            }
          }
        }
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_InvertSelection :
    {
      InvertSelectionEvent *sev = (InvertSelectionEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received InvertSelectionEvent: Set %d\n",sev->setid);

      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        // Scan all loops
        for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) {
          Loop *l = app->getTMAP()->GetMap(i);
          if (l != 0) {
            // Loop exists in map
            LoopList *prev;
            LoopList *exists = LoopList::Scan(*ll,i,&prev);
            if (exists != 0) {
              // Loop exists in list- remove
              // printf("REMOVE!\n");
              *ll = LoopList::Remove(*ll,exists,prev);
              l->ChangeSelectedCount(-1);
            } else if (exists == 0) {
              // Loop not in list- add
              // printf("ADD!\n");
              *ll = LoopList::AddBegin(*ll,i);
              l->ChangeSelectedCount(1);
            }
          }
        }
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_TriggerSnapshot :
    {
      TriggerSnapshotEvent *sev = (TriggerSnapshotEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received TriggerSnapshotEvent: Snapshot #%d\n",
               sev->snapid);

      if (app->TriggerSnapshot(sev->snapid))
        printf("CORE: Invalid snapshot #%d- can't trigger\n",sev->snapid);
    }
    break;

  case T_EV_CreateSnapshot :
    {
      CreateSnapshotEvent *sev = (CreateSnapshotEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received CreateSnapshotEvent: Snapshot #%d\n",
               sev->snapid);

      if (app->CreateSnapshot(sev->snapid) == 0)
        printf("CORE: Invalid snapshot #%d- can't create\n",sev->snapid);
    }
    break;

  case T_EV_SwapSnapshots :
    {
      SwapSnapshotsEvent *sev = (SwapSnapshotsEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received SwapSnapshotsEvent: Snapshot #%d <> #%d\n",
               sev->snapid1,sev->snapid2);

      if (app->SwapSnapshots(sev->snapid1,sev->snapid2) != 0)
        printf("CORE: Invalid snapshot- can't swap\n");
    }
    break;

  case T_EV_RenameSnapshot :
    {
      RenameSnapshotEvent *sev = (RenameSnapshotEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received RenameSnapshotEvent: Snapshot #%d\n",
               sev->snapid);

      FloDisplaySnapshots *sdisp = (FloDisplaySnapshots *) app->getCFG()->GetDisplayByType(FD_Snapshots);
      if (sdisp != 0)
        sdisp->Rename(sev->snapid);
    }
    break;

  case T_EV_SetSelectedLoopsTriggerVolume :
    {
      SetSelectedLoopsTriggerVolumeEvent *sev = (SetSelectedLoopsTriggerVolumeEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received SetSelectedLoopsTriggerVolumeEvent: Set %d: Volume %f\n",
               sev->setid,sev->vol);
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        LoopList *cur = *ll;
        while (cur != 0) {
          SetTriggerVol(cur->l_idx,sev->vol);
          cur = cur->next;
        }
        
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_AdjustSelectedLoopsAmp :
    {
      AdjustSelectedLoopsAmpEvent *sev = (AdjustSelectedLoopsAmpEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received AdjustSelectedLoopsAmpEvent: Set %d: "
               "Amp factor %f\n",
               sev->setid,sev->ampfactor);
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        LoopList *cur = *ll;
        while (cur != 0) {
          SetLoopVolume(cur->l_idx,
                        sev->ampfactor * GetLoopVolume(cur->l_idx));
          cur = cur->next;
        }
        
      } else 
        printf("CORE: Invalid set id #%d when selecting loop\n",sev->setid);
    }
    break;

  case T_EV_EraseSelectedLoops :
    {
      EraseSelectedLoopsEvent *sev = (EraseSelectedLoopsEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received EraseSelectedLoopsEvent: Set: %d\n",sev->setid);
      
      LoopList **ll = app->getLOOPSEL(sev->setid);
      if (ll != 0) {
        LoopList *cur = *ll;
        while (cur != 0) {
          DeleteLoop(cur->l_idx);
          cur = cur->next;
        }       
      } else 
        printf("CORE: Invalid set id #%d when erasing selected loops\n",
               sev->setid);
    }
    break;
    
  case T_EV_SetAutoLoopSaving :
    {
      SetAutoLoopSavingEvent *sev = (SetAutoLoopSavingEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetAutoLoopSavingEvent (%s)\n",
               (sev->save ? "on" : "off"));
      SetAutoLoopSaving(sev->save);
    }
    break;

  case T_EV_SaveLoop :
    {
      SaveLoopEvent *sev = (SaveLoopEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SaveLoopEvent (%d)\n",sev->index);
      SaveLoop(sev->index);
    }
    break;

  case T_EV_SaveNewScene :
    {
      // OK!
      if (CRITTERS)
        printf("CORE: Received SaveNewSceneEvent\n");
      SaveNewScene();
    }
    break;

  case T_EV_SaveCurrentScene :
    {
      // OK!
      if (CRITTERS)
        printf("CORE: Received SaveCurrentSceneEvent\n");
      SaveCurScene();
    }
    break;
    
  case T_EV_SetLoadLoopId :
    {
      SetLoadLoopIdEvent *sev = (SetLoadLoopIdEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetLoadLoopIdEvent (%d)\n",sev->index);
      loadloopid = sev->index;
    }
    break;

  case T_EV_SetDefaultLoopPlacement :
    {
      SetDefaultLoopPlacementEvent *sev = (SetDefaultLoopPlacementEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetDefaultLoopPlacementEvent (%d>%d)\n",
               sev->looprange.lo,sev->looprange.hi);
      default_looprange = sev->looprange;
    }
    break;

  case T_EV_SlideMasterInVolume :
    {
      SlideMasterInVolumeEvent *vev = (SlideMasterInVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SlideMasterInVolumeEvent(%f)\n", vev->slide);
      AdjustInputVolume(vev->slide);
    }
    break;

  case T_EV_SlideMasterOutVolume :
    {
      SlideMasterOutVolumeEvent *vev = (SlideMasterOutVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SlideMasterOutVolumeEvent(%f)\n", vev->slide);
      AdjustOutputVolume(vev->slide);
    }
    break;

  case T_EV_SlideInVolume :
    {
      SlideInVolumeEvent *vev = (SlideInVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SlideInVolumeEvent(%d: %f)\n", vev->input, vev->slide);
      app->getISET()->AdjustInputVol(vev->input-1, vev->slide);
    }
    break;

  case T_EV_SetMasterInVolume :
    {
      SetMasterInVolumeEvent *vev = (SetMasterInVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetMasterInVolumeEvent(%f, %f)\n", vev->vol, vev->fadervol);
      SetInputVolume(vev->vol,vev->fadervol);
    }
    break;

  case T_EV_SetMasterOutVolume :
    {
      SetMasterOutVolumeEvent *vev = (SetMasterOutVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetMasterOutVolumeEvent(%f, %f)\n", vev->vol, vev->fadervol);
      SetOutputVolume(vev->vol,vev->fadervol);
    }
    break;

  case T_EV_SetInVolume :
    {
      SetInVolumeEvent *vev = (SetInVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetInVolumeEvent(%d: %f, %f)\n", vev->input, vev->vol, vev->fadervol);
      app->getISET()->SetInputVol(vev->input-1, vev->vol, vev->fadervol);
    }
    break;

  case T_EV_ToggleInputRecord :
    {
      ToggleInputRecordEvent *vev = (ToggleInputRecordEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received ToggleInputRecordEvent(%d)\n", vev->input);
      app->getISET()->SelectInput(vev->input-1,(app->getISET()->InputSelected(vev->input-1) == 0 ? 1 : 0));
    }
    break;

  case T_EV_DeletePulse :
    {
      DeletePulseEvent *dev = (DeletePulseEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received DeletePulse(%d)\n", dev->pulse);
      DeletePulse(dev->pulse);
    }
    break;

  case T_EV_SelectPulse :
    {
      SelectPulseEvent *sev = (SelectPulseEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SelectPulse(%d)\n", sev->pulse);
      SelectPulse(sev->pulse);
    }
    break;

  case T_EV_TapPulse :
    {
      TapPulseEvent *tev = (TapPulseEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received TapPulse(%d) %s\n", tev->pulse,
               (tev->newlen ? "[new length]" : ""));
      TapPulse(tev->pulse,tev->newlen);
    }
    break;

  case T_EV_SwitchMetronome :
    {
      SwitchMetronomeEvent *swev = (SwitchMetronomeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SwitchMetronome(%d) %s\n", swev->pulse,
               (swev->metronome ? "[on]" : "[off]"));
      SwitchMetronome(swev->pulse,swev->metronome);
    }
    break;

  case T_EV_SetSyncType :
    {
      SetSyncTypeEvent *sev = (SetSyncTypeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetSyncType(%d)\n", sev->stype);
      app->SetSyncType(sev->stype);
    }
    break;

  case T_EV_SetSyncSpeed :
    {
      SetSyncSpeedEvent *sev = (SetSyncSpeedEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetSyncSpeed(%d)\n", sev->sspd);
      app->SetSyncSpeed(sev->sspd);
    }
    break;

  case T_EV_SetMidiSync :
    {
      SetMidiSyncEvent *sev = (SetMidiSyncEvent *) ev;
      
      // OK!
      if (CRITTERS)
        printf("CORE: Received SetMidiSync(%d)\n", sev->midisync);
      app->getMIDI()->SetMIDISyncTransmit(sev->midisync);
      app->RefreshPulseSync();
    }
    break;
    
  case T_EV_SetTriggerVolume :
    {
      SetTriggerVolumeEvent *laev = (SetTriggerVolumeEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetTriggerVolume(%d,%f)\n", laev->index, 
               laev->vol);
      SetTriggerVol(laev->index,laev->vol);
    }
    break;

  case T_EV_SlideLoopAmp :
    {
      SlideLoopAmpEvent *laev = (SlideLoopAmpEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SlideLoopAmp(%d,%f)\n", laev->index, 
               laev->slide);
      AdjustLoopVolume(laev->index,laev->slide);
    }
    break;

  case T_EV_SetLoopAmp :
    {
      SetLoopAmpEvent *laev = (SetLoopAmpEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received SetLoopAmp(%d,%f)\n", laev->index, laev->amp);
      SetLoopVolume(laev->index,laev->amp);
    }
    break;

  case T_EV_AdjustLoopAmp :
    {
      AdjustLoopAmpEvent *laev = (AdjustLoopAmpEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received AdjustLoopAmp(%d,%f)\n", laev->index, 
               laev->ampfactor);
      SetLoopVolume(laev->index,
                    laev->ampfactor * GetLoopVolume(laev->index));
    }
    break;

  case T_EV_TriggerLoop :
    {
      TriggerLoopEvent *tev = (TriggerLoopEvent *) ev;
      int index = tev->index,
        engage = tev->engage;
      float vol = tev->vol;
      char od = tev->od,
        shot = tev->shot;
      UserVariable *od_fb = tev->od_fb;
      float *od_fb_ptr = 0;
      if (od_fb != 0) {
        if (od_fb->GetType() == T_float)
          od_fb_ptr = (float *) od_fb->GetValue();
        else 
          printf("CORE: ERROR: Overdub feedback assigned to variable '%s'- but that variable is not a 'float'!\n",od_fb->GetName());
      }
      
      // OK!
      if (CRITTERS) {
        printf("CORE: Received TriggerLoop(%d,%.2f)", index, vol);
        if (od) {
          printf(" [overdub]");
          if (od_fb != 0) {
            printf(" (feedback ");
            od_fb->Print();
            printf(")\n");
          } else
            printf("\n");
        }
        if (shot)
          printf(" (shot)");
        if (engage != -1)
          printf(" (%s)\n",(engage ? "force on" : "force off"));
        else
          printf("\n");
      }

      if ((engage == -1 || engage == 1) &&
          (GetStatus(index) == T_LS_Recording || 
           GetStatus(index) == T_LS_Overdubbing ||
           (GetStatus(index) == T_LS_Playing && od == 1))) {
        // Stop-start case
        nframes_t ofs = 0;
        if (GetStatus(index) == T_LS_Overdubbing && od == 1) {
          // Don't allow retrigger from overdub to overdub-- override to play
          od = 0;
        } else if (GetStatus(index) == T_LS_Playing) {
          // Play->overdub case- start overdub where play left off
          ofs = ((PlayProcessor *) plist[index])->GetPlayedLength();
        }

        Deactivate(index);                // Stop
        Activate(index,shot,vol,ofs,od,od_fb_ptr); // Start
      } else if ((engage == -1 || engage == 0) && IsActive(index)) {
        // Stop case (play and no overdub)
        Deactivate(index);
      }
      else if (engage == -1 || engage == 1) {
        // Start case (record)
        Activate(index,shot,vol,0,od,od_fb_ptr);
      }
    }
    break;

  case T_EV_TriggerSelectedLoops :
    {
      TriggerSelectedLoopsEvent *tev = (TriggerSelectedLoopsEvent *) ev;

      if (CRITTERS)
        printf("CORE: Received TriggerSelectedLoops(set #%d,%.2f)\n", 
               tev->setid,tev->vol);

      LoopList **ll = app->getLOOPSEL(tev->setid);
      if (ll != 0) {
        // Get all loops from this set
        LoopList *cur = *ll;
        while (cur != 0) {
          if (IsActive(cur->l_idx)) {
            // Overdub/play on this loop
            if (tev->toggleloops)
              Deactivate(cur->l_idx);
          } else {
            // Loop idle-- start play
            // No overdub/shot/etc, just straight play
            Activate(cur->l_idx,0,tev->vol,0,0,0);
          }

          cur = cur->next;
        }
      } else 
        printf("CORE: Invalid set id #%d when triggering selected loops\n",
               tev->setid);      
    }
    break;

  case T_EV_MoveLoop :
    {
      MoveLoopEvent *mev = (MoveLoopEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received MoveLoop(%d->%d)\n", mev->oldloopid, 
               mev->newloopid);
      MoveLoop(mev->oldloopid,mev->newloopid);
    }
    break;

  case T_EV_RenameLoop :
    {
      RenameLoopEvent *rev = (RenameLoopEvent *) ev;

      if (rev->in == 1) {
        RenameLoop(rev->loopid);
        if (CRITTERS)
          printf("LOOPMGR: Received RenameLoop(loopid: %d)\n",rev->loopid);
      }
    }
    break;

  case T_EV_EraseLoop :
    {
      EraseLoopEvent *eev = (EraseLoopEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received EraseLoop(%d)\n", eev->index);
      DeleteLoop(eev->index);
    }
    break;

  case T_EV_EraseAllLoops :
    {
      // OK!
      if (CRITTERS)
        printf("CORE: Received EraseAllLoops\n");

      // Erase scene settings
      app->setCURSCENE(0);

      // Erase all loops!
      // printf("DEBUG: ERASE LOOPS!\n");
      for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) 
        DeleteLoop(i);
      // And all pulses!
      // printf("DEBUG: ERASE PULSES!\n");
      for (int i = 0; i < MAX_PULSES; i++)
        DeletePulse(i);
      // printf("DEBUG: DONE!\n\n");
      // And all snapshots!
      Snapshot *s = app->getSNAPS();
      for (int i = 0; i < app->getCFG()->GetMaxSnapshots(); i++)
        s[i].DeleteSnapshot();
    }
    break;

  case T_EV_SlideLoopAmpStopAll :
    {
      // OK!
      if (CRITTERS)
        printf("CORE: Received SlideLoopAmpStopAll\n");

      for (int i = 0; i < app->getCFG()->GetNumTriggers(); i++) 
        SetLoopdVolume(i,1.0);
    }
    break;

  case T_EV_ALSAMixerControlSet :
    {
      ALSAMixerControlSetEvent *aev = (ALSAMixerControlSetEvent *) ev;

      // OK!
      if (CRITTERS)
        printf("CORE: Received AlsaMixerControlSet (hw:%d numid=%d %d,%d,%d,%d)\n",
            aev->hwid,aev->numid,aev->val1,aev->val2,aev->val3,aev->val4);

#ifdef __MACOSX__
      printf("CORE: Not implemented on Mac.\n");
#else
      app->getHMIX()->ALSAMixerControlSet(aev->hwid,aev->numid,
          aev->val1,aev->val2,aev->val3,aev->val4);
#endif
    }
    break;

  default:
    break;
  }
}

int Fweelin::go()
{
  running = 1;

  // Final setup
  // Broadcast start session event!
  Event *proto = Event::GetEventByType(T_EV_StartSession);
  if (proto == 0) {
    printf("GO: Can't get start event prototype!\n");
  } else {
    Event *cpy = (Event *) proto->RTNew();
    if (cpy == 0)
      printf("CORE: WARNING: Can't send event- RTNew() failed\n");
    else 
      emg->BroadcastEventNow(cpy, this);
  }

  // Broadcast events for starting all interfaces!
  cfg->StartInterfaces();

  // *** SDL IO is now done in main thread- Mac OS X SDL requires it, and on Linux it's one less thread
  SDLIO::run_sdl_thread(sdlio.get());

  // Old method
#if 0
  // Now just wait.. the threads will take care of everything 
  while (sdlio->IsActive()) {
    usleep(100000);
  };
#endif
  
  // Cleanup
  if (vid != 0)
    vid->close();
  sdlio->close();
  midi->close();
  audio->close();
  if (vid != 0)
    vid.reset();
  sdlio.reset();
  midi.reset();
  hmix.reset();
  audio.reset();
  iset.reset();
  abufs.reset();
  snaps.clear();
  fs_inputs.clear();

#ifndef __MACOSX__
  osc.reset();
#endif

#if USE_FLUIDSYNTH
  fluidp.reset();
#endif

  browsers.clear();

  printf("MAIN: end stage 1\n");

  //sleep(1);

  // Manually reset audio memory to its original state-
  // not preallocated!
  getAMPEAKS()->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);
  getAMAVGS()->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);
  audiomem->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);

  // And main classes..
  tmap.reset();
  printf(" 1\n");
  loopmgr.reset();
  printf(" 2\n");
  rp.reset();
  printf(" 3\n");
  bmg.reset();
  printf(" 4\n");

  printf("MAIN: end stage 2\n");

  //::delete audiomem;
  scope.reset();

  printf("MAIN: end stage 3\n");

  // Delete preallocated type managers
  pre_audioblock.reset();
  pre_extrachannel.reset();
  pre_timemarker.reset();

  printf("MAIN: end stage 4\n");
  //sleep(2);

  cfg.reset();
  printf(" 1\n");
  //sleep(2);
  emg.reset();
  //sleep(2);
  printf(" 2\n");
  mmg.reset();

  SDL_Quit();
  
  RT_RWThreads::CloseAll();

  printf("MAIN: end\n");

  return 0;
}

BED_MarkerPoints *Fweelin::getAMPEAKSPULSE() { 
  AudioBlock *peaks = getAMPEAKS();
  if (peaks != 0)
    return dynamic_cast<BED_MarkerPoints *>
      (getAMPEAKS()->GetExtendedData(T_BED_MarkerPoints));
  else
    return 0;
};

AudioBlock *Fweelin::getAMPEAKS() { 
  return 
    dynamic_cast<PeaksAvgsManager *>(bmg->GetBlockManager(audiomem,
                                                          T_MC_PeaksAvgs))->
    GetPeaks();
};

AudioBlock *Fweelin::getAMAVGS() { 
  return 
    dynamic_cast<PeaksAvgsManager *>(bmg->GetBlockManager(audiomem,
                                                          T_MC_PeaksAvgs))->
    GetAvgs();
  };

AudioBlockIterator *Fweelin::getAMPEAKSI() { 
  return
    dynamic_cast<PeaksAvgsManager *>(bmg->GetBlockManager(audiomem,
                                                          T_MC_PeaksAvgs))->
    GetPeaksI();
};

AudioBlockIterator *Fweelin::getAMAVGSI() { 
  return
    dynamic_cast<PeaksAvgsManager *>(bmg->GetBlockManager(audiomem,
                                                          T_MC_PeaksAvgs))->
    GetAvgsI();
};

AudioBlockIterator *Fweelin::getAUDIOMEMI() {
  return amrec->GetIterator();
};

Browser *Fweelin::GetBrowserFromConfig(BrowserItemType b) {
  FloDisplay *cur = cfg->displays;
  while (cur != 0) {
    if (cur->GetFloDisplayType() == FD_Browser &&
        ((Browser *) cur)->GetType() == b)
      return (Browser *) cur;
    cur = cur->next;
  }
  return 0;
};

// Returns non-zero if all streamers have status 'status', else zero
char Fweelin::CheckStreamStatus(char status) {
  char check = 1;

  if (fs_finalout != 0)
    check &= fs_finalout->GetStatus() == status;
  if (fs_loopout != 0)
    check &= fs_loopout->GetStatus() == status;
  for (int i = 0; i < iset->GetNumInputs(); i++)
    if (fs_inputs[i] != 0)
      check &= fs_inputs[i]->GetStatus() == status;

  return check;
};

void Fweelin::ToggleDiskOutput()
{
  if (CheckStreamStatus(FileStreamer::STATUS_STOPPED)) {
    // Create appropriate base filename for output
    streamoutname = LibraryHelper::GetNextAvailableStreamOutFilename(this,writenum,streamoutname_display);

    // Now start all streamers
    char write_timing = 1;  // Only write timing file once
    if (fs_finalout != 0) {
      fs_finalout->StartWriting(streamoutname,"-final",write_timing,getCFG()->GetStreamOutFormat());
      write_timing = 0;
    }
    if (fs_loopout != 0) {
      fs_loopout->StartWriting(streamoutname,"-loops",write_timing,getCFG()->GetStreamOutFormat());
      write_timing = 0;
    }
    for (int i = 0; i < iset->GetNumInputs(); i++)
      if (fs_inputs[i] != 0) {
        std::ostringstream tmp;
        tmp << "-input" << i+1;
        fs_inputs[i]->StartWriting(streamoutname,tmp.str().c_str(),write_timing,getCFG()->GetStreamOutFormat());
        write_timing = 0;
      }
  } else {
    // Stop disk output
    if (fs_finalout != 0)
      fs_finalout->StopWriting();
    if (fs_loopout != 0)
      fs_loopout->StopWriting();
    for (int i = 0; i < iset->GetNumInputs(); i++)
      if (fs_inputs[i] != 0)
        fs_inputs[i]->StopWriting();

    streamoutname = "";
    streamoutname_display = "";

    // Advance to next logical filename
    writenum++;
  }
};

long int Fweelin::getSTREAMSIZE(FileStreamer *fs, char &frames) {
  const char *streamname = fs->GetOutputName().c_str();
  struct stat st;
  if (stat(streamname,&st) == 0) {
    frames = 0;
    return st.st_size;
  } else {
    frames = 1;
    return fs->GetOutputSize();
  }
}

float Fweelin::getSTREAMSTATS(const char *&stream_type, int &num_streams) {
  stream_type = cfg->GetAudioFileExt(cfg->GetStreamOutFormat());
  num_streams = 0;
  char frames;
  long int totalsize = 0;

  if (fs_finalout != 0) {
    long int tmp = getSTREAMSIZE(fs_finalout,frames);
    if (!frames)
      totalsize += tmp;
    num_streams++;
  }

  if (fs_loopout != 0) {
    long int tmp = getSTREAMSIZE(fs_loopout,frames);
    if (!frames)
      totalsize += tmp;
    num_streams++;
  }

  for (int i = 0; i < iset->GetNumInputs(); i++)
    if (fs_inputs[i] != 0) {
      long int tmp = getSTREAMSIZE(fs_inputs[i],frames);
      if (!frames)
        totalsize += tmp;
      num_streams++;
    }

  return totalsize / (1024.*1024);
};

long int Fweelin::getSTREAMER_TotalOutputSize(int &numstreams) {
  long int totalsize = 0;
  numstreams = 0;

  if (fs_finalout != 0) {
    totalsize += fs_finalout->GetOutputSize();
    numstreams++;
  }

  if (fs_loopout != 0) {
    totalsize += fs_loopout->GetOutputSize();
    numstreams++;
  }

  for (int i = 0; i < iset->GetNumInputs(); i++)
    if (fs_inputs[i] != 0) {
      totalsize += fs_inputs[i]->GetOutputSize();
      numstreams++;
    }

  return totalsize;
};

int Fweelin::setup()
{
  char tmp[255];
  FweelinStartupGuard guard;

  // Keep all memory inline
  mlockall(MCL_CURRENT | MCL_FUTURE);

  // Init and Register main thread as a writer
  RT_RWThreads::InitAll();
  RT_RWThreads::RegisterReaderOrWriter();
  rt_threads_ready = 1;
  guard.Push(RollbackSetupCallback, this, 0);
  
  // Initialize vars
  for (int i = 0; i < NUM_LOOP_SELECTION_SETS; i++)
    loopsel[i] = 0;

#ifndef __MACOSX__
  if (!XInitThreads()) {
    printf("MAIN: ERROR: FreeWheeling requires threaded Xlib support\n");
    return 0;
  }
#else  
  FweelinMac::LinkFweelin(this);
#endif
  
  /* Initialize SDL- this happens here because it is common to video, keys &
     config */ 
  // SDL_INIT_NOPARACHUTE
  /* (SDL_INIT_JOYSTICK | SDL_INIT_EVENTTHREAD) < 0) { */
  if ( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0 ) {
    printf("MAIN: ERROR: Can't initialize SDL: %s\n",SDL_GetError());
    guard.Rollback();
    return 0;
  }
  sdl_ready = 1;
  atexit(SDL_Quit);

  // Memory manager
  mmg.reset(new MemoryManager());

  // Load configuration from .rc file
  cfg.reset(new FloConfig(this));

  // Create system variables so that config will have them first!
  AddIntConst(cfg.get(), "BROWSE_loop", (int) B_Loop);
  AddIntConst(cfg.get(), "BROWSE_scene", (int) B_Scene);
  AddIntConst(cfg.get(), "BROWSE_loop_tray", (int) B_Loop_Tray);
  AddIntConst(cfg.get(), "BROWSE_scene_tray", (int) B_Scene_Tray);
  AddIntConst(cfg.get(), "BROWSE_patch", (int) B_Patch);
  {
    const char *const system_vars[] = {
      "SYSTEM_midi_transpose",
      "SYSTEM_master_in_volume",
      "SYSTEM_master_out_volume",
      "SYSTEM_cur_pitchbend",
      "SYSTEM_bender_tune",
      "SYSTEM_cur_limiter_gain",
      "SYSTEM_audio_cpu_load",
      "SYSTEM_sync_active",
      "SYSTEM_sync_transmit",
      "SYSTEM_midisync_transmit",
      "SYSTEM_fluidsynth_enabled",
      "SYSTEM_num_midi_outs",
      "SYSTEM_num_help_pages",
      "SYSTEM_num_loops_in_map",
      "SYSTEM_num_recording_loops_in_map",
      "SYSTEM_num_patchbanks",
      "SYSTEM_cur_patchbank_tag",
      "SYSTEM_num_switchable_interfaces",
      "SYSTEM_cur_switchable_interface",
      "SYSTEM_snapshot_page_firstidx"
    };
    AddEmptyVariables(cfg.get(), system_vars, sizeof(system_vars) / sizeof(system_vars[0]));
  }
  for (int i = 0; i < 4; i++) {
    char tmp2[255];
    snprintf(tmp2, 255, "SYSTEM_in_%d_volume", i + 1);
    cfg->AddEmptyVariable(tmp2);
    snprintf(tmp2, 255, "SYSTEM_in_%d_peak", i + 1);
    cfg->AddEmptyVariable(tmp2);
    snprintf(tmp2, 255, "SYSTEM_in_%d_record", i + 1);
    cfg->AddEmptyVariable(tmp2);
  }
  for (int i = 0; i < LAST_REC_COUNT; i++) {
    snprintf(tmp,sizeof(tmp),"SYSTEM_loopid_lastrecord_%d",i);
    cfg->AddEmptyVariable(tmp);
  }

  // Now parse and setup config
  cfg->Parse();

  // Event manager
  emg.reset(new EventManager());

  vid.reset(new VideoIO(this));
  if (vid->activate()) {
    printf("MAIN: ERROR: Can't start video handler!\n");
    guard.Rollback();
    return 1;
  }
  while (!vid->IsActive())
    usleep(100000);

  abufs.reset(new AudioBuffers(this));
  iset.reset(new InputSettings(this,abufs->numins));
  audio.reset(new AudioIO(this));
  if (audio->open()) {
    printf("MAIN: ERROR: Can't start system level audio!\n");
    guard.Rollback();
    return 1;
  }  
  fragmentsize = audio->getbufsz();
  printf("MAIN: Core block size: %d\n",fragmentsize);

  // Linkup to browsers
  browsers.assign((int) B_Last, (Browser *) 0);

  // Setup patch browser, if defined in config
  {
    Browser *br = GetBrowserFromConfig(B_Patch);
    if (br != 0) {
      browsers[B_Patch] = br;
      br->Setup(this,this); // We handle callbacks ourselves for patch browser
    }
  }

  // Setup parameter sets to listen for events
  {
    FloDisplay *cur = cfg->displays;
    while (cur != 0) {
      if (cur->GetFloDisplayType() == FD_ParamSet)
        ((FloDisplayParamSet *) cur)->ListenEvents();
      cur = cur->next;
    }
  }

  // FluidSynth
#if USE_FLUIDSYNTH
  // Create synth
  printf("MAIN: Creating integrated FluidSynth.\n");
  fluidp.reset(new FluidSynthProcessor(this,cfg->GetFluidStereo()));
  
  // Setup patch names
  fluidp->SetupPatches();
#endif

  // Setup sample buffer for visual scope
  scope.reset(new sample_t[fragmentsize]);
  scope_len = fragmentsize;

  // Block manager
  bmg.reset(new BlockManager(this));

  // Preallocated type managers
  pre_audioblock.reset(new PreallocatedType(mmg.get(), ::new AudioBlock(),
                                            sizeof(AudioBlock),
                                            FloConfig::
                                            NUM_PREALLOCATED_AUDIO_BLOCKS));

  if (cfg->IsStereoMaster()) 
    // Only preallocate for stereo blocks if we are running in stereo
    pre_extrachannel.reset(new PreallocatedType(mmg.get(), ::new BED_ExtraChannel(),
                                                sizeof(BED_ExtraChannel),
                                                FloConfig::
                                                NUM_PREALLOCATED_AUDIO_BLOCKS));
  else 
    pre_extrachannel.reset();
  pre_timemarker.reset(new PreallocatedType(mmg.get(),::new TimeMarker(),
                                            sizeof(TimeMarker),
                                            FloConfig::
                                            NUM_PREALLOCATED_TIME_MARKERS));

  rp.reset(new RootProcessor(this,iset.get()));
  writenum = 1;
  streamoutname = "";
  streamoutname_display = "";
  curscene = 0;
#if 0
  scenedispname[0] = '\0';
  scenefilename[0] = '\0';
#endif

  // Fixed audio memory
  nframes_t memlen = (nframes_t) (audio->get_srate() * 
                                  cfg->AUDIO_MEMORY_LEN),
    scopelen = cfg->GetScopeSampleLen(),
    chunksize = memlen/scopelen;
  // Note here we bypass using Preallocated RTNew because we want
  // a single block of our own size, not many preallocated blocks
  // chained together..
  audiomem = ::new AudioBlock(memlen);
  if (audiomem == 0) {
    printf("CORE: ERROR: Can't create audio memory!\n");
    guard.Rollback();
    return 1;
  }
  audiomem->Zero();
  // If we are running in stereo, create a custom right channel to match
  // our left channel audio memory
  if (cfg->IsStereoMaster()) {
    BED_ExtraChannel *audiomem_r = ::new BED_ExtraChannel(memlen);
    if (audiomem_r == 0) {
      printf("CORE: ERROR: Can't create audio memory (right channel)!\n");
      guard.Rollback();
      return 1;
    }
    
    audiomem->AddExtendedData(audiomem_r);
  }

  // So we have to set a pointer manually to the manager..
  // Because some functions depend on using audiomem as a basis
  // to access RTNew
  audiomem->SetupPreallocated(pre_audioblock.get(),
                              Preallocated::PREALLOC_BASE_INSTANCE);

  // Compute running peaks and averages from audio mem (for scope)
  AudioBlock *peaks = ::new AudioBlock(scopelen),
    *avgs = ::new AudioBlock(scopelen);
  if (peaks == 0 || avgs == 0) {
    printf("CORE: ERROR: Can't create peaks/averages memory!\n");
    delete peaks;
    delete avgs;
    guard.Rollback();
    return 1;
  }
  peaks->Zero();
  avgs->Zero();
  // **BUG-- small leak-- the above two are never deleted
  peaks->SetupPreallocated(pre_audioblock.get(),
                           Preallocated::PREALLOC_BASE_INSTANCE);
  avgs->SetupPreallocated(pre_audioblock.get(),
                          Preallocated::PREALLOC_BASE_INSTANCE);
  audiomem->AddExtendedData(new BED_PeaksAvgs(peaks,avgs,chunksize));

  int nt = cfg->GetNumTriggers();
  tmap.reset(new TriggerMap(this,nt));
  loopmgr.reset(new LoopManager(this));

  // Setup loop & scene browsers & trays, if defined in config
  {
    Browser *br = GetBrowserFromConfig(B_Loop);
    if (br != 0) {
      browsers[B_Loop] = br;
      br->Setup(this,loopmgr.get());
    }
    loopmgr->SetupLoopBrowser();

    br = GetBrowserFromConfig(B_Scene);
    if (br != 0) {
      browsers[B_Scene] = br;
      br->Setup(this,loopmgr.get());
    }
    loopmgr->SetupSceneBrowser();

    br = GetBrowserFromConfig(B_Loop_Tray);
    if (br != 0) {
      browsers[B_Loop_Tray] = br;
      br->Setup(this,loopmgr.get());
    }
  }

  // Create snapshots
  snaps.resize(cfg->GetMaxSnapshots());

  // Input methods 
  sdlio.reset(new SDLIO(this));
  midi.reset(new MidiIO(this));

  if (sdlio->activate()) {
    printf("(start) cant start keyboard handler\n");
    guard.Rollback();
    return 1;
  }
  if (midi->activate()) {
    printf("(start) cant start midi\n");
    guard.Rollback();
    return 1;
  }
  
  // Create Hardware Mixer interface
  hmix.reset(new HardwareMixerInterface(this));

#ifndef __MACOSX__
  osc.reset(new OSCClient(this));
#endif

  // Linkup system variables
  {
    SystemVarLink system_links[] = {
      {"SYSTEM_num_midi_outs", T_int, (char *) &(cfg->midiouts)},
      {"SYSTEM_midi_transpose", T_int, (char *) &(cfg->transpose)},
      {"SYSTEM_master_in_volume", T_float, (char *) rp->GetInputVolumePtr()},
      {"SYSTEM_master_out_volume", T_float, (char *) &(rp->outputvol)},
      {"SYSTEM_cur_pitchbend", T_int, (char *) &(midi->curbender)},
      {"SYSTEM_bender_tune", T_int, (char *) &(midi->bendertune)},
      {"SYSTEM_audio_cpu_load", T_float, (char *) &(audio->cpuload)},
      {"SYSTEM_sync_active", T_char, (char *) &(audio->sync_active)},
      {"SYSTEM_sync_transmit", T_char, (char *) &(audio->timebase_master)},
      {"SYSTEM_midisync_transmit", T_char, (char *) &(midi->midisyncxmit)},
#if USE_FLUIDSYNTH
      {"SYSTEM_fluidsynth_enabled", T_char, (char *) &(fluidp->enable)},
#endif
      {"SYSTEM_num_help_pages", T_int, (char *) &(vid->numhelppages)},
      {"SYSTEM_num_loops_in_map", T_int, (char *) &(loopmgr->numloops)},
      {"SYSTEM_num_recording_loops_in_map", T_int,
       (char *) &(loopmgr->numrecordingloops)},
    };
    LinkSystemVars(cfg.get(), system_links, sizeof(system_links) / sizeof(system_links[0]));
  }
  if (browsers[B_Patch] != 0) {
    SystemVarLink patch_links[] = {
      {"SYSTEM_num_patchbanks", T_int,
       (char *) &(((PatchBrowser *) browsers[B_Patch])->num_pb)},
      {"SYSTEM_cur_patchbank_tag", T_int,
       (char *) &(((PatchBrowser *) browsers[B_Patch])->pb_cur_tag)},
    };
    LinkSystemVars(cfg.get(), patch_links, sizeof(patch_links) / sizeof(patch_links[0]));
  }
  {
    SystemVarLink interface_links[] = {
      {"SYSTEM_num_switchable_interfaces", T_int, (char *) &(cfg->numinterfaces)},
      {"SYSTEM_cur_switchable_interface", T_int, (char *) &(vid->cur_iid)},
    };
    LinkSystemVars(cfg.get(), interface_links, sizeof(interface_links) / sizeof(interface_links[0]));
  }
  for (int i = 0; i < LAST_REC_COUNT; i++) {
    snprintf(tmp,sizeof(tmp),"SYSTEM_loopid_lastrecord_%d",i);
    cfg->LinkSystemVariable(tmp,T_int,
                            (char *) &(loopmgr->lastrecidx[i]));
  }
  for (int i = 0; i < iset->numins; i++) {
    snprintf(tmp,255,"SYSTEM_in_%d_volume",i+1);
    cfg->LinkSystemVariable(tmp,T_float,
                            (char *) &(iset->invols[i]));
    snprintf(tmp,255,"SYSTEM_in_%d_peak",i+1);
    cfg->LinkSystemVariable(tmp,T_float,
                            (char *) &(iset->inpeak[i]));
    snprintf(tmp,255,"SYSTEM_in_%d_record",i+1);
    cfg->LinkSystemVariable(tmp,T_char,
                            (char *) &(iset->selins[i]));
  }
  {
    FloDisplaySnapshots *sn = 
      (FloDisplaySnapshots *) cfg->GetDisplayByType(FD_Snapshots);
    if (sn != 0)
      cfg->LinkSystemVariable("SYSTEM_snapshot_page_firstidx",T_int,
                              (char *) &(sn->firstidx));
  }

  // Finally, final Config start
  cfg->Start();

  // Now start signal processing
  if (audio->activate(rp.get())) {
    printf("MAIN: Error with signal processing start!\n");
    guard.Rollback();
    return 1;
  }

  // Add disk output threads
  if (cfg->IsStreamFinal())
    fs_finalout = new FileStreamer(this,0,cfg->IsStereoMaster());
  else
    fs_finalout = 0;

  if (cfg->IsStreamLoops())
    fs_loopout = new FileStreamer(this,0,cfg->IsStereoMaster());
  else
    fs_loopout = 0;

  printf("CORE: Creating disk streamers for %d inputs\n",iset->GetNumInputs());
  fs_inputs.assign(iset->GetNumInputs(), (FileStreamer *) 0);
  for (int i = 0; i < iset->GetNumInputs(); i++)
    if (cfg->IsStreamInputs(i))
      fs_inputs[i] = new FileStreamer(this,i,cfg->IsStereoInput(i));
    else
      fs_inputs[i] = 0;

  // *** ALL THREADS THAT WRITE TO SRMWRingBuffers MUST BE CREATED BEFORE THIS POINT ***

  // Now that all threads are present, initialize ring buffers
  emg->FinalPrep();
  rp->FinalPrep();

  // Add core audio processing elements (requires event queue, initialized above)

  // Add 'individual inputs' disk streams
  for (int i = 0; i < iset->GetNumInputs(); i++)
    if (fs_inputs[i] != 0)
      rp->AddChild(fs_inputs[i],ProcessorItem::TYPE_GLOBAL,1);  // Silent- no output from file streamer

  // Add 'loop output' disk stream
  // In series following a limiter just for loop outputs
  if (fs_loopout != 0) {
    rp->AddChild(new AutoLimitProcessor(this),ProcessorItem::TYPE_GLOBAL_SECOND_CHAIN);
    rp->AddChild(fs_loopout,ProcessorItem::TYPE_GLOBAL_SECOND_CHAIN,1); // Streamer is silent
  }

  // Add monitor mix
  float *inputvol = rp->GetInputVolumePtr(); // Where to get input vol from
  rp->AddChild(new PassthroughProcessor(this, iset.get(), inputvol),
               ProcessorItem::TYPE_GLOBAL); // Monitor mix is global- it is summed in after the gain stage for all loops

  // Now do master output autolimit
  masterlimit = new AutoLimitProcessor(this);
  rp->AddChild(masterlimit,ProcessorItem::TYPE_FINAL);
  cfg->LinkSystemVariable("SYSTEM_cur_limiter_gain",T_float,
                          (char *) &(masterlimit->curlimitvol));

  // Add 'final output' disk stream
  if (fs_finalout != 0)
    rp->AddChild(fs_finalout,ProcessorItem::TYPE_FINAL,1);

  // Begin recording into audio memory (use mono/stereo memory as appropriate)
  amrec = new RecordProcessor(this, iset.get(), inputvol, audiomem,
                              cfg->IsStereoMaster());
  if (amrec == 0) {
    printf("CORE: ERROR: Can't create core RecordProcessor!\n");
    guard.Rollback();
    return 1;
  }
  bmg->PeakAvgOn(audiomem,amrec->GetIterator());
  rp->AddChild(amrec,ProcessorItem::TYPE_HIPRIORITY);

  guard.Release();
  return 0;
}

void Fweelin::RollbackSetup() {
  if (vid != 0) {
    vid->close();
    vid.reset();
  }

  if (sdlio != 0) {
    sdlio->close();
    sdlio.reset();
  }

  if (midi != 0) {
    midi->close();
    midi.reset();
  }

  if (audio != 0) {
    audio->close();
    audio.reset();
  }

  hmix.reset();

  iset.reset();

  abufs.reset();

  snaps.clear();

  fs_inputs.clear();
  fs_finalout = 0;
  fs_loopout = 0;

#ifndef __MACOSX__
  osc.reset();
#endif

#if USE_FLUIDSYNTH
  fluidp.reset();
#endif

  browsers.clear();

  if (amrec != 0 && bmg != 0 && audiomem != 0) {
    getAMPEAKS()->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);
    getAMAVGS()->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);
  }
  if (audiomem != 0)
    audiomem->SetupPreallocated(0,Preallocated::PREALLOC_BASE_INSTANCE);

  tmap.reset();

  loopmgr.reset();

  rp.reset();
  masterlimit = 0;
  amrec = 0;

  bmg.reset();

  audiomem = 0;

  scope.reset();
  scope_len = 0;

  pre_audioblock.reset();
  pre_extrachannel.reset();
  pre_timemarker.reset();

  cfg.reset();

  emg.reset();

  mmg.reset();

  if (sdl_ready) {
    SDL_Quit();
    sdl_ready = 0;
  }

  if (rt_threads_ready) {
    RT_RWThreads::CloseAll();
    rt_threads_ready = 0;
  }
}

void Fweelin::ItemSelected (BrowserItem *item) {
  // Main app handles selected callback for patch browser
  if (item->GetType() != B_Patch)
    printf("CORE: ERROR- Patch Browser contains items of invalid type!\n");
  else {
    PatchBrowser *br = (PatchBrowser *) getBROWSER(B_Patch);

    if (br != 0) {
      // Update MIDI with newly selected patch
      br->SetMIDIForPatch();

      PatchBank *pb = br->GetCurPatchBank();
      if (pb->port == 0) {
        // We are selecting in a Fluidsynth bank-- send patch change
#if USE_FLUIDSYNTH
        getFLUIDP()->SendPatchChange((PatchItem *) item);
#else
        printf("CORE: ERROR: Can't change FluidSynth patches- no FluidSynth "
               "support!\n");
#endif
      } else {
        // Check if change messages to be sent?
        if (!pb->suppresschg) {
          // Tell MIDI to send out bank/program change(s) for new patch
          getMIDI()->SendBankProgramChange((PatchItem *) item);
        }
      }
    }
  }
};
