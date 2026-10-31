# Mic-Lab V1.0 Implementation Plan

> **For agentic workers:** Use executing-plans to implement task-by-task; read PRODUCT.md first. User selected implementation in this chat. Continue independent work without additional routine permission gates.

**Goal:** Build the confirmed USB microphone with metering, gain, mute and a persistent 30-second clip.

**Architecture:** Single codec worker feeds mono PCM to a bounded USB FIFO and recording state machine. A separate storage worker mounts the pinned official LittleFS backend and commits verified files. LVGL reads snapshots and posts commands. Iris remains the sole TinyUSB installer.

**Tech Stack:** ESP-IDF 6.2 / ESP32-S31, pinned Mosaico utils/BSP, TinyUSB UAC2, ES8311, LVGL, LittleFS. Host C tests with GCC.

**Spec:** [PRODUCT.md](PRODUCT.md)

**Scope note, 2026-10-31:** The checked tasks below cover the initial audio foundation only. The user subsequently identified a DIY main screen as a core requirement. That feature is not implemented or covered by these completed tasks; see HANDOFF.md before continuing.

## Global Constraints

- Fixed 48000Hz, signed 16-bit mono; record limit 1440000 samples.
- Unique `miclab` NVS and `/nand/mic-lab/` storage. No formatting, no partition changes, no edits to other projects or upstream dependencies.
- Existing dirty Trader-Lab/index changes are preserved. Work in new project directories in current checkout; do not commit unrelated work.
- 30-second persistence was explicitly requested; RAM-only recording is insufficient.
- No automatic device flash; document pending hardware verification separately.

## Review Focus

- Mute during queued audio and replay transitions must not leak old voice data.
- Corrupt/truncated recordings and failed writes must preserve old saved data.
- CDC/UAC full/high-speed descriptors and actual sample timing must agree.
- Host close/reopen or disconnect must not send stale buffered audio.
- Storage work and rapid controls must not race with capture or playback.

## Task 1: Deterministic audio and recording core

Files: `main/mic_core.[ch]`, `tests/test_core.c`, `tools/test.ps1`.

Interfaces: `mic_core_init`, `mic_core_record`, `mic_core_stop`, `mic_core_play`, `mic_core_process`, `mic_core_playback`; model owns a borrowed PCM buffer, no platform IO.

- [x] Write host tests for zero/mid/full-scale meters, clipping, mute, capacity, stop, replay and ignored conflicting transitions. Run RED.
- [x] Implement fixed-size processing and bounded state transitions. Run GREEN with GCC warnings as errors.

## Task 2: Persistent recording

Files: `main/clip_file.[ch]`, `main/mic_storage.[ch]`, `components/mic_storage_backend/CMakeLists.txt`, `tests/test_clip.c`.

Interfaces: file save/load return status, validate fixed rate/sample count/CRC; platform worker calls file functions with the official storage lease and publishes completion.

- [x] Test valid roundtrip, truncation, corrupt payload/header, zero/over-limit input, failed replacement preserving old clip. Run RED.
- [x] Implement versioned header, CRC, temp-write/sync/readback/rename, bounded restore. Run GREEN.

## Task 3: UAC and codec integration

Files: `main/usb_mic.[ch]`, `main/usb_layout.[ch]`, `main/mic_tusb_extra.h`, `main/mic_audio.[ch]`, project CMake/defaults/tools.

- [x] Test descriptor extension rejects endpoint collisions/truncation and retains both CDCs; test FS/HS mono endpoint sizes and intervals. Run RED.
- [x] Add UAC2 at interfaces 4/5, IN 0x85; compile audio class consistently in TinyUSB/consumers. Fixed 48k clock controls, bounded host volume/mute requests.
- [x] Audio worker captures via BSP, extracts actual mono channel, handles hardware gain and playback exclusively; USB worker supplies bounded PCM, zero on mute/error/replay and clears on host transitions.
- [x] Build against fixed toolchain, inspect descriptor output and package partition manifest.

## Task 4: Device UI and delivery

Files: `main/main.c`, `main/mic_ui.[ch]`, generated fonts, READMEs and verification docs.

- [x] Single screen meter, gain controls, mute, record/save/replay status. Persistent gain/mute settings; no writes from LVGL callbacks.
- [x] Add status-only diagnostic RPC and screen capture backend; mark healthy after initialized services.
- [x] Run core/file/descriptor tests, build official update bundle, inspect actual LVGL rendering if available.
- [x] Fresh reviewer checks feature and failure paths; fix meaningful findings, then document evidence and remaining hardware tests.

## Progress / rulings

- Preflight: new project directory avoids existing uncommitted Trader-Lab work; the validated dependency pins and partition layout are reused.
- User chose persistent recording. Official `app_storage` code can mount the existing volume without formatting; reuse it without enabling the rest of the factory application.
- Continuous USB audio during NAND saves requires a separate worker; RAM clip is immutable while saving.
- Completed on 2026-10-29: four host test executables, actual LVGL checks/rendering, S31 build and official update package all pass. See VERIFICATION.md and BUILD-ARTIFACT.json. No device flash performed.
- Review corrections: ES8311 uses real 6 dB gain steps; playback drains 40 ms before closing; USB audio and stack share core 0, with stack priority 7 above audio 6 and short FIFO critical sections. FIFO disallows overwrite; short-packet diagnostics have documented limits.
- Reconnect resets stream state through a chained TinyUSB event callback. Feature-unit controls, replay tail and reconnect issues have regression tests that failed before correction.
