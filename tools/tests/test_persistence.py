# SPDX-License-Identifier: MIT
"""Run actual DataStore/NativeFS and command branches with host SDK boundaries."""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


class PersistenceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = os.environ.get('HUBO_HOST_CXX') or shutil.which('clang++')
        if not compiler:
            raise unittest.SkipTest('set HUBO_HOST_CXX to the host clang++')
        cls.directory = tempfile.TemporaryDirectory(prefix='hubo-store-')
        cls.addClassCleanup(cls.directory.cleanup)
        folder = Path(cls.directory.name)
        for name in ['Arduino.h', 'Stream.h', 'Mesh.h', 'Identity.h', 'Utils.h',
                     'esp_spiffs.h', 'nvs_flash.h', 'esp_log.h']:
            (folder / name).write_text('#pragma once\n#include "persistence_stubs.h"\n', encoding='utf-8')
        shutil.copy(HERE / 'persistence_stubs.h', folder)
        native_fs = (ROOT / 'components/native_store/native_fs.cpp').read_text(encoding='utf-8')
        native_fs = native_fs.replace('&& ::rename(', '&& ::injected_rename(').replace('&& ::mkdir(', '&& ::host_mkdir(')
        (folder / 'native_fs.inc').write_text(native_fs, encoding='utf-8')
        source = (ROOT / 'components/companion/MyMesh.cpp').read_text(encoding='utf-8')
        # Compile the production branch bodies verbatim, without recreating their save logic.
        wanted = ['CMD_SET_ADVERT_NAME', 'CMD_SET_ADVERT_LATLON', 'CMD_RESET_PATH',
                  'CMD_ADD_UPDATE_CONTACT', 'CMD_REMOVE_CONTACT', 'CMD_SET_RADIO_PARAMS',
                  'CMD_SET_RADIO_TX_POWER', 'CMD_SET_TUNING_PARAMS', 'CMD_SET_OTHER_PARAMS',
                  'CMD_SET_PATH_HASH_MODE', 'CMD_SET_CHANNEL', 'CMD_SET_DEVICE_PIN',
                  'CMD_SET_CUSTOM_VAR', 'CMD_SET_DEFAULT_FLOOD_SCOPE', 'CMD_SET_AUTOADD_CONFIG',
                  'CMD_REBOOT']
        start = source.index('void MyMesh::handleCmdFrame(')
        end = source.index('\nstatic bool save_filter', start)
        branches = re.split(r'(?=^  } else if \(cmd_frame\[0\])', source[start:end], flags=re.M)
        chosen = [b for b in branches[1:] if any(re.match(r'  } else if \(cmd_frame\[0\] == '+w+r'\b', b) for w in wanted)]
        # The original final unsupported-command clause belongs to the last unrelated branch.
        text = 'void MyMesh::handleCmdFrame(size_t len) {\n  if (false) {\n' + ''.join(chosen) + '  }\n}\n'
        definitions = '\n'.join(line for line in source.splitlines() if line.startswith('#define '))
        functions = []
        for signature in ['void MyMesh::updateContactFromFrame', 'void MyMesh::writeOKFrame',
                          'void MyMesh::writeErrFrame', 'void MyMesh::onContactOverwrite',
                          'static bool save_filter', 'bool MyMesh::saveContacts']:
            a = source.index(signature)
            b = source.index('\n}\n', a) + 3
            functions.append(source[a:b])
        header = (ROOT / 'components/companion/MyMesh.h').read_text(encoding='utf-8')
        a = header.index('  bool savePrefsChecked()')
        b = header.index('\n  }', a) + 4
        functions.append(header[a:b].replace('bool savePrefsChecked()', 'bool MyMesh::savePrefsChecked()'))
        a = source.index('  if (dirty_contacts_expiry && millisHasNowPassed')
        b = source.index('\n  }', a) + 4
        functions.append('void MyMesh::retryContacts() {\n' + source[a:b] + '\n}')
        (folder / 'commands.inc').write_text(definitions + '\n' + '\n'.join(functions) + text, encoding='utf-8')
        cls.binary = folder / 'persistence.exe'
        command = [compiler, '-std=c++17', '-O0', '-g', '-static', '-DESP_PLATFORM',
                   '-I', str(folder), '-I', str(HERE),
                   '-I', str(ROOT / 'components/native_store/include'),
                   '-I', str(ROOT / 'components/native_store'),
                   '-I', str(ROOT / 'components/companion'),
                   '-I', str(ROOT / 'upstream/meshcore/src'),
                   str(HERE / 'persistence_harness.cpp'),
                   str(ROOT / 'components/native_store/DataStore.cpp'),
                   str(ROOT / 'upstream/meshcore/src/helpers/ConfigSerializer.cpp'),
                   '-o', str(cls.binary)]
        result = subprocess.run(command, capture_output=True, text=True, encoding='utf-8')
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)

    def run_case(self, name):
        with tempfile.TemporaryDirectory(prefix='store-', dir=Path(self.directory.name)) as folder:
            result = subprocess.run([str(self.binary), name, folder], capture_output=True,
                                    text=True, encoding='utf-8', timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_contacts_save_open_failure(self): self.run_case('contacts-open')
    def test_contacts_save_short_write(self): self.run_case('contacts-write')
    def test_contacts_save_close_failure(self): self.run_case('contacts-close')
    def test_contacts_save_commit_failure(self): self.run_case('contacts-commit')
    def test_channels_save_open_failure(self): self.run_case('channels-open')
    def test_channels_save_short_write(self): self.run_case('channels-write')
    def test_channels_save_close_failure(self): self.run_case('channels-close')
    def test_channels_save_commit_failure(self): self.run_case('channels-commit')
    def test_prefs_save_failures(self): self.run_case('prefs-failures')
    def test_successful_cold_read(self): self.run_case('cold-read')
    def test_preferences_command_errors_restore_ram(self): self.run_case('prefs-commands')
    def test_contact_command_errors_restore_ram(self): self.run_case('contact-commands')
    def test_channel_command_error_restores_ram(self): self.run_case('channel-command')
    def test_radio_command_failure_restores_driver(self): self.run_case('radio-commands')
    def test_command_success_is_readable_after_restart(self): self.run_case('command-cold-read')
    def test_full_contact_overwrite_restores_single_object(self): self.run_case('contact-full-commands')
    def test_background_save_failure_logs_and_retries(self): self.run_case('background-commands')
    def test_reboot_aborts_on_save_failure(self): self.run_case('reboot-commands')
    def test_transient_contact_update_restores_slot(self): self.run_case('transient-commands')


if __name__ == '__main__':
    unittest.main()
