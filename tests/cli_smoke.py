#!/usr/bin/env python3
"""Exercise the actual executable, default network OFF and periodic loopback output."""
import socket
import subprocess
import sys
import time
exe = sys.argv[1]
r = subprocess.run([exe, '--simulate'], input='r 255\ng 128\nb 64\nw 20\nm 128\nquit\n', text=True, capture_output=True, check=True, timeout=10)
if 'Output RGBW: 128 64 32 10' not in r.stdout or 'network disabled' not in r.stdout:
    raise RuntimeError(r.stdout)
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.bind(('127.0.0.1', 6454))
    sock.settimeout(3)
    p = subprocess.Popen([exe, '--simulate', '--target', '127.0.0.1', '--universe', '32767'], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        p.stdin.write('r 255\nm 128\nbtn 1 1\nbtn 1 0\n')
        p.stdin.flush()
        expected = bytes([128, 0, 0, 0, 64])
        matched = 0
        deadline = time.monotonic() + 5
        while matched < 3 and time.monotonic() < deadline:
            packet = sock.recv(2048)
            if len(packet) != 530 or packet[:12] != b'Art-Net\x00\x00P\x00\x0e' or packet[14:18] != bytes([255, 127, 2, 0]):
                raise RuntimeError('Invalid ArtDmx framing')
            if packet[18:23] == expected:
                matched += 1
        if matched < 3:
            raise RuntimeError('Missing periodic output while console input is idle')
        p.communicate('quit\n', timeout=5)
        if p.returncode:
            raise RuntimeError('CLI failed')
        cleared = False
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            packet = sock.recv(2048)
            if not any(packet[18:]):
                cleared = True
                break
        if not cleared:
            raise RuntimeError('Missing zero output on exit')
    finally:
        if p.poll() is None:
            p.kill()
            p.communicate()
print('CLI scaling, periodic loopback ArtDmx and exit cleanup passed')
