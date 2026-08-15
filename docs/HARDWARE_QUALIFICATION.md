# H752-02 Qualification Evidence and Physical Result Record

## Scope and current status

- Product under test: **H752-02**, meaning 915 MHz SX1262 + GPS per LILYGO's
  product listing.
- Firmware scope: safe staged board qualification plus retained field UI shell.
- Latest physical-device evidence: **CORRECTED HOME PHYSICALLY ACCEPTED**.
- The original authoritative photograph showed severe retained blocks/horizontal bands,
  a line crossing text, header overlap, thin monospaced-looking typography,
  insufficient grayscale separation, clipped/detached labels and values, and an
  unusable bottom navigation. After framebuffer correction, reinforced Inter
  antialiasing, normalized DEVICE icon weight, and one guarded recovery clear,
  the operator accepted typography, icons, clipping, and background cleanliness.
- No result below may be changed to PASS solely because compilation succeeds.

## Candidate definitions and sources

Upstream repository: <https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO>

Official component references used for the bounded battery contract:

- TI BQ27220 product page and datasheet: <https://www.ti.com/product/BQ27220>
- TI BQ25896 product page and datasheet: <https://www.ti.com/product/BQ25896>
- LILYGO T5 E-Paper S3 Pro repository: <https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO>

The firmware's `0x6B` BQ25896 address is the explicit fitted-board/task contract
to be checked on the device. It must not be generalized from a different charger
variant or silently changed based only on a generic datasheet address.

Revision inspected: `5067e1fd6a66cf8b06e0b484070dc1b405eac1aa`.

Product evidence: the official LILYGO store page identifies SKU `H752-02` as
“915Mhz With GPS [H752-02]”. The same page lists SX1262 and supports 915 MHz.
The SKU suffix therefore selects the fitted radio band/GPS option; it is not an
electrical-definition branch.

### Default: current-Pro candidate

The `h752_02_candidate` environment uses the official repository's current
`H752-01` implementation as the likely electrical baseline:

| Function | Candidate | Official source |
|---|---:|---|
| I²C SDA / SCL | 39 / 40 | `H752-01:docs/pin_define.md` |
| SPI MISO / MOSI / SCLK | 21 / 13 / 14 | same |
| SD CS | 12 | same |
| SX1262 CS / DIO1 / RESET / BUSY | 46 / 10 / 1 / 47 | same |
| GPS MCU RX / TX | 44 / 43 | same and `examples/GPS` |
| PCA9535 | I²C `0x20` | README, pin map, IO example |
| LoRa/GPS rail enable | PCA9535 P0.0 | `H752-01:docs/pinmap.md` |
| E-paper baseline | `epd_board_v7`, ED047TC1, 960×540 | official display test |
| PlatformIO board manifest | `boards/T5-ePaper-S3.json` | official H752-01 repository |

This is a **candidate**, not confirmation that the physical unit matches.

### Why H752 definitions are not silently mixed

The older official `H752` branch is materially different:

| Function | Legacy H752 source |
|---|---:|
| I²C SDA / SCL | 6 / 5 |
| SPI MISO / MOSI / SCLK | 8 / 17 / 18 |
| SX1262 CS / DIO1 / RESET / BUSY | 46 / 3 / 43 / 44 |

Most importantly, GPIO43/44 are radio RESET/BUSY on legacy H752 but GPS TX/RX
on current H752-01. Combining those definitions is unsafe and invalid. The
legacy source does not provide GPS pins, so `legacy_h752_comparison` refuses GPS
and current-Pro display tests rather than guessing.

## Conservative test behavior

- Boot only drives SD CS and LoRa CS high to deselect the shared SPI devices.
- I²C starts at 100 kHz.
- Shared peripheral power requires the explicit `rail on` command.
- `lora probe` uses 915 MHz, 10 dBm configuration, performs no transmit, then
  sleeps. `lora rx` is receive-only.
- `gps listen` opens RX only (`TX = -1`) and sends no configuration commands.
- The UI uses the verified current-Pro `epd_board_v7` / ED047TC1 baseline,
  inverted portrait rotation, the unchanged 1560 mV VCOM API value, and full
  `MODE_GC16` updates. Every page starts by whitening the complete 259,200-byte
  packed framebuffer, renders into a separately guarded PSRAM buffer, verifies
  64-byte canaries, and only then copies the complete frame to EPDiy. High voltage
  is powered off after every update. Partial refresh is OFF.
- UI cleanup revision 10 records the current startup contract. One guarded
  LILYGO-derived `epd_fullclear()` runs on every boot because the bistable panel
  can retain an image across MCU power loss. The in-RAM guard still limits this
  to one panel-wide clear per boot; normal navigation never full-clears.
- A dedicated full-screen startup page advances through completed hardware,
  worker-start, connecting, and ready milestones. Each visible step is a complete
  known-white `MODE_GC16` composition, never a region or partial update. Guardian
  and weather workers begin only after the first high-voltage cycle powers off,
  run in the background, and receive a bounded five-second initial settling
  window. The percentages are milestone labels, not elapsed-time predictions.
- The sourced candidate SPI/radio pins overlap EPD v7 parallel-data GPIOs.
  Display refresh therefore pauses receive mode and releases Arduino SPI GPIO
  ownership first. This is a candidate-profile mitigation, not proof that the
  SKU's final electrical map is confirmed.
- GPS diagnostics redact NMEA payloads and coordinates by default.
- Battery sampling is read-only and bounded. BQ27220 `0x55` StateOfCharge `0x2C`
  is read twice, little-endian, and accepted only when both values agree in
  `0..100`. BQ25896 board address `0x6B` `REG0B` is read for `CHRG_STAT[4:3]`.
  There are no gauge/charger configuration, control, seal, reset, calibration, or
  data-memory writes. Sampling is 90 seconds normally, 45 seconds while charging,
  and only in scheduled awake windows. A still-fresh validated SOC may be retained
  across a failed attempt while the state reports error; after 270 seconds without
  a valid SOC the UI reports stale and hides the percentage. This contract does
  not prove pack capacity accuracy or replace physical charge/discharge testing.
- The clock defaults to UTC/24-hour, uses NTP after existing Wi-Fi connectivity,
  and treats a PCF8563 voltage-low indication as invalid. Weather is an isolated,
  Wi-Fi-passive Open-Meteo consumer with sanitized diagnostics and a 15-minute
  minimum poll interval.
- The target device's physical four-corner touch qualification is complete and
  persisted in NVS. Typography firmware uploads preserve it; do not reset or
  repeat qualification unless the operator explicitly requests it.
- On first run, unqualified touch opens a guided on-device four-corner setup.
  Qualification is saved once in ESP32 Preferences/NVS and can be repeated from
  Device. `touch corners` remains the safe serial fallback. The current sourced
  board profile does not identify a GT911 interrupt GPIO, so touch is polled and
  diagnostics explicitly report IRQ observation as unavailable rather than
  guessing a pin.
- Touch is routed before GPS, weather-position publication, low-power/network
  policy, time, and battery polling. The display coordinator owns a one-slot
  priority latch where navigation outranks and background demand coalesces.
  Physical GC16 remains synchronous, so polling cannot observe a touch that begins
  and ends entirely inside `epd_hl_update_screen()`. Post-update report draining
  prevents replay but is not a guarantee that one next navigation action is retained.
- Low-power presets OFF/5/15/30/60 are persisted in `sf_power`. Active presets use
  45-second scheduled windows, suspend workers between windows, turn Wi-Fi off only
  after both workers are idle, sleep LoRa RX, and disable the shared rail unless GPS
  weather requires it. Policy rail changes preserve the user's GPS preference.
  Guardian confirmed-offline/auth conditions hold services awake. This is timer
  monitoring only: no GT911 wake IRQ is sourced and no deep sleep is used.
- Timezone setup presents five non-overlapping options, descriptions, selected
  markers, and Back using full-frame clearing. Compact Settings cards use a separate
  reflow rather than shrinking/clipping the shared layout.
- Weather configuration persists city, region, country, and postal metadata. HOME
  shows a meaningful selected location; GPS/manual labels preserve coordinate
  privacy. Detail current fields and the condition icon are invalidated on location
  or unit changes. Logs exclude postal/city query, coordinates, URLs, and bodies.

## Performance diagnostics qualification

Release default is `SUPPORTFORGE_PERF_DIAGNOSTICS=0`. Build the single intended
qualification image with `pio run -e h752_02_diagnostics`; that environment inherits
the candidate board flags and sets the macro to `1`. Do not enable coordinate touch debug.
The expected allowlisted metrics are press-to-action, accepted/debounced/dropped/
coalesced input counts, render wait, physical GC16 operation duration, navigation
latency, worker heartbeats, queue high-water marks, heap/PSRAM, and reset/watchdog
classification. Review logs for accidental credentials, endpoint URLs, coordinates,
postal/city queries, tokens, and request/response bodies before retaining evidence.

GC16 duration and software/input delay must be reported separately. A long physical
waveform is not evidence that touch routing waited on networking, GPS, I²C, NTP, or
weather. Conversely, polling cannot measure a contact never observed during the
synchronous update.

## UI rendering fidelity

`ui/ui_spec.json` is the authoritative contract for portrait dimensions,
framebuffer format, geometry, palette, navigation, touch thresholds, font roles,
and declared text regions. `tools/generate_ui_contract.py` generates the C++
constants. Firmware embeds final-size 4-bit coverage assets from genuine Inter
SemiBold and Bold files under OFL-1.1. Primary roles use physically reinforced
antialiasing selected from the panel qualification page: solid cores remain exact
black while only strong edge coverage is retained as near-black. Hardware Diagnostics also embeds Atkinson Regular
and Inter Regular/Medium/SemiBold/Bold comparison samples using both deliberate
strong antialiasing and monochrome strategies. Text is measured from generated
advances and clipped to explicit component bounds; bitmaps are never scaled.

`tools/render_ui_previews.py` uses the same approved TTF and role sizes, geometry,
and palette, but remains an independent Pillow renderer. Its PNGs are therefore
**design previews only** and structurally equivalent, not framebuffer-identical.
`tools/framebuffer_to_png.py` converts an actual 960×540 packed 4-bit firmware
framebuffer dump and is the preferred pixel-authoritative artifact. Neither form
proves physical ED047TC1 waveform response, ghosting history, optics, or ambient
behavior; those require direct physical acceptance.

## Required physical UI acceptance after upload

Do not promote the corrected UI to physically verified from compilation, serial
output, or PNG review. Ask the operator to confirm all items:

1. Old blocks and bands are gone.
2. No horizontal line crosses text.
3. The header contains no overlap.
4. Cards are visibly distinct.
5. Physical typography resembles the approved design preview.
6. Every label remains visibly associated with its value.
7. All five navigation items are visible.
8. Guided touch setup accepts all four corners.
9. Bottom navigation works.
10. No stale content appears after one page change.
11. Battery transitions clear correctly at `100→99`, `99→9`, `9→--`, and `--→valid`.
12. Battery percentage remains legible across black fill and white unfilled regions;
    the static charging mark remains visible in both.
13. USB-C, wireless/MagSafe-style charging if fitted, unplugged, and discharge states
    match observed behavior without claiming precision beyond the gauge SOC.
14. All five timezone labels/descriptions, selected markers, touch targets, and Back
    are visible and aligned.
15. HOME shows the actual city/region or postal value; Weather Detail shows source,
    freshness, useful current fields, and no stale icon after Save & Fetch/unit change.
16. OFF/5/15/30/60 low-power presets persist; status says timer monitoring and
    `EXIT LOW POWER` works. Confirm monitoring stops if physical power is removed.
17. Diagnostics timing logs separate press-to-action/render wait/navigation latency
    from GC16 duration and contain no private configuration.
18. Every cold/restart boot visibly clears the entire panel, shows the polished
    milestone progress page without retained blocks or region-only artifacts, and
    transitions to HOME/TOUCH SETUP after the bounded connection window.

Hardware Diagnostics includes a temporary labeled grayscale qualification strip
for identifying which theme levels remain physically distinct. It is not shown
on HOME.

## Vendored EPDiy provenance

- `lib/epdiy` is a trimmed, byte-for-byte snapshot of the official LILYGO
  repository's `H752-01:lib/epdiy` tree at revision
  `5067e1fd6a66cf8b06e0b484070dc1b405eac1aa`. Only non-build bulk directories
  (`examples`, `hardware`, `doc`, `scripts`, and `test`) were omitted; the
  original metadata and LGPL-3.0 license are retained. See `lib/epdiy/UPSTREAM.md`.
- LILYGO's snapshot declares epdiy `2.0.0`, but it materially differs from stock
  upstream epdiy 2.0.0 in the V7 board/power and LCD-output implementation. It
  is therefore intentionally vendored as a coherent library rather than claimed
  to be identical to stock upstream.

## Physical result record

Fill this from captured serial output and direct observation. Do not infer.

| Item | Candidate / test | Physical result | Evidence |
|---|---|---|---|
| PCB/revision marking | Photograph/inspect | **UNREPORTED** | |
| MCU boot and PSRAM | Boot log | **UNREPORTED** | |
| I²C bus 39/40 | `i2c` | **UNREPORTED** | |
| PCA9535 and shared rail | `rail on` | **UNREPORTED** | |
| SX1262 SPI identity | `lora probe` | **UNREPORTED** | |
| 915 MHz receive | `lora rx` + known transmitter | **UNREPORTED** | |
| L76K UART activity | `gps listen` | **UNREPORTED** | |
| GPS valid outdoor fix | inspect NMEA | **UNREPORTED** | |
| ED047TC1 display | revision-10 every-boot clear + full-screen startup progress + corrected pages | **PHYSICAL RECHECK REQUIRED** | Previous HOME was accepted; startup sequence and revised battery/weather/timezone/low-power pages require direct acceptance |
| Touch | persisted four-corner result | **PHYSICALLY VERIFIED** | Qualification retained in NVS; do not reset during typography work |
| RTC / charger / gauge | I²C identity first | **UNREPORTED** | |
| Battery USB-C / wireless / unplugged / discharge | Observe read-only SOC and charge class | **PHYSICAL CHECK REQUIRED** | Do not infer from build or unit tests |
| Timezone five-option layout | Settings → Timezone | **PHYSICAL CHECK REQUIRED** | Longest descriptions, marker, Back, touch alignment |
| Weather meaningful location | HOME + Weather Detail | **PHYSICAL CHECK REQUIRED** | City/region or postal; no coordinate disclosure |
| Timer-monitoring low power | OFF/5/15/30/60 + exit | **PHYSICAL CHECK REQUIRED** | Not deep sleep; removal of power stops monitoring |
| Performance diagnostics | 60-second allowlisted PERF records | **PHYSICAL CHECK REQUIRED** | Separate GC16 duration from software latency |
| SD card | `sd test` read-only mount | **PHYSICAL CHECK REQUIRED** | |

## Reporting template

```text
Device label/SKU:
PCB markings:
Power source and measured current:
Firmware Git commit:
PlatformIO environment:
Serial capture path:

Command:
Candidate definition and source:
Observed serial result:
Observed physical result:
PASS / FAIL / INCONCLUSIVE:
Notes and photos:
```
