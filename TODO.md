# LR1121 radio support for the LilyGo T-Pager

Adding a new, additive `tpager_lr1121` PlatformIO env so the T-Pager's
LR1121 radio-module variant builds and flashes without touching the
existing `tpager`/`tpager_cooperative` (SX1262) build. Full design rationale
and the Meshtastic-confirmed hardware values (pins, TCXO voltage, RF-switch
table) this is based on live in the session that wrote this file; the short
version: Meshtastic ships official LR1121 support for this exact board
(`variants/esp32s3/tlora-pager` in a local meshtastic checkout) and this
driver matches its chip bring-up (RadioLib version, chip class, pin/TCXO/
RF-switch values, init call order, settle-time retry) byte-for-byte, wrapped
in ratspeak-handheld's own `LoRaInterface`/`BoardRadio` driver shape instead
of Meshtastic's own radio/mesh stack.

- [x] `RSRADIO_LR11X0` macro + two call sites (`RadioBandwidth.h`, `DeviceDiagnostics.cpp`)
- [x] `BoardRadio.h`: new `RATPAGER_LR1121` branch (needed for `LoRaInterface`'s
      `BoardRadio*` constructor to resolve to the right concrete type)
- [x] `src/core/radio/LR1121Radio.h` / `.cpp` (new files, Meshtastic-matched init)
- [x] `src/boards/tpager/main.cpp`: `#if defined(RATPAGER_LR1121)` branches
      (radio construction via `BoardRadio`, boot-log chip name, SX1262-only
      diagnostic register probes skipped)
- [x] `src/boards/tpager/config/BoardConfig.h`: LR1121 `#ifdef` block
- [x] `platformio.ini`: `[tpager_lr1121_base]` + `[env:tpager_lr1121]`
- [x] `tools/release_boards.json`: added `tpager_lr1121` to the `tpager` board's
      `app_environments` — turned out to be a real build blocker (the
      post-build `merge_firmware.py` step looks up board metadata by env name),
      not deferrable as originally thought
- [x] Build verify: `tpager_lr1121`, `tpager`, `tpager_cooperative`, `m9` all
      compile and link clean (RAM/Flash usage all sane, no regressions). All
      four initially hit an identical **pre-existing** failure in the final
      `esptool merge-bin` packaging step: PlatformIO's venv had a stale/broken
      esptool install (`pip show` claimed 5.1.2, but the actual code on disk
      and what ran was real esptool 4.5.1, old-style `merge_bin`). Fixed by
      `~/.platformio/penv/bin/python -m pip install --upgrade esptool` (now a
      genuine 5.6.0) — this was a Mac-wide PlatformIO venv issue, not caused
      by or specific to this change. `pio run -e tpager_lr1121` and
      `pio run -e tpager` both now succeed fully end to end, merged `.bin`
      written for both.
- [x] `check_runtime_boundary.py` passes (`PASS: LVGL views, owner-only board
      loops, shared display SPI`)
- [x] `src/core/runtime/FactoryResetRecovery.h`: fixed a pre-existing input
      race, found while testing on real hardware — every screen repaint
      called `keyboard.discardPending()`, which drains the keyboard's
      hardware FIFO, not just a software flag. On the SECOND+ repaint (e.g.
      after pressing `r` to reach the confirm prompt), that discard could eat
      a fast, deliberate follow-up keypress (a quick `r` then `Enter`) before
      it was ever read, silently no-op'ing the erase. Now only the very first
      paint discards (guarding the originally-intended case: a stale keypress
      queued before the screen ever appeared); later repaints, caused by the
      user's own key press inside the loop, no longer self-sabotage. Shared
      by all boards — rebuilt `tpager_lr1121`, `tpager`, `m9` clean after.
- [x] `src/core/runtime/FactoryResetRecovery.h`: added `Serial.printf("[RESET]
      %s\n", detail)` right after a `performFactoryReset()` attempt — the race
      fix above got the Enter keypress registering correctly, but the actual
      erase is now failing at some `ResetStep` (SD/NVS/internal — see
      `FactoryReset.cpp`'s `ResetResult::detail()`) and silently bouncing back
      to the review screen. This surfaces which step in the serial log instead
      of relying on reading the small on-device screen text.
- [x] Diagnosed `[RESET] Reset intent could not be saved`: this physical
      T-Pager had leftover storage-partition content from a prior firmware
      with a different partition layout — LittleFS couldn't mount it, so the
      in-app reset couldn't even write its own "in progress" marker (chicken-
      and-egg). Fixed by a full chip erase (`esptool erase-flash`, needs the
      stub loader — `--no-stub` fails with "ROM does not support
      erase_flash") followed by a clean `pio run -e tpager_lr1121 -t upload`.
      Not a firmware bug; standard remedy when a chip's partition layout
      changes between flashes.
- [x] **Flashed and verified on real hardware.** Boots clean — no reset-
      recovery screen, no boot loop. Confirmed receiving real LoRa traffic
      over the LR1121 radio: `[LORA_IF] RX: RSSI=-83 dBm, status=0x04` in the
      serial log. SPI bus, RF-switch table, and antenna path all working.
      TX (actually sending/announcing) not separately confirmed yet — next
      thing to check in normal use.
- [x] Updated the Europe radio-region preset (`src/core/config/Config.h`,
      `src/ui/lvgl/screens/LvSettingsScreen.cpp`) from a plain 868.000 MHz /
      250 kHz / SF9 / CR5 default to 869.463 MHz / 125 kHz / SF8 / CR 4/5 /
      22 dBm — the de facto common combination across the EU Reticulum/RNode
      community (per reticulum.site/lora's observed-settings data), not a
      regulatory requirement. 22 dBm chosen deliberately over 27: it's the
      real ceiling for both SX1262 (clamps silently at 22) and LR1121
      (RadioLib rejects anything above 22 instead of clamping — asking for 27
      would just fail the call, not "default to highest"). One shared
      "Europe" entry, not split by country — the value isn't Netherlands-
      specific, it's the EU-wide duty-cycle-free sub-band convention.
      Americas/Australia/Asia unchanged. Build-verified across all five envs
      (`tpager_lr1121`, `tpager`, `tdeck`, `cardputer`, `m9`).
