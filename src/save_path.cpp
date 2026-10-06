/* SPDX-License-Identifier: Unlicense */

#include "save_path.hpp"

#include <glibmm.h>

#include <sys/stat.h>

namespace needle {
namespace {

bool has_audio_ext(const std::string& path)
{
  const Glib::ustring low = Glib::ustring(path).lowercase();
  return low.size() >= 4 &&
         (low.substr(low.size() - 4) == ".wav" || low.substr(low.size() - 4) == ".ogg" ||
          low.substr(low.size() - 4) == ".oga" || low.substr(low.size() - 4) == ".mp3" ||
          (low.size() >= 5 && low.substr(low.size() - 5) == ".flac"));
}

}  // namespace

std::string with_filter_ext(const std::string& path, const std::string& filter_name)
{
  if (has_audio_ext(path))
    return path;
  std::string ext = ".wav";
  const Glib::ustring n(filter_name);
  if (n.find("FLAC") != Glib::ustring::npos)
    ext = ".flac";
  else if (n.find("Ogg") != Glib::ustring::npos)
    ext = ".ogg";
  else if (n.find("MP3") != Glib::ustring::npos)
    ext = ".mp3";
  return path + ext;
}

bool save_needs_overwrite_prompt(const std::string& chosen, const std::string& final_path)
{
  if (final_path == chosen)
    return false;
  struct stat st;
  return ::stat(final_path.c_str(), &st) == 0;
}

}  // namespace needle
