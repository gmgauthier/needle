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

  return suite_test::done("recorder");
}
