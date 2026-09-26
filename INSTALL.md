# Installing Needle

Four ways to get a binary, in the order LCOS cares about:

| Artifact | Who it is for |
|---|---|
| **`.deb`** | LCOS, Devuan Excalibur, Debian Trixie. Preferred. |
| **Source tarball** | Distro packagers and `meson setup && ninja install`. |
| **AppImage** | Fallback for distros that do not install `.deb` files. |
| **Git build** | Developers. See below. |

Version comes from `meson.build` (currently `0.3.0`).

## Runtime needs

- GTK 3 / gtkmm-3.0
- GStreamer 1.0 (plugins-base, plugins-good, plugins-ugly for MP3; Pulse or ALSA)

```
sudo apt install libgtkmm-3.0-1t64 gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly
```

## 1. Debian package (preferred)

```
sudo apt install ./dist/needle_0.3.0-1_amd64.deb
```

Or, from this tree:

```
./scripts/release.sh deb
sudo apt install ./dist/needle_0.3.0-1_amd64.deb
```

Uninstall: `sudo apt remove needle`.

## 2. Source tarball

```
tar -xf needle-0.3.0.tar.xz
cd needle-0.3.0
sudo apt install build-essential meson ninja-build pkg-config \
  libgtkmm-3.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev
meson setup build --prefix=/usr
meson compile -C build
sudo meson install -C build
```

## 3. AppImage (fallback)

```
./scripts/release.sh appimage
```

Requires `linuxdeploy` on `$PATH`.

## 4. Git build

```
sudo apt install build-essential meson ninja-build pkg-config g++ \
  libgtkmm-3.0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good clang-format cppcheck
meson setup build
meson compile -C build
./build/needle
```

## License

[The Unlicense](https://unlicense.org). See [UNLICENSE](UNLICENSE).
