# SPDX-License-Identifier: MIT
"""Verify from a real-device log that an already paired connection does not request the security handshake again."""
import argparse
import re
from pathlib import Path


def check(text):
    paired=False
    restored=False
    connections=0
    security_events=0
    described=False
    for line in text.splitlines():
        if 'BLE disconnect reason=' in line:
            paired=restored=described=False
        elif 'BLE pairing status=0' in line:
            paired=True
        elif 'BLE connect status=0' in line:
            connections+=1
        elif 'BLE connection security:' in line:
            described=True
            restored='encrypted=1 authenticated=1' in line
        elif 'BLE security initiate rc=' in line:
            assert not (restored if described else paired), 'security requested again after pairing/security restore had completed'
            security_events+=1
        elif 'BLE security restored; skip initiate' in line:
            security_events+=1
    assert connections and security_events, 'no complete connection and security-handling records; cannot declare a pass'
    return True


def check_device_reply_window(text):
    request_time=None
    samples=0
    for line in text.splitlines():
        received=re.search(r'BLE rx: time_ms=(\d+) command=22 ',line)
        sent=re.search(r'BLE tx: time_ms=(\d+) response=13 .*rc=0',line)
        if received:request_time=int(received[1])
        if sent and request_time is not None:
            elapsed=int(sent[1])-request_time
            assert elapsed>=60, f'device-info notification delayed only {elapsed}ms; the first-packet write completion window was not left'
            samples+=1;request_time=None
    assert samples,'no complete device query/reply timing; cannot declare a pass'


def check_notify_spacing(text):
    previous=None
    samples=0
    for line in text.splitlines():
        if any(marker in line for marker in ('BLE connect status=0', 'BLE disconnect reason=', 'BLE host reset')):
            previous=None
        sent=re.search(r'BLE tx: time_ms=(\d+) response=\d+ bytes=\d+ rc=0\b',line)
        if not sent:continue
        now=int(sent[1])
        if previous is not None:
            elapsed=(now-previous)&0xffffffff
            assert elapsed>=60, f'successful notification interval only {elapsed}ms'
            samples+=1
        previous=now
    assert samples,'no consecutive successful notifications within one connection; cannot declare a pass'


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('log')
    parser.add_argument('--reply-window',action='store_true')
    parser.add_argument('--notify-spacing',action='store_true')
    args=parser.parse_args()
    text=Path(args.log).read_text(encoding='utf-8',errors='replace')
    if args.reply_window:check_device_reply_window(text)
    elif args.notify_spacing:check_notify_spacing(text)
    else:check(text)
    print('PASS: device reply timing' if args.reply_window else
          'PASS: all notification spacing' if args.notify_spacing else 'PASS: security restoration')
