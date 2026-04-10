#ifndef FWEELIN_DSP_PROFILE_H
#define FWEELIN_DSP_PROFILE_H

#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

namespace DspProfile {

bool Enabled();
uint64_t NowTicks();
void RecordAudioCallback(uint64_t elapsed_ticks, uint64_t frames);
void RecordRootProcess(uint64_t elapsed_ticks, uint64_t frames);
void RecordProcessChain(int type, uint64_t elapsed_ticks, uint64_t frames);
void RecordPulseProcess(uint64_t elapsed_ticks, uint64_t frames);
void RecordRecordProcess(uint64_t elapsed_ticks, uint64_t frames);
void RecordRecordInputMix(uint64_t elapsed_ticks, uint64_t frames);
void RecordRecordOverdubMix(uint64_t elapsed_ticks, uint64_t frames);
void RecordRecordWrite(uint64_t elapsed_ticks, uint64_t frames);
void PrintReport(FILE *out);

}

#endif
