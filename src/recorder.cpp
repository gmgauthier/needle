/* SPDX-License-Identifier: Unlicense */

#include "recorder.hpp"
#include "paths.hpp"

#include <glib/gstdio.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/miscutils.h>
#include <gst/app/gstappsink.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace needle {
namespace {

bool copy_file(const std::string& from, const std::string& to)
{
  gchar* contents = nullptr;
  gsize len = 0;
  GError* err = nullptr;
  if (!g_file_get_contents(from.c_str(), &contents, &len, &err)) {
    if (err)
      g_error_free(err);
    return false;
  }
  const gboolean ok = g_file_set_contents(to.c_str(), contents, static_cast<gssize>(len), &err);
  g_free(contents);
  if (!ok) {
    if (err)
      g_error_free(err);
    return false;
  }
  return true;
}

}  // namespace

Recorder::Recorder()
{
  input_.id = "default";
  input_.label = "Default";
  input_.backend = AudioBackend::system_default;
}

void Recorder::set_input(const AudioDevice& device)
{
  input_ = device;
}

Recorder::~Recorder()
{
  tear_pipeline();
}

void Recorder::tear_pipeline()
{
  if (tick_.connected())
    tick_.disconnect();
  if (bus_watch_) {
    g_source_remove(bus_watch_);
    bus_watch_ = 0;
  }
  if (pipeline_) {
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
  }
}

void Recorder::set_state(RecState s)
{
  state_ = s;
  signal_state_.emit(state_);
}

bool Recorder::start_pipeline(const std::string& desc, const char* sink_name,
                              const std::string& location)
{
  tear_pipeline();
  GError* err = nullptr;
  pipeline_ = gst_parse_launch(desc.c_str(), &err);
  if (!pipeline_) {
    Glib::ustring msg = err && err->message ? err->message : "Could not build pipeline";
    if (err)
      g_error_free(err);
    signal_error_.emit(msg);
    return false;
  }
  if (err) {
    g_error_free(err);
    err = nullptr;
  }
  if (sink_name && !location.empty()) {
    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline_), sink_name);
    if (sink) {
      g_object_set(sink, "location", location.c_str(), nullptr);
      gst_object_unref(sink);
    }
  }
  if (input_.backend != AudioBackend::system_default && !input_.id.empty() &&
      input_.id != "default") {
    GstElement* src = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
    if (src) {
      g_object_set(src, "device", input_.id.c_str(), nullptr);
      gst_object_unref(src);
    }
  }
  GstElement* wave = gst_bin_get_by_name(GST_BIN(pipeline_), "wave");
  if (wave) {
    gst_app_sink_set_emit_signals(GST_APP_SINK(wave), FALSE);
    GstCaps* caps = gst_caps_from_string("audio/x-raw,format=F32LE,channels=1,layout=interleaved");
    gst_app_sink_set_caps(GST_APP_SINK(wave), caps);
    gst_caps_unref(caps);
    gst_object_unref(wave);
  }
  GstBus* bus = gst_element_get_bus(pipeline_);
  bus_watch_ = gst_bus_add_watch(bus, &Recorder::on_bus, this);
  gst_object_unref(bus);
  if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    tear_pipeline();
    signal_error_.emit("Could not start audio");
    return false;
  }
  tick_ = Glib::signal_timeout().connect(sigc::mem_fun(*this, &Recorder::on_tick), 50);
  return true;
}

gboolean Recorder::on_bus(GstBus*, GstMessage* msg, gpointer data)
{
  auto* self = static_cast<Recorder*>(data);
  switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
      GError* err = nullptr;
      gst_message_parse_error(msg, &err, nullptr);
      const Glib::ustring text = err && err->message ? err->message : "GStreamer error";
      if (err)
        g_error_free(err);
      self->tear_pipeline();
      self->set_state(self->has_tape_ ? RecState::stopped : RecState::empty);
      self->signal_error_.emit(text);
      break;
    }
    case GST_MESSAGE_EOS:
      self->stop();
      break;
    default:
      break;
  }
  return TRUE;
}

void Recorder::pull_wave()
{
  if (!pipeline_ || state_ != RecState::recording)
    return;
  GstElement* el = gst_bin_get_by_name(GST_BIN(pipeline_), "wave");
  if (!el)
    return;
  auto* as = GST_APP_SINK(el);
  double peak = 0;
  bool any = false;
  for (;;) {
    GstSample* sample = gst_app_sink_try_pull_sample(as, 0);
    if (!sample)
      break;
    GstBuffer* buf = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (buf && gst_buffer_map(buf, &map, GST_MAP_READ) && map.size >= sizeof(float)) {
      const auto* f = reinterpret_cast<const float*>(map.data);
      const size_t n = map.size / sizeof(float);
      for (size_t i = 0; i < n; ++i) {
        const double a = std::fabs(static_cast<double>(f[i]));
        if (a > peak)
          peak = a;
      }
      gst_buffer_unmap(buf, &map);
      any = true;
    }
    gst_sample_unref(sample);
  }
  gst_object_unref(el);
  if (any)
    signal_level_.emit(std::min(1.0, peak * 2.5));
}

bool Recorder::on_tick()
{
  pull_wave();
  query_times();
  signal_position_.emit(position_ns_, duration_ns_);
  return true;
}

void Recorder::query_times()
{
  if (!pipeline_)
    return;
  gint64 pos = 0;
  if (gst_element_query_position(pipeline_, GST_FORMAT_TIME, &pos))
    position_ns_ = pos;
  gint64 dur = 0;
  if (gst_element_query_duration(pipeline_, GST_FORMAT_TIME, &dur) && dur > 0)
    duration_ns_ = dur;
  if (state_ == RecState::recording && position_ns_ > duration_ns_)
    duration_ns_ = position_ns_;
}

bool Recorder::new_tape()
{
  stop();
  path_ = tape_path();
  g_unlink(path_.c_str());
  has_tape_ = false;
  dirty_ = false;
  position_ns_ = 0;
  duration_ns_ = 0;
  set_state(RecState::empty);
  signal_position_.emit(0, 0);
  return true;
}

bool Recorder::open_wav(const std::string& path)
{
  stop();
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR)) {
    signal_error_.emit("File not found");
    return false;
  }
  path_ = tape_path();
  if (!copy_file(path, path_)) {
    signal_error_.emit("Could not open WAV");
    return false;
  }
  has_tape_ = true;
  dirty_ = false;
  position_ns_ = 0;
  duration_ns_ = 0;
  probe_duration();
  set_state(RecState::stopped);
  signal_position_.emit(position_ns_, duration_ns_);
  return true;
}

bool Recorder::save_as(const std::string& path)
{
  if (!has_tape_)
    return false;
  stop();
  if (!copy_file(path_, path)) {
    signal_error_.emit("Could not save WAV");
    return false;
  }
  dirty_ = false;
  return true;
}

bool Recorder::record()
{
  if (state_ == RecState::recording)
    return true;
  stop();
  path_ = tape_path();
  g_unlink(path_.c_str());
  const char* src_el = "autoaudiosrc";
  if (input_.backend == AudioBackend::pulse && !input_.id.empty() && input_.id != "default")
    src_el = "pulsesrc";
  else if (input_.backend == AudioBackend::alsa && !input_.id.empty() && input_.id != "default")
    src_el = "alsasrc";
  const std::string desc =
      std::string(src_el) +
      " name=src ! audioconvert ! audioresample ! "
      "audio/x-raw,rate=44100,channels=1 ! tee name=t "
      "t. ! queue ! wavenc ! filesink name=fs "
      "t. ! queue leaky=downstream max-size-buffers=8 ! "
      "audioconvert ! audio/x-raw,format=F32LE,channels=1,layout=interleaved ! "
      "appsink name=wave sync=false max-buffers=8 drop=true";
  if (!start_pipeline(desc, "fs", path_))
    return false;
  has_tape_ = true;
  dirty_ = true;
  position_ns_ = 0;
  duration_ns_ = 0;
  set_state(RecState::recording);
  return true;
}

bool Recorder::play()
{
  if (!has_tape_)
    return false;
  if (state_ == RecState::playing)
    return true;
  const gint64 start = position_ns_;
  stop();
  position_ns_ = start;
  const char* desc = "filesrc name=fs ! wavparse ! audioconvert ! audioresample ! autoaudiosink";
  if (!start_pipeline(desc, "fs", path_))
    return false;
  set_state(RecState::playing);
  if (start > 0 && pipeline_) {
    gst_element_get_state(pipeline_, nullptr, nullptr, 400 * GST_MSECOND);
    gst_element_seek_simple(pipeline_, GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            start);
  }
  return true;
}

bool Recorder::stop()
{
  const RecState was = state_;
  if (was == RecState::playing)
    query_times();
  tear_pipeline();
  if (was == RecState::recording) {
    has_tape_ = Glib::file_test(path_, Glib::FILE_TEST_IS_REGULAR);
    probe_duration();
    position_ns_ = 0;
    set_state(has_tape_ ? RecState::stopped : RecState::empty);
  } else if (has_tape_) {
    set_state(RecState::stopped);
  } else {
    set_state(RecState::empty);
  }
  signal_position_.emit(position_ns_, duration_ns_);
  return true;
}

bool Recorder::seek(gint64 ns)
{
  if (!has_tape_)
    return false;
  if (ns < 0)
    ns = 0;
  if (duration_ns_ > 0 && ns > duration_ns_)
    ns = duration_ns_;
  position_ns_ = ns;
  if (state_ == RecState::playing && pipeline_) {
    gst_element_seek_simple(pipeline_, GST_FORMAT_TIME,
                            static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE),
                            ns);
  }
  signal_position_.emit(position_ns_, duration_ns_);
  return true;
}

void Recorder::probe_duration()
{
  if (path_.empty() || !Glib::file_test(path_, Glib::FILE_TEST_IS_REGULAR))
    return;
  GError* err = nullptr;
  GstElement* p = gst_parse_launch("filesrc name=fs ! wavparse ! fakesink", &err);
  if (err) {
    g_error_free(err);
    err = nullptr;
  }
  if (!p)
    return;
  GstElement* fs = gst_bin_get_by_name(GST_BIN(p), "fs");
  if (fs) {
    g_object_set(fs, "location", path_.c_str(), nullptr);
    gst_object_unref(fs);
  }
  gst_element_set_state(p, GST_STATE_PAUSED);
  gst_element_get_state(p, nullptr, nullptr, 500 * GST_MSECOND);
  gint64 dur = 0;
  if (gst_element_query_duration(p, GST_FORMAT_TIME, &dur) && dur > 0)
    duration_ns_ = dur;
  gst_element_set_state(p, GST_STATE_NULL);
  gst_object_unref(p);
}

}  // namespace needle
