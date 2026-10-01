/* SPDX-License-Identifier: Unlicense */

#include "tape_pcm.hpp"
#include "check.hpp"

#include <cmath>
#include <cstdlib>
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

  return suite_test::done("tape");
}
