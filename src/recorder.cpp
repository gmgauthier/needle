/* SPDX-License-Identifier: Unlicense */

#include "recorder.hpp"
#include "tape_pcm.hpp"
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

std::string lower_ext(const std::string& path)
{
  const std::string base = Glib::path_get_basename(path);
  const auto dot = base.rfind('.');
  if (dot == std::string::npos || dot + 1 >= base.size())
    return {};
  return Glib::ustring(base.substr(dot)).lowercase().raw();
}

const char* encode_desc(const std::string& ext)
{
  if (ext == ".flac")
    return "filesrc name=in ! wavparse ! audioconvert ! audioresample ! "
           "flacenc ! filesink name=out";
  if (ext == ".ogg" || ext == ".oga")
    return "filesrc name=in ! wavparse ! audioconvert ! audioresample ! "
           "vorbisenc ! oggmux ! filesink name=out";
  if (ext == ".mp3")
    return "filesrc name=in ! wavparse ! audioconvert ! audioresample ! "
           "lamemp3enc target=bitrate bitrate=128 ! xingmux ! filesink name=out";
  return nullptr;
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
    // A recoverable parse error (a missing element) leaves a pipeline that never starts.
    Glib::ustring msg = err->message ? err->message : "Could not build pipeline";
    g_error_free(err);
    gst_object_unref(pipeline_);
    pipeline_ = nullptr;
    signal_error_.emit(msg);
    return false;
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
      if (self->state_ == RecState::recording)
        self->discard_take();
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

bool Recorder::transcode(const std::string& desc, const std::string& in_path,
                         const std::string& out_path, const char* fail)
{
  GError* err = nullptr;
  GstElement* p = gst_parse_launch(desc.c_str(), &err);
  if (!p) {
    Glib::ustring msg = err && err->message ? err->message : fail;
    if (err)
      g_error_free(err);
    signal_error_.emit(msg);
    return false;
  }
  if (err) {
    // A missing element leaves a pipeline that would never post EOS.
    Glib::ustring msg = err->message ? err->message : fail;
    g_error_free(err);
    gst_object_unref(p);
    signal_error_.emit(msg);
    return false;
  }
  // filesink truncates on open. Write a side file and rename it over out_path on success.
  const std::string part = out_path + ".part";
  g_unlink(part.c_str());
  if (GstElement* in = gst_bin_get_by_name(GST_BIN(p), "in")) {
    g_object_set(in, "location", in_path.c_str(), nullptr);
    gst_object_unref(in);
  }
  if (GstElement* out = gst_bin_get_by_name(GST_BIN(p), "out")) {
    g_object_set(out, "location", part.c_str(), nullptr);
    gst_object_unref(out);
  }
  if (gst_element_set_state(p, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
    gst_element_set_state(p, GST_STATE_NULL);
    gst_object_unref(p);
    g_unlink(part.c_str());
    signal_error_.emit(fail);
    return false;
  }
  GstBus* bus = gst_element_get_bus(p);
  GstMessage* msg = gst_bus_timed_pop_filtered(
      bus, GST_CLOCK_TIME_NONE, static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));
  bool ok = true;
  if (!msg) {
    ok = false;
    signal_error_.emit(fail);
  } else if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
    GError* ge = nullptr;
    gst_message_parse_error(msg, &ge, nullptr);
    const Glib::ustring text = ge && ge->message ? ge->message : fail;
    if (ge)
      g_error_free(ge);
    signal_error_.emit(text);
    ok = false;
  }
  if (msg)
    gst_message_unref(msg);
  gst_object_unref(bus);
  gst_element_set_state(p, GST_STATE_NULL);
  gst_object_unref(p);
  if (ok && g_rename(part.c_str(), out_path.c_str()) != 0) {
    signal_error_.emit(fail);
    ok = false;
  }
  if (!ok)
    g_unlink(part.c_str());
  return ok;
}

bool Recorder::open_file(const std::string& path)
{
  stop();
  if (!Glib::file_test(path, Glib::FILE_TEST_IS_REGULAR)) {
    signal_error_.emit("File not found");
    return false;
  }
  path_ = tape_path();
  const std::string ext = lower_ext(path);
  bool ok = false;
  if (ext == ".wav") {
    ok = copy_file(path, path_);
    if (!ok)
      signal_error_.emit("Could not open WAV");
  } else {
    ok = transcode(
        "filesrc name=in ! decodebin ! audioconvert ! audioresample ! "
        "audio/x-raw,rate=44100,channels=1 ! wavenc ! filesink name=out",
        path, path_, "Could not open sound");
  }
  if (!ok)
    return false;
  has_tape_ = true;
  dirty_ = false;
  position_ns_ = 0;
  duration_ns_ = 0;
  probe_duration();
  set_state(RecState::stopped);
  signal_position_.emit(position_ns_, duration_ns_);
  emit_tape_wave();
  return true;
}

bool Recorder::save_as(const std::string& path)
{
  stop();
  if (!has_tape_)
    return false;
  const std::string ext = lower_ext(path);
  const char* desc = encode_desc(ext);
  bool ok = false;
  if (!desc) {
    ok = copy_file(path_, path);
    if (!ok)
      signal_error_.emit("Could not save WAV");
  } else {
    const char* fail = "Could not save sound";
    if (ext == ".flac")
      fail = "Could not save FLAC";
    else if (ext == ".ogg" || ext == ".oga")
      fail = "Could not save Ogg Vorbis";
    else if (ext == ".mp3")
      fail = "Could not save MP3";
    ok = transcode(desc, path_, path, fail);
  }
  if (!ok)
    return false;
  dirty_ = false;
  return true;
}

bool Recorder::apply_effect(TapeEffect fx)
{
  stop();
  if (!has_tape_ || path_.empty() || !Glib::file_test(path_, Glib::FILE_TEST_IS_REGULAR)) {
    signal_error_.emit("No sound to process.");
    return false;
  }
  TapePcm pcm;
  std::string err;
  if (!load_wav(path_, pcm, err)) {
    signal_error_.emit(err);
    return false;
  }
  switch (fx) {
    case TapeEffect::vol_up:
      fx_volume(pcm, 1.25f);
      break;
    case TapeEffect::vol_down:
      fx_volume(pcm, 0.75f);
      break;
    case TapeEffect::speed_up:
      fx_speed(pcm, 2.f);
      break;
    case TapeEffect::speed_down:
      fx_speed(pcm, 0.5f);
      break;
    case TapeEffect::echo:
      fx_echo(pcm);
      break;
    case TapeEffect::reverse:
      fx_reverse(pcm);
      break;
  }
  const std::string tmp = path_ + ".fx";
  if (!save_wav(tmp, pcm, err)) {
    signal_error_.emit(err);
    return false;
  }
  if (g_rename(tmp.c_str(), path_.c_str()) != 0) {
    g_unlink(tmp.c_str());
    signal_error_.emit("Could not replace tape");
    return false;
  }
  dirty_ = true;
  probe_duration();
  position_ns_ = 0;
  set_state(RecState::stopped);
  signal_position_.emit(position_ns_, duration_ns_);
  emit_tape_wave();
  return true;
}

void Recorder::emit_tape_wave()
{
  if (path_.empty() || !Glib::file_test(path_, Glib::FILE_TEST_IS_REGULAR))
    return;
  TapePcm pcm;
  std::string err;
  if (!load_wav(path_, pcm, err))
    return;
  signal_wave_.emit(pcm_envelope(pcm, 240));
}

void Recorder::discard_take()
{
  // The take never replaced the tape: drop it and restore the tape's duration.
  if (!take_path_.empty())
    g_unlink(take_path_.c_str());
  take_path_.clear();
  position_ns_ = 0;
  duration_ns_ = 0;
  if (has_tape_)
    probe_duration();
}

bool Recorder::record()
{
  if (state_ == RecState::recording)
    return true;
  stop();
  path_ = tape_path();
  // Capture into a side file. The current tape is replaced only when Stop commits the take.
  take_path_ = path_ + ".rec";
  g_unlink(take_path_.c_str());
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
  if (!start_pipeline(desc, "fs", take_path_)) {
    g_unlink(take_path_.c_str());
    take_path_.clear();
    return false;
  }
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
    if (!take_path_.empty() && Glib::file_test(take_path_, Glib::FILE_TEST_IS_REGULAR) &&
        g_rename(take_path_.c_str(), path_.c_str()) == 0) {
      has_tape_ = true;
      dirty_ = true;
    } else if (!take_path_.empty()) {
      g_unlink(take_path_.c_str());
    }
    take_path_.clear();
    probe_duration();
    position_ns_ = 0;
    set_state(has_tape_ ? RecState::stopped : RecState::empty);
    if (has_tape_)
      emit_tape_wave();
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
