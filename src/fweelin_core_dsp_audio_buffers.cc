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
#include <string.h>

#include "fweelin_core_dsp.h"

namespace {
const float MAX_DVOL = 1.5f;
}

AudioBuffers::AudioBuffers(Fweelin *app, const AudioBuffers *input_source) : app(app) {
  if (input_source != 0) {
    numins_ext = input_source->numins_ext;
    numins = input_source->numins;
    ins[0] = input_source->ins[0];
    ins[1] = input_source->ins[1];
    owns_ins[0] = owns_ins[1] = 0;
  } else {
    numins_ext = app->getCFG()->GetExtAudioIns();
    numins = numins_ext + GetIntAudioIns();
    ins[0] = new sample_t *[numins];
    ins[1] = new sample_t *[numins];
    owns_ins[0] = owns_ins[1] = 1;
    memset(ins[0], 0, sizeof(sample_t *) * numins);
    memset(ins[1], 0, sizeof(sample_t *) * numins);
  }

  numouts = GetAudioOuts();
  outs[0] = new sample_t *[numouts];
  outs[1] = new sample_t *[numouts];
  owns_outs[0] = owns_outs[1] = 1;
  memset(outs[0], 0, sizeof(sample_t *) * numouts);
  memset(outs[1], 0, sizeof(sample_t *) * numouts);
}

AudioBuffers::~AudioBuffers() {
  if (owns_ins[0])
    delete[] ins[0];
  if (owns_ins[1])
    delete[] ins[1];
  if (owns_outs[0])
    delete[] outs[0];
  if (owns_outs[1])
    delete[] outs[1];
}

char AudioBuffers::IsStereoInput(int n) {
  return app->getCFG()->IsStereoInput(n);
}

char AudioBuffers::IsStereoOutput(int n) {
  return app->getCFG()->IsStereoOutput(n);
}

char AudioBuffers::IsStereoMaster() {
  return app->getCFG()->IsStereoMaster();
}

void AudioBuffers::MixInputs(nframes_t len, sample_t **dest, InputSettings *iset,
                             float inputvol, char compute_stats) {
  const static int DCOFS_MINIMUM_SAMPLE_COUNT = 10000;
  const static float DCOFS_LOWPASS_COEFF = 0.99f;
  const static float DCOFS_ONEMINUS_LOWPASS_COEFF =
      1.0f - DCOFS_LOWPASS_COEFF;
  const static nframes_t PEAK_HOLD_LENGTH = 1;

  int stereomix = (dest[1] != 0 ? 1 : 0);
  memset(dest[0], 0, sizeof(sample_t) * len);
  if (stereomix)
    memset(dest[1], 0, sizeof(sample_t) * len);

  nframes_t phold = 0;
  if (compute_stats)
    phold = app->getAUDIO()->get_srate() * PEAK_HOLD_LENGTH;

  for (int i = 0; i < numins; i++) {
    if (!iset->selins[i])
      continue;

    sample_t *const in0 = ins[0][i];
    sample_t *const in1 = (stereomix ? (ins[1][i] != 0 ? ins[1][i] : in0) : 0);
    const float vol = iset->invols[i] * inputvol;

    if (compute_stats) {
      int cnt = iset->inscnt[i];
      nframes_t peaktime = iset->inpeaktime[i];
      sample_t peak = (cnt - peaktime > phold ? 0 : iset->inpeak[i]);

      sample_t sum0 = iset->insums[0][i];
      const sample_t dcofs0 = iset->insavg[0][i];
      for (nframes_t idx = 0; idx < len; idx++) {
        const sample_t s = in0[idx];
        const sample_t sabs = fabsf(s);
        sum0 += s;
        if (sabs > peak) {
          peak = sabs;
          peaktime = cnt;
        }
        cnt++;
        dest[0][idx] += (s - dcofs0) * vol;
      }

      iset->inscnt[i] = cnt;
      iset->insums[0][i] = sum0;
      iset->inpeak[i] = peak;
      iset->inpeaktime[i] = peaktime;
      if (cnt > DCOFS_MINIMUM_SAMPLE_COUNT) {
        iset->insavg[0][i] =
            (DCOFS_LOWPASS_COEFF * iset->insavg[0][i]) +
            (DCOFS_ONEMINUS_LOWPASS_COEFF * sum0 / cnt);
      }

      if (stereomix) {
        sample_t sum1 = iset->insums[1][i];
        const sample_t dcofs1 = iset->insavg[1][i];
        for (nframes_t idx = 0; idx < len; idx++) {
          const sample_t s = in1[idx];
          const sample_t sabs = fabsf(s);
          sum1 += s;
          if (sabs > peak) {
            peak = sabs;
            peaktime = cnt;
          }
          dest[1][idx] += (s - dcofs1) * vol;
        }

        iset->insums[1][i] = sum1;
        iset->inpeak[i] = peak;
        iset->inpeaktime[i] = peaktime;
        if (cnt > DCOFS_MINIMUM_SAMPLE_COUNT) {
          iset->insavg[1][i] =
              (DCOFS_LOWPASS_COEFF * iset->insavg[1][i]) +
              (DCOFS_ONEMINUS_LOWPASS_COEFF * sum1 / cnt);
        }
      }
      continue;
    }

    const sample_t dcofs0 = iset->insavg[0][i];
    for (nframes_t idx = 0; idx < len; idx++)
      dest[0][idx] += (in0[idx] - dcofs0) * vol;

    if (stereomix) {
      const sample_t dcofs1 = iset->insavg[1][i];
      for (nframes_t idx = 0; idx < len; idx++)
        dest[1][idx] += (in1[idx] - dcofs1) * vol;
    }
  }
}

char InputSettings::IsSelectedStereo() {
  for (int i = 0; i < numins; i++) {
    if (selins[i] && app->getCFG()->IsStereoInput(i))
      return 1;
  }
  return 0;
}

void InputSettings::AdjustInputVol(int n, float adjust) {
  if (HasInput(n)) {
    if (dinvols[n] < MAX_DVOL)
      dinvols[n] += adjust * app->getAUDIO()->GetTimeScale();
    if (dinvols[n] < 0.0)
      dinvols[n] = 0.0;
  } else {
    printf("CORE: InputSettings- input number %d not in range.\n", n);
  }
}
