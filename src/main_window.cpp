/* SPDX-License-Identifier: Unlicense */

#include "main_window.hpp"
#include "about_dialog.hpp"
#include "paths.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>

namespace needle {
namespace {

void add_sound_filters(Gtk::FileChooserDialog& dlg)
{
  auto all = Gtk::FileFilter::create();
  all->set_name("Audio files");
  all->add_pattern("*.wav");
  all->add_pattern("*.flac");
  all->add_pattern("*.ogg");
  all->add_pattern("*.oga");
  all->add_pattern("*.mp3");
  dlg.add_filter(all);

  struct Item {
    const char* name;
    const char* pat;
  };
  const Item items[] = {
      {"Wave (*.wav)", "*.wav"},
      {"FLAC (*.flac)", "*.flac"},
      {"Ogg Vorbis (*.ogg)", "*.ogg"},
      {"MP3 (*.mp3)", "*.mp3"},
  };
  for (const auto& it : items) {
    auto f = Gtk::FileFilter::create();
    f->set_name(it.name);
    f->add_pattern(it.pat);
    if (std::string(it.pat) == "*.ogg")
      f->add_pattern("*.oga");
    dlg.add_filter(f);
  }
}

bool has_audio_ext(const std::string& path)
{
  const Glib::ustring low = Glib::ustring(path).lowercase();
  return low.size() >= 4 &&
         (low.substr(low.size() - 4) == ".wav" || low.substr(low.size() - 4) == ".ogg" ||
          low.substr(low.size() - 4) == ".oga" || low.substr(low.size() - 4) == ".mp3" ||
          (low.size() >= 5 && low.substr(low.size() - 5) == ".flac"));
}

std::string with_filter_ext(std::string path, const Glib::RefPtr<Gtk::FileFilter>& filter)
{
  if (has_audio_ext(path))
    return path;
  std::string ext = ".wav";
  if (filter) {
    const Glib::ustring n = filter->get_name();
    if (n.find("FLAC") != Glib::ustring::npos)
      ext = ".flac";
    else if (n.find("Ogg") != Glib::ustring::npos)
      ext = ".ogg";
    else if (n.find("MP3") != Glib::ustring::npos)
      ext = ".mp3";
  }
  return path + ext;
}

}  // namespace

MainWindow::MainWindow()
{
  set_title("Needle");
  set_resizable(false);
  set_border_width(0);
  get_style_context()->add_class("needle-window");

  accel_ = Gtk::AccelGroup::create();
  add_accel_group(accel_);

  load_css();
  build_menu();

  well_.set_border_width(8);
  well_.pack_start(wave_, Gtk::PACK_SHRINK);

  pos_lab_.set_xalign(0);
  len_lab_.set_xalign(1);
  times_.pack_start(pos_lab_, Gtk::PACK_EXPAND_WIDGET);
  times_.pack_start(len_lab_, Gtk::PACK_EXPAND_WIDGET);
  well_.pack_start(times_, Gtk::PACK_SHRINK);

  slider_.set_range(0, 1);
  slider_.set_increments(0.05, 1);
  slider_.set_draw_value(false);
  slider_.set_sensitive(false);
  slider_.add_events(Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
  slider_.signal_button_press_event().connect(sigc::mem_fun(*this, &MainWindow::on_slider_press),
                                              false);
  slider_.signal_button_release_event().connect(
      sigc::mem_fun(*this, &MainWindow::on_slider_release), false);
  well_.pack_start(slider_, Gtk::PACK_SHRINK);

  btn_rec_.get_style_context()->add_class("needle-rec");
  transport_.set_halign(Gtk::ALIGN_CENTER);
  transport_.pack_start(btn_start_, Gtk::PACK_SHRINK);
  transport_.pack_start(btn_end_, Gtk::PACK_SHRINK);
  transport_.pack_start(btn_rec_, Gtk::PACK_SHRINK);
  transport_.pack_start(btn_stop_, Gtk::PACK_SHRINK);
  transport_.pack_start(btn_play_, Gtk::PACK_SHRINK);
  well_.pack_start(transport_, Gtk::PACK_SHRINK);
  mic_lab_.set_xalign(0);
  mic_.set_hexpand(true);
  mic_row_.pack_start(mic_lab_, Gtk::PACK_SHRINK);
  mic_row_.pack_start(mic_, Gtk::PACK_EXPAND_WIDGET);
  well_.pack_start(mic_row_, Gtk::PACK_SHRINK);
  status_.set_xalign(0);
  well_.pack_start(status_, Gtk::PACK_SHRINK);

  btn_start_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_seek_start));
  btn_end_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_seek_end));
  btn_rec_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_record));
  btn_stop_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_stop));
  btn_play_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_play));

  rec_.signal_state().connect(sigc::mem_fun(*this, &MainWindow::on_state));
  rec_.signal_level().connect(sigc::mem_fun(*this, &MainWindow::on_level));
  rec_.signal_position().connect(sigc::mem_fun(*this, &MainWindow::on_position));
  rec_.signal_error().connect(sigc::mem_fun(*this, &MainWindow::on_error));
  rec_.signal_wave().connect(sigc::mem_fun(*this, &MainWindow::on_wave));
  mic_.signal_changed().connect(sigc::mem_fun(*this, &MainWindow::apply_selected_device));

  root_.pack_start(menubar_, Gtk::PACK_SHRINK);
  root_.pack_start(well_, Gtk::PACK_SHRINK);
  add(root_);
  settings_.load();
  fill_devices();
  rec_.new_tape();
  sync_buttons();
  show_all();
}

void MainWindow::load_css()
{
  const std::string css_path = find_data_file("skin/lcos/lcos.css");
  if (css_path.empty()) {
    std::cerr << "needle: lcos.css not found\n";
    return;
  }
  try {
    auto css = Gtk::CssProvider::create();
    css->load_from_path(css_path);
    Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(), css,
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  } catch (const Glib::Error& e) {
    std::cerr << "needle: CSS: " << e.what() << "\n";
  }
}

Gtk::MenuItem* MainWindow::add_item(Gtk::Menu& menu, const Glib::ustring& label,
                                    const sigc::slot<void()>& slot, guint key,
                                    Gdk::ModifierType mods)
{
  auto* item = Gtk::manage(new Gtk::MenuItem(label, true));
  item->signal_activate().connect(slot);
  if (key != 0)
    item->add_accelerator("activate", accel_, key, mods, Gtk::ACCEL_VISIBLE);
  menu.append(*item);
  return item;
}

void MainWindow::build_menu()
{
  auto add_menu = [this](const Glib::ustring& label, Gtk::Menu& menu) {
    auto* top = Gtk::manage(new Gtk::MenuItem(label, true));
    top->set_submenu(menu);
    menubar_.append(*top);
  };

  auto* file = Gtk::manage(new Gtk::Menu());
  add_item(*file, "_New", sigc::mem_fun(*this, &MainWindow::on_new), GDK_KEY_n, Gdk::CONTROL_MASK);
  add_item(*file, "_Open…", sigc::mem_fun(*this, &MainWindow::on_open), GDK_KEY_o,
           Gdk::CONTROL_MASK);
  add_item(*file, "_Save", sigc::mem_fun(*this, &MainWindow::on_save), GDK_KEY_s,
           Gdk::CONTROL_MASK);
  add_item(*file, "Save _As…", sigc::mem_fun(*this, &MainWindow::on_save_as));
  file->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
  add_item(*file, "E_xit", sigc::mem_fun(*this, &MainWindow::on_quit));
  add_menu("_File", *file);

  auto* edit = Gtk::manage(new Gtk::Menu());
  add_item(*edit, "_Copy",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet), Glib::ustring("Copy")));
  add_item(*edit, "_Paste",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_not_yet), Glib::ustring("Paste")));
  add_menu("_Edit", *edit);

  auto* fx = Gtk::manage(new Gtk::Menu());
  add_item(*fx, "Increase Volume",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::vol_up));
  add_item(*fx, "Decrease Volume",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::vol_down));
  add_item(*fx, "Increase Speed",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::speed_up));
  add_item(*fx, "Decrease Speed",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::speed_down));
  add_item(*fx, "Add Echo",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::echo));
  add_item(*fx, "Reverse",
           sigc::bind(sigc::mem_fun(*this, &MainWindow::on_effect), TapeEffect::reverse));
  add_menu("E_ffects", *fx);

  auto* help = Gtk::manage(new Gtk::Menu());
  add_item(*help, "_About Needle", sigc::mem_fun(*this, &MainWindow::on_about));
  add_menu("_Help", *help);
}

bool MainWindow::confirm_discard()
{
  if (!rec_.dirty())
    return true;
  Gtk::MessageDialog dlg(*this, "Save the current sound?", false, Gtk::MESSAGE_QUESTION,
                         Gtk::BUTTONS_NONE, true);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Discard", Gtk::RESPONSE_NO);
  dlg.add_button("_Save", Gtk::RESPONSE_YES);
  const int r = dlg.run();
  if (r == Gtk::RESPONSE_CANCEL)
    return false;
  if (r == Gtk::RESPONSE_YES)
    on_save();
  return !rec_.dirty() || r == Gtk::RESPONSE_NO;
}

void MainWindow::on_new()
{
  if (!confirm_discard())
    return;
  rec_.new_tape();
  save_path_.clear();
  wave_.clear();
}

void MainWindow::on_open()
{
  if (!confirm_discard())
    return;
  Gtk::FileChooserDialog dlg(*this, "Open sound", Gtk::FILE_CHOOSER_ACTION_OPEN);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Open", Gtk::RESPONSE_ACCEPT);
  apply_folder(dlg);
  add_sound_filters(dlg);
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return;
  if (rec_.open_file(dlg.get_filename())) {
    save_path_ = dlg.get_filename();
    remember_folder(save_path_);
    wave_.clear();
  }
}

void MainWindow::on_save()
{
  if (save_path_.empty()) {
    on_save_as();
    return;
  }
  rec_.save_as(save_path_);
}

void MainWindow::on_save_as()
{
  Gtk::FileChooserDialog dlg(*this, "Save sound", Gtk::FILE_CHOOSER_ACTION_SAVE);
  dlg.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
  dlg.add_button("_Save", Gtk::RESPONSE_ACCEPT);
  dlg.set_do_overwrite_confirmation(true);
  apply_folder(dlg);
  dlg.set_current_name("sound.wav");
  add_sound_filters(dlg);
  if (dlg.run() != Gtk::RESPONSE_ACCEPT)
    return;
  const std::string path = with_filter_ext(dlg.get_filename(), dlg.get_filter());
  if (rec_.save_as(path)) {
    save_path_ = path;
    remember_folder(path);
  }
}

void MainWindow::on_quit()
{
  if (!confirm_discard())
    return;
  rec_.stop();
  hide();
}

void MainWindow::on_about()
{
  AboutDialog dlg(*this);
  dlg.run();
}

void MainWindow::on_not_yet(const Glib::ustring& feature)
{
  status_.set_text(feature + " is not in this version.");
}

void MainWindow::on_effect(TapeEffect fx)
{
  rec_.apply_effect(fx);
}

void MainWindow::on_wave(const std::vector<double>& env)
{
  wave_.clear();
  for (double a : env)
    wave_.push_level(a);
}

void MainWindow::on_seek_start()
{
  rec_.seek(0);
}

void MainWindow::on_seek_end()
{
  rec_.seek(rec_.duration_ns());
}

bool MainWindow::on_slider_press(GdkEventButton*)
{
  slider_drag_ = true;
  return false;
}

bool MainWindow::on_slider_release(GdkEventButton*)
{
  slider_drag_ = false;
  const double sec = slider_.get_value();
  rec_.seek(static_cast<gint64>(sec * 1e9));
  return false;
}

void MainWindow::remember_folder(const std::string& path)
{
  const std::string dir = Glib::path_get_dirname(path);
  if (dir.empty())
    return;
  settings_.last_folder = dir;
  settings_.save();
}

void MainWindow::apply_folder(Gtk::FileChooser& dlg)
{
  if (!settings_.last_folder.empty() &&
      Glib::file_test(settings_.last_folder, Glib::FILE_TEST_IS_DIR))
    dlg.set_current_folder(settings_.last_folder);
}

void MainWindow::fill_devices()
{
  devices_ = list_audio_inputs();
  const std::string keep =
      mic_.get_active_id().empty() ? settings_.audio_device : mic_.get_active_id().raw();
  mic_.remove_all();
  for (const auto& d : devices_)
    mic_.append(d.id, d.label);
  const std::string pick = pick_audio_device(devices_, keep);
  mic_.set_active_id(pick);
  apply_selected_device();
}

AudioDevice MainWindow::selected_device() const
{
  const std::string id = mic_.get_active_id().raw();
  for (const auto& d : devices_) {
    if (d.id == id)
      return d;
  }
  AudioDevice def;
  def.id = "default";
  def.label = "Default";
  def.backend = AudioBackend::system_default;
  return def;
}

void MainWindow::apply_selected_device()
{
  const AudioDevice d = selected_device();
  rec_.set_input(d);
  settings_.audio_device = d.id;
  settings_.save();
}

void MainWindow::on_record()
{
  fill_devices();
  rec_.record();
}

void MainWindow::on_stop()
{
  rec_.stop();
}

void MainWindow::on_play()
{
  rec_.play();
}

void MainWindow::on_state(RecState s)
{
  sync_buttons();
  switch (s) {
    case RecState::empty:
      status_.set_text("Stopped");
      break;
    case RecState::stopped:
      status_.set_text("Stopped");
      break;
    case RecState::recording:
      status_.set_text("Recording");
      wave_.clear();
      break;
    case RecState::playing:
      status_.set_text("Playing");
      break;
  }
}

void MainWindow::on_level(double amp)
{
  wave_.push_level(amp);
}

void MainWindow::on_position(gint64 pos, gint64 dur)
{
  pos_lab_.set_text("Position: " + format_secs(pos));
  len_lab_.set_text("Length: " + format_secs(dur));
  const double max = dur > 0 ? static_cast<double>(dur) / 1e9 : 1.0;
  slider_.set_range(0, max);
  if (!slider_drag_)
    slider_.set_value(static_cast<double>(pos) / 1e9);
  if (dur > 0)
    wave_.set_playing_progress(static_cast<double>(pos) / static_cast<double>(dur));
}

void MainWindow::on_error(const Glib::ustring& msg)
{
  status_.set_text(msg);
}

void MainWindow::sync_buttons()
{
  const RecState s = rec_.state();
  const bool rec = s == RecState::recording;
  const bool play = s == RecState::playing;
  btn_rec_.set_sensitive(!rec && !play);
  btn_play_.set_sensitive(rec_.has_tape() && !rec && !play);
  btn_stop_.set_sensitive(rec || play);
  btn_start_.set_sensitive(rec_.has_tape() && !rec);
  btn_end_.set_sensitive(rec_.has_tape() && !rec);
  slider_.set_sensitive(rec_.has_tape() && !rec);
  mic_.set_sensitive(!rec);
}

Glib::ustring MainWindow::format_secs(gint64 ns) const
{
  if (ns < 0)
    ns = 0;
  const double s = static_cast<double>(ns) / 1e9;
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << s << " sec";
  return os.str();
}

}  // namespace needle
