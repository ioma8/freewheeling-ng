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

#include "fweelin_dsp_profile.h"

#include <stdlib.h>
#include <string.h>

#ifdef __MACOSX__
#include <mach/mach_time.h>
#else
#include <time.h>
#endif

#include "fweelin_core_dsp.h"

namespace {

struct ProfileCounter {
  uint64_t calls;
  uint64_t frames;
  uint64_t total_ticks;
  uint64_t max_ticks;
};

constexpr int kProcessChainTypes = ProcessorItem::TYPE_FINAL + 1;

struct ProfileState {
  char enabled;
  char output_path[1024];
  uint64_t snapshot_counter;
  ProfileCounter audio_callback;
  ProfileCounter root_process;
  ProfileCounter pulse_process;
  ProfileCounter record_process;
  ProfileCounter record_input_mix;
  ProfileCounter record_overdub_mix;
  ProfileCounter record_write;
  ProfileCounter processchain[kProcessChainTypes];
#ifdef __MACOSX__
  mach_timebase_info_data_t timebase;
#endif
};

void write_report_at_exit();
void write_report_snapshot();

ProfileState &state() {
  static ProfileState profile_state = {
      0,
      {0},
      0,
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {0, 0, 0, 0},
      {{0, 0, 0, 0},
       {0, 0, 0, 0},
       {0, 0, 0, 0},
       {0, 0, 0, 0},
       {0, 0, 0, 0}}
#ifdef __MACOSX__
      ,
      {0, 0}
#endif
  };
  static bool initialized = false;
  if (!initialized) {
    profile_state.enabled = (getenv("FW_PROFILE_DSP") != 0 ? 1 : 0);
    const char *output_path = getenv("FW_PROFILE_DSP_OUT");
    if (output_path != 0) {
      strncpy(profile_state.output_path, output_path,
              sizeof(profile_state.output_path) - 1);
      profile_state.output_path[sizeof(profile_state.output_path) - 1] = '\0';
    }
#ifdef __MACOSX__
    mach_timebase_info(&profile_state.timebase);
#endif
    if (profile_state.enabled)
      atexit(write_report_at_exit);
    initialized = true;
  }
  return profile_state;
}

void record(ProfileCounter &counter, uint64_t elapsed_ticks, uint64_t frames) {
  counter.calls++;
  counter.frames += static_cast<uint64_t>(frames);
  counter.total_ticks += elapsed_ticks;
  if (elapsed_ticks > counter.max_ticks)
    counter.max_ticks = elapsed_ticks;
}

double ticks_to_microseconds(uint64_t ticks) {
#ifdef __MACOSX__
  const ProfileState &profile_state = state();
  return static_cast<double>(ticks) *
         static_cast<double>(profile_state.timebase.numer) /
         static_cast<double>(profile_state.timebase.denom) / 1000.0;
#else
  return static_cast<double>(ticks) / 1000.0;
#endif
}

void print_counter(FILE *out, const char *name, const ProfileCounter &counter) {
  if (counter.calls == 0)
    return;

  const double total_us = ticks_to_microseconds(counter.total_ticks);
  const double max_us = ticks_to_microseconds(counter.max_ticks);
  const double avg_us = total_us / static_cast<double>(counter.calls);
  const double avg_ns_per_frame =
      (counter.frames > 0 ? (total_us * 1000.0) / static_cast<double>(counter.frames)
                          : 0.0);
  fprintf(out,
          "DSP PROFILE: %s calls=%llu avg_us=%.3f max_us=%.3f avg_ns_per_frame=%.3f\n",
          name,
          static_cast<unsigned long long>(counter.calls),
          avg_us,
          max_us,
          avg_ns_per_frame);
}

const char *processchain_name(int type) {
  switch (type) {
  case ProcessorItem::TYPE_DEFAULT:
    return "processchain_default";
  case ProcessorItem::TYPE_GLOBAL:
    return "processchain_global";
  case ProcessorItem::TYPE_GLOBAL_SECOND_CHAIN:
    return "processchain_global_second";
  case ProcessorItem::TYPE_HIPRIORITY:
    return "processchain_hipriority";
  case ProcessorItem::TYPE_FINAL:
    return "processchain_final";
  default:
    return "processchain_unknown";
  }
}

void write_report_at_exit() {
  ProfileState &profile_state = state();
  if (!profile_state.enabled)
    return;

  write_report_snapshot();
}

void write_report_snapshot() {
  ProfileState &profile_state = state();
  if (!profile_state.enabled)
    return;

  if (profile_state.output_path[0] != '\0') {
    FILE *out = fopen(profile_state.output_path, "w");
    if (out != 0) {
      DspProfile::PrintReport(out);
      fclose(out);
      return;
    }
  }

  DspProfile::PrintReport(stderr);
}

}

namespace DspProfile {

bool Enabled() { return state().enabled != 0; }

uint64_t NowTicks() {
#ifdef __MACOSX__
  return mach_absolute_time();
#else
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000000000ULL +
         static_cast<uint64_t>(ts.tv_nsec);
#endif
}

void RecordAudioCallback(uint64_t elapsed_ticks, uint64_t frames) {
  ProfileState &profile_state = state();
  record(profile_state.audio_callback, elapsed_ticks, frames);
  profile_state.snapshot_counter++;
  if (profile_state.output_path[0] != '\0' &&
      (profile_state.snapshot_counter % 128) == 0)
    write_report_snapshot();
}

void RecordRootProcess(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().root_process, elapsed_ticks, frames);
}

void RecordProcessChain(int type, uint64_t elapsed_ticks, uint64_t frames) {
  if (type < 0 || type >= kProcessChainTypes)
    return;
  record(state().processchain[type], elapsed_ticks, frames);
}

void RecordPulseProcess(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().pulse_process, elapsed_ticks, frames);
}

void RecordRecordProcess(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().record_process, elapsed_ticks, frames);
}

void RecordRecordInputMix(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().record_input_mix, elapsed_ticks, frames);
}

void RecordRecordOverdubMix(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().record_overdub_mix, elapsed_ticks, frames);
}

void RecordRecordWrite(uint64_t elapsed_ticks, uint64_t frames) {
  record(state().record_write, elapsed_ticks, frames);
}

void PrintReport(FILE *out) {
  ProfileState &profile_state = state();
  if (!profile_state.enabled)
    return;

  print_counter(out, "audio_callback", profile_state.audio_callback);
  print_counter(out, "root_process", profile_state.root_process);
  print_counter(out, "pulse_process", profile_state.pulse_process);
  print_counter(out, "record_process", profile_state.record_process);
  print_counter(out, "record_input_mix", profile_state.record_input_mix);
  print_counter(out, "record_overdub_mix", profile_state.record_overdub_mix);
  print_counter(out, "record_write", profile_state.record_write);
  for (int i = 0; i < kProcessChainTypes; i++)
    print_counter(out, processchain_name(i), profile_state.processchain[i]);
}

}
