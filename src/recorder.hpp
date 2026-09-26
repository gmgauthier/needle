/* SPDX-License-Identifier: Unlicense */

#pragma once

#include <glibmm.h>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include "audio_devices.hpp"

#include <gst/gst.h>

#include <string>

namespace needle {

enum class RecState { empty, stopped, recording, playing };

class Recorder {
 public:
  Recorder();
  ~Recorder();

  RecState state() const
  {
    return state_;
  }
  bool dirty() const
  {
    return dirty_;
  }
  bool has_tape() const
  {
    return has_tape_;
  }
  const std::string& path() const
  {
    return path_;
  }
  gint64 position_ns() const
  {
    return position_ns_;
  }
  gint64 duration_ns() const
  {
    return duration_ns_;
  }

  void set_input(const AudioDevice& device);
  const AudioDevice& input() const
  {
    return input_;
  }

  bool new_tape();
  bool open_file(const std::string& path);
  bool save_as(const std::string& path);
  bool record();
  bool play();
  bool stop();
  bool seek(gint64 ns);

  sigc::signal<void, RecState>& signal_state()
  {
    return signal_state_;
  }
  sigc::signal<void, double>& signal_level()
  {
    return signal_level_;
  }
  sigc::signal<void, gint64, gint64>& signal_position()
  {
    return signal_position_;
  }
  sigc::signal<void, Glib::ustring>& signal_error()
  {
    return signal_error_;
  }

 private:
  void tear_pipeline();
  bool start_pipeline(const std::string& desc, const char* sink_name, const std::string& location);
  static gboolean on_bus(GstBus* bus, GstMessage* msg, gpointer data);
  bool on_tick();
  void set_state(RecState s);
  void query_times();
  void pull_wave();
  void probe_duration();
  bool transcode(const std::string& desc, const std::string& in_path, const std::string& out_path,
                 const char* fail);

  AudioDevice input_;
  GstElement* pipeline_ = nullptr;
  guint bus_watch_ = 0;
  RecState state_ = RecState::empty;
  bool dirty_ = false;
  bool has_tape_ = false;
  std::string path_;
  gint64 position_ns_ = 0;
  gint64 duration_ns_ = 0;
  sigc::connection tick_;
  sigc::signal<void, RecState> signal_state_;
  sigc::signal<void, double> signal_level_;
  sigc::signal<void, gint64, gint64> signal_position_;
  sigc::signal<void, Glib::ustring> signal_error_;
};

}  // namespace needle
