project(
  'needle',
  'cpp',
  version: '1.0.10',
  license: 'Unlicense',
  meson_version: '>=0.56',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'prefix=/usr',
  ],
)

gtkmm = dependency('gtkmm-3.0')
gst = dependency('gstreamer-1.0')
gst_app = dependency('gstreamer-app-1.0')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('APP_ID', 'org.gmgauthier.Needle')
conf.set_quoted('DATADIR', get_option('prefix') / get_option('datadir') / 'needle')
conf.set_quoted('SOURCE_ROOT', meson.project_source_root())

configure_file(
  output: 'config.hpp',
  configuration: conf,
)

inc = include_directories('.', 'src')

executable(
  'needle',
  'src/main.cpp',
  'src/application.cpp',
  'src/paths.cpp',
  'src/main_window.cpp',
  'src/about_dialog.cpp',
  'src/wave_view.cpp',
  'src/recorder.cpp',
  'src/tape_pcm.cpp',
  'src/audio_devices.cpp',
  'src/settings.cpp',
  include_directories: inc,
  dependencies: [gtkmm, gst, gst_app],
  install: true,
  install_dir: get_option('bindir'),
)

install_data(
  'data/needle.desktop',
  install_dir: get_option('datadir') / 'applications',
)

install_data(
  'data/skin/lcos/lcos.css',
  install_dir: get_option('datadir') / 'needle' / 'skin' / 'lcos',
)

install_data(
  'brand/icon-tile.svg',
  install_dir: get_option('datadir') / 'needle' / 'brand',
)

install_data(
  'brand/icon-tile.svg',
  rename: 'needle.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)

needle_tests = executable(
  'needle-tests',
  'tests/test_tape.cpp',
  'src/tape_pcm.cpp',
  include_directories: inc,
)
test('tape', needle_tests)

recorder_tests = executable(
  'needle-recorder-tests',
  'tests/test_recorder.cpp',
  'src/recorder.cpp',
  'src/paths.cpp',
  'src/tape_pcm.cpp',
  include_directories: inc,
  dependencies: [gtkmm, gst, gst_app],
)
test('recorder', recorder_tests)
