/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <gtkmm.h>

#include <deque>

namespace needle {

class WaveView : public Gtk::DrawingArea {
 public:
  WaveView();

  void clear();
  void push_level(double amplitude);
  void set_playing_progress(double t);  // 0–1 while playing a saved tape

 protected:
  bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;

 private:
  std::deque<double> samples_;
  double play_t_ = -1;
};

}  // namespace needle
