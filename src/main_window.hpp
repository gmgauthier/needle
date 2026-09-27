/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "audio_devices.hpp"
#include "recorder.hpp"
#include "settings.hpp"
#include "wave_view.hpp"

#include <gtkmm.h>

#include <vector>

namespace needle {

class MainWindow : public Gtk::Window {
 public:
  MainWindow();

 private:
  void load_css();
  void build_menu();
  void on_new();
  void on_open();
  void on_save();
  void on_save_as();
  void on_quit();
  void on_about();
  void on_not_yet(const Glib::ustring& feature);
  void on_effect(TapeEffect fx);
  void on_wave(const std::vector<double>& env);
  void on_seek_start();
  void on_seek_end();
  bool on_slider_press(GdkEventButton* event);
  bool on_slider_release(GdkEventButton* event);
  void remember_folder(const std::string& path);
  void apply_folder(Gtk::FileChooser& dlg);
  void on_record();
  void on_stop();
  void on_play();
  void on_state(RecState s);
  void on_level(double amp);
  void on_position(gint64 pos, gint64 dur);
  void on_error(const Glib::ustring& msg);
  bool confirm_discard();
  void sync_buttons();
  void fill_devices();
  void apply_selected_device();
  AudioDevice selected_device() const;
  Glib::ustring format_secs(gint64 ns) const;

  Gtk::MenuItem* add_item(Gtk::Menu& menu, const Glib::ustring& label,
                          const sigc::slot<void()>& slot, guint key = 0,
                          Gdk::ModifierType mods = Gdk::ModifierType(0));

  Gtk::Box root_{Gtk::ORIENTATION_VERTICAL, 0};
  Gtk::MenuBar menubar_;
  Gtk::Box well_{Gtk::ORIENTATION_VERTICAL, 6};
  WaveView wave_;
  Gtk::Box times_{Gtk::ORIENTATION_HORIZONTAL, 12};
  Gtk::Label pos_lab_{"Position: 0.00 sec"};
  Gtk::Label len_lab_{"Length: 0.00 sec"};
  Gtk::Scale slider_{Gtk::ORIENTATION_HORIZONTAL};
  Gtk::Box transport_{Gtk::ORIENTATION_HORIZONTAL, 6};
  Gtk::Button btn_start_{"|<"};
  Gtk::Button btn_end_{">|"};
  Gtk::Button btn_rec_{"Rec"};
  Gtk::Button btn_stop_{"Stop"};
  Gtk::Button btn_play_{"Play"};
  Gtk::Box mic_row_{Gtk::ORIENTATION_HORIZONTAL, 6};
  Gtk::Label mic_lab_{"Microphone"};
  Gtk::ComboBoxText mic_;
  Gtk::Label status_{"Stopped"};
  Glib::RefPtr<Gtk::AccelGroup> accel_;
  Recorder rec_;
  Settings settings_;
  std::vector<AudioDevice> devices_;
  std::string save_path_;
  bool slider_drag_ = false;
};

}  // namespace needle
