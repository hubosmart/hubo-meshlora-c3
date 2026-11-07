# SPDX-License-Identifier: MIT
"""Compile the real BLE/queue sources and test send and session behavior behind the host SDK boundary."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent
HEADERS = [
    'Arduino.h', 'esp_err.h', 'nvs_flash.h', 'esp_log.h',
    'freertos/FreeRTOS.h', 'freertos/queue.h',
    'nimble/nimble_port.h', 'nimble/nimble_port_freertos.h',
    'host/ble_hs.h', 'host/ble_sm.h', 'host/util/util.h',
    'services/gap/ble_svc_gap.h', 'services/gatt/ble_svc_gatt.h',
]


class BleTransportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get('HUBO_HOST_CXX') or shutil.which('clang++')
        if not compiler:
            raise unittest.SkipTest('set HUBO_HOST_CXX to the host clang++')
        if not Path(compiler).is_file():
            raise RuntimeError('set HUBO_HOST_CXX to a usable clang++ path')
        cls.directory = tempfile.TemporaryDirectory(prefix='hubo-ble-host-')
        cls.addClassCleanup(cls.directory.cleanup)
        folder = Path(cls.directory.name)
        for header in HEADERS:
            path = folder / header
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('#pragma once\n', encoding='utf-8')
        cls.binary = folder / ('ble-transport.exe' if os.name == 'nt' else 'ble-transport')
        command = [compiler, '-std=c++17', '-O0', '-g', '-static', '-pthread',
                   '-include', str(HERE / 'ble_stubs.h'), '-I', str(folder),
                   '-I', str(ROOT / 'components/companion_transport/include'),
                   '-I', str(ROOT / 'components/companion/include'),
                   '-I', str(ROOT / 'upstream/meshcore/src'),
                   str(HERE / 'ble_transport_harness.cpp'),
                   str(ROOT / 'components/companion/NativeSerialInterface.cpp'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, name):
        result = subprocess.run([str(self.binary), name], capture_output=True,
                                text=True, encoding='utf-8', timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_burst_notifications_are_spaced(self): self.run_case('burst')
    def test_device_info_and_following_notification(self): self.run_case('device-info')
    def test_failed_notify_preserves_frame_and_spacing(self): self.run_case('notify-retry')
    def test_allocation_failure_preserves_frame(self): self.run_case('alloc-retry')
    def test_spacing_across_clock_wrap(self): self.run_case('wrap')
    def test_device_info_window_across_clock_wrap(self): self.run_case('wrap-device-info')
    def test_disconnect_drops_old_rx_and_tx(self): self.run_case('disconnect')
    def test_host_reset_drops_old_rx_and_tx(self): self.run_case('reset')
    def test_secure_restore_does_not_restart_smp(self): self.run_case('secure')
    def test_auth_subscription_and_mtu_gate_requests(self): self.run_case('readiness')
    def test_old_subscription_cannot_change_current_session(self): self.run_case('stale-subscription')


if __name__ == '__main__':
    unittest.main()
