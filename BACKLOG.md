# Needle backlog

Current release: **v0.2.0**. Last updated: 2026-09-26.

Windows 95 Sound Recorder. Binary `needle`. Suite catalog: `lcos-projects/PRODUCT-BACKLOG.md`. Plan: [DEVELOPMENT.md](DEVELOPMENT.md). How to land work: [DEVELOPMENT.md](DEVELOPMENT.md#process) — `feature/` / `fix/` branches, PRs to `master`, lint gate, semver on shipped PRs.

## High Priority

M4 codecs (this branch): Save As / Open FLAC, Ogg Vorbis, MP3. Tape stays WAV.

## Low Priority

- Effects: volume, speed, reverse, echo (Win95 menus)
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

**v0.2.0** — M2: seek slider, last folder in `~/.config/needle/needle.ini`, README screenshot.

**v0.1.0** — Window, green waveform (right-origin scroll), Record / Stop / Play, WAV New / Open / Save As, microphone combo, `.deb` / tarball / AppImage.
