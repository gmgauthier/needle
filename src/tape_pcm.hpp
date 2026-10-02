/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace needle {

struct TapePcm {
  int rate = 44100;
  int channels = 1;
  std::vector<float> samples;
};

bool load_wav(const std::string& path, TapePcm& out, std::string& err);
// 16-bit PCM data size for frames x channels. False when the RIFF header cannot hold it.
bool wav_data_bytes(size_t frames, int channels, uint32_t& bytes);
bool save_wav(const std::string& path, const TapePcm& pcm, std::string& err);

void fx_volume(TapePcm& pcm, float gain);
void fx_speed(TapePcm& pcm, float factor);
void fx_echo(TapePcm& pcm);
void fx_reverse(TapePcm& pcm);

std::vector<double> pcm_envelope(const TapePcm& pcm, int buckets);

}  // namespace needle
