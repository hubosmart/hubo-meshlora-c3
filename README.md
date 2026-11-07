English | [简体中文](./README.zh-CN.md)

# Hubo MeshLoRa C3 firmware

## What it is

Radio firmware for an ESP32-C3 paired with an Ebyte E22 LoRa module. It runs the [MeshCore](https://github.com/meshcore-dev/MeshCore) companion-radio protocol and exposes it over BLE, so a Hubo terminal or any MeshCore companion client can use the board as its radio.

- Target: `esp32c3`, 4 MB flash, ESP-IDF 5.5 (the lock file pins 5.5.4)
- Transport: BLE (NimBLE, one connection)
- Crypto: software AES-128, SHA-256/HMAC and Curve25519/Ed25519 from [rweather/Crypto](https://github.com/rweather/arduinolibs) (MIT, `upstream/crypto`), plus the Ed25519 library bundled with MeshCore (zlib, `upstream/meshcore/lib/ed25519`)

Custom variables (for example the GPS switch and status on the GNSS board) are described in the protocol repository: <https://github.com/hubosmart/hubo-mesh-protocols>.

## Variants

A variant is a list of `sdkconfig` overlay files, applied in the order shown.

| Variant | Board | Module | Radio chip profile | Overlay files |
|---|---|---|---|---|
| `std` | Standard MeshLoRa C3 board | E22-400M30S | SX1262-compatible profile (`HUBO_E22_SX1262_COMPAT=y`, for modules that report SX1261) | `sdkconfig.defaults;sdkconfig.ble.defaults` |
| `rev2gps` | GNSS board (ATGM336H GNSS) | E22-400M22S | SX1268 (`HUBO_E22_SX1262_COMPAT=n`) | `sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults` |
| `rev2gps-probe` | GNSS board, bring-up | E22-400M22S | SX1268 | `rev2gps` overlays plus `sdkconfig.hwprobe.defaults` |

What the overlays set:

- `sdkconfig.rev2gps.defaults`: `CONFIG_HUBO_E22_RXEN_GPIO=0` (the E22 RXEN pin moves to GPIO0; the default is GPIO2), `CONFIG_HUBO_GPS_EN_N_GPIO=2` (GPIO2 is the active-low GNSS power switch; setting it also enables the GPS telemetry branch), and `CONFIG_HUBO_E22_SX1262_COMPAT=n`.
- `sdkconfig.hwprobe.defaults`: `CONFIG_HUBO_HW_PROBE=y`. Powers the GNSS at boot, talks to it on UART1 (GPIO20 RX / GPIO21 TX) and stores the result in a global for a debugger to read. Debug builds only.

## Build

Install ESP-IDF 5.5 and activate it, then pick one variant. Each variant uses its own build directory.

```sh
# std
idf.py -B build-std -D SDKCONFIG=build-std/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults" set-target esp32c3 build

# rev2gps
idf.py -B build-rev2gps -D SDKCONFIG=build-rev2gps/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults" set-target esp32c3 build

# rev2gps-probe
idf.py -B build-rev2gps-probe -D SDKCONFIG=build-rev2gps-probe/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults;sdkconfig.hwprobe.defaults" set-target esp32c3 build
```

## Flash

```sh
idf.py -B build-std -p <PORT> flash
```

Partition table (`partitions.csv`):

| Name | Type | Offset | Size |
|---|---|---|---|
| nvs | data/nvs | 0x9000 | 0x6000 |
| phy_init | data/phy | 0xf000 | 0x1000 |
| factory | app | 0x10000 | 0x100000 |
| storage | data/spiffs | 0x110000 | 0x200000 |
| test_store | data/spiffs | 0x310000 | 0x70000 |

**First flash of a blank chip: initialize `storage`.** The mesh task fails to start if the `storage` SPIFFS partition has never been formatted. Create an empty image and write it once:

```sh
mkdir empty_dir
python $IDF_PATH/components/spiffs/spiffsgen.py 0x200000 empty_dir storage.bin
python -m esptool --chip esp32c3 -p <PORT> write-flash 0x110000 storage.bin
```

Do this before or after the normal `flash`, but only on blank chips: it erases any saved identity, contacts and channels.

## Default radio settings

Set in `components/companion/CMakeLists.txt`:

| Parameter | Value |
|---|---|
| Frequency | 433.650 MHz |
| Bandwidth | 62.5 kHz |
| Spreading factor | SF8 |
| Coding rate | CR5 (4/5) |
| TX power (chip output) | 10 dBm |
| TX power upper limit | 10 dBm |

These can be changed at run time through the companion protocol; the TX power limit above is compiled in.

## Regulatory notice

Radio rules for the 433 MHz band differ by country and region and usually limit both power and permitted use. For example, 433.05-434.79 MHz in China is limited to 10 mW e.r.p., and the EU 433 MHz band is also limited to 10 mW e.r.p. and comes with duty-cycle restrictions.

This firmware caps the radio chip's output at 10 dBm. Modules with a power amplifier, such as the E22-400M30S, radiate more than the chip output, so the actual transmit power can be well above 10 dBm.

You are responsible for checking and following the radio regulations where you operate the device. This note is not legal advice.

## Tests

Host-side tests (a clang++ is required):

```sh
export HUBO_HOST_CXX=/path/to/clang++
python -m unittest discover -s tools/tests -p "test_*.py"
```

An on-target test app lives in `test_app/`.

## Related repositories

- [hubo-mesh-protocols](https://github.com/hubosmart/hubo-mesh-protocols): protocol documents and custom variables
- [hubo-wadamesh](https://github.com/hubosmart/hubo-wadamesh): terminal firmware
- [hubo-backpack-spec](https://github.com/hubosmart/hubo-backpack-spec): backpack board specification

## License

MIT, see [LICENSE](./LICENSE). Third-party components and their licenses are listed in [NOTICE.md](./NOTICE.md).
