/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace needle {

struct Settings {
  std::string audio_device = "default";
  std::string last_folder;

  void load();
  void save() const;
};

}  // namespace needle
