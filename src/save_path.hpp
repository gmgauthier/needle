/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <string>

namespace needle {

// The path Save As writes. A name that already ends in an audio extension is
// unchanged. Otherwise the selected filter's extension is appended.
std::string with_filter_ext(const std::string& path, const std::string& filter_name);

// True when the filter extension moved the save onto a different file that
// already exists. The chooser only confirmed `chosen`.
bool save_needs_overwrite_prompt(const std::string& chosen, const std::string& final_path);

}  // namespace needle
