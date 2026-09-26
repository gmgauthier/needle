/* SPDX-License-Identifier: Unlicense */

#pragma once

#include "recorder.hpp"
#include "wave_view.hpp"

#include <gtkmm.h>

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
  void on_seek_start();
  void on_seek_end();
  void on_record();
  void on_stop();
  void on_play();
  void on_state(RecState s);
  void on_level(double amp);
  void on_position(gint64 pos, gint64 dur);
  void on_error(const Glib::ustring& msg);
  bool confirm_discard();
  void sync_buttons();
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
  Gtk::Box transport_{Gtk::ORIENTATION_HORIZONTAL, 6};
  Gtk::Button btn_start_{"|<"};
  Gtk::Button btn_end_{">|"};
  Gtk::Button btn_rec_{"Rec"};
  Gtk::Button btn_stop_{"Stop"};
  Gtk::Button btn_play_{"Play"};
  Gtk::Label status_{"Stopped"};
  Glib::RefPtr<Gtk::AccelGroup> accel_;
  Recorder rec_;
  std::string save_path_;
};

}  // namespace needle
