/* SPDX-License-Identifier: Unlicense */

#include "audio_devices.hpp"

#include <glibmm.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace needle {
namespace {

std::string trim(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
    s.erase(s.begin());
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
    s.pop_back();
  return s;
}

std::string lower(std::string s)
{
  for (char& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

bool run_cmd(const std::string& cmd, std::string& out)
{
  std::string err;
  int status = 0;
  try {
    Glib::spawn_command_line_sync(cmd, &out, &err, &status);
  } catch (const Glib::Error&) {
    return false;
  }
  return status == 0;
}

std::vector<AudioDevice> from_pulse()
{
  std::string text;
  if (!run_cmd("pactl list sources", text))
    return {};
  std::vector<AudioDevice> out;
  std::string name;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    const std::string t = trim(line);
    if (t.compare(0, 5, "Name:") == 0) {
      name = trim(t.substr(5));
    } else if (t.compare(0, 12, "Description:") == 0 && !name.empty()) {
      AudioDevice d;
      d.id = name;
      d.label = trim(t.substr(12));
      d.backend = AudioBackend::pulse;
      if (d.label.empty())
        d.label = d.id;
      out.push_back(d);
      name.clear();
    }
  }
  return out;
}

std::vector<AudioDevice> from_alsa()
{
  std::string text;
  if (!run_cmd("arecord -l", text))
    return {};
  std::vector<AudioDevice> out;
  std::istringstream in(text);
  std::string line;
  while (std::getline(in, line)) {
    /* card 1: PCH [HDA Intel PCH], device 0: ALC897 Analog [ALC897 Analog] */
    const auto card_at = line.find("card ");
    const auto dev_at = line.find("device ");
    if (card_at == std::string::npos || dev_at == std::string::npos)
      continue;
    int card = -1;
    int dev = -1;
    if (std::sscanf(line.c_str() + card_at, "card %d:", &card) != 1)
      continue;
    if (std::sscanf(line.c_str() + dev_at, "device %d:", &dev) != 1)
      continue;
    std::string label;
    const auto bra = line.find('[');
    const auto ket = line.find(']');
    if (bra != std::string::npos && ket != std::string::npos && ket > bra)
      label = line.substr(bra + 1, ket - bra - 1);
    AudioDevice d;
    d.id = Glib::ustring::compose("hw:%1,%2", card, dev).raw();
    d.label = label.empty() ? d.id : label + " (" + d.id + ")";
    d.backend = AudioBackend::alsa;
    out.push_back(d);
  }
  return out;
}

}  // namespace

std::string pulse_default_source()
{
  std::string text;
  if (!run_cmd("pactl get-default-source", text))
    return {};
  return trim(text);
}

bool looks_internal(const AudioDevice& d)
{
  const std::string hay = lower(d.label.raw() + " " + d.id);
  if (hay.find("monitor") != std::string::npos)
    return false;
  if (hay.find("built-in") != std::string::npos)
    return true;
  if (hay.find("internal") != std::string::npos)
    return true;
  return d.id.find("alsa_input.pci-") == 0 && hay.find("analog-stereo") != std::string::npos;
}

std::vector<AudioDevice> list_audio_inputs()
{
  std::vector<AudioDevice> found = from_pulse();
  if (found.empty())
    found = from_alsa();

  std::vector<AudioDevice> out;
  AudioDevice def;
  def.id = "default";
  def.label = found.empty() ? "Default / Internal microphone" : "Default";
  def.backend = AudioBackend::system_default;
  out.push_back(def);
  for (const auto& d : found)
    out.push_back(d);
  return out;
}

std::string pick_audio_device(const std::vector<AudioDevice>& devices, const std::string& saved)
{
  auto has = [&](const std::string& id) {
    return std::any_of(devices.begin(), devices.end(),
                       [&](const AudioDevice& d) { return d.id == id; });
  };
  if (!saved.empty() && has(saved))
    return saved;
  const std::string pulse_def = pulse_default_source();
  if (!pulse_def.empty() && has(pulse_def))
    return pulse_def;
  for (const auto& d : devices) {
    if (looks_internal(d))
      return d.id;
  }
  return "default";
}

}  // namespace needle
