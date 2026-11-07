# Third-party notices

This repository contains the third-party components below under `upstream/`. **Most of them are MIT
licensed; the exceptions are listed in the second table.** Each component's license text is in its own
directory or, where the directory has none, in the source file headers. Files inside these components
may carry additional copyright lines from other contributors; they are kept unchanged.

## Components

| Component | Directory | Purpose | License | Copyright |
|---|---|---|---|---|
| MeshCore | `upstream/meshcore` | Mesh protocol core | MIT (`license.txt`) | Copyright (c) 2025 Scott Powell / rippleradios.com |
| meshcore_c | `upstream/meshcore_c` | C companion protocol library | MIT (`LICENSE`) | Copyright (c) 2026 Scott Penrose / Digital Dimensions |
| RadioLib | `upstream/radiolib` | LoRa radio driver | MIT (`license.txt`) | Copyright (c) 2018 Jan Gromeš |
| ArduinoJson | `upstream/arduinojson` | JSON library | MIT (`LICENSE.txt`) | Copyright © 2014-2025, Benoit BLANCHON |
| base64 | `upstream/base64` | Base64 encoding | MIT (`LICENSE`) | Copyright (c) 2016 Densaugeo |
| CayenneLPP | `upstream/cayenne` | Sensor telemetry encoding | MIT (`LICENSE.md`) | See notes below |
| Crypto (rweather) | `upstream/crypto` | AES-128, SHA-256/HMAC, Curve25519/Ed25519 | MIT (in source headers) | Copyright (C) 2015 Southern Storm Software, Pty Ltd. |

## Files under other licenses

| Files | License | Built into the firmware | Notes |
|---|---|---|---|
| `upstream/meshcore/lib/ed25519/` (except `sha512.c`) | zlib (`license.txt`) | Yes | Copyright (c) 2015 Orson Peters. The license notice may not be removed or altered. |
| `upstream/meshcore/lib/ed25519/sha512.c` | LibTomCrypt notice: "free for all purposes without any express guarantee it works" | Yes | Tom St Denis |
| `upstream/radiolib/src/utils/ConvCode.h` (with `ConvCode.cpp`) | The Clear BSD License (Semtech), reproduced below | Yes (RadioLib's convolutional coder is linked) | Redistributions must retain the notice; binary redistributions must reproduce it in the documentation — this file serves that purpose. |
| `upstream/radiolib/src/modules/LR11x0/` (15 firmware headers), `upstream/radiolib/src/modules/SX126x/patches/SX126x_patch_scan.h` | The Clear BSD License (Semtech), in each file header | No (checked against the linked firmware image) | Source redistribution keeps the notice in each file. |
| `upstream/base64/catch.hpp` | Boost Software License 1.0 | No (test framework) | |

## Notes

- CayenneLPP: `LICENSE.md` contains the MIT terms without a copyright line. The copyright lines in the
  source files are "Copyright © 2017 The Things Network" (`src/CayenneLPP.cpp`, `src/CayenneLPP.h`) and
  "Copyright (C) 2021 by Manuel Weichselbaumer" (`src/CayenneLPPMessage.h`, `src/CayenneLPPPolyline.*`).
  The library's unit-test directory (LGPL) is not included in this repository.
- Crypto: the directory has no LICENSE file. The notice is in each source file header, e.g.
  `libraries/Crypto/SHA256.cpp`: "Copyright (C) 2015 Southern Storm Software, Pty Ltd." followed by the
  standard MIT permission notice. The upstream README states the libraries are distributed under the MIT license.
- Build dependencies that are not distributed in this repository: ESP-IDF (Apache-2.0) and the managed
  components recorded in `dependencies.lock` (each under its own license; the lock file currently lists only ESP-IDF).

## Files derived from MeshCore

These files under `components/` are adapted from MeshCore (Copyright (c) 2025 Scott Powell /
rippleradios.com, MIT) and are released here under the MIT license together with our changes:

- `components/companion/MyMesh.cpp`, `components/companion/MyMesh.h`, `components/companion/AbstractUITask.h`
- `components/native_store/DataStore.cpp`, `components/native_store/DataStore.h`, `components/native_store/NodePrefs.h`, `components/native_store/IdentityStore.cpp`
- `components/hubo_radio/RadioLibWrappers.cpp`, `components/hubo_radio/RadioLibWrappers.h`

## The Clear BSD License (Semtech), as found in RadioLib

```text
The Clear BSD License
Copyright Semtech Corporation 2021. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the disclaimer
below) provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of the Semtech corporation nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
```
