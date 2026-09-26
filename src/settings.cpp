/* SPDX-License-Identifier: Unlicense */

#include "settings.hpp"

#include <glib.h>
#include <glibmm/fileutils.h>
#include <glibmm/keyfile.h>
#include <glibmm/miscutils.h>

namespace needle {
namespace {

std::string config_path()
{
  const std::string dir = Glib::build_filename(Glib::get_user_config_dir(), "needle");
  g_mkdir_with_parents(dir.c_str(), 0700);
  return Glib::build_filename(dir, "needle.ini");
}

}  // namespace

void Settings::load()
{
  Glib::KeyFile kf;
  try {
    kf.load_from_file(config_path());
  } catch (const Glib::Error&) {
    return;
  }
  try {
    if (kf.has_key("audio", "device"))
      audio_device = kf.get_string("audio", "device");
  } catch (const Glib::Error&) {
  }
  try {
    if (kf.has_key("files", "last_folder"))
      last_folder = kf.get_string("files", "last_folder");
  } catch (const Glib::Error&) {
  }
}

void Settings::save() const
{
  Glib::KeyFile kf;
  try {
    kf.load_from_file(config_path());
  } catch (const Glib::Error&) {
  }
  kf.set_string("audio", "device", audio_device);
  kf.set_string("files", "last_folder", last_folder);
  try {
    kf.save_to_file(config_path());
  } catch (const Glib::Error&) {
  }
}

}  // namespace needle
