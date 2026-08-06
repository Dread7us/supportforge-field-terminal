# supportFORGE Field Terminal — H752-02 Hardware Qualification

This repository currently contains **qualification firmware**, not the complete
supportFORGE Field Terminal application.

`H752-02` is treated as LILYGO's product/SKU designation for the T5 E-Paper S3
Pro configuration with an **SX1262 for 915 MHz** and **GPS (L76K)**. It is not a
required Git branch name.

## Build

```powershell
pio run -e h752_02_candidate
```

Upload and monitor only after identifying the device's serial port:

```powershell
pio run -e h752_02_candidate -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

At boot, the firmware does not enable the shared LoRa/GPS rail, transmit RF,
write to GPS, or power the e-paper panel. Type `help` for staged commands.

## Required physical sequence

1. Record the PCB markings, product label, antenna band, and installed GPS part.
2. Run `profile`, then `i2c`. Save the complete serial output.
3. For the current-Pro candidate, expect—but do not pre-declare—devices such as
   PCA9535 `0x20`, PCF8563 `0x51`, BQ27220 `0x55`, BQ25896 `0x6B`, TPS65185
   `0x68`, and GT911 (`0x5D` is documented). Report actual results.
4. Attach the correct **915 MHz antenna** before radio work. Run `rail on`, then
   `lora probe`. This initializes and sleeps the SX1262 without transmitting.
   `lora rx` is receive-only.
5. Run `gps listen` outdoors. It observes UART RX at 9600 baud for 15 seconds and
   never drives the candidate GPS TX pin. Serial bytes establish UART activity;
   a valid NMEA fix is separate evidence of navigation performance.
6. Run `display test` only after the current-Pro I²C/PCA9535 evidence is
   consistent. Visually report whether the border and solid center rectangle
   appear, including orientation, artifacts, and refresh behavior.
7. Run `rail off` when peripheral testing is complete.

Use `docs/HARDWARE_QUALIFICATION.md` as the evidence and result record. A
successful build is not physical confirmation.
