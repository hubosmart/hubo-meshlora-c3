# SPDX-License-Identifier: MIT
"""The log validator must check the measured intervals and must not report a pass when samples are missing."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

CHECKER = Path(__file__).resolve().parents[1] / 'check_ble_reconnect.py'


class NotifyLogTests(unittest.TestCase):
    def run_log(self, text):
        with tempfile.TemporaryDirectory(prefix='hubo-ble-log-') as directory:
            path = Path(directory) / 'capture.log'
            path.write_text(text, encoding='utf-8')
            return subprocess.run([sys.executable, '-B', '-X', 'utf8', str(CHECKER),
                                   str(path), '--notify-spacing'], capture_output=True,
                                  text=True, encoding='utf-8', timeout=10)

    def test_complete_spacing_passes(self):
        result = self.run_log('BLE connect status=0\n'
                              'BLE tx: time_ms=1000 response=5 bytes=2 rc=0\n'
                              'BLE tx: time_ms=1060 response=6 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_early_notification_fails(self):
        result = self.run_log('BLE tx: time_ms=1000 response=5 bytes=2 rc=0\n'
                              'BLE tx: time_ms=1010 response=6 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 1, result.stderr)

    def test_failed_attempt_does_not_advance_clock(self):
        result = self.run_log('BLE tx: time_ms=1000 response=5 bytes=2 rc=0\n'
                              'BLE tx: time_ms=1050 response=6 bytes=2 rc=12\n'
                              'BLE tx: time_ms=1060 response=6 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_reconnect_resets_interval(self):
        result = self.run_log('BLE tx: time_ms=1000 response=5 bytes=2 rc=0\n'
                              'BLE disconnect reason=531\nBLE connect status=0\n'
                              'BLE tx: time_ms=1010 response=5 bytes=2 rc=0\n'
                              'BLE tx: time_ms=1070 response=6 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_wrap_interval_passes(self):
        result = self.run_log('BLE tx: time_ms=4294967280 response=5 bytes=2 rc=0\n'
                              'BLE tx: time_ms=44 response=6 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_single_notification_is_not_evidence(self):
        result = self.run_log('BLE tx: time_ms=1000 response=5 bytes=2 rc=0\n')
        self.assertEqual(result.returncode, 1, result.stderr)


if __name__ == '__main__':
    unittest.main()
