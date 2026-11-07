# SPDX-License-Identifier: MIT
"""AES-128, SHA-256 and HMAC-SHA256 of rweather/Crypto checked against standard vectors and Python cryptography.
The firmware uses exactly this upstream/crypto source; here the same source is compiled on the host for comparison.
Requires a host clang++: the HUBO_HOST_CXX environment variable or clang++ on PATH."""
import hashlib
import hmac
import os
import random
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

ROOT = Path(__file__).resolve().parents[2]
CRYPTO = ROOT / "upstream" / "crypto" / "libraries" / "Crypto"
SRCS = ["AES128.cpp", "AESCommon.cpp", "BlockCipher.cpp", "SHA256.cpp", "Hash.cpp", "Crypto.cpp"]


def build():
    cxx = os.environ.get("HUBO_HOST_CXX") or shutil.which("clang++")
    if not cxx:
        raise unittest.SkipTest("no host clang++ (set HUBO_HOST_CXX)")
    out = Path(tempfile.mkdtemp()) / "crypto_harness.exe"
    # -static: llvm-mingw output depends on the libc++ runtime in the toolchain directory by default; link statically so the DLL is not missing at run time
    cmd = [cxx, "-std=c++17", "-O1", "-static", "-DHOST_BUILD", f"-I{CRYPTO}", str(ROOT / "tools" / "tests" / "crypto_harness.cpp")]
    cmd += [str(CRYPTO / s) for s in SRCS] + ["-o", str(out)]
    subprocess.run(cmd, check=True)
    return out


class CryptoVectors(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = build()

    def run_lines(self, lines):
        r = subprocess.run([str(self.exe)], input="\n".join(lines) + "\n", capture_output=True, text=True, check=True)
        return r.stdout.split()

    def test_aes_fips197(self):
        out = self.run_lines(["AES 000102030405060708090a0b0c0d0e0f 00112233445566778899aabbccddeeff"])
        self.assertEqual(out[0], "69c4e0d86a7b0430d8cdb78070b4c55a")
        self.assertEqual(out[1], "00112233445566778899aabbccddeeff")

    def test_sha256_abc_and_empty(self):
        out = self.run_lines(["SHA 616263", "SHA -"])
        self.assertEqual(out[0], "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        self.assertEqual(out[1], "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")

    def test_hmac_rfc4231_case2(self):
        out = self.run_lines(["HMAC 4a656665 7768617420646f2079612077616e7420666f72206e6f7468696e673f 32"])
        self.assertEqual(out[0], "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843")

    def test_random_against_python(self):
        rnd = random.Random(20261105)
        lines, want = [], []
        for _ in range(200):
            key, blk = rnd.randbytes(16), rnd.randbytes(16)
            lines.append(f"AES {key.hex()} {blk.hex()}")
            enc = Cipher(algorithms.AES(key), modes.ECB()).encryptor().update(blk)  # single-block check; MeshCore builds its own mode on top of the block operation
            want += [enc.hex(), blk.hex()]
            msg = rnd.randbytes(rnd.randrange(0, 300))
            lines.append(f"SHA {msg.hex() or '-'}")
            want.append(hashlib.sha256(msg).hexdigest())
            hkey = rnd.randbytes(rnd.choice([16, 32, 64, 80]))  # includes long keys over 64 bytes
            outlen = rnd.choice([2, 16, 32])                     # MeshCore truncates to 2 and 16 bytes
            lines.append(f"HMAC {hkey.hex()} {msg.hex() or '-'} {outlen}")
            want.append(hmac.new(hkey, msg, hashlib.sha256).digest()[:outlen].hex())
        self.assertEqual(self.run_lines(lines), want)


if __name__ == "__main__":
    unittest.main()
