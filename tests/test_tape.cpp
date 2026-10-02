/* SPDX-License-Identifier: Unlicense */

#include "tape_pcm.hpp"
#include "check.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace {

std::string temp_wav()
{
  return "/tmp/needle-test-" + std::to_string(static_cast<long long>(getpid())) + ".wav";
}

needle::TapePcm tone()
{
  needle::TapePcm pcm;
  pcm.rate = 1000;
  pcm.channels = 1;
  pcm.samples.resize(100);
  for (int i = 0; i < 100; ++i)
    pcm.samples[static_cast<size_t>(i)] = (i < 50) ? 0.5f : -0.25f;
  return pcm;
}

void put_u32(std::vector<uint8_t>& b, uint32_t v)
{
  for (int i = 0; i < 4; ++i)
    b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}

void put_u16(std::vector<uint8_t>& b, uint16_t v)
{
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}

void put_tag(std::vector<uint8_t>& b, const char* tag)
{
  b.insert(b.end(), tag, tag + 4);
}

// A plain WAV with an explicit nBlockAlign.
std::vector<uint8_t> plain_wav(uint16_t tag, uint16_t bits, uint16_t channels, uint16_t align,
                               const std::vector<uint8_t>& payload)
{
  std::vector<uint8_t> b;
  put_tag(b, "RIFF");
  put_u32(b, static_cast<uint32_t>(4 + 8 + 16 + 8 + payload.size()));
  put_tag(b, "WAVE");
  put_tag(b, "fmt ");
  put_u32(b, 16);
  put_u16(b, tag);
  put_u16(b, channels);
  put_u32(b, 8000);
  put_u32(b, 8000u * align);
  put_u16(b, align);
  put_u16(b, bits);
  put_tag(b, "data");
  put_u32(b, static_cast<uint32_t>(payload.size()));
  b.insert(b.end(), payload.begin(), payload.end());
  return b;
}

// A WAV with a WAVE_FORMAT_EXTENSIBLE fmt chunk whose SubFormat GUID carries `sub`.
std::vector<uint8_t> extensible_wav(uint16_t sub, uint16_t bits, uint16_t channels,
                                    const std::vector<uint8_t>& payload)
{
  static const uint8_t guid_tail[14] = {0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80,
                                        0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
  const uint16_t align = static_cast<uint16_t>(bits / 8 * channels);
  std::vector<uint8_t> b;
  put_tag(b, "RIFF");
  put_u32(b, static_cast<uint32_t>(4 + 8 + 40 + 8 + payload.size()));
  put_tag(b, "WAVE");
  put_tag(b, "fmt ");
  put_u32(b, 40);
  put_u16(b, 0xFFFE);
  put_u16(b, channels);
  put_u32(b, 48000);
  put_u32(b, 48000u * align);
  put_u16(b, align);
  put_u16(b, bits);
  put_u16(b, 22);
  put_u16(b, bits);
  put_u32(b, channels == 2 ? 3u : 4u);
  put_u16(b, sub);
  b.insert(b.end(), guid_tail, guid_tail + 14);
  put_tag(b, "data");
  put_u32(b, static_cast<uint32_t>(payload.size()));
  b.insert(b.end(), payload.begin(), payload.end());
  return b;
}

void write_bytes(const std::string& path, const std::vector<uint8_t>& b)
{
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

bool near(float a, float b)
{
  return std::fabs(a - b) < 0.002f;
}

}  // namespace

int main()
{
  const std::string path = temp_wav();
  std::string err;

  {
    needle::TapePcm pcm;
    CHECK(!needle::load_wav("/no/such/tape.wav", pcm, err));
    CHECK(!err.empty());
  }
  {
    needle::TapePcm pcm;
    CHECK(!needle::load_wav("/etc/hostname", pcm, err));
    CHECK(err.find("WAVE") != std::string::npos);
  }

  {
    needle::TapePcm pcm = tone();
    pcm.channels = 2;
    pcm.samples = {0.5f, -0.5f, 0.25f, -0.25f};
    needle::fx_reverse(pcm);
    CHECK(near(pcm.samples[0], 0.25f));
    CHECK(near(pcm.samples[1], -0.25f));
    CHECK(near(pcm.samples[2], 0.5f));
    CHECK(near(pcm.samples[3], -0.5f));
    needle::fx_reverse(pcm);
    CHECK(near(pcm.samples[0], 0.5f));
    CHECK(near(pcm.samples[1], -0.5f));
  }

  {
    needle::TapePcm pcm = tone();
    needle::fx_volume(pcm, 0.f);
    for (float s : pcm.samples)
      CHECK(s == 0.f);
    pcm.samples = {0.8f};
    needle::fx_volume(pcm, 10.f);
    CHECK(near(pcm.samples[0], 1.f));
    pcm.samples = {-0.8f};
    needle::fx_volume(pcm, 10.f);
    CHECK(near(pcm.samples[0], -1.f));
  }

  {
    needle::TapePcm pcm = tone();
    const auto before = pcm.samples;
    needle::fx_reverse(pcm);
    CHECK(near(pcm.samples.front(), before.back()));
    CHECK(near(pcm.samples.back(), before.front()));
    needle::fx_reverse(pcm);
    CHECK(pcm.samples.size() == before.size());
    for (size_t i = 0; i < before.size(); ++i)
      CHECK(near(pcm.samples[i], before[i]));
  }

  {
    needle::TapePcm pcm;
    pcm.rate = 10;
    pcm.channels = 1;
    pcm.samples = {1.f, 0.f, 0.f, 0.f, 0.f};
    needle::fx_echo(pcm);
    CHECK(near(pcm.samples[0], 1.f));
    CHECK(near(pcm.samples[1], 0.5f));
  }

  {
    needle::TapePcm pcm = tone();
    const size_t frames = pcm.samples.size();
    needle::fx_speed(pcm, 2.f);
    CHECK(pcm.samples.size() == frames / 2);
    needle::fx_speed(pcm, 0.f);
    CHECK(pcm.samples.size() == frames / 2);
  }

  {
    needle::TapePcm pcm = tone();
    const auto env = needle::pcm_envelope(pcm, 4);
    CHECK(env.size() == 4);
    CHECK(env[0] > 0.4);
    CHECK(env[3] > 0.2);
    const auto empty = needle::pcm_envelope(needle::TapePcm{}, 3);
    CHECK(empty.size() == 3);
    CHECK(empty[0] == 0.0);
  }

  {
    needle::TapePcm pcm = tone();
    CHECK(needle::save_wav(path, pcm, err));
    needle::TapePcm loaded;
    CHECK(needle::load_wav(path, loaded, err));
    CHECK(loaded.rate == pcm.rate);
    CHECK(loaded.channels == 1);
    CHECK(loaded.samples.size() == pcm.samples.size());
    for (size_t i = 0; i < pcm.samples.size(); ++i)
      CHECK(near(loaded.samples[i], pcm.samples[i]));
    std::remove(path.c_str());
  }

  {
    // A pre-data chunk whose size wraps 8 + sz in 32 bits must not hang the scan.
    std::vector<uint8_t> b;
    put_tag(b, "RIFF");
    put_u32(b, 4 + 8 + 8);
    put_tag(b, "WAVE");
    put_tag(b, "JUNK");
    put_u32(b, 0xFFFFFFF8u);
    put_u32(b, 0);
    put_u32(b, 0);
    write_bytes(path, b);
    needle::TapePcm pcm;
    CHECK(!needle::load_wav(path, pcm, err));
    std::remove(path.c_str());
  }

  {
    // 16-bit PCM sizes that do not fit the RIFF header must be refused, not wrapped.
    uint32_t bytes = 0;
    CHECK(needle::wav_data_bytes(100, 1, bytes));
    CHECK(bytes == 200);
    CHECK(needle::wav_data_bytes(100, 2, bytes));
    CHECK(bytes == 400);
    const size_t max_samples = (0xFFFFFFFFull - 36) / 2;
    CHECK(needle::wav_data_bytes(max_samples, 1, bytes));
    CHECK(bytes == max_samples * 2);
    CHECK(!needle::wav_data_bytes(max_samples + 1, 1, bytes));
    CHECK(!needle::wav_data_bytes(0x80000000ull, 1, bytes));
    CHECK(!needle::wav_data_bytes(0x40000000ull, 2, bytes));
    CHECK(!needle::wav_data_bytes(10, 0, bytes));
  }

  {
    // ffmpeg writes 24-bit as WAVE_FORMAT_EXTENSIBLE with the PCM SubFormat.
    // Stereo frames: (+0.5, -0.5), (-1.0, 0.25).
    const std::vector<uint8_t> payload = {0x00, 0x00, 0x40, 0x00, 0x00, 0xC0,
                                          0x00, 0x00, 0x80, 0x00, 0x00, 0x20};
    write_bytes(path, extensible_wav(1, 24, 2, payload));
    needle::TapePcm pcm;
    CHECK(needle::load_wav(path, pcm, err));
    CHECK(pcm.rate == 48000);
    CHECK(pcm.channels == 2);
    CHECK(pcm.samples.size() == 4);
    if (pcm.samples.size() == 4) {
      CHECK(near(pcm.samples[0], 0.5f));
      CHECK(near(pcm.samples[1], -0.5f));
      CHECK(near(pcm.samples[2], -1.f));
      CHECK(near(pcm.samples[3], 0.25f));
    }
    std::remove(path.c_str());
  }
  {
    // Extensible with the IEEE float SubFormat.
    std::vector<uint8_t> payload;
    const float v = -0.75f;
    uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    put_u32(payload, u);
    write_bytes(path, extensible_wav(3, 32, 1, payload));
    needle::TapePcm pcm;
    CHECK(needle::load_wav(path, pcm, err));
    CHECK(pcm.samples.size() == 1 && near(pcm.samples[0], -0.75f));
    std::remove(path.c_str());
  }
  {
    // An extensible SubFormat that is neither PCM nor float is still refused.
    write_bytes(path, extensible_wav(2, 16, 1, {0, 0}));
    needle::TapePcm pcm;
    CHECK(!needle::load_wav(path, pcm, err));
    std::remove(path.c_str());
  }

  {
    // The writer's own empty tape (44 bytes, data size 0 at EOF) must round-trip.
    needle::TapePcm empty;
    empty.rate = 22050;
    empty.channels = 2;
    CHECK(needle::save_wav(path, empty, err));
    needle::TapePcm loaded;
    loaded.samples = {0.5f};
    CHECK(needle::load_wav(path, loaded, err));
    CHECK(loaded.rate == 22050);
    CHECK(loaded.channels == 2);
    CHECK(loaded.samples.empty());
    std::remove(path.c_str());
  }

  {
    // 24-bit mono in a 4-byte container: nBlockAlign 4, one pad byte per frame.
    const std::vector<uint8_t> payload = {0x00, 0x00, 0x40, 0xEE, 0x00, 0x00, 0xC0, 0xEE,
                                          0x00, 0x00, 0x20, 0xEE};
    write_bytes(path, plain_wav(1, 24, 1, 4, payload));
    needle::TapePcm pcm;
    CHECK(needle::load_wav(path, pcm, err));
    CHECK(pcm.samples.size() == 3);
    if (pcm.samples.size() == 3) {
      CHECK(near(pcm.samples[0], 0.5f));
      CHECK(near(pcm.samples[1], -0.5f));
      CHECK(near(pcm.samples[2], 0.25f));
    }
    std::remove(path.c_str());
  }
  {
    // Packed 24-bit (nBlockAlign 3) still decodes.
    const std::vector<uint8_t> payload = {0x00, 0x00, 0x40, 0x00, 0x00, 0xC0};
    write_bytes(path, plain_wav(1, 24, 1, 3, payload));
    needle::TapePcm pcm;
    CHECK(needle::load_wav(path, pcm, err));
    CHECK(pcm.samples.size() == 2);
    if (pcm.samples.size() == 2) {
      CHECK(near(pcm.samples[0], 0.5f));
      CHECK(near(pcm.samples[1], -0.5f));
    }
    std::remove(path.c_str());
  }

  return suite_test::done("tape");
}
