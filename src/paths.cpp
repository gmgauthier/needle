/* SPDX-License-Identifier: Unlicense */

#include "paths.hpp"
#include "config.hpp"

#include <glib.h>
#include <glibmm.h>

#include <vector>

namespace needle {
namespace {

bool exists_regular(const std::string& path)
{
  return Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR);
}

}  // namespace

std::string find_data_file(const std::string& relative)
{
  std::vector<std::string> roots;

  if (const char* env = g_getenv("NEEDLE_DATA"))
    roots.emplace_back(env);

  if (const char* appdir = g_getenv("APPDIR"))
    roots.emplace_back(Glib::build_filename(appdir, "usr/share/needle"));

  roots.emplace_back(SOURCE_ROOT);
  roots.emplace_back(std::string(SOURCE_ROOT) + "/data");
  roots.emplace_back(DATADIR);

  for (const auto& root : roots) {
    const std::string candidate = Glib::build_filename(root, relative);
    if (exists_regular(candidate))
      return candidate;
  }
  return {};
}

std::string cache_dir()
{
  const std::string dir = Glib::build_filename(Glib::get_user_cache_dir(), "needle");
  g_mkdir_with_parents(dir.c_str(), 0700);
  return dir;
}

std::string tape_path()
{
  return Glib::build_filename(cache_dir(), "tape.wav");
}

}  // namespace needle
