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

#include <pthread.h>
#include <string.h>

#include "fweelin_core_dsp.h"
#include "fweelin_dsp_profile.h"

namespace {
const float MAX_VOL = 5.0f;
const float MAX_DVOL = 1.5f;
}

RootProcessor::RootProcessor(Fweelin *app, InputSettings *iset)
    : Processor(app), processor_commands(0), iset(iset), outputvol(1.0),
      doutputvol(1.0), inputvol(1.0), dinputvol(1.0), firstchild(0),
      samplecnt(0) {
  abtmp = new AudioBuffers(app, app->getABUFS());
  preabtmp = new AudioBuffers(app, app->getABUFS());
  buf[0] = new sample_t[app->getBUFSZ()];
  prebuf[0] = new sample_t[prelen];
  if (abtmp->IsStereoMaster()) {
    buf[1] = new sample_t[app->getBUFSZ()];
    prebuf[1] = new sample_t[prelen];
  } else {
    buf[1] = 0;
    prebuf[1] = 0;
  }

  app->getEMG()->ListenEvent(this, 0, T_EV_CleanupProcessor);
}

RootProcessor::~RootProcessor() {
  ProcessorItem *cur = firstchild;
  while (cur != 0) {
    ProcessorItem *tmp = cur->next;
    delete cur->p;
    delete cur;
    cur = tmp;
  }

  delete[] buf[0];
  delete[] prebuf[0];
  if (buf[1] != 0)
    delete[] buf[1];
  if (prebuf[1] != 0)
    delete[] prebuf[1];

  delete abtmp;
  delete preabtmp;
  delete processor_commands;

  app->getEMG()->UnlistenEvent(this, 0, T_EV_CleanupProcessor);
}

void RootProcessor::FinalPrep() {
  printf("RP: Create ringbuffers and begin.\n");
  processor_commands = new ProcessorCommandQueue();
}

void RootProcessor::AdjustOutputVolume(float adjust) {
  if (doutputvol < MAX_DVOL)
    doutputvol += adjust * app->getAUDIO()->GetTimeScale();
  if (doutputvol < 0.0)
    doutputvol = 0.0;
}

void RootProcessor::AdjustInputVolume(float adjust) {
  if (dinputvol < MAX_DVOL)
    dinputvol += adjust * app->getAUDIO()->GetTimeScale();
  if (dinputvol < 0.0)
    dinputvol = 0.0;
}

void RootProcessor::AddChild(Processor *o, int type, char silent) {
  dopreprocess();

  ProcessorItem *item = new ProcessorItem(o, type, silent);
  if (processor_commands == 0 || !processor_commands->EnqueueAdd(item)) {
    printf("RP: ERROR: can't enqueue processor add\n");
    delete item;
  }
}

void RootProcessor::DelChild(Processor *o) {
  if (o != 0) {
    dopreprocess();
    o->Halt();
    if (processor_commands == 0 || !processor_commands->EnqueueDelete(o))
      printf("RP: ERROR: can't enqueue processor delete\n");
  }
}

void RootProcessor::processchain(char pre, nframes_t len, AudioBuffers *ab,
                                 AudioBuffers *abchild, const int ptype,
                                 const char mixintoout) {
  uint64_t profile_start_ticks = 0;
  if (!pre && DspProfile::Enabled())
    profile_start_ticks = DspProfile::NowTicks();

  int stereo = (ab->outs[1][0] != 0 && abchild->outs[1][0] != 0 ? 1 : 0);

  ProcessorItem *cur = firstchild;
  while (cur != 0) {
    if (cur->status != ProcessorItem::STATUS_PENDING_DELETE &&
        cur->type == ptype) {
      cur->p->process(pre, len, abchild);

      if (!pre && cur->status == ProcessorItem::STATUS_LIVE_PENDING_DELETE)
        cur->status = ProcessorItem::STATUS_PENDING_DELETE;

      if (mixintoout && !cur->silent) {
        for (int chan = 0; chan <= stereo; chan++) {
          sample_t *sumout = ab->outs[chan][0];
          sample_t *out = abchild->outs[chan][0];
          for (nframes_t j = 0; j < len; j++)
            sumout[j] += out[j];
        }
      }
    }

    cur = cur->next;
  }

  if (profile_start_ticks != 0)
    DspProfile::RecordProcessChain(ptype,
                                   DspProfile::NowTicks() - profile_start_ticks,
                                   len);
}

void RootProcessor::ReceiveEvent(Event *ev, EventProducer */*from*/) {
  switch (ev->GetType()) {
  case T_EV_CleanupProcessor: {
    CleanupProcessorEvent *cleanevt = (CleanupProcessorEvent *)ev;
    delete cleanevt->processor->p;
    delete cleanevt->processor;
    break;
  }

  default:
    break;
  }
}

void RootProcessor::UpdateProcessors() {
  if (processor_commands != 0) {
    ProcessorCommand cmd;
    while (processor_commands->ReadNext(&cmd)) {
      if (cmd.type == ProcessorCommand::CMD_ADD) {
        ProcessorItem *cur = firstchild;
        if (cur == 0)
          firstchild = cmd.item;
        else {
          while (cur->next != 0)
            cur = cur->next;
          cur->next = cmd.item;
        }
      } else if (cmd.type == ProcessorCommand::CMD_REQUEST_DELETE) {
        ProcessorItem *cur = firstchild;
        while (cur != 0) {
          if (cur->p == cmd.processor &&
              cur->status == ProcessorItem::STATUS_GO) {
            cur->status = ProcessorItem::STATUS_LIVE_PENDING_DELETE;
            break;
          }
          cur = cur->next;
        }
      }
    }

    ProcessorItem *cur = firstchild;
    ProcessorItem *prev = 0;
    while (cur != 0) {
      if (cur->status == ProcessorItem::STATUS_PENDING_DELETE) {
        ProcessorItem *tmp = cur->next;
        if (prev != 0)
          prev->next = tmp;
        else
          firstchild = tmp;

        CleanupProcessorEvent *cleanevt =
            (CleanupProcessorEvent *)Event::GetEventByType(
                T_EV_CleanupProcessor);
        cleanevt->processor = cur;
        app->getEMG()->BroadcastEvent(cleanevt, this);
        cur = tmp;
      } else {
        prev = cur;
        cur = cur->next;
      }
    }
  }
}

void RootProcessor::process(char pre, nframes_t len, AudioBuffers *ab) {
  uint64_t profile_start_ticks = 0;
  if (!pre && DspProfile::Enabled())
    profile_start_ticks = DspProfile::NowTicks();

  nframes_t fragmentsize = app->getBUFSZ();
  if (len > fragmentsize)
    len = fragmentsize;

  sample_t *out[2] = {ab->outs[0][0], ab->outs[1][0]};
  int stereo = (out[1] != 0 ? 1 : 0);
  memset(out[0], 0, sizeof(sample_t) * len);
  if (stereo)
    memset(out[1], 0, sizeof(sample_t) * len);

  if (!pre) {
    if (processor_commands == 0)
      return;

    UpdateProcessors();

#if USE_FLUIDSYNTH
    FluidSynthProcessor *fluidp = app->getFLUIDP();
    fluidp->process(0, len, ab);
#endif

    if (doutputvol != 1.0 || dinputvol != 1.0) {
      if (doutputvol > 1.0 && outputvol < MIN_VOL)
        outputvol = MIN_VOL;
      if (outputvol < MAX_VOL)
        outputvol *= doutputvol;
      if (dinputvol > 1.0 && inputvol < MIN_VOL)
        inputvol = MIN_VOL;
      if (inputvol < MAX_VOL)
        inputvol *= dinputvol;
    }

    for (int i = 0; i < iset->numins; i++) {
      if (iset->dinvols[i] > 1.0 && iset->invols[i] < MIN_VOL)
        iset->invols[i] = MIN_VOL;
      iset->invols[i] *= iset->dinvols[i];
    }
  }

  if (!pre) {
    abtmp->outs[0][0] = buf[0];
    abtmp->outs[1][0] = buf[1];

    processchain(pre, len, ab, abtmp, ProcessorItem::TYPE_HIPRIORITY, 1);
    processchain(pre, len, ab, abtmp, ProcessorItem::TYPE_DEFAULT, 1);
  } else {
    preabtmp->outs[0][0] = prebuf[0];
    preabtmp->outs[1][0] = prebuf[1];

    processchain(pre, len, ab, preabtmp, ProcessorItem::TYPE_HIPRIORITY, 1);
    processchain(pre, len, ab, preabtmp, ProcessorItem::TYPE_DEFAULT, 1);
  }

  for (int chan = 0; chan <= stereo; chan++) {
    sample_t *o = out[chan];
    for (nframes_t l = 0; l < len; l++)
      o[l] *= outputvol;
  }

  if (!pre) {
    fadepreandcurrent(ab);

    sample_t *saved_in0 = abtmp->ins[0][0];
    sample_t *saved_in1 = abtmp->ins[1][0];
    memcpy(buf[0], out[0], sizeof(sample_t) * len);
    if (out[1] != 0)
      memcpy(buf[1], out[1], sizeof(sample_t) * len);
    abtmp->ins[0][0] = buf[0];
    abtmp->ins[1][0] = buf[1];
    abtmp->outs[0][0] = buf[0];
    abtmp->outs[1][0] = buf[1];
    processchain(pre, len, abtmp, abtmp,
                 ProcessorItem::TYPE_GLOBAL_SECOND_CHAIN, 0);

    abtmp->ins[0][0] = saved_in0;
    abtmp->ins[1][0] = saved_in1;
    processchain(pre, len, ab, abtmp, ProcessorItem::TYPE_GLOBAL, 1);

    abtmp->ins[0][0] = out[0];
    abtmp->ins[1][0] = out[1];
    abtmp->outs[0][0] = out[0];
    abtmp->outs[1][0] = out[1];
    processchain(pre, len, ab, abtmp, ProcessorItem::TYPE_FINAL, 0);

    samplecnt += len;
  }

  for (int i = 1; i < ab->numouts; i++) {
    for (int chan = 0; chan <= stereo; chan++) {
      if (ab->outs[chan][i] != 0)
        memcpy(ab->outs[chan][i], ab->outs[chan][0],
               sizeof(sample_t) * len);
    }
  }

  if (profile_start_ticks != 0)
    DspProfile::RecordRootProcess(DspProfile::NowTicks() - profile_start_ticks,
                                  len);
}
