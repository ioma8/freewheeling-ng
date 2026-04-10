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
   along with FreeWheeling.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <X11/Xlib.h>

#include "fweelin_core.h"
#include "fweelin_fluidsynth.h"
#include "fweelin_paramset.h"
#include "fweelin_startup_guard.h"

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
