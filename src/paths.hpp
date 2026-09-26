/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace needle {

std::string find_data_file(const std::string& relative);
std::string cache_dir();
std::string tape_path();

}  // namespace needle
