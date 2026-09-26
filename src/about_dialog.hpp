/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

namespace needle {

class AboutDialog : public Gtk::Dialog {
 public:
  explicit AboutDialog(Gtk::Window& parent);
};

}  // namespace needle
