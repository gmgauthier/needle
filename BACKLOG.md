# Needle backlog

Current release: **v1.0.1**. Last updated: 2026-10-01.

Windows 95 Sound Recorder. Binary `needle`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

None.

## Low Priority

- Insert File / Mix with File

## Out of Scope

- MP3 / Ogg as the native tape
- A player (that is EarBlaster)
- Mixer / pavucontrol
- Network
- Custom title bar. `GTK_THEME` in the environment still wins; else Clearlooks-Phenix, then Clearlooks, then Adwaita:light (process only)
- Bryan’s seal
- GNOME Sound Recorder re-theme

## Shipped

**v1.0.1** — Headless meson test suite, and known defects recorded in BUG-BACKLOG.md.

**v1.0.0** — Effects: Increase/Decrease Volume, Increase/Decrease Speed, Add Echo, Reverse. Waveform envelope rebuilt after Stop/Open so play stays in sync.

**v0.3.0** — M4: Save As / Open FLAC, Ogg Vorbis, MP3. Tape stays WAV.

**v0.2.0** — M2: seek slider, last folder in `~/.config/needle/needle.ini`, README screenshot.

**v0.1.0** — Window, green waveform (right-origin scroll), Record / Stop / Play, WAV New / Open / Save As, microphone combo, `.deb` / tarball / AppImage.
