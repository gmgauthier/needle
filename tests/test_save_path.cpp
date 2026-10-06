/* SPDX-License-Identifier: Unlicense */

#include "save_path.hpp"
#include "check.hpp"

#include <cstdio>
#include <fstream>
#include <string>
#include <unistd.h>

int main()
{
  using needle::save_needs_overwrite_prompt;
  using needle::with_filter_ext;

  CHECK(with_filter_ext("/tmp/take", "FLAC (*.flac)") == "/tmp/take.flac");
  CHECK(with_filter_ext("/tmp/take", "Ogg Vorbis (*.ogg)") == "/tmp/take.ogg");
  CHECK(with_filter_ext("/tmp/take", "MP3 (*.mp3)") == "/tmp/take.mp3");
  CHECK(with_filter_ext("/tmp/take", "Wave (*.wav)") == "/tmp/take.wav");
  CHECK(with_filter_ext("/tmp/take", "Audio files") == "/tmp/take.wav");
  CHECK(with_filter_ext("/tmp/take.wav", "FLAC (*.flac)") == "/tmp/take.wav");
  CHECK(with_filter_ext("/tmp/take.FLAC", "Wave (*.wav)") == "/tmp/take.FLAC");
  CHECK(with_filter_ext("/tmp/take.oga", "MP3 (*.mp3)") == "/tmp/take.oga");

  const std::string typed = "/tmp/needle-save-" + std::to_string(static_cast<long long>(getpid()));
  const std::string existing = typed + ".flac";
  std::remove(existing.c_str());

  CHECK(!save_needs_overwrite_prompt(typed, with_filter_ext(typed, "FLAC (*.flac)")));

  std::ofstream(existing) << "x";
  CHECK(save_needs_overwrite_prompt(typed, with_filter_ext(typed, "FLAC (*.flac)")));
  CHECK(!save_needs_overwrite_prompt(existing, with_filter_ext(existing, "FLAC (*.flac)")));

  std::remove(existing.c_str());
  return suite_test::done("save_path");
}
