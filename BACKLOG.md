# Needle backlog

Current release: **v1.0.8**. Last updated: 2026-10-02.

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

**v1.0.8** — Fix: the tape decoder honours nBlockAlign (padded PCM containers).

**v1.0.7** — Fix: an empty WAV with a zero-length data chunk at EOF loads, so the writer's own empty tape round-trips.

**v1.0.6** — Fix: Effects and the envelope accept WAVE_FORMAT_EXTENSIBLE WAVs (PCM and float SubFormats).

**v1.0.5** — Fix: an Effect no longer wraps WAV sizes past 4 GiB; it refuses and keeps the tape.

**v1.0.4** — Fix: a failed non-WAV Open or compressed Save As no longer truncates the tape or the destination.

**v1.0.3** — Fix: Rec no longer deletes the current tape when capture fails to start.

**v1.0.2** — Fix: a WAV chunk size near 4 GiB no longer hangs the UI.

**v1.0.1** — Headless meson test suite, and known defects recorded in BUG-BACKLOG.md.

**v1.0.0** — Effects: Increase/Decrease Volume, Increase/Decrease Speed, Add Echo, Reverse. Waveform envelope rebuilt after Stop/Open so play stays in sync.

**v0.3.0** — M4: Save As / Open FLAC, Ogg Vorbis, MP3. Tape stays WAV.

**v0.2.0** — M2: seek slider, last folder in `~/.config/needle/needle.ini`, README screenshot.

**v0.1.0** — Window, green waveform (right-origin scroll), Record / Stop / Play, WAV New / Open / Save As, microphone combo, `.deb` / tarball / AppImage.
