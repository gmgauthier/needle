# Bug backlog

Reviewed 2026-10-01 against the 1.0.0 sources.

`meson test` runs `tests/test_tape.cpp` (`tape`). It checks reverse, volume, echo at a one-frame delay, speed, and a short WAV round trip. It does not lock the 16-bit scale mismatch or the 4 GB header wrap. Ordinary mono 16-bit takes at 44.1 kHz are fine. Reverse keeps channel pairs. Packed 8/16/24/32-bit and float32 PCM decode with the right sign.

## Open

None.

## Closed

### A huge chunk size hangs the UI

- Severity: crash
- Confidence: high
- Where: `src/tape_pcm.cpp:83`
- Trigger: Open, or any Effect, on a WAV whose pre-`data` chunk size is `0xFFFFFFF8`. A few dozen bytes are enough: `RIFF`/`WAVE` plus a `JUNK` chunk of that size.
- Outcome: `sz` is `uint32_t`, so `8 + sz` wraps to 0 in 32-bit arithmetic before it is added to the `size_t` cursor. The scan loop never advances. `load_wav` does not return. `emit_tape_wave` and `apply_effect` call it on the GTK thread, so the window freezes.
- Fixed: v1.0.2

### Record deletes the current tape before capture starts

- Severity: data-loss
- Confidence: high
- Where: `src/recorder.cpp:432`
- Trigger: A tape is already loaded or recorded. Press Rec. The source element fails to start (`GST_STATE_CHANGE_FAILURE`, or a later bus error after the unlink).
- Outcome: `g_unlink` removes `~/.cache/needle/tape.wav` before `start_pipeline`. On a synchronous start failure `has_tape_` is left as it was, the old drawing stays up, and Play then fails. The unsaved take is gone. New, Open, and Quit ask first. Rec does not.
- Fixed: v1.0.3

### Non-WAV open and compressed Save As truncate the destination before success

- Severity: data-loss
- Confidence: high
- Where: `src/recorder.cpp:274`
- Trigger: Open a corrupt or undecodable FLAC, Ogg, or MP3 while a tape exists, or Save As over an existing compressed file and the encode errors.
- Outcome: `filesink` `location` is set, then the pipeline is set to `PLAYING`. filesink truncates on open. Failure returns false only after that. On a failed Open the UI can still show the previous take while the cache file is partial. WAV Save As uses `g_file_set_contents` and is not this path.
- Fixed: v1.0.4

### Effect rewrite wraps WAV sizes past 4 GiB

- Severity: data-loss
- Confidence: high
- Where: `src/tape_pcm.cpp:143`, `src/tape_pcm.cpp:147`
- Trigger: Any Effect on a tape whose 16-bit PCM payload does not fit in `uint32_t`. Mono 44.1 kHz crosses that at about 13.5 hours. Stereo crosses it at about half that. Record itself has no duration cap.
- Outcome: `save_wav` stores `static_cast<uint32_t>(frames * channels * 2)` as the `data` chunk size and in the RIFF size, then writes every sample. `load_wav` trusts the `data` size, so the next Effect or envelope keeps only the wrapped prefix. Players that honor the header drop the rest.
- Fixed: v1.0.5

### Extensible WAV plays, but effects and the envelope do not

- Severity: incorrect
- Confidence: high
- Where: `src/tape_pcm.cpp:125`, `src/recorder.cpp:422`
- Trigger: Open a 24-bit WAV written by current ffmpeg (`WAVE_FORMAT_EXTENSIBLE`, tag 65534). Open of `.wav` is a byte copy, so `wavparse` still plays it.
- Outcome: `load_wav` accepts format tag 1 (PCM 8/16/24/32) and tag 3 (float 32) only. Anything else is "Unsupported WAVE format". `emit_tape_wave` returns without a signal. Effects refuse the file. The post-Open envelope is not drawn, so the view stays empty or keeps the previous take while the playhead tracks the new file.
- Fixed: v1.0.6

### A zero-length data chunk at EOF is rejected

- Severity: incorrect
- Confidence: high
- Where: `src/tape_pcm.cpp:87`
- Trigger: `save_wav` of a tape with no samples. The file is 44 bytes: `data` size 0, payload offset equal to the file size. The same layout as any other legal empty PCM WAV.
- Outcome: The check `data_off >= buf.size()` fails, and `load_wav` reports "WAVE header is incomplete". The writer's own empty file does not round-trip. A following Effect or envelope rebuild no-ops.
- Fixed: v1.0.7

### Decoder ignores nBlockAlign

- Severity: incorrect
- Confidence: high
- Where: `src/tape_pcm.cpp:95`
- Trigger: PCM whose container stride is wider than the packed width. Format tag 1, 24-bit, `nBlockAlign` 4, one pad byte per frame.
- Outcome: The stride is computed as `(bps / 8) * channels` and the header's block align is never read. Frames are stepped 3 bytes at a time, so the pad is consumed as audio. An Effect then saves that garbage as 16-bit and replaces the tape. Packed 24-bit (`align` 3) decodes correctly.
- Fixed: v1.0.8

### 16-bit load and save use different full-scale divisors

- Severity: incorrect
- Confidence: high
- Where: `src/tape_pcm.cpp:116`, `src/tape_pcm.cpp:160`
- Trigger: Any Effect. `apply_effect` rewrites the tape through `save_wav`.
- Outcome: Load divides by 32768. Save multiplies by 32767 and rounds. Full-scale 32767 is written back as 32766. −32768 is written back as −32767. Each Effect nudges the peaks down another LSB.
- Fixed: v1.0.9

### Echo never appends a tail

- Severity: incorrect
- Confidence: high
- Where: `src/tape_pcm.cpp:218`
- Trigger: Add Echo. Delay is `rate / 10` frames (100 ms at 44.1 kHz, at least 1 frame).
- Outcome: The mix is in place and the vector is not extended. A clip shorter than the delay is unchanged. On a longer clip the echo of the last delay interval is discarded instead of ringing out after the dry audio. Sums above full scale are hard-clipped.
- Fixed: v1.0.10

### Record meter is scaled by 2.5 and then clipped

- Severity: incorrect
- Confidence: high
- Where: `src/recorder.cpp:211`
- Trigger: Record anything whose float peak is above 0.4. Stop, which rebuilds the envelope with `pcm_envelope` (absolute peak, no 2.5).
- Outcome: The live trace pins at full scale while the samples on disk are still at 0.4. After Stop the same audio is drawn at less than half that height. Real clipping and merely loud input look the same during the take.
- Fixed: v1.0.11
