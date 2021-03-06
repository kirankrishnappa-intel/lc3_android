/*
 * DctIV.cpp
 *
 * Copyright 2021 HIMSA II K/S - www.himsa.com. Represented by EHIMA -
 * www.ehima.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "DctIV.hpp"

#include <cmath>

/*
 * Notes on the choice of the applied FFT library:
 *  - Different fast transform implementations for the DCT-IV can be selected
 * via USE_FFTW USE_FFTW_FOR_FFT  // assume NF being 4 times an integer
 *      USE_KISSFFT       // assume NF being 4 times an integer
 *     where only one of them is meaningful to be defined
 *
 *  - Defining none of the fast transforms will lead to a direct implementation
 *     which is good for testing but usually to slow for practial applications
 *
 *  - USE_FFTW: The free "fftw" library should be fastest, but may be more
 * complicated to be build for embedded target devices. It provides dedicated
 * optimization for the DCT-IV and thus is preferred from a technical
 * perspective. In terms of licences "fftw" is free in the sense of GPL, which
 * is not liked in some products. There might be the option to ask the authors
 * for other licences (which they propose). Nevertheless, the "license" issue
 *     has driven us to not bundle "fftw" directly with the LC3 implementation,
 * although the API for using it is provided and tested.
 *
 *  - USE_FFTW_FOR_FFT: this is a not fully optimized alternative approach using
 * the complex fft provided by "fftw" instead of the dedicated optimized DCT-IV
 * implementation. This option is mainly provided to demonstrate the transition
 * from USE_FFTW to USE_KISSFFT
 *
 *  - USE_KISSFFT: the free "kissfft" library is sligthly slower than "fftw",
 * but gives a majority of the possible performance gain over a "stupid" direct
 * DCT-IV implementation. "kissfft" does not provide a dedicated optimization
 * for DCT-IV so that some additional code for implementing a DCT-IV by a
 * complex fft has to be added. This also implies additional memory needed. The
 * big advantage of the "kissfft" is that it is less restrictive in terms of its
 * license. Further, the template based C++ implementation reduces its impact on
 * the build system to a minimum (we just need to include the right header
 * without needing to compile and link a separate file).
 *
 *   - USE_KISSFFT is defined below to be the default choice. The other
 * alternatives may be selecting by providing its define via the compiler switch
 * -D<define>
 *
 *   - The impact of the defines is restricted to this *.cpp file for clarity
 * reasons. Ok, to make this work, we needed a few "ugly" pointer casts, but we
 * really wanted to make sure, that no other code or build impact is generated
 * by the options in this file.
 */
#define USE_OWN_FFT

#if defined USE_FFTW || defined USE_FFTW_FOR_FFT
#include <fftw3.h>

#elif defined USE_KISSFFT
#include "kissfft.hh"

class KissfftConfig {
 public:
  KissfftConfig(uint16_t N) : inbuf(nullptr), outbuf(nullptr), fft(nullptr) {
    inbuf = new std::complex<double>[N];
    outbuf = new std::complex<double>[N];
    twiddle = new std::complex<double>[N];
    fft = new kissfft<double>(N, false);

    const double pi = std::acos(-1);
    for (uint16_t n = 0; n < N; n++) {
      twiddle[n] =
          std::complex<double>(std::cos(-pi * (8 * n + 1) / (8.0 * N * 2)),
                               std::sin(-pi * (8 * n + 1) / (8.0 * N * 2)));
    }
  }

  ~KissfftConfig() {
    if (nullptr != fft) {
      delete fft;
    }
    if (nullptr != inbuf) {
      delete[] inbuf;
    }
    if (nullptr != outbuf) {
      delete[] outbuf;
    }
    if (nullptr != twiddle) {
      delete[] twiddle;
    }
  }

  void transform() {
    if (nullptr != fft) {
      fft->transform(inbuf, outbuf);
    }
  }

  std::complex<double>* inbuf;
  std::complex<double>* outbuf;
  std::complex<double>* twiddle;
  kissfft<double>* fft;
};

#elif defined USE_OWN_FFT
#include <complex>

#include "fft.h"
#endif

DctIVDbl::DctIVDbl(uint16_t NF_)
    : NF(NF_), in(nullptr), out(nullptr), dctIVconfig(nullptr) {
  in = new double[NF];
  out = new double[NF];

#if defined USE_FFTW
  dctIVconfig = fftw_plan_r2r_1d(NF, in, out, FFTW_REDFT11, FFTW_ESTIMATE);

#elif defined USE_FFTW_FOR_FFT
  dctIVconfig = fftw_plan_dft_1d(NF / 2, reinterpret_cast<fftw_complex*>(in),
                                 reinterpret_cast<fftw_complex*>(out),
                                 FFTW_FORWARD, FFTW_ESTIMATE);

#elif defined USE_KISSFFT
  dctIVconfig = new KissfftConfig(NF / 2);

#elif defined USE_OWN_FFT

  int N = NF / 2;
  std::complex<double>* twiddle = new std::complex<double>[N];
  dctIVconfig = twiddle;
  const double pi = std::acos(-1);
  for (uint16_t n = 0; n < N; n++) {
    twiddle[n] =
        std::complex<double>(std::cos(-pi * (8 * n + 1) / (8.0 * N * 2)),
                             std::sin(-pi * (8 * n + 1) / (8.0 * N * 2)));
  }

#endif

  for (uint16_t n = 0; n < NF; n++) {
    in[n] = 0;
    out[n] = 0;
  }
}

DctIVDbl::~DctIVDbl() {
#if defined USE_FFTW || defined USE_FFTW_FOR_FFT
  if (nullptr != dctIVconfig) {
    fftw_destroy_plan(reinterpret_cast<fftw_plan>(dctIVconfig));
  }

#elif defined USE_KISSFFT
  if (nullptr != dctIVconfig) {
    KissfftConfig* kissfftConfig =
        reinterpret_cast<KissfftConfig*>(dctIVconfig);
    delete kissfftConfig;
  }

#elif defined USE_OWN_FFT
  std::complex<double>* twiddle = (std::complex<double>*)dctIVconfig;
  delete[] twiddle;

#endif
  if (nullptr != in) {
    delete[] in;
  }
  if (nullptr != out) {
    delete[] out;
  }
}

void DctIVDirectDbl(uint16_t N, const double* const tw, double* const X) {
  const double pi = std::acos(-1);
  for (uint16_t k = 0; k < N; k++) {
    X[k] = 0;
    for (uint16_t n = 0; n < N; n++) {
      X[k] += tw[n] * std::cos(pi / N * (n + 0.5) * (k + 0.5));
    }
    X[k] *= 2;
  }
}

void DctIVDbl::run() {
#ifdef USE_FFTW
  fftw_execute(reinterpret_cast<fftw_plan>(dctIVconfig));

#elif defined USE_FFTW_FOR_FFT
  const double pi = std::acos(-1);
  // assume NF being 4 times an integer
  for (uint16_t n = 1; n < NF / 2; n += 2) {
    double buffer;
    buffer = in[n];
    in[n] = in[NF - n];
    in[NF - n] = buffer;
  }
  for (uint16_t n = 0; n < NF; n += 2) {
    double real = in[n + 0];
    double imag = in[n + 1];
    in[n + 0] = real * std::cos(-pi * (4 * n + 1) / (8.0 * NF)) -
                imag * std::sin(-pi * (4 * n + 1) / (8.0 * NF));
    in[n + 1] = real * std::sin(-pi * (4 * n + 1) / (8.0 * NF)) +
                imag * std::cos(-pi * (4 * n + 1) / (8.0 * NF));
  }

  fftw_execute(reinterpret_cast<fftw_plan>(dctIVconfig));

  for (uint16_t n = 0; n < NF; n += 2) {
    double real = out[n + 0];
    double imag = out[n + 1];
    out[n + 0] = 2 * (real * std::cos(-pi * (4 * n + 1) / (8.0 * NF)) -
                      imag * std::sin(-pi * (4 * n + 1) / (8.0 * NF)));
    out[n + 1] = 2 * (real * std::sin(-pi * (4 * n + 1) / (8.0 * NF)) +
                      imag * std::cos(-pi * (4 * n + 1) / (8.0 * NF)));
  }
  for (uint16_t n = 1; n < NF / 2; n += 2) {
    double buffer;
    buffer = out[n];
    out[n] = -out[NF - n];
    out[NF - n] = -buffer;
  }

#elif defined USE_KISSFFT
  // assume NF being 4 times an integer
  KissfftConfig* kissfftConfig = reinterpret_cast<KissfftConfig*>(dctIVconfig);
  for (uint16_t n = 0; n < NF / 2; n++) {
    kissfftConfig->inbuf[n] =
        kissfftConfig->twiddle[n] *
        std::complex<double>(in[2 * n], in[NF - 2 * n - 1]);
  }

  kissfftConfig->transform();

  for (uint16_t n = 0; n < NF / 2; n++) {
    std::complex<double> complexOut =
        kissfftConfig->twiddle[n] * kissfftConfig->outbuf[n];
    out[2 * n] = complexOut.real() * 2;
    out[NF - 2 * n - 1] = -complexOut.imag() * 2;
  }

#elif defined USE_OWN_FFT

  fft_complex inbuf[NF / 2];
  fft_complex outbuf[NF / 2];

  // assume NF being 4 times an integer
  for (uint16_t n = 1; n < NF / 2; n += 2) {
    double buffer;
    buffer = in[n];
    in[n] = in[NF - n];
    in[NF - n] = buffer;
  }

  std::complex<double>* twiddle = (std::complex<double>*)dctIVconfig;
  for (uint16_t n = 0; n < NF / 2; n++) {
    double real = in[2 * n + 0];
    double imag = in[2 * n + 1];
    in[2 * n + 0] = real * twiddle[n].real() - imag * twiddle[n].imag();
    in[2 * n + 1] = real * twiddle[n].imag() + imag * twiddle[n].real();
  }

  for (uint16_t n = 0; n < NF / 2; n++) {
    inbuf[n].re = in[2 * n];
    inbuf[n].im = in[2 * n + 1];
  }

  fft_complex* actal_output = fft(false, inbuf, NF / 2, inbuf, outbuf);

  for (uint16_t n = 0; n < NF / 2; n++) {
    double real = actal_output[n].re;
    double imag = actal_output[n].im;
    out[2 * n + 0] = 2 * (real * twiddle[n].real() - imag * twiddle[n].imag());
    out[2 * n + 1] = 2 * (real * twiddle[n].imag() + imag * twiddle[n].real());
  }

  for (uint16_t n = 1; n < NF / 2; n += 2) {
    double buffer;
    buffer = out[n];
    out[n] = -out[NF - n];
    out[NF - n] = -buffer;
  }

#else
  DctIVDirectDbl(NF, in, out);
#endif
}
