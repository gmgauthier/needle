/* SPDX-License-Identifier: Unlicense */

#include "tape_pcm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace needle {
namespace {

uint16_t ru16(const uint8_t* p)
{
  return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

uint32_t ru32(const uint8_t* p)
{
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void wu16(std::vector<uint8_t>& b, uint16_t v)
{
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}

void wu32(std::vector<uint8_t>& b, uint32_t v)
{
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
  b.push_back(static_cast<uint8_t>(v >> 16));
  b.push_back(static_cast<uint8_t>(v >> 24));
}

float clamp1(float x)
{
  if (x > 1.f)
    return 1.f;
  if (x < -1.f)
    return -1.f;
  return x;
}

}  // namespace

bool load_wav(const std::string& path, TapePcm& out, std::string& err)
{
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    err = "Could not read tape";
    return false;
  }
  std::vector<uint8_t> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (buf.size() < 12 || std::memcmp(buf.data(), "RIFF", 4) != 0 ||
      std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
    err = "Tape is not a WAVE file";
    return false;
  }
  uint16_t format = 1;
  uint16_t channels = 1;
  uint32_t rate = 44100;
  uint16_t bits = 16;
  size_t data_off = 0;
  size_t data_len = 0;
  size_t i = 12;
  while (i + 8 <= buf.size()) {
    const uint32_t sz = ru32(buf.data() + i + 4);
    if (std::memcmp(buf.data() + i, "fmt ", 4) == 0 && sz >= 16 && i + 8 + 16 <= buf.size()) {
      format = ru16(buf.data() + i + 8);
      channels = ru16(buf.data() + i + 10);
      rate = ru32(buf.data() + i + 12);
      bits = ru16(buf.data() + i + 22);
      // WAVE_FORMAT_EXTENSIBLE: the real tag is the first two bytes of the SubFormat GUID.
      static const uint8_t guid_tail[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                                            0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
      if (format == 0xFFFE && sz >= 40 && i + 8 + 40 <= buf.size() &&
          std::memcmp(buf.data() + i + 8 + 26, guid_tail, sizeof guid_tail) == 0)
        format = ru16(buf.data() + i + 8 + 24);
    } else if (std::memcmp(buf.data() + i, "data", 4) == 0) {
      data_off = i + 8;
      data_len = sz;
      break;
    }
    // Advance in size_t: 8 + sz wraps in 32 bits for sizes near 4 GiB.
    const size_t step = 8 + static_cast<size_t>(sz) + (sz & 1u);
    if (step > buf.size() - i)
      break;
    i += step;
  }
  if (!data_off || channels < 1 || rate < 1 || data_off >= buf.size()) {
    err = "WAVE header is incomplete";
    return false;
  }
  if (data_off + data_len > buf.size())
    data_len = buf.size() - data_off;
  const int ch = static_cast<int>(channels);
  const int bps = static_cast<int>(bits);
  const int block = (bps / 8) * ch;
  if (block <= 0) {
    err = "Unsupported WAVE format";
    return false;
  }
  const size_t frames = data_len / static_cast<size_t>(block);
  out.rate = static_cast<int>(rate);
  out.channels = ch;
  out.samples.assign(frames * static_cast<size_t>(ch), 0.f);
  const uint8_t* d = buf.data() + data_off;
  for (size_t f = 0; f < frames; ++f) {
    const uint8_t* fr = d + f * static_cast<size_t>(block);
    for (int c = 0; c < ch; ++c) {
      float s = 0.f;
      if (format == 3 && bps == 32) {
        uint32_t u = ru32(fr + c * 4);
        std::memcpy(&s, &u, 4);
      } else if (format == 1 && bps == 8) {
        s = (static_cast<int>(fr[c]) - 128) / 128.f;
      } else if (format == 1 && bps == 16) {
        const int16_t v = static_cast<int16_t>(ru16(fr + c * 2));
        s = v / 32768.f;
      } else if (format == 1 && bps == 24) {
        const int32_t v =
            static_cast<int32_t>(fr[c * 3] | (fr[c * 3 + 1] << 8) | (fr[c * 3 + 2] << 16));
        const int32_t signed24 = (v & 0x800000) ? (v | ~0xFFFFFF) : v;
        s = signed24 / 8388608.f;
      } else if (format == 1 && bps == 32) {
        const int32_t v = static_cast<int32_t>(ru32(fr + c * 4));
        s = v / 2147483648.f;
      } else {
        err = "Unsupported WAVE format";
        return false;
      }
      out.samples[f * static_cast<size_t>(ch) + static_cast<size_t>(c)] = clamp1(s);
    }
  }
  return true;
}

bool wav_data_bytes(size_t frames, int channels, uint32_t& bytes)
{
  if (channels < 1)
    return false;
  // The RIFF size is 36 + data and must also fit in 32 bits.
  const uint64_t limit = (0xFFFFFFFFull - 36) / 2;
  const uint64_t ch = static_cast<uint64_t>(channels);
  if (frames > limit / ch)
    return false;
  bytes = static_cast<uint32_t>(static_cast<uint64_t>(frames) * ch * 2);
  return true;
}

bool save_wav(const std::string& path, const TapePcm& pcm, std::string& err)
{
  if (pcm.channels < 1 || pcm.rate < 1) {
    err = "Invalid tape";
    return false;
  }
  const int ch = pcm.channels;
  const size_t frames = pcm.samples.size() / static_cast<size_t>(ch);
  uint32_t data_bytes = 0;
  if (!wav_data_bytes(frames, ch, data_bytes)) {
    err = "Sound is too long for a WAV tape";
    return false;
  }
  std::vector<uint8_t> b;
  b.reserve(44 + data_bytes);
  b.insert(b.end(), {'R', 'I', 'F', 'F'});
  wu32(b, 36 + data_bytes);
  b.insert(b.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  wu32(b, 16);
  wu16(b, 1);
  wu16(b, static_cast<uint16_t>(ch));
  wu32(b, static_cast<uint32_t>(pcm.rate));
  wu32(b, static_cast<uint32_t>(pcm.rate * ch * 2));
  wu16(b, static_cast<uint16_t>(ch * 2));
  wu16(b, 16);
  b.insert(b.end(), {'d', 'a', 't', 'a'});
  wu32(b, data_bytes);
  for (size_t i = 0; i < frames * static_cast<size_t>(ch); ++i) {
    float s = clamp1(i < pcm.samples.size() ? pcm.samples[i] : 0.f);
    int v = static_cast<int>(std::lround(s * 32767.f));
    if (v > 32767)
      v = 32767;
    if (v < -32768)
      v = -32768;
    wu16(b, static_cast<uint16_t>(static_cast<int16_t>(v)));
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    err = "Could not write tape";
    return false;
  }
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
  return static_cast<bool>(out);
}

void fx_volume(TapePcm& pcm, float gain)
{
  for (float& s : pcm.samples)
    s = clamp1(s * gain);
}

void fx_speed(TapePcm& pcm, float factor)
{
  if (factor <= 0.f || pcm.channels < 1)
    return;
  const int ch = pcm.channels;
  const size_t frames = pcm.samples.size() / static_cast<size_t>(ch);
  if (frames == 0)
    return;
  size_t out_frames = static_cast<size_t>(std::lround(static_cast<double>(frames) / factor));
  if (out_frames < 1)
    out_frames = 1;
  std::vector<float> out(out_frames * static_cast<size_t>(ch));
  for (size_t i = 0; i < out_frames; ++i) {
    const double src = static_cast<double>(i) * factor;
    size_t a = static_cast<size_t>(src);
    if (a >= frames)
      a = frames - 1;
    size_t b = a + 1;
    if (b >= frames)
      b = frames - 1;
    const float t = static_cast<float>(src - static_cast<double>(a));
    for (int c = 0; c < ch; ++c) {
      const float sa = pcm.samples[a * static_cast<size_t>(ch) + static_cast<size_t>(c)];
      const float sb = pcm.samples[b * static_cast<size_t>(ch) + static_cast<size_t>(c)];
      out[i * static_cast<size_t>(ch) + static_cast<size_t>(c)] = sa + (sb - sa) * t;
    }
  }
  pcm.samples.swap(out);
}

void fx_echo(TapePcm& pcm)
{
  const int ch = pcm.channels;
  if (ch < 1)
    return;
  const size_t frames = pcm.samples.size() / static_cast<size_t>(ch);
  size_t delay = static_cast<size_t>(pcm.rate) / 10;
  if (delay < 1)
    delay = 1;
  const auto orig = pcm.samples;
  for (size_t i = delay; i < frames; ++i) {
    for (int c = 0; c < ch; ++c) {
      const size_t di = i * static_cast<size_t>(ch) + static_cast<size_t>(c);
      const size_t si = (i - delay) * static_cast<size_t>(ch) + static_cast<size_t>(c);
      pcm.samples[di] = clamp1(orig[di] + 0.5f * orig[si]);
    }
  }
}

void fx_reverse(TapePcm& pcm)
{
  const int ch = pcm.channels;
  if (ch < 1)
    return;
  const size_t frames = pcm.samples.size() / static_cast<size_t>(ch);
  for (size_t i = 0; i < frames / 2; ++i) {
    const size_t j = frames - 1 - i;
    for (int c = 0; c < ch; ++c)
      std::swap(pcm.samples[i * static_cast<size_t>(ch) + static_cast<size_t>(c)],
                pcm.samples[j * static_cast<size_t>(ch) + static_cast<size_t>(c)]);
  }
}

std::vector<double> pcm_envelope(const TapePcm& pcm, int buckets)
{
  std::vector<double> env(static_cast<size_t>(std::max(1, buckets)), 0.0);
  const int ch = std::max(1, pcm.channels);
  const size_t frames = pcm.samples.size() / static_cast<size_t>(ch);
  if (frames == 0)
    return env;
  const size_t n = env.size();
  for (size_t i = 0; i < frames; ++i) {
    float peak = 0.f;
    for (int c = 0; c < ch; ++c)
      peak = std::max(peak,
                      std::fabs(pcm.samples[i * static_cast<size_t>(ch) + static_cast<size_t>(c)]));
    const size_t b = i * n / frames;
    if (b < n)
      env[b] = std::max(env[b], static_cast<double>(peak));
  }
  return env;
}

}  // namespace needle
