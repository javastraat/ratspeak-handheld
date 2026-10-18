# SenseCAP Indicator D1L board — status & handoff

Self-contained status doc — written so this can be picked up on a different
machine or after a break, without needing the conversation that produced it.

## What this is

Adding a real `sensecap_d1l` board to ratspeak-handheld (radio, display,
touch, pin config, new PlatformIO env), plus two shared LVGL UI gaps that
block any touch-only, keyboard-less device: no on-screen way to type text,
and no way to delete a saved contact/node by touch.

Hardware facts (pins, I2C expander address, the display CS-assertion fix,
RF-switch table, TCXO voltage) are taken from an already-completed,
real-hardware port of this *same physical device* in a separate project,
`~/Software/meshcore-dev` (different firmware, MIT-licensed — facts reused,
no code copied, same reasoning as the earlier LR1121 T-Pager work in this
repo). If that repo isn't present on whatever machine picks this up, the key
facts are repeated below so nothing is lost.

## Current status: hardware bring-up CONFIRMED WORKING on real hardware

Both the shared UI fixes and the new board scaffold are done, build-verified
across all 8 environments (zero regression), and the new board has been
flashed to the real unit and tested — radio, display, and touch all
confirmed working, including across a genuine USB power-cycle (not just a
flash-triggered reset).

**Not yet done: Phase 2 — the full LXMF app** (LVGL UI, screens, hotkeys,
protocol runtime, settings, WiFi). The current `main.cpp` is a deliberately
minimal, LVGL-free bring-up that only proves the new hardware drivers work.
See "What's left" below.

## Files added/changed this round

Shared UI work (benefits T-Deck too, inert on non-touch boards):
- `src/ui/lvgl/lv_conf.h` — `LV_USE_KEYBOARD` 0→1
- `src/ui/lvgl/LvVirtualKeyboard.h`/`.cpp` — new. Layout-only `lv_keyboard`;
  synthesizes `KeyEvent`s through the existing `dispatchKey()` path instead
  of touching any screen's text storage.
- `src/ui/lvgl/UIManager.h` — new `LvScreen::wantsVirtualKeyboard()` virtual
- `src/core/runtime/LvglApplication.h`/`.cpp` — new public `injectKeyEvent()`
- `src/ui/lvgl/screens/LvNameInputScreen.h`, `LvSettingsScreen.h`,
  `LvNodesScreen.h`, `LvMessagesScreen.h`, `LvMessageView.h`/`.cpp` — each
  gets a `wantsVirtualKeyboard()` override (`LvMessageView` via a new
  `composerActive()` helper, mirroring `handleKey()`'s existing fallthrough
  condition without touching it)
- `src/ui/lvgl/screens/LvContactsScreen.cpp` — native `LV_EVENT_LONG_PRESSED`
  on rows (calls existing `handleLongPress()` as-is; rows keep normal LVGL
  click-focus) + `_confirmDelete` guard on the `CLICKED` handler
- `src/ui/lvgl/screens/LvNodesScreen.h`/`.cpp` — long-press branch added to
  the existing `LV_EVENT_ALL` row handler (these rows clear
  `LV_OBJ_FLAG_CLICK_FOCUSABLE`, so it sources the target hex from the
  already-press-time-pinned `_pressedHex`, not group focus); extracted
  `handleLongPress()`'s body into `confirmOrOpenAction(hex)`, shared by the
  encoder/keyboard path and the new touch path; new `_longPressFired` flag
  guards `CLICKED` the same way

New `sensecap_d1l` board:
- `src/boards/sensecap_d1l/config/BoardConfig.h` — pins/addresses (see below)
- `src/core/hal/IOExpander.h`/`.cpp` — new, independent TCA9535/PCA9535 driver
- `src/core/radio/SensecapSX1262Radio.h`/`.cpp` — RadioLib's `SX1262` class
  wrapped behind a custom `Hal` (`SensecapRadioHal`) that redirects
  CS/RESET/BUSY/IRQ through the IO expander; same pattern as the existing
  `LR1121Radio` in this repo
- `src/boards/sensecap_d1l/hal/Display.h`/`.cpp` — ST7701 480×480 RGB/DPI
  panel via LovyanGFX's `Bus_RGB`/`Panel_ST7701`; `begin()` asserts the
  panel's expander-routed CS bit low before `_gfx.init()`'s bit-banged init
  commands, high after — the confirmed-necessary fix, built in from the
  start (see "Hardware facts" below)
- `src/core/hal/TouchInput.cpp`/`.h` — new `RSSENSECAPD1L` branch
  (`readFT6336()`) alongside the existing GT911 one; plain `Wire`, not
  LVGL/LGFX's native touch driver (confirmed to break the shared I2C bus on
  this hardware in meshcore-dev's own port)
- `src/boards/sensecap_d1l/hal/Keyboard.h` — new, trivial no-op stub. Exists
  only because `UIManager.h` unconditionally `#include`s a per-board
  `hal/Keyboard.h` even though it never references the type, and this board
  has no physical keyboard at all
- `src/boards/sensecap_d1l/main.cpp` — new. Deliberately minimal: no
  LVGL/UIManager/app wiring yet, just I2C/expander/display/radio/touch
  bring-up + a raw-LovyanGFX status screen + serial RX logging. See its own
  header comment and "What's left" below for why and what Phase 2 needs to
  add.
- `src/core/radio/BoardRadio.h` — new `RSSENSECAPD1L` branch
- `src/core/radio/RadioBandwidth.h`, `src/core/diagnostics/DeviceDiagnostics.cpp` —
  *not* touched for this board (DeviceDiagnostics.cpp is excluded from this
  env's build entirely for now — see build_src_filter notes below)
- `platformio.ini` — new `[sensecap_d1l_base]` + `[env:sensecap_d1l]`
- `tools/release_boards.json` — new `sensecap_d1l` entry (`web_flasher:
  false` — not a real release artifact yet)

## Hardware facts (confirmed, reused from meshcore-dev)

- I2C bus: `SDA=39`, `SCL=40`
- IO GPIO expander: TCA9535/PCA9535-family, **address `0x20`** (confirmed by
  a real I2C bus scan on this exact unit — Meshtastic's own pin definitions
  for this board have this wrong, `0x40`; don't trust that source for this
  address), IRQ on GPIO `42` (shared across *all* expander inputs, not just
  the radio's DIO1 — see `SensecapRadioHal::attachInterrupt()`'s comment)
- Radio SPI (real GPIOs): `SCK=41`, `MISO=47`, `MOSI=48` — shared physically
  with the display's one-time bit-banged init commands; display must fully
  init *before* the radio claims these pins for its real SPI peripheral
- Radio pins (virtual, via expander): CS=bit0, RESET=bit1, BUSY=bit2,
  DIO1/IRQ=bit3
- Display panel CS/RESET: expander bits 4/5 (confirmed: CS must be asserted
  **low before** the ST7701's bit-banged init commands run, **high after** —
  without this, the display only works by electrical coincidence and fails
  specifically across a real power cycle, not a flash-triggered reset. This
  was meshcore-dev's hardest-won bug; built in here from the start, not
  rediscovered)
- Display: ST7701, 480×480, RGB/DPI bus (`Bus_RGB`/`Panel_ST7701` in
  LovyanGFX) — `pclk`=GPIO21, `henable`=GPIO18, `hsync`=GPIO16, `vsync`=GPIO17,
  `d0..d15` = GPIO 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0; backlight on GPIO45
- Touch: FT6336U-family, I2C address **0x48** (confirmed via the same real
  I2C scan — not 0x38, an earlier wrong guess corrected during this work).
  Touch chip's native coordinate frame is rotated 180° from the display's
  logical frame — both axes inverted in `readFT6336()` (confirmed via real
  measured taps in meshcore-dev, not theory)
- One physical button, GPIO38 (`PIN_USER_BTN`) — role not yet used here
- Flash: 8MB (confirmed real hardware, not guessed — this is *not* the same
  as "no PSRAM"; PSRAM is confirmed present and used for the display's
  framebuffer)
- **USB**: this board's ESP32-S3 has no native USB wired to the host — only
  a CH340 bridge on UART0. Confirmed on real hardware: use
  `ARDUINO_USB_CDC_ON_BOOT=0`/`ARDUINO_USB_MODE=0` (same unflag/flag pair
  `m9_base` already uses for its own CH340 bridge), or `Serial` output
  silently vanishes into an unconnected native-USB peripheral. The device
  also exposes a *second*, unrelated USB-serial port from its RP2040
  co-processor, still running Seeed's stock sensor-polling firmware
  (`GET DATA FROM AHT20`, etc.) — easy to confuse with the ESP32-S3's own
  port; don't flash or debug against that one.
- **Unverified in both projects** (no schematic, just "didn't blow up"):
  TCXO voltage `2.4V`, DIO2-as-RF-switch enabled. RadioLib's SX1262
  `begin()` auto-retries at 0V TCXO on failure, so a wrong guess fails safe.

## How to build / flash (commands actually used)

```bash
cd ~/Software/ratspeak-handheld
pio run -e sensecap_d1l                                             # build only
pio run -e sensecap_d1l -t upload --upload-port /dev/cu.usbserial-11210   # flash (CH340 port)
```

Serial monitor (same CH340 port, 115200 baud) — the native-USB port
(`/dev/cu.usbmodem*`) is the *other* chip (RP2040), not this firmware:

```bash
python3 -c "
import serial, time
ser = serial.Serial('/dev/cu.usbserial-11210', 115200, timeout=1)
end = time.time() + 15
while time.time() < end:
    line = ser.readline()
    if line: print(line.decode('utf-8', errors='replace'), end='')
"
```

Regression check after any future change (run before trusting anything):

```bash
pio run -e tdeck -e tdeck_cooperative -e tpager -e tpager_cooperative -e tpager_lr1121 -e cardputer -e m9 -e sensecap_d1l
```

## What's left

1. **Radio TX** — not yet exercised. RX and `isRadioOnline()` are confirmed;
   sending a packet from this board hasn't been tried.
2. **Phase 2: the full LXMF app.** Current `main.cpp` is intentionally
   minimal/LVGL-free. Bringing in the real app means, roughly, adapting
   T-Pager's `main.cpp` (`src/boards/tpager/main.cpp`) feature by feature —
   screens, hotkeys, protocol runtime, settings, WiFi — while stripping
   what T-Pager has that this board doesn't (audio, GPS, SD, physical
   keyboard/scrollwheel). Concretely this also means:
   - Restore `${common.extra_scripts}` in `platformio.ini` (the WiFi/RTC/TLS
     SDK-backport patch scripts + `merge_firmware.py` were dropped for this
     phase *only* because nothing referenced those SDK paths yet — see the
     note below). Also restore `UserConfig.cpp`/`SettingsTransaction.cpp`
     and the rest of `core/` (`runtime/`, `reticulum/`, `history/`,
     `transport/`, `storage/`, `protocol/`) and `+<ui/lvgl/>` to
     `build_src_filter` instead of the current narrow include-list.
   - Re-add `DeviceDiagnostics.cpp` and give it an `RSSENSECAPD1L` branch
     (same pattern as the existing `RSM9`/`RSRADIO_LR11X0` ones) once this
     board's radio driver is in real use by the app.
   - 480×480 is a new aspect ratio for this codebase's LVGL screens (vs.
     480×222 T-Pager or 320×240 T-Deck/M9) — real layout question, not
     assumed to just work; worth an actual look once screens are wired up.
3. **`LvSettingsScreen::handleLongPress()`'s touch gap** — same underlying
   issue as the Contacts/Nodes fix above, lower priority (Escape already
   covers most of its cases), not addressed this round.
4. **`InputSource` enum has no `Touch`/`Virtual` value** — synthesized
   on-screen-keyboard events are tagged `InputSource::Keyboard` for now,
   since nothing currently branches on `.source` in a way that breaks;
   revisit if that assumption turns out wrong.

### Why the build_src_filter looks narrow right now (context for Phase 2)

`UIManager.h` unconditionally needs a per-board `hal/Keyboard.h` (hence the
stub) and, transitively, almost every other `core/` subdirectory —
`runtime/`, `reticulum/`, `history/`, `transport/`, `storage/`, `protocol/`,
and the `ui/lvgl/screens/` tree — reaches the Rust FFI protocol headers
(`ratspeak_protocol.h`), which this minimal env doesn't link. Rather than an
ever-growing exclusion list against the whole `core/` tree,
`build_src_filter` was narrowed to an explicit include-list: `core/config/`
(minus `UserConfig.cpp`/`SettingsTransaction.cpp`), `core/hal/`,
`core/radio/` (minus the native `SX1262.cpp`, unused — this board uses
`SensecapSX1262Radio` instead), plus this board's own files. Phase 2 needs
to widen this back out alongside restoring the full app.

Also dropped from this env's `extra_scripts`: `patch_wifi_sockets_pio.py`,
`patch_wifi_runtime_pio.py`, `merge_firmware.py`. All three assume every
build uses WiFi/RTC/TLS SDK backports (true for every other board today) and
fail their own "exactly once" sanity checks when nothing references those
paths at all. `pio run -t upload` works fine directly against the plain
`firmware.bin` without the merged factory image for now.

## Note: this file is currently untracked (not committed)

`git status` shows `TODO.md` as untracked, same as the rest of this round's
new/changed files. If picking this up on a *different* machine, either
commit+push first, or copy the working tree across manually — this file
alone won't travel with the repo on its own.
