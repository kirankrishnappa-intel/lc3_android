/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <fuzzer/FuzzedDataProvider.h>

#include "include/lc3.h"


extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider fdp(data, size);

  int dt_us = fdp.PickValueInArray({10000, 7500});
  int sr_hz = fdp.PickValueInArray({8000, 16000, 24000, 32000, /*44100,*/ 48000});
  unsigned enc_size = lc3_encoder_size(dt_us, sr_hz);
  uint16_t output_byte_count = fdp.ConsumeIntegralInRange(20, 400);
  uint16_t num_frames = lc3_frame_samples(dt_us, sr_hz);

  if (fdp.remaining_bytes() < num_frames * 2) {
    return 0;
  }

  std::vector<uint16_t> input_frames(num_frames);
  fdp.ConsumeData(input_frames.data(),
                  input_frames.size() * 2 /* each frame is 2 bytes */);

  void* lc3_encoder_mem = nullptr;
  lc3_encoder_mem = malloc(enc_size);
  lc3_encoder_t  lc3_encoder = lc3_setup_encoder(dt_us, sr_hz, lc3_encoder_mem);

  std::vector<uint8_t> output(output_byte_count);
  lc3_encode(lc3_encoder, (const int16_t*)input_frames.data(), 1,
                     output.size(), output.data());

  free(lc3_encoder_mem);
  lc3_encoder_mem = nullptr;
  return 0;
}