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

## Current status: Phase 2 (full LXMF app) CONFIRMED WORKING on real hardware

Everything is confirmed working end-to-end on the real unit: radio TX/RX,
display, touch, on-screen keyboard, wake-from-sleep (touch and the physical
button), touch-only settings editing, LXMF announce and message delivery in
both directions, and peer discovery — all tested against another real device
(Meshpoint dashboard showed `PD2EMC D1L` with `lxmf.delivery`, and messages/
announces now flow both ways). `main.cpp` is the same 4-line shim into
`src/core/runtime/LvglApplication.cpp` that `tdeck`/`m9` use. Build-verified
across all 8 environments throughout (zero regression) every step of the way.

See "Phase 2 implementation notes" for what had to change in shared files to
get Phase 2 wired in at all (D1L is the first board on this runtime routed
through an I2C GPIO expander instead of real GPIOs), and "Phase 2 post-flash
fixes" for a second round of bugs found only by actually running the real
app on real hardware — including a serious one (RX silently never worked,
only TX did) with a root cause already independently confirmed in
meshcore-dev's own port of this exact board.

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
- `src/boards/sensecap_d1l/main.cpp` — now the 4-line `LvglApplication.cpp`
  shim (was a deliberately minimal, LVGL-free bring-up main during the
  earlier hardware-confirmation phase — see "Phase 2 implementation notes")
- `src/core/radio/BoardRadio.h` — new `RSSENSECAPD1L` branch
- `platformio.ini` — new `[sensecap_d1l_base]` + `[env:sensecap_d1l]`
- `tools/release_boards.json` — new `sensecap_d1l` entry (`web_flasher:
  false` — not a real release artifact yet)

## Hardware facts (confirmed, reused from meshcore-dev)

- I2C bus: `SDA=39`, `SCL=40`
- IO GPIO expander: TCA9535/PCA9535-family, **address `0x20`** (confirmed by
  a real I2C bus scan on this exact unit — Meshtastic's own pin definitions
  for this board have this wrong, `0x40`; don't trust that source for this
  address), IRQ on GPIO `42` (shared across *all* expander inputs, not just
  the radio's DIO1 — see `SensecapRadioHal::attachInterrupt()`'s comment,
  and "Phase 2 post-flash fixes" below for why this isn't just a theoretical
  concern on this board)
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
  `d0..d15` = GPIO 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0; backlight on GPIO45.
  `offset_rotation=2` (confirmed correct, on-device: 1 was 90° off/"on its
  side", 2 reads upright)
- Touch: FT6336U-family, I2C address **0x48** (confirmed via the same real
  I2C scan — not 0x38, an earlier wrong guess corrected during this work).
  Touch chip's native coordinate frame is rotated 180° from the display's
  logical frame — both axes inverted in `readFT6336()` (confirmed via real
  measured taps in meshcore-dev, not theory). That inversion held up
  unchanged when `offset_rotation` moved 1→2 on this board — confirmed on
  real hardware (taps still land correctly), not re-derived; don't assume
  touch mapping needs to track every future display-rotation tweak
- One physical button, GPIO38 (`PIN_USER_BTN`) — wired as a second wake
  source (tap) and hold-to-sleep (~1.2s hold, mirrors every other board's
  trackball/encoder hold); confirmed working on real hardware. Also used
  during early bring-up to manually fire a test TX packet (confirmed via an
  RF-activity LED blink on another RT node) before the full app existed.
- Radio TX and RX: both confirmed working on real hardware, bidirectionally,
  against another device (see "Current status" above). RX did NOT work
  until a real driver bug was found and fixed — see "Phase 2 post-flash
  fixes" below; don't assume RX "just works" the same way TX did.
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

If the device ever lands on the keyboard-only `FactoryResetRecovery` screen
and seems stuck: it isn't anymore (see "Phase 2 post-flash fixes" — a touch
path was added), but if it somehow still won't budge, a full chip erase
clears it for good: `esptool.py --chip esp32s3 --port PORT erase-flash`,
then reflash. That's a last resort, not the normal path.

## What's left

1. ~~**Radio TX**~~ — confirmed working on real hardware, including through
   the full Phase 2 app (not just the early manual test).
2. ~~**Phase 2: the full LXMF app.**~~ Confirmed working end-to-end on real
   hardware. See "Phase 2 implementation notes" and "Phase 2 post-flash
   fixes" below for everything this actually took.
   - 480×480 is a new aspect ratio for this codebase's LVGL screens (vs.
     480×222 T-Pager or 320×240 T-Deck/M9) — hasn't caused a reported
     problem yet, but hasn't had a dedicated layout review either; keep an
     eye out for cramped/misaligned screens specifically on this board.
3. **`LvSettingsScreen::handleLongPress()`'s touch gap** — same underlying
   issue as the Contacts/Nodes fix above, lower priority (Escape already
   covers most of its cases), not addressed this round.
4. **`InputSource` enum has no `Touch`/`Virtual` value** — synthesized
   on-screen-keyboard events are tagged `InputSource::Keyboard` for now,
   since nothing currently branches on `.source` in a way that breaks;
   revisit if that assumption turns out wrong.
5. **`_textEditing`/`_freqEditing` touch gap** — the same "second tap on an
   already-editing row did nothing useful" bug fixed for `ENUM_CHOICE`/
   `INTEGER` settings (see "Phase 2 post-flash fixes") wasn't checked for
   text-field editing or the frequency cursor-digit editor. Not reported
   broken yet — worth checking if it turns out to have the same gap.

## Phase 2 implementation notes

D1L is the first board on `LvglApplication.cpp` (the runtime `tdeck`/`m9`
already share) routed through an I2C GPIO expander instead of real GPIOs —
porting it surfaced several assumptions that file and a few other shared
`core/`/`ui/` files had baked in, undetected until now because every board
on these paths so far had real GPIOs, an SD slot, and/or a physical
keyboard. Concretely, this round:

- Gave `LvglApplication.cpp` its own D1L branch for the boot sequence:
  `expander.begin()` → `display.begin()`/`beginLVGL()` → *then*
  `sharedSPI.begin()` + `radio.begin()` — the reverse of the generic
  radio-then-display order every other board uses, because D1L's display
  init and the radio's real SPI peripheral share physical pins 41/47/48 (see
  "Hardware facts" above). Reusing the generic order here would have quietly
  reintroduced the exact power-cycle bug that was already hunted down once.
- Added a new `IOExpander`-aware radio-construction branch alongside the
  existing `RSM9`/generic ones, and gated `sdStore.begin()` behind a new
  `#if HAS_SD` (previously unconditional — every other board on this path
  has real SD hardware, so nobody had hit this).
- New per-board files `hal/Power.h`/`.cpp`, `input/InputManager.h`/`.cpp`,
  `audio/AudioNotify.h` — these are per-board, not shared, and D1L never had
  versions of them before (the minimal bring-up `main.cpp` never used
  `Power`/`InputManager`/`AudioNotify` at all). `Power::batteryVoltage()` /
  `batteryPercent()` / `isCharging()` are honest stubs (return 0/false) —
  no confirmed battery ADC pin exists yet (see "Hardware facts"), so this
  deliberately doesn't guess one. `InputManager` is a new touch-only
  shape (`begin(Keyboard*, TouchInput*)`, no trackball) — the first of its
  kind in this codebase; `LvInput::init(Keyboard*, TouchInput*)` already
  supported this shape, it just had no caller.
- Extended several two/three-board `#if` gates in shared `core/`/`ui/` files
  to include `RSSENSECAPD1L`, each confirmed necessary by a real compile or
  link failure, not guessed preemptively: `core/compat/Tls.c` (wraps the
  *entire* corrected mbedtls translation unit — without this, the stock SDK
  archive member silently supplies the two TLS functions this project
  patched for a known bug, and `merge_firmware.py`'s `check_sdk_backports.py`
  catches it at link time), the Serial TX-buffer-sizing guards in
  `LvglApplication.cpp` and `DeviceDiagnostics.cpp` (D1L is CH340/UART like
  `RSM9`, same reasoning as that board's existing carve-out).
- `src/boards/sensecap_d1l/config/BoardConfig.h` gained `I2C_FREQUENCY` (the
  shared runtime references it as a macro; the old minimal `main.cpp` had
  the same value, `400000`, as a literal).

## Phase 2 post-flash fixes

The build-wired Phase 2 app from above was then actually flashed and run on
real hardware — this round is everything that only showed up once it was
running, not just compiling:

- **Boot deadlock on first flash — `FactoryResetRecovery.h`.** This
  pre-LVGL recovery screen (runs whenever `flash.resetState() != Clear`) is
  keyboard-only by design and never touched touch at all, so D1L had no way
  to ever leave it. Root cause of hitting it at all: the littlefs data
  partition still held leftover content from the stock Meshtastic firmware
  this unit ran before (`pio run -t upload` only rewrites the app partition,
  never the data partition), so the "blank partition → auto-format" path
  never triggered on first boot. Fixed two ways: (1) `esptool erase_flash` +
  reflash, a one-time fix for the stale partition itself; (2) added an
  optional touch path to `factoryResetRecovery()` (new `Touch* touch =
  nullptr` parameter, `Touch` template-defaulted to `void` so `tpager`'s and
  `cardputer`'s existing 5-arg calls are untouched) — top third of the
  screen advances (review, then confirm-erase), bottom third always backs
  out, so a stray tap can't trigger the destructive action by itself.
- **On-screen keyboard invisible.** No LVGL theme engine is initialized
  anywhere in this app (every screen styles its own objects directly against
  `Theme::*`), so `lv_keyboard_create()` — the first thing in this codebase
  to ever rely on LVGL's own default widget styling — rendered fully
  unstyled. `LvVirtualKeyboard::create()` now sets `Theme::BG_SURFACE`/
  `BG_ELEVATED`/`TEXT_PRIMARY`/etc. on it directly, matching every other
  screen's own styling calls.
- **Enter/Backspace keys showed as tofu boxes.** `LV_FONT_DEFAULT` is this
  app's own custom font (`lv_font_rsdeck_14`), which doesn't carry LVGL's
  symbol glyph range the keyboard's Enter/Backspace/etc. icons need. Fixed
  by setting `&lv_font_montserrat_16` (already enabled in `lv_conf.h`, used
  elsewhere in this codebase for exactly this reason) on the keyboard's
  `LV_PART_ITEMS`.
- **Screen wouldn't wake from touch or the physical button.** Two separate
  bugs: (1) `sensecap_d1l/input/InputManager.cpp`'s touch poll gated
  `_activity` behind `screenOn`, copied verbatim from T-Deck's equivalent —
  but T-Deck's real wake path is its keyboard (which explicitly bypasses
  that gate), not touch; touch is D1L's *only* wake source, so the gate
  silently made wake-by-touch impossible. Removed the gate — the existing
  `wakeOnlyInput`/`LvInput::setEnabled(screenWasOn)` machinery already
  prevents a stale touch from misfiring as a tap on the waking frame, so
  nothing else needed to change. (2) `PIN_USER_BTN` (GPIO38) was never
  `pinMode()`'d or polled anywhere in the Phase 2 app at all — added both
  (`InputManager::begin()`, debounced edge-trigger in `update()`) as a
  second wake source.
- **Settings screen: couldn't change an `ENUM_CHOICE` value (e.g. Region
  Preset) by touch.** Tapping a row to enter edit mode already worked
  (synthesizes `KeyEvent{.enter=true}` through `handleKey()`, pre-existing
  code) — but once editing, cycling the value only responds to left/right
  `KeyEvent`s, and the same row's tap handler unconditionally closed edit
  mode on any tap, so a second tap just closed and reopened it, never
  actually changing anything. Fixed in `LvSettingsScreen.cpp`'s row click
  handler: if the tapped row is the one already being edited, map tap
  X-position within the row to left-third/middle-third/right-third →
  synthesized left/enter/right `KeyEvent`, through the same `handleKey()`
  path. Confirmed end-to-end on real hardware: region set correctly, LXMF
  announce received by another device (Meshpoint dashboard showed
  `PD2EMC D1L` with `lxmf.delivery`).
- **RX never worked (TX did) — the big one.** D1L could send (confirmed:
  announces reached another device) but never received anything — not a
  settings/region mismatch, a genuine driver bug. Root cause, independently
  confirmed in meshcore-dev's own port of this exact board (see its
  `IndicatorRadioHal.h`/`.cpp`, `radio_poll_irq()`): the IO expander's
  shared IRQ line fires on *any* watched input changing, not just DIO1 —
  critically including BUSY, which RadioLib's SPI layer polls constantly, so
  it fires far more often than real packet-ready events. The original ISR
  (`SensecapRadioHal::attachInterrupt()`) called RadioLib's real
  packet-received callback directly on every one of those triggers; its SPI
  follow-up read could then land in the middle of the chip actively writing
  a real received packet, corrupting it before `parsePacket()` ever got a
  valid chance to read it — exactly meshcore-dev's own documented finding
  ("real adverts from another device were never received, despite the radio
  ... transmitting successfully, and the shared IRQ line firing
  constantly"). Also wrong in the original: the interrupt was attached with
  RadioLib's own requested polarity (active-high, correct for native DIO1)
  instead of the expander's actual open-drain active-low INT behavior, and
  `IO_EXPANDER_IRQ` had no pull-up configured.
  Fixed (`SensecapSX1262Radio.h`/`.cpp`), same shape as meshcore-dev's
  confirmed-working fix: the ISR now only sets a lightweight pending flag;
  a new `pollIrq()` — called every tick from `LoRaInterface::loop()`
  (`#if defined(RSSENSECAPD1L)`, right where `packetAvailable` is about to
  be read, so it runs on the same task and ordering) — checks DIO1
  specifically over I2C and only *then* invokes the real callback, filtering
  out BUSY-only noise. Attach polarity fixed to `FALLING` for the expander
  line regardless of what mode RadioLib requests, and a pull-up added on
  `IO_EXPANDER_IRQ`. One build hiccup along the way: the ISR trampoline
  being defined inline in the header (not the `.cpp`, unlike the pre-existing
  `SensecapSX1262Radio::interrupt()`) hit a real Xtensa linker error
  ("dangerous relocation: l32r: literal placed after use") — moved the
  definition to the `.cpp` to match the working pattern. Confirmed on real
  hardware: announces and messages now flow in both directions, and peer
  discovery works (D1L shows up as a peer on the other device too).

### New: button hold-to-sleep

Not a bug fix, a feature request confirmed working after the above: the one
physical button (`PIN_USER_BTN`) was wake-only (a tap bumps activity, same
as touch) — holding it while the screen was already on did nothing, unlike
every other board's trackball/encoder hold (which blanks the screen via
`dispatchLongPress()` → `powerMgr.forceScreenOff()`). `InputManager` now
tracks hold duration the same way `PointerInput` does for
trackball/encoder: ~1.2s threshold, and critically, a hold that *woke* the
screen doesn't also immediately sleep it again (`_btnFromScreenOn` must
still be true when the threshold fires) — same guard, same reasoning as
`PointerInput`'s own comment ("a wake hold must not wake the display again
after timeout"). `hadLongPress()` now reports this instead of always
returning `false`; no other board's code touched. Confirmed on real
hardware: hold sleeps, tap still wakes normally afterward.

## Note on this file's git status

This file is intentionally kept out of the public GitHub repo — it's a
local, free-form working doc, not something meant for the published
history. It's `.gitignore`'d; if it ever shows as tracked again (e.g. from
an older clone), `git rm --cached TODO.md` untracks it without touching the
file on disk.
