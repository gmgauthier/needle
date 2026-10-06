/* SPDX-License-Identifier: Unlicense */

#include "recorder.hpp"
#include "paths.hpp"
#include "tape_pcm.hpp"
#include "check.hpp"

#include <glib.h>
#include <glib/gstdio.h>
#include <gst/gst.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

std::string slurp(const std::string& path)
{
  std::ifstream in(path, std::ios::binary);
  if (!in)
    return {};
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void spit(const std::string& path, const std::string& bytes)
{
  std::ofstream out(path, std::ios::binary);
  out << bytes;
}

needle::TapePcm tone()
{
  needle::TapePcm pcm;
  pcm.rate = 8000;
  pcm.channels = 1;
  pcm.samples.resize(800);
  for (size_t i = 0; i < pcm.samples.size(); ++i)
    pcm.samples[i] = (i % 16 < 8) ? 0.5f : -0.5f;
  return pcm;
}

}  // namespace

int main(int argc, char** argv)
{
  gchar* tmp = g_dir_make_tmp("needle-rec-XXXXXX", nullptr);
  CHECK(tmp != nullptr);
  if (!tmp)
    return suite_test::done("recorder");
  const std::string dir = tmp;
  g_free(tmp);
  g_setenv("XDG_CACHE_HOME", dir.c_str(), TRUE);
  gst_init(&argc, &argv);

  const std::string take = dir + "/take.wav";
  std::string err;
  CHECK(needle::save_wav(take, tone(), err));
  const std::string take_bytes = slurp(take);
  CHECK(!take_bytes.empty());

  {
    // Rec with a source that cannot start must leave the loaded tape alone.
    needle::Recorder rec;
    CHECK(rec.open_file(take));
    CHECK(rec.has_tape());
    const std::string tape = needle::tape_path();
    CHECK(tape.rfind(dir, 0) == 0);
    CHECK(slurp(tape) == take_bytes);

    needle::AudioDevice bogus;
    bogus.id = "needle-test-no-such-source";
    bogus.label = "None";
    bogus.backend = needle::AudioBackend::pulse;
    rec.set_input(bogus);
    // The source may fail at once, or start and then post an error on the bus.
    const bool started = rec.record();
    for (int i = 0; started && rec.state() == needle::RecState::recording && i < 500; ++i) {
      while (g_main_context_iteration(nullptr, FALSE)) {
      }
      g_usleep(10000);
    }
    CHECK(rec.has_tape());
    CHECK(rec.state() == needle::RecState::stopped);
    CHECK(!rec.dirty());
    CHECK(slurp(tape) == take_bytes);
  }

  {
    // A failed non-WAV Open must not truncate the tape the window still shows.
    needle::Recorder rec;
    CHECK(rec.open_file(take));
    const std::string tape = needle::tape_path();
    const std::string bad_flac = dir + "/broken.flac";
    spit(bad_flac, "fLaC this is not a decodable stream at all");
    CHECK(!rec.open_file(bad_flac));
    CHECK(rec.has_tape());
    CHECK(slurp(tape) == take_bytes);
  }

  {
    // A compressed Save As that fails must leave an existing destination alone.
    const std::string junk_wav = dir + "/junk.wav";
    spit(junk_wav, "RIFF0000WAVEnot really a wave file");
    needle::Recorder rec;
    CHECK(rec.open_file(junk_wav));
    const std::string dest = dir + "/keep.flac";
    const std::string keep = "previously saved flac bytes";
    spit(dest, keep);
    CHECK(!rec.save_as(dest));
    CHECK(slurp(dest) == keep);
    CHECK(!g_file_test((dest + ".part").c_str(), G_FILE_TEST_EXISTS));
  }

  if (GstElementFactory* enc = gst_element_factory_find("flacenc")) {
    gst_object_unref(enc);
    // A good compressed Save As lands at the destination, with no side file left.
    needle::Recorder rec;
    CHECK(rec.open_file(take));
    const std::string dest = dir + "/good.flac";
    spit(dest, "old");
    CHECK(rec.save_as(dest));
    CHECK(slurp(dest).rfind("fLaC", 0) == 0);
    CHECK(!g_file_test((dest + ".part").c_str(), G_FILE_TEST_EXISTS));
    CHECK(rec.open_file(dest));
    CHECK(rec.has_tape());

    // A stereo file that is not 44.1 kHz stays that way, and Save leaves it untouched.
    needle::TapePcm wide;
    wide.rate = 22050;
    wide.channels = 2;
    wide.samples.resize(2205 * 2);
    for (size_t i = 0; i < wide.samples.size(); i += 2) {
      wide.samples[i] = (i % 40 < 20) ? 0.4f : -0.4f;
      wide.samples[i + 1] = (i % 28 < 14) ? 0.2f : -0.2f;
    }
    const std::string src = dir + "/wide.wav";
    CHECK(needle::save_wav(src, wide, err));
    CHECK(rec.open_file(src));
    const std::string flac = dir + "/wide.flac";
    CHECK(rec.save_as(flac));
    const std::string flac_bytes = slurp(flac);
    CHECK(flac_bytes.rfind("fLaC", 0) == 0);
    CHECK(rec.open_file(flac));
    CHECK(!rec.dirty());
    needle::TapePcm opened;
    CHECK(needle::load_wav(needle::tape_path(), opened, err));
    CHECK(opened.rate == 22050);
    CHECK(opened.channels == 2);
    bool sides_differ = false;
    for (size_t i = 0; i + 1 < opened.samples.size(); i += 2) {
      const float d = opened.samples[i] - opened.samples[i + 1];
      if (d > 0.05f || d < -0.05f)
        sides_differ = true;
    }
    CHECK(sides_differ);
    CHECK(rec.save(flac));
    CHECK(slurp(flac) == flac_bytes);
    CHECK(rec.apply_effect(needle::TapeEffect::reverse));
    CHECK(rec.dirty());
    CHECK(rec.save(flac));
    CHECK(slurp(flac) != flac_bytes);
    CHECK(slurp(flac).rfind("fLaC", 0) == 0);
  }

  return suite_test::done("recorder");
}
