/*
   Does power come from fancy toys
   or does power come from the integrity of our walk?
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

#ifdef __MACOSX__

#include <sys/time.h>

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include "fweelin_config.h"
#include "fweelin_audioio.h"
#include "fweelin_core_dsp.h"

#if USE_FLUIDSYNTH
#include "fweelin_fluidsynth.h"
#endif

static void configure_audio_buffer_list(AudioBufferList *abl,
                                        sample_t *left,
                                        sample_t *right,
                                        nframes_t frames) {
  abl->mNumberBuffers = 2;
  abl->mBuffers[0].mNumberChannels = 1;
  abl->mBuffers[0].mDataByteSize = frames * sizeof(sample_t);
  abl->mBuffers[0].mData = left;
  abl->mBuffers[1].mNumberChannels = 1;
  abl->mBuffers[1].mDataByteSize = frames * sizeof(sample_t);
  abl->mBuffers[1].mData = right;
}

static inline nframes_t audio_max_frames_per_slice(nframes_t device_frames) {
  const nframes_t kPreferredFramesPerSlice = 16384;
  return (device_frames > kPreferredFramesPerSlice ?
          device_frames : kPreferredFramesPerSlice);
}

static void register_audio_thread_if_needed(AudioIO *inst, pthread_t thread) {
  if (inst->audio_thread == 0) {
    inst->audio_thread = thread;
    return;
  }

  if (pthread_equal(inst->audio_thread, thread))
    return;

  if (inst->audio_thread_2 == 0) {
    inst->audio_thread_2 = thread;
    RT_RWThreads::RegisterReaderOrWriter(thread);
    return;
  }

  if (pthread_equal(inst->audio_thread_2, thread))
    return;
}

OSStatus AudioIO::input_process(void *arg,
                                AudioUnitRenderActionFlags *ioActionFlags,
                                const AudioTimeStamp *inTimeStamp,
                                UInt32 inBusNumber,
                                UInt32 inNumberFrames,
                                AudioBufferList */*ioData*/) {
  AudioIO *inst = static_cast<AudioIO *>(arg);
  register_audio_thread_if_needed(inst, pthread_self());

  inst->capture_abl->mBuffers[0].mDataByteSize =
    inst->capture_frames * sizeof(sample_t);
  inst->capture_abl->mBuffers[1].mDataByteSize =
    inst->capture_frames * sizeof(sample_t);

  OSStatus err = AudioUnitRender(inst->input_unit,
                                 ioActionFlags,
                                 inTimeStamp,
                                 inBusNumber,
                                 inNumberFrames,
                                 inst->capture_abl);
  if (err != noErr) {
    memset(inst->capture[0], 0, sizeof(sample_t) * inst->capture_frames);
    memset(inst->capture[1], 0, sizeof(sample_t) * inst->capture_frames);
  }

  return noErr;
}

OSStatus AudioIO::process(void *arg,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp *inTimeStamp,
                          UInt32 /*inBusNumber*/,
                          UInt32 inNumberFrames,
                          AudioBufferList *ioData) {
  AudioIO *inst = static_cast<AudioIO *>(arg);
  const unsigned int kCPULoadSamplePeriod = 16;
  const double kNanosPerSecond = 1000000000.0;

  register_audio_thread_if_needed(inst, pthread_self());

  if (inst->cpuload_sample_count == 0)
    inst->cpuload_start_ticks = mach_absolute_time();

  inst->app->getEMG()->WakeupIfNeeded();
  inst->app->getMMG()->WakeupIfNeeded();

  *ioActionFlags |= kAudioUnitRenderAction_OutputIsSilence;

  AudioBuffers *ab = inst->app->getABUFS();
  for (int i = 0; i < ab->numins_ext; i++) {
    inst->iport[0][i] = inst->capture[0];
    inst->iport[1][i] = (ab->IsStereoInput(i) ? inst->capture[1] : 0);
    ab->ins[0][i] = inst->iport[0][i];
    ab->ins[1][i] = inst->iport[1][i];
  }
  if (ab->numins > ab->numins_ext) {
#if USE_FLUIDSYNTH
    ab->ins[0][ab->numins_ext] = 0;
    ab->ins[1][ab->numins_ext] = 0;
#endif
  }
  inst->oport[0][0] = static_cast<sample_t *>(ioData->mBuffers[0].mData);
  inst->oport[1][0] = (ioData->mNumberBuffers > 1 ?
                       static_cast<sample_t *>(ioData->mBuffers[1].mData) :
                       0);
  ab->outs[0][0] = inst->oport[0][0];
  ab->outs[1][0] = inst->oport[1][0];

  inst->cpuload_sample_frames += inNumberFrames;
  if (inst->rp != 0)
    inst->rp->process(0, inNumberFrames, ab);

  inst->cpuload_sample_count++;
  if (inst->cpuload_sample_count >= kCPULoadSamplePeriod) {
    uint64_t end_ticks = mach_absolute_time();
    uint64_t elapsed_ticks = end_ticks - inst->cpuload_start_ticks;
    double elapsed = ((double) elapsed_ticks *
                      (double) inst->cpuload_timebase.numer /
                      (double) inst->cpuload_timebase.denom) / kNanosPerSecond;
    double period = (double) inst->cpuload_sample_frames / (double) inst->srate;
    if (period > 0.0)
      inst->cpuload = (float) (elapsed / period);
    inst->cpuload_sample_count = 0;
    inst->cpuload_sample_frames = 0;
  }

  inst->timebase_master = 1;
  inst->transport_roll = 0;
  inst->sync_active = 0;
  return noErr;
}

OSStatus AudioIO::audio_shutdown(void */*arg*/) {
  fprintf(stderr, "AUDIO: shutdown\n");
  return noErr;
}

int AudioIO::open() {
  AudioComponentDescription desc;
  memset(&desc, 0, sizeof(desc));
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_HALOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;

  AudioComponent comp_in = AudioComponentFindNext(0, &desc);
  if (comp_in == 0) {
    fprintf(stderr, "AUDIO: cannot find HAL input unit\n");
    return 1;
  }
  if (AudioComponentInstanceNew(comp_in, &input_unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot create HAL input unit\n");
    return 1;
  }

  AudioComponent comp = AudioComponentFindNext(0, &desc);
  if (comp == 0) {
    fprintf(stderr, "AUDIO: cannot find HAL output unit\n");
    return 1;
  }

  if (AudioComponentInstanceNew(comp, &unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot create HAL output unit\n");
    return 1;
  }

  UInt32 enable = 1;
  if (AudioUnitSetProperty(input_unit,
                           kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Input,
                           1,
                           &enable,
                           sizeof(enable)) != noErr) {
    fprintf(stderr, "AUDIO: cannot enable input on HAL input unit\n");
    return 1;
  }
  enable = 0;
  if (AudioUnitSetProperty(input_unit,
                           kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Output,
                           0,
                           &enable,
                           sizeof(enable)) != noErr) {
    fprintf(stderr, "AUDIO: cannot disable output on HAL input unit\n");
    return 1;
  }
  enable = 1;
  if (AudioUnitSetProperty(unit,
                           kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Input,
                           1,
                           &enable,
                           sizeof(enable)) != noErr) {
    fprintf(stderr, "AUDIO: cannot enable input on HAL unit\n");
    return 1;
  }
  if (AudioUnitSetProperty(unit,
                           kAudioOutputUnitProperty_EnableIO,
                           kAudioUnitScope_Output,
                           0,
                           &enable,
                           sizeof(enable)) != noErr) {
    fprintf(stderr, "AUDIO: cannot enable output on HAL unit\n");
    return 1;
  }

  AudioDeviceID device = kAudioObjectUnknown;
  UInt32 size = sizeof(device);
  AudioObjectPropertyAddress addr = {
    kAudioHardwarePropertyDefaultInputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
  };
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &addr,
                                 0,
                                 0,
                                 &size,
                                 &device) != noErr) {
    fprintf(stderr, "AUDIO: cannot query default input device\n");
    return 1;
  }
  if (AudioUnitSetProperty(input_unit,
                           kAudioOutputUnitProperty_CurrentDevice,
                           kAudioUnitScope_Global,
                           0,
                           &device,
                           sizeof(device)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set input device\n");
    return 1;
  }

  AudioDeviceID out_device = kAudioObjectUnknown;
  size = sizeof(out_device);
  AudioObjectPropertyAddress out_addr = {
    kAudioHardwarePropertyDefaultOutputDevice,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
  };
  if (AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                 &out_addr,
                                 0,
                                 0,
                                 &size,
                                 &out_device) != noErr) {
    fprintf(stderr, "AUDIO: cannot query default output device\n");
    return 1;
  }
  if (AudioUnitSetProperty(unit,
                           kAudioOutputUnitProperty_CurrentDevice,
                           kAudioUnitScope_Global,
                           0,
                           &out_device,
                           sizeof(out_device)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set current device\n");
    return 1;
  }

  Float64 sample_rate = 48000.0;
  size = sizeof(sample_rate);
  AudioObjectPropertyAddress rate_addr = {
    kAudioDevicePropertyNominalSampleRate,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
  };
  if (AudioObjectGetPropertyData(out_device,
                                 &rate_addr,
                                 0,
                                 0,
                                 &size,
                                 &sample_rate) != noErr ||
      sample_rate <= 0.0) {
    sample_rate = 48000.0;
  }

  AudioStreamBasicDescription fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.mSampleRate = sample_rate;
  fmt.mFormatID = kAudioFormatLinearPCM;
  fmt.mFormatFlags = kAudioFormatFlagsNativeFloatPacked | kAudioFormatFlagIsNonInterleaved;
  fmt.mBytesPerPacket = sizeof(Float32);
  fmt.mFramesPerPacket = 1;
  fmt.mBytesPerFrame = sizeof(Float32);
  fmt.mChannelsPerFrame = 2;
  fmt.mBitsPerChannel = 32;

  if (AudioUnitSetProperty(unit,
                           kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Input,
                           0,
                           &fmt,
                           sizeof(fmt)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set output stream format\n");
    return 1;
  }
  if (AudioUnitSetProperty(input_unit,
                           kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Output,
                           1,
                           &fmt,
                           sizeof(fmt)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set input stream format\n");
    return 1;
  }
  if (AudioUnitSetProperty(unit,
                           kAudioUnitProperty_StreamFormat,
                           kAudioUnitScope_Output,
                           1,
                           &fmt,
                           sizeof(fmt)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set input stream format\n");
    return 1;
  }

  UInt32 max_frames = audio_max_frames_per_slice(512);

  if (AudioUnitSetProperty(input_unit,
                           kAudioUnitProperty_MaximumFramesPerSlice,
                           kAudioUnitScope_Global,
                           0,
                           &max_frames,
                           sizeof(max_frames)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set input max frames per slice\n");
    return 1;
  }
  if (AudioUnitSetProperty(unit,
                           kAudioUnitProperty_MaximumFramesPerSlice,
                           kAudioUnitScope_Global,
                           0,
                           &max_frames,
                           sizeof(max_frames)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set output max frames per slice\n");
    return 1;
  }

  AURenderCallbackStruct cb;
  cb.inputProc = input_process;
  cb.inputProcRefCon = this;
  if (AudioUnitSetProperty(input_unit,
                           kAudioOutputUnitProperty_SetInputCallback,
                           kAudioUnitScope_Global,
                           0,
                           &cb,
                           sizeof(cb)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set input callback\n");
    return 1;
  }

  cb.inputProc = process;
  cb.inputProcRefCon = this;
  if (AudioUnitSetProperty(unit,
                           kAudioUnitProperty_SetRenderCallback,
                           kAudioUnitScope_Input,
                           0,
                           &cb,
                           sizeof(cb)) != noErr) {
    fprintf(stderr, "AUDIO: cannot set render callback\n");
    return 1;
  }

  if (AudioUnitInitialize(input_unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot initialize input audio unit\n");
    return 1;
  }

  if (AudioUnitInitialize(unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot initialize audio unit\n");
    return 1;
  }

  UInt32 frames = 512;
  size = sizeof(frames);
  AudioObjectPropertyAddress frame_addr = {
    kAudioDevicePropertyBufferFrameSize,
    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMain
  };
  if (AudioObjectGetPropertyData(device,
                                 &frame_addr,
                                 0,
                                 0,
                                 &size,
                                 &frames) != noErr || frames == 0) {
    frames = 512;
  }

  bufsize = frames;
  capture_frames = max_frames;
  srate = (nframes_t) fmt.mSampleRate;
  timescale = (float) bufsize / (float) srate;
  cpuload = 0.0f;
  sync_start_frame = 0;
  repos = 0;
  timebase_master = 1;
  sync_active = 0;
  transport_roll = 0;
  jpos.frame = 0;
  jpos.valid = 0;
  jpos.bar = 0;
  jpos.beat = 0;
  jpos.beats_per_minute = 120.0;
  jpos.beats_per_bar = 4.0f;
  jpos.beat_type = 4;
  jpos.ticks_per_beat = 1920;
  jpos.tick = 0;
  jpos.bar_start_tick = 0;
  jpos.frame_rate = srate;

  AudioBuffers *ab = app->getABUFS();
  iport[0] = new sample_t *[ab->numins_ext];
  iport[1] = new sample_t *[ab->numins_ext];
  oport[0] = new sample_t *[ab->numouts];
  oport[1] = new sample_t *[ab->numouts];

  capture[0] = (sample_t *) calloc(capture_frames, sizeof(sample_t));
  capture[1] = (sample_t *) calloc(capture_frames, sizeof(sample_t));
  size_t abl_size = sizeof(AudioBufferList) + sizeof(AudioBuffer);
  capture_abl = (AudioBufferList *) calloc(1, abl_size);
  configure_audio_buffer_list(capture_abl, capture[0], capture[1], capture_frames);

  printf("AUDIO: sample rate %u, buffer size %u, capture capacity %u\n",
         (unsigned) srate,
         (unsigned) bufsize,
         (unsigned) capture_frames);
  printf("AUDIO: using %d external inputs, %d total inputs\n",
         ab->numins_ext,
         ab->numins);

  return 0;
}

int AudioIO::activate(Processor *rp) {
  this->rp = rp;

  if (AudioOutputUnitStart(input_unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot start input audio unit\n");
    return 1;
  }
  if (AudioOutputUnitStart(unit) != noErr) {
    fprintf(stderr, "AUDIO: cannot start audio unit\n");
    AudioOutputUnitStop(input_unit);
    return 1;
  }

  while (audio_thread == 0)
    usleep(10000);

  RT_RWThreads::RegisterReaderOrWriter(audio_thread);
  return 0;
}

nframes_t AudioIO::getbufsz() {
  return bufsize;
}

void AudioIO::RelocateTransport(nframes_t /*pos*/) {
}

void AudioIO::close() {
  if (input_unit != 0) {
    AudioOutputUnitStop(input_unit);
    AudioUnitUninitialize(input_unit);
    AudioComponentInstanceDispose(input_unit);
    input_unit = 0;
  }
  if (unit != 0) {
    AudioOutputUnitStop(unit);
    AudioUnitUninitialize(unit);
    AudioComponentInstanceDispose(unit);
    unit = 0;
  }

  delete[] iport[0];
  delete[] iport[1];
  delete[] oport[0];
  delete[] oport[1];

  free(capture[0]);
  free(capture[1]);
  free(capture_abl);

  printf("AUDIO: end\n");
}

#else

// Non-macOS JACK implementation is intentionally left in the original tree.

#endif
