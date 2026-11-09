#!/usr/bin/env python3
"""Exercise actual process termination and a fresh startup without a GUI or Arduino."""
import socket
import subprocess
import sys
import tempfile
with tempfile.TemporaryDirectory(prefix='brasscue-restart-') as folder:
    stopped = subprocess.run([sys.argv[1], 'write-crash', folder], timeout=10)
    if stopped.returncode != 17:
        raise RuntimeError(f'Unexpected crash probe result: {stopped.returncode}')
    subprocess.run([sys.argv[1], 'read', folder], timeout=10, check=True)
    subprocess.run([sys.argv[1], 'read', folder], timeout=10, check=True)

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
        receiver.bind(('127.0.0.1', 6454))
        receiver.settimeout(5)
        subprocess.run([sys.argv[1], 'read-send', folder], timeout=10, check=True)
        found = False
        for _ in range(4):
            packet = receiver.recv(2048)
            if len(packet) == 530 and packet[14:16] == bytes([1, 0]) and packet[486] == 119:
                expected = bytes([119, 0, 0, 0]) * 11
                if packet[486:530] != expected:
                    raise RuntimeError('Recovered RGBW group output is incorrect')
                found = True
        if not found:
            raise RuntimeError('No restored output after explicit loopback enable')
