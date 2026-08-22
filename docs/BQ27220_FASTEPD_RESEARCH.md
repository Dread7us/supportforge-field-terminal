# BQ27220 recovery evidence and FastEPD compile-only spike

## Scope and immutable sources

This report is source-level research only. No battery data-memory write, unseal,
reset, calibration, configuration command, upload, front-light operation, or panel
partial update was performed.

- LILYGO T5 S3 4.7 Pro repository, branch `H752-01`, commit
  `587632e0c6ac327741b8c5e0d14c9e29f154b101`.
- LILYGO FastEPD repository, branch `main`, commit
  `95f8696466e386fce84dbe10edb8713a8a9be387` (library metadata version 1.3.0).
- The T5 repository also bundles a different FastEPD tree at Git object
  `4513b8ae5176bbfb42500f595c7a03b465aa32d5` (metadata version 1.4.0).
- Local production baseline: Field Terminal commit
  `5abf64b46cbc7c475a49a2b37a9dd556f6d79a3a` at the start of this investigation.

`src/secrets.h` is excluded from the evaluation environment and was not read.

### Verification record

- `python -m unittest discover -s test -v`: **179 tests passed**.
- `pio run -e h752_02_candidate`: **success**, with the production dependency graph
  unchanged and no FastEPD dependency.
- `pio run -e fast_epd_h752_02_compile_only`: **success** using the local pinned
  snapshot; no upload target was invoked.
- ELF symbol inspection confirms the deterministic smoke routine and its FastEPD
  init/full-update calls are linked despite never being called by `setup()`.
- All **11** copied upstream files match the SHA-256 values in `UPSTREAM.md`.
- `git diff --check`: **success**.

Both builds emit the existing Arduino ESP32 2.0.14 `uartSetPins` return-without-value
framework warning. PlatformIO also reports multiple installed Core versions and runs
Core 6.1.18 for these commands. Neither warning is introduced by this spike. The
initial remote Git dependency attempt and one stale parallel build-cache attempt
failed before a successful clean single-job build; the ordinary cached evaluation
rebuild then also passed.

## 1. BQ27220 evidence

### Official factory path

At the pinned T5 commit, `examples/factory/main/main.cpp` constructs `BQ27220`,
starts `Wire`, and calls no-argument `bq27220.init()` before charger and display
startup. `lib/BQ27220/bq27220.h` defaults that call to `gauge_data_memory`.
The standalone BQ27220 example follows the same path. This is **not** a read-only
initialization API.

`BQ27220::init()` in `lib/BQ27220/bq27220.cpp:202-278` does the following:

1. verify device number `0x0220`;
2. unseal even for protected inspection;
3. inspect `INITCOMP`, `CFGUPDATE`, and selected `BATT_ID` (expects profile 0);
4. compare every table record;
5. on any state/profile/data mismatch, issue RESET and wait up to 4 seconds for
   `INITCOMP`, request full access, enter configuration-update mode, write the
   complete table in order, exit with `EXIT_CFG_UPDATE_REINIT`, wait 2 seconds,
   and compare the complete table again;
6. seal and verify the sealed state.

The low-level writer sends the big-endian address/value payload through MAC data,
then checksum `0xFF - sum(address + data)` and total length. It waits 10 ms after
each write. The table loop accumulates failures but continues through all records.
Critically, `init()` ignores the Boolean returned by the write pass itself and
relies on the subsequent whole-table comparison. If the write pass fails,
`dateMemoryCheck(..., true)` exits configuration-update mode **only when its
accumulated result is true**. Therefore the factory implementation alone is not a
sufficient power-loss/failure-safe recovery tool.

### Exact table present in the official source

Addresses and values below are from `lib/BQ27220/bq27220_data_memory.{h,c}`.
Multi-byte values are represented here as numeric values; the library serializes
them in the device-required byte order.

| Address | Field | Type | Official value |
|---:|---|---:|---:|
| `0x929B` | GaugingConfig | 16-bit bitfield | `CCT=1, CSYNC=0, EDV_CMP=0, SC=1, FIXED_EDV0=1, FCC_LIM=1, FC_FOR_VDQ=1, IGNORE_SD=1, SME0=0` |
| `0x9206` | OperationConfigA | U16 | `0x0C8C` |
| `0x9208` | OperationConfigB | U8 | `0x4C` |
| `0x929D` | Full Charge Capacity | U16 | `1500` mAh |
| `0x929F` | Design Capacity | U16 | `1500` mAh |
| `0x92A3` | EMF | U16 | `3743` |
| `0x92A9` | C0 | U16 | `149` |
| `0x92AB` | R0 | U16 | `867` |
| `0x92AD` | T0 | U16 | `4030` |
| `0x92AF` | R1 | U16 | `316` |
| `0x92B1` | TC | U8 | `9` |
| `0x92B2` | C1 | U8 | `0` |
| `0x92BD..0x92D1` | StartDOD 0/10/20/30/40/50/60/70/80/90/100 | U16 | `4173, 4043, 3925, 3821, 3725, 3665, 3619, 3585, 3515, 3439, 2713` |
| `0x92B4` | EDV0 | U16 | `3031` |
| `0x92B7` | EDV1 | U16 | `3385` |
| `0x92BA` | EDV2 | U16 | `3501` |
| `0x91DE` | Calibration Current Deadband | U8 | `1` |
| `0x9217` | Sleep Current | I16 | `1` |

That order is the exact source order and must not be casually reordered. There is
no chemistry-ID selection or Qmax record in this table. It is a CEDV parameter
image, not proof of an identified cell chemistry/profile. The source comment above
the table says **“T-Embed-CC1101 CEDV Gauging Configuration”**, which materially
weakens any claim that these values were characterized specifically for this pack.

### H752-02 applicability decision

The official product README specifies a 3.7 V / 1500 mAh battery and the official
factory path uses the table above. The repository and its reachable history contain
no `H752-02` gauge profile, no installed-cell manufacturer/part number, no cell test
report, no Golden Image/`.gg` file, and no statement that the CEDV constants apply
to every GPS/LoRa SKU. `H752-02` is sold as the 915 MHz + GPS option; that supports
the current-Pro electrical candidate but does not identify the battery cell.

**Recovery readiness: NO-GO / blocked.** The 1500 mAh capacity values are official,
but an exact device-matching CEDV/chemistry profile cannot be proven. The observed
RM 2386 mAh / FCC 3000 mAh / SOC 80% proves the live model is impossible for the
documented pack; it does not prove that replaying this ambiguous CEDV table is safe.

### Current read-only comparison coverage

Production reads three stable samples of standard commands: SOC `0x2C`, voltage
`0x08`, RM `0x10`, FCC `0x12`, average current `0x14`, and DesignCapacity `0x3C`,
plus BQ25896 `REG0B`. It correctly rejects RM/FCC above 1500 and DesignCapacity not
equal to 1500. It does **not** read protected data memory, operation/control status,
seal state, BATT_ID, or any CEDV/configuration field. Consequently the only verified
live mismatch currently available is:

| Observation | Verified official expectation | Result |
|---|---:|---|
| RM | `<= 1500 mAh` | `2386 mAh` — mismatch |
| FCC | `1500 mAh` table / physically `<=1500` policy | `3000 mAh` — mismatch |
| SOC | cannot be trusted as capacity-accurate under invalid model | `80%` — observation only |
| DesignCapacity and protected table | not supplied in the stated live evidence | comparison pending |

### Backup-first recovery design for separate approval

This design is intentionally **not implemented as executable write code**. It may
advance only after a legible installed battery label/part number, PCB revision, and
manufacturer-supported matching CEDV profile resolve the blocker.

1. Use a dedicated, one-shot maintenance image with network/radio/display workers
   absent, no secrets, no automatic `init()`, and a compile-time write lock that a
   separate approval must deliberately remove.
2. Require stable external USB power plus installed battery, charger input detected,
   battery voltage in an approved non-extreme range, low absolute current, no thermal
   fault, and repeated stable I2C observations. Abort before unseal on any failure.
3. While still sealed, persist/print device number, firmware/control/operation status,
   seal state, BATT_ID, standard commands, charger state, and all publicly readable
   diagnostics. Do not claim protected DM is backed up yet.
4. After explicit operator confirmation, unseal only as required to read protected
   DM. Read every address/size in the approved profile plus any adjacent/profile
   selection fields needed for a restorable image. Store raw bytes, decoded values,
   address, size, timestamp, firmware identity, and a checksum to serial and a local
   nonvolatile file. Read the complete backup twice and require byte equality.
5. Compare all fields against the approved immutable profile and print a field-level
   plan. Require a second confirmation tied to the backup checksum. If already equal,
   reseal and exit without reset or write.
6. Follow the official ordering only: validate profile/state; reset only if the
   approved procedure requires it; verify `INITCOMP`; request full access only if
   required; enter CFGUPDATE and verify it; write one exact approved record at a
   time in table order; immediately read back that record; stop on first mismatch.
7. Always execute a cleanup/finally path. If CFGUPDATE was entered, attempt the
   documented exit/reinitialize command even after a write failure, wait, and verify
   `CFGUPDATE=0` and `INITCOMP=1`. Then reseal and verify `SEC=sealed` regardless of
   success. Never continue writing after power instability or verification failure.
8. On reboot after interruption, start read-only. Detect unsealed/full-access or
   CFGUPDATE state, print `RECOVERY INCOMPLETE`, and do not resume writes
   automatically. Exit CFGUPDATE/reseal only under a separately reviewed recovery
   decision informed by the persisted backup and current byte-level comparison.
9. After a successful whole-table readback, reinitialize/reseal exactly once, then
   collect fresh stable SOC/RM/FCC/DesignCapacity/voltage/current, operation status,
   BATT_ID, seal state, and BQ25896 input/charge/fault state. Require FCC and design
   capacity to be physically plausible; do not expect SOC to become accurate
   immediately or overwrite SOC/calibration manually.
10. Perform a bounded manufacturer-approved full-charge/rest/discharge validation
    later. A configuration readback alone does not validate learned FCC, SOC accuracy,
    temperature compensation, termination, or low-voltage behavior.

## 2. FastEPD compatibility findings

### Panel and current-Pro electrical map

ED047TC1 is not listed in FastEPD's `BBEP_DISPLAY_*` enum, but the T5 examples use
explicit `960x540` dimensions and 4-bpp mode. This demonstrates generic timing/path
usage with that resolution, not a named ED047TC1 waveform guarantee.

The current-Pro schematic/pin documentation defines an **8-bit** panel bus:

- D0..D7 = GPIO `5, 6, 7, 15, 16, 17, 18, 8`
- CKH/CL = 4, STV = 45, CKV = 48, STH = 41, LE = 42
- OE, MODE, TPS PWRUP/WAKEUP/PWRGOOD, and VCOM are through PCA9535/TPS651851
- front light `BL_EN` = GPIO 11, active-high PWM
- touch = GT911 on shared I2C 39/40, INT 3, reset 9

The official standalone FastEPD `BB_PANEL_LILYGO_T5PRO` profile instead defines:

- D0..D7 = GPIO `11, 12, 13, 14, 21, 47, 45, 38`
- CKV 39, SPH 9, CL 10, bit-banged control SDA/SCL 2/42, shift strobe 1
- dummy GPIO 46 (commented as LoRa CS)
- no direct power pin and a custom shift-register power procedure

That profile collides with front light GPIO 11, SD/SPI 12/13/14/21, LoRa BUSY 47,
EPD STV 45, PCA interrupt 38, I2C SDA 39, RTC interrupt 2, LoRa reset 1, and LoRa CS
46. It describes a different LilyGo electrical revision. **It is unsafe for the
current-Pro/H752-01 schematic and cannot qualify H752-02.**

FastEPD's generic `BB_PANEL_EPDIY_V7` profile has the correct current-Pro D0..D7
array and shared I2C pins, but assigns direct PWR GPIO11 and other v7 controls that
do not match the current-Pro TPS/PCA ownership. It therefore is not a safe substitute
either. The local EPDiy v7 profile's stale 16-bit upper byte consumes GPIO11 as D10;
FastEPD can eliminate that stale upper byte only through a new custom current-Pro
8-bit profile with the exact PCA9535/TPS651851 sequence—not through either supplied
profile.

### Power, refresh, and coexistence

- FastEPD stages full images in current/previous buffers. Partial update is currently
  1-bpp only. Full 4-bpp updates use a grayscale matrix and power off by default
  after neutral discharge unless `bKeepOn` is requested.
- Upstream documentation claims typical `<200 ms` partial, about `1 s` 1-bpp full,
  and about `2 s` grayscale full. These are claims, not measurements on this device.
- FastEPD uses ESP32-S3 LCD i80 DMA and a completion callback, but update loops remain
  synchronous to the caller. Field Terminal must continue pausing/releasing shared
  peripherals and must measure touch starvation and FreeRTOS worker latency.
- No official example proves the exact current-Pro profile together with GT911,
  GPS, LoRa, battery/charger polling, Wi-Fi, and Field Terminal workers.
- Partial update remains **NO-GO** until physical waveform/ghosting validation on an
  exact corrected profile. The compile-only spike calls only full 4-bpp APIs.
- Front light remains **NO-GO**. A corrected 8-bit profile would free GPIO11 from
  stale display ownership and could make a later LOW-only test possible, but FastEPD
  itself does not prove the installed PCB or safely enable the light.

### Migration cost and recommendation

Production already composes the exact packed 4-bpp 259,200-byte frame FastEPD needs,
so pixel-buffer adaptation is moderate. The larger cost is a reviewed custom board
power profile, orientation validation, replacement of EPDiy full-clear/GC16 semantics,
failure cleanup, framebuffer ownership, radio/SPI arbitration, touch quiet/drain
behavior, diagnostics, and physical grayscale/ghosting qualification.

**Recommendation: controlled future benchmark only; production migration NO-GO
today.** The performance upside (upstream ~2 s grayscale claim and i80 DMA) is large
enough to justify a physical benchmark after PCB identity and custom profile review.
It is not large enough to accept wrong pins or unqualified waveforms.

The isolated `fast_epd_h752_02_compile_only` environment excludes all production
sources and links a deterministic 960x540 4-bpp smoke routine against a minimal,
unmodified snapshot of the pinned official FastEPD commit under
`evaluation/lib/FastEPD`. Its `UPSTREAM.md` records source URL, immutable commit,
scope, license retention, and SHA-256 hashes. The local snapshot is necessary because
PlatformIO's recursive Git dependency copy included unrelated ESP-IDF example
submodules and exceeded Windows path limits before compilation. Only this evaluation
environment adds `evaluation/lib` to library discovery; production dependency
resolution is unchanged. `setup()` retains a pointer to the smoke routine so its API
surface is linked but never invokes it, so even an accidental boot performs no panel
initialization. This proves API/toolchain/resource compatibility only; it is not
physical-panel evidence.

The compile-only image builds successfully with Arduino ESP32 2.0.14 and the pinned
FastEPD 1.3.0 snapshot. Its linked ELF contains
`compileDeterministicSmokeScreen()`, `FASTEPD::initPanel`, and
`FASTEPD::fullUpdate`. PlatformIO reports **22,956 / 327,680 bytes RAM (7.0%)** and
**284,709 / 6,553,600 bytes flash (4.3%)**. For comparison, the unchanged production
candidate reports **56,164 bytes RAM (17.1%)** and **2,098,453 bytes flash (32.0%)**,
or 33,208 more RAM bytes and 1,813,744 more flash bytes. This delta is **not** a
renderer-replacement saving estimate: the spike deliberately excludes the complete
application, its workers, assets, EPDiy, and production dependencies. It establishes
only the absolute compile/link cost of the isolated smoke image.

### Recommended next physical test

First collect a clear battery-label and PCB-revision evidence set; do not upload a
display experiment yet. Once the board is proven to match the current-Pro schematic,
review a custom FastEPD 8-bit TPS/PCA profile offline. Under separate upload approval,
run one instrumented **full-screen 4-bpp full refresh only** from a known white panel,
front light forced off, partial update disabled, LoRa TX disabled, and stable USB
power. Capture update duration, current peak/energy, heap/PSRAM, reset/watchdog state,
touch before/during/after, GPS/LoRa/battery I2C continuity, grayscale distinction,
complete power-off, retained-image artifacts, and photographs. Compare the same
deterministic frame against production EPDiy GC16; stop on any pin, power-good,
temperature, artifact, or peripheral anomaly.