[English](./README.md) | 简体中文

# Hubo MeshLoRa C3 固件

## 简介

ESP32-C3 搭配亿佰特（Ebyte）E22 LoRa 模块的电台固件。运行 [MeshCore](https://github.com/meshcore-dev/MeshCore) 伴随协议（companion radio），通过 BLE 对外提供服务，Hubo 终端或任何 MeshCore 伴随客户端都可以把这块板子当作电台使用。

- 目标：`esp32c3`，4 MB 闪存，ESP-IDF 5.5（锁文件固定为 5.5.4）
- 传输：BLE（NimBLE，单连接）
- 加密：AES-128、SHA-256/HMAC 与 Curve25519/Ed25519 用 [rweather/Crypto](https://github.com/rweather/arduinolibs)（MIT，`upstream/crypto`）的软件实现，另有 MeshCore 自带的 Ed25519 库（zlib，`upstream/meshcore/lib/ed25519`）

自定义变量（例如 GNSS 板上的 GPS 开关与状态）见协议仓库：<https://github.com/hubosmart/hubo-mesh-protocols>。

## 变体

一个变体就是一组按顺序叠加的 `sdkconfig` 文件。

| 变体 | 板子 | 模块 | 射频芯片档位 | 叠加文件 |
|---|---|---|---|---|
| `std` | 标准 MeshLoRa C3 板 | E22-400M30S | SX1262 兼容档位（`HUBO_E22_SX1262_COMPAT=y`，用于上报 SX1261 的模块） | `sdkconfig.defaults;sdkconfig.ble.defaults` |
| `rev2gps` | GNSS 版（ATGM336H GNSS） | E22-400M22S | SX1268（`HUBO_E22_SX1262_COMPAT=n`） | `sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults` |
| `rev2gps-probe` | GNSS 版联调 | E22-400M22S | SX1268 | `rev2gps` 的叠加文件，再加 `sdkconfig.hwprobe.defaults` |

各叠加文件的内容：

- `sdkconfig.rev2gps.defaults`：`CONFIG_HUBO_E22_RXEN_GPIO=0`（E22 的 RXEN 改接 GPIO0，默认是 GPIO2）、`CONFIG_HUBO_GPS_EN_N_GPIO=2`（GPIO2 为低有效的 GNSS 电源开关，设置后同时启用 GPS 遥测分支）、`CONFIG_HUBO_E22_SX1262_COMPAT=n`。
- `sdkconfig.hwprobe.defaults`：`CONFIG_HUBO_HW_PROBE=y`。启动时给 GNSS 上电，经 UART1（GPIO20 RX / GPIO21 TX）与其通信，并把结果存入全局变量供调试器读取。仅用于调试构建。

## 编译

安装并激活 ESP-IDF 5.5，然后选一个变体。每个变体使用各自的构建目录。

```sh
# std
idf.py -B build-std -D SDKCONFIG=build-std/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults" set-target esp32c3 build

# rev2gps
idf.py -B build-rev2gps -D SDKCONFIG=build-rev2gps/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults" set-target esp32c3 build

# rev2gps-probe
idf.py -B build-rev2gps-probe -D SDKCONFIG=build-rev2gps-probe/sdkconfig -D "SDKCONFIG_DEFAULTS=sdkconfig.defaults;sdkconfig.ble.defaults;sdkconfig.rev2gps.defaults;sdkconfig.hwprobe.defaults" set-target esp32c3 build
```

## 烧录

```sh
idf.py -B build-std -p <PORT> flash
```

分区表（`partitions.csv`）：

| 名称 | 类型 | 偏移 | 大小 |
|---|---|---|---|
| nvs | data/nvs | 0x9000 | 0x6000 |
| phy_init | data/phy | 0xf000 | 0x1000 |
| factory | app | 0x10000 | 0x100000 |
| storage | data/spiffs | 0x110000 | 0x200000 |
| test_store | data/spiffs | 0x310000 | 0x70000 |

**空片首次烧录必须初始化 `storage`。** `storage` 这个 SPIFFS 分区从未格式化过时，mesh 任务会启动失败。先生成一个空镜像，再写入一次：

```sh
mkdir empty_dir
python $IDF_PATH/components/spiffs/spiffsgen.py 0x200000 empty_dir storage.bin
python -m esptool --chip esp32c3 -p <PORT> write-flash 0x110000 storage.bin
```

可以在常规 `flash` 之前或之后执行，但只能对空片做：它会清掉已保存的身份、联系人和频道。

## 默认射频参数

在 `components/companion/CMakeLists.txt` 中设置：

| 参数 | 值 |
|---|---|
| 频率 | 433.650 MHz |
| 带宽 | 62.5 kHz |
| 扩频因子 | SF8 |
| 编码率 | CR5（4/5） |
| 发射功率（芯片输出） | 10 dBm |
| 发射功率上限 | 10 dBm |

这些参数可在运行时经伴随协议修改；上面的功率上限是编译进固件的。

## 合规说明

各国/地区对 433 MHz 频段的功率与用途都有限制，且并不一致。例如中国 433.05-434.79 MHz 为 10 mW e.r.p.；欧盟 433 MHz 频段同样是 10 mW e.r.p.，并有占空比限制。

本固件把射频芯片的输出限制在 10 dBm。但带功率放大器的模块（如 E22-400M30S）实际发射功率高于芯片输出，可能远超 10 dBm。

使用者须自行确认并遵守所在地的无线电管理规定。本说明不构成法律意见。

## 测试

主机侧测试（需要 clang++）：

```sh
export HUBO_HOST_CXX=/path/to/clang++
python -m unittest discover -s tools/tests -p "test_*.py"
```

板上测试程序在 `test_app/`。

## 相关仓库

- [hubo-mesh-protocols](https://github.com/hubosmart/hubo-mesh-protocols)：协议文档与自定义变量
- [hubo-wadamesh](https://github.com/hubosmart/hubo-wadamesh)：终端固件
- [hubo-backpack-spec](https://github.com/hubosmart/hubo-backpack-spec)：背包板规范

## 许可证

MIT，见 [LICENSE](./LICENSE)。第三方组件及其许可证见 [NOTICE.md](./NOTICE.md)。
