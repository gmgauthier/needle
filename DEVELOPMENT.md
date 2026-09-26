# Needle development plan

A gtkmm-3 **Sound Recorder** for LCOS. The *window* is Windows 95 `sndrec32`.

Display name: **Needle**  
Binary / repo / package: `needle`  
APP_ID: `org.gmgauthier.Needle`  
License: The Unlicense (`UNLICENSE`)  
Repos: https://gitea.scriptorium/gmgauthier/needle (origin), https://github.com/gmgauthier/needle

Shares GStreamer with EarBlaster. GNOME Sound Recorder is a phone app — not this window.

## Status (2026-09-26)

**M0–M1 in tree.** Window plus record / stop / play of a mono 44.1 kHz WAV. Microphone combo lists Pulse (then ALSA) inputs; empty list falls back to Default / Internal microphone.

## 1. Locked decisions

| Decision | Choice |
|---|---|
| Product | Original. Chrome is Windows 95 Sound Recorder, not GNOME |
| Name | Needle. Binary `needle`. APP_ID `org.gmgauthier.Needle` |
| Rejected | TapeDeck, Greenwave, SndRec, Dicta |
| Toolkit | C++17, gtkmm-3.0, GTK3 CSS, Meson |
| Engine | GStreamer `pulsesrc` / `alsasrc` / `autoaudiosrc` → `wavenc`. Same stack as EarBlaster |
| File | Uncompressed **WAV**. One tape in the window |
| Look | Small decorated window. Green waveform on black. Transport: seek-start, seek-end, Record, Stop, Play |
| Network | None |
| Never as v1 | MP3/Ogg encode, mixer, effects (echo/reverse/speed), clipboard audio, 60-second cap as a product |
| Init | No systemd |
| Brand | LCOS beige / navy. No Bryan’s seal. Mark is a green trace on navy |
| License | The Unlicense |
| Versioning | Semantic. `meson.build` is the source of truth. See **Process**. |

## 2. Window

```
File  Edit  Effects  Help
+--------------------------------------+
|  green waveform on black             |
+--------------------------------------+
 Position: 0.00 sec     Length: 0.00 sec
 [ |< ] [ >| ] [ Rec ] [ Stop ] [ Play ]
 Microphone [ combo of inputs ]
 status
```

File: New, Open…, Save, Save As…, Exit.  
Edit / Effects: parked (Win95 had copy/paste and echo; not v1).  
Help: About Needle.

## 3. Architecture

`Recorder` owns two GStreamer pipelines (record, play). Record tees into `wavenc` + `level` so the waveform can move while recording. Play uses `filesrc ! wavparse`. Default source only — no device combo.

`WaveView` is a Cairo `DrawingArea`.

Temp tape: `~/.cache/needle/tape.wav`. Save copies it.

## 4. Milestones

### M0 — Window

Scaffold. Menus, green empty trace, transport buttons, About. Record is a stub.

### M1 — Record (this slice)

Record / Stop / Play a WAV from `autoaudiosrc`. Live level into the waveform. New / Open / Save As. Position and length.

### M2 — Chrome polish

Seek on the slider, last-folder in `~/.config/needle/needle.ini`, README **Vended by Grok Build**, screenshot.

### M3 — Package

`debian/`, `scripts/release.sh` → `.deb`, tarball, AppImage. Tag `v0.1.0`.

## 5. Parked

Effects (volume / speed / reverse / echo). Insert File. Mix. Device picker. MP3.

## 6. Traps

- GNOME Sound Recorder / a phone waveform
- Mixing host GStreamer plugins with a bundled AppImage library (EarBlaster 0.1.2 closed this)
- Custom title bar
- Overriding `GTK_THEME` when it is already set. Unset: Clearlooks-Phenix, then Clearlooks, then Adwaita:light
- Bryan’s seal
- A second audio *player* (that is EarBlaster)

## Process

Do not commit to `master`. Every change lands through a pull request.

### Branches

- `feature/<short-name>` — new user-visible work
- `fix/<short-name>` — bugs, packaging nits, regressions

Open a pull request into `master`. Merge only after review.

### Gates

A pull request must pass **lint** before merge. CI runs `./scripts/lint.sh` (no `--fix`). Locally:

- `./scripts/lint.sh --fix` — clang-format rewrites `src/`
- `./scripts/lint.sh` — SPDX headers, no tabs, clang-format `--dry-run --Werror`, cppcheck (`warning`) on `src/`
- `meson compile` with this tree’s `warning_level=2` is clean (no new warnings)

Do not pass `--fix` in CI. Do not merge a red PR.

**Tests** are required when they exist (`meson test -C build`). Until a test suite lands, the gate is lint plus a clean compile plus a manual pass of the change.

### Semantic versioning

Every **shipped** pull request — merged to `master` and tagged as a release — bumps the version. `meson.build` is the source of truth. Keep these in lockstep in the same PR:

- `meson.build` `version:`
- `debian/changelog` (new stanza)
- git tag `vMAJOR.MINOR.PATCH` after merge

Then `./scripts/release.sh` produces `.deb`, tarball, and AppImage.

| Bump | When |
|---|---|
| **PATCH** (`x.y.Z`) | Bug fix or packaging. No new user-facing feature. |
| **MINOR** (`x.Y.0`) | New backward-compatible feature. |
| **MAJOR** (`X.0.0`) | Breaking change: native file format, dropped config keys, removed UI users rely on. |
