/* SPDX-License-Identifier: Unlicense */

#include "wave_view.hpp"

namespace needle {
namespace {

constexpr int kMaxSamples = 240;

}  // namespace

WaveView::WaveView()
{
  set_size_request(340, 88);
  get_style_context()->add_class("needle-wave");
}

void WaveView::clear()
{
  samples_.clear();
  play_t_ = -1;
  queue_draw();
}

void WaveView::push_level(double amplitude)
{
  play_t_ = -1;
  if (amplitude < 0)
    amplitude = 0;
  if (amplitude > 1)
    amplitude = 1;
  samples_.push_back(amplitude);
  while (static_cast<int>(samples_.size()) > kMaxSamples)
    samples_.pop_front();
  queue_draw();
}

void WaveView::set_playing_progress(double t)
{
  play_t_ = t;
  queue_draw();
}

bool WaveView::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
  const int w = get_allocated_width();
  const int h = get_allocated_height();
  cr->set_source_rgb(0, 0, 0);
  cr->rectangle(0, 0, w, h);
  cr->fill();

  cr->set_source_rgb(0, 0.35, 0);
  cr->set_line_width(1);
  cr->move_to(0, h * 0.5);
  cr->line_to(w, h * 0.5);
  cr->stroke();

  cr->set_source_rgb(0.2, 1.0, 0.2);
  cr->set_line_width(1.5);

  if (samples_.empty()) {
    cr->move_to(0, h * 0.5);
    cr->line_to(w, h * 0.5);
    cr->stroke();
    return true;
  }

  const int n = static_cast<int>(samples_.size());
  for (int i = 0; i < n; ++i) {
    const double x = n == 1 ? 0 : (static_cast<double>(i) / (n - 1)) * (w - 1);
    const double amp = samples_[static_cast<size_t>(i)];
    const double y = h * 0.5 - amp * (h * 0.45);
    if (i == 0)
      cr->move_to(x, y);
    else
      cr->line_to(x, y);
  }
  cr->stroke();

  if (play_t_ >= 0) {
    const double x = play_t_ * (w - 1);
    cr->set_source_rgb(1, 1, 0.2);
    cr->set_line_width(1);
    cr->move_to(x, 0);
    cr->line_to(x, h);
    cr->stroke();
  }
  return true;
}

}  // namespace needle
