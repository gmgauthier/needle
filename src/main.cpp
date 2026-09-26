/* SPDX-License-Identifier: Unlicense */

#include "application.hpp"

#include <glib.h>
#include <glibmm/miscutils.h>
#include <gst/gst.h>

#include <string>

namespace {

bool theme_has_gtk3(const char* name)
{
  const std::string home = Glib::build_filename(Glib::get_home_dir(), ".themes", name, "gtk-3.0");
  const std::string sys = Glib::build_filename("/usr/share/themes", name, "gtk-3.0");
  return g_file_test(home.c_str(), G_FILE_TEST_IS_DIR) ||
         g_file_test(sys.c_str(), G_FILE_TEST_IS_DIR);
}

/* Process-only theme. GTK_THEME in the environment still wins.
 * Else Clearlooks-Phenix, then Clearlooks, then Adwaita:light. */
void prefer_light_theme()
{
  if (g_getenv("GTK_THEME") != nullptr)
    return;
  if (theme_has_gtk3("Clearlooks-Phenix"))
    g_setenv("GTK_THEME", "Clearlooks-Phenix", FALSE);
  else if (theme_has_gtk3("Clearlooks"))
    g_setenv("GTK_THEME", "Clearlooks", FALSE);
  else
    g_setenv("GTK_THEME", "Adwaita:light", FALSE);
}

void setup_gst_plugin_path()
{
  const char* appdir = g_getenv("APPDIR");
  if (appdir == nullptr || appdir[0] == '\0')
    return;

  std::string path;
  const char* bundled[] = {"/usr/lib/gstreamer-1.0", "/usr/lib/x86_64-linux-gnu/gstreamer-1.0",
                           "/usr/lib/aarch64-linux-gnu/gstreamer-1.0", nullptr};
  for (int i = 0; bundled[i]; ++i) {
    const std::string cand = std::string(appdir) + bundled[i];
    if (g_file_test(cand.c_str(), G_FILE_TEST_IS_DIR)) {
      path = cand;
      break;
    }
  }
  if (path.empty())
    return;

  g_unsetenv("GST_PLUGIN_PATH");
  g_unsetenv("GST_PLUGIN_SYSTEM_PATH");
  g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", path.c_str(), TRUE);
  g_setenv("GST_PLUGIN_PATH_1_0", path.c_str(), TRUE);

  const std::string scanner =
      std::string(appdir) + "/usr/lib/gstreamer1.0/gstreamer-1.0/gst-plugin-scanner";
  if (g_file_test(scanner.c_str(), G_FILE_TEST_IS_EXECUTABLE))
    g_setenv("GST_PLUGIN_SCANNER_1_0", scanner.c_str(), TRUE);

  const std::string cache = std::string(g_get_user_cache_dir()) + "/gstreamer-1.0";
  g_mkdir_with_parents(cache.c_str(), 0700);
  const std::string registry = cache + "/needle-appimage.bin";
  g_setenv("GST_REGISTRY_1_0", registry.c_str(), TRUE);
}

}  // namespace

int main(int argc, char* argv[])
{
  if (g_getenv("GDK_BACKEND") == nullptr)
    g_setenv("GDK_BACKEND", "x11", FALSE);
  g_set_prgname("needle");
  prefer_light_theme();
  setup_gst_plugin_path();
  gst_init(&argc, &argv);

  return needle::Application::create()->run(argc, argv);
}
