# H752-02 Qualification Evidence and Physical Result Record

## Scope and current status

- Product under test: **H752-02**, meaning 915 MHz SX1262 + GPS per LILYGO's
  product listing.
- Firmware scope: safe staged board qualification plus retained field UI shell.
- Physical-device status at repository creation: **NOT TESTED / NOT REPORTED**.
- No result below may be changed to PASS solely because compilation succeeds.

## Candidate definitions and sources

Upstream repository: <https://github.com/Xinyuan-LilyGO/T5S3-4.7-e-paper-PRO>

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
  `MODE_GC16` updates. The complete page is composed before panel power-on and
  high voltage is powered off after every update. Partial refresh is OFF.
- Normal boot/page changes do not full-clear. `display clear` is an explicit,
  once-per-boot LILYGO-derived cleanup operation for genuine recovery only.
- GPS diagnostics redact NMEA payloads and coordinates by default.
- Touch navigation stays locked until `touch corners` physically qualifies the
  inverted-portrait transform at all four corners.
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
| ED047TC1 display | `display test` + visual check | **UNREPORTED** | |
| Touch | `touch corners` | **LOGIC VERIFIED / PHYSICAL CHECK REQUIRED** | |
| RTC / charger / gauge | I²C identity first | **UNREPORTED** | |
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
