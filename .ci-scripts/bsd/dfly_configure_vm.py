#!/usr/bin/env python3
"""Drive the DragonFly BSD VM setup via QEMU sendkey and a seed ISO.

The provision script creates a seed ISO containing a setup script and the SSH
public key, attached as a CD-ROM.  This script:

  1. Waits for boot to finish (VGA screendump stability).
  2. Types three short commands via sendkey: root login, mount the seed ISO,
     run the setup script.
  3. Verifies SSH becomes reachable on the forwarded port.

Each sendkey attempt is followed by an SSH reachability check.  On failure the
commands are retyped — the setup script on the ISO is idempotent (it exits
immediately if sshd is already running).

Reads DFLY_MONITOR_SOCK and (optionally) DFLY_ARTIFACTS_DIR from the
environment.  Called by dragonfly-provision.bash.
"""
import hashlib
import os
import socket
import sys
import time

KEYMAP = {
    ' ': 'spc', '\n': 'ret', '-': 'minus', '=': 'equal',
    '/': 'slash', '.': 'dot', ',': 'comma', ';': 'semicolon',
    "'": 'apostrophe', '\\': 'backslash', '`': 'grave_accent',
    '[': 'bracket_left', ']': 'bracket_right',
    '_': 'shift-minus', '+': 'shift-equal', ':': 'shift-semicolon',
    '"': 'shift-apostrophe', '<': 'shift-comma', '>': 'shift-dot',
    '?': 'shift-slash', '{': 'shift-bracket_left', '}': 'shift-bracket_right',
    '|': 'shift-backslash', '~': 'shift-grave_accent',
    '!': 'shift-1', '@': 'shift-2', '#': 'shift-3', '$': 'shift-4',
    '%': 'shift-5', '^': 'shift-6', '&': 'shift-7', '*': 'shift-8',
    '(': 'shift-9', ')': 'shift-0',
}

BIOS_WAIT = 5
BOOT_TIMEOUT = 360
STABLE_SECONDS = 10
SCREENDUMP_INTERVAL = 5
INTER_KEY_DELAY = 0.08
SSH_PORT = 2222
SSH_CHECK_INTERVAL = 5
SSH_CHECK_TIMEOUT = 90
MAX_ATTEMPTS = 5


def send_hmp(sock, cmd):
    """Send a command to the QEMU monitor and wait for the prompt."""
    sock.sendall((cmd + '\n').encode())
    data = b''
    sock.settimeout(2.0)
    try:
        while b'(qemu)' not in data:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    sock.settimeout(None)


def send_line(sock, text):
    """Type a line of text into the VGA console via QEMU sendkey."""
    for ch in text + '\n':
        if ch in KEYMAP:
            key = KEYMAP[ch]
        elif ch.isalpha():
            key = f'shift-{ch.lower()}' if ch.isupper() else ch
        elif ch.isdigit():
            key = ch
        else:
            continue
        send_hmp(sock, f'sendkey {key}')
        time.sleep(INTER_KEY_DELAY)


def screendump(monitor, path):
    """Capture a VGA screendump to a file, return the raw bytes (or None)."""
    send_hmp(monitor, f'screendump {path}')
    time.sleep(0.5)
    try:
        with open(path, 'rb') as f:
            return f.read()
    except OSError:
        return None


def _ppm_pixel_hash(data):
    """Hash just the pixel data of a PPM file, ignoring the header."""
    if not data or not data.startswith(b'P6'):
        return hashlib.sha1(data or b'').hexdigest()
    pos = data.find(b'\n', 3)
    if pos < 0:
        return hashlib.sha1(data).hexdigest()
    pos = data.find(b'\n', pos + 1)
    if pos < 0:
        return hashlib.sha1(data).hexdigest()
    return hashlib.sha1(data[pos + 1:]).hexdigest()


def wait_for_boot(monitor, artifacts_dir):
    """Wait until the VGA console stabilizes, indicating boot is complete.

    Takes periodic screendumps and compares their hashes.  Returns True once
    the screen has been identical for STABLE_SECONDS, or False on timeout.
    Saves the last screendump as 'last-console.ppm' in artifacts_dir.
    """
    time.sleep(BIOS_WAIT)

    dump_path = os.path.join(artifacts_dir, 'console-check.ppm') \
        if artifacts_dir else None
    last_path = os.path.join(artifacts_dir, 'last-console.ppm') \
        if artifacts_dir else None

    prev_hash = None
    stable_since = None
    deadline = time.time() + BOOT_TIMEOUT

    while time.time() < deadline:
        if dump_path:
            try:
                os.remove(dump_path)
            except OSError:
                pass
            data = screendump(monitor, dump_path)
            if last_path and data:
                try:
                    with open(last_path, 'wb') as f:
                        f.write(data)
                except OSError:
                    pass
        else:
            data = None

        if data:
            h = _ppm_pixel_hash(data)
            if h == prev_hash:
                if stable_since is None:
                    stable_since = time.time()
                elif time.time() - stable_since >= STABLE_SECONDS:
                    print(f"  screen stable for {STABLE_SECONDS}s — boot done")
                    return True
            else:
                prev_hash = h
                stable_since = None
        else:
            stable_since = None

        time.sleep(SCREENDUMP_INTERVAL)

    return False


def ssh_reachable():
    """Check if SSH is reachable on the forwarded port."""
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3)
        s.connect(('127.0.0.1', SSH_PORT))
        banner = s.recv(256)
        s.close()
        return b'SSH' in banner
    except (socket.timeout, ConnectionRefusedError, OSError):
        return False


def main():
    monitor_sock = os.environ.get("DFLY_MONITOR_SOCK", "dfly-monitor.sock")
    artifacts_dir = os.environ.get("DFLY_ARTIFACTS_DIR", "")

    monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    monitor.connect(monitor_sock)
    time.sleep(0.5)
    monitor.recv(4096)

    try:
        print("Waiting for boot to finish (screendump stability)...")
        boot_ok = wait_for_boot(monitor, artifacts_dir)
        if not boot_ok:
            print("  WARNING: boot did not stabilize within timeout; "
                  "trying anyway")

        for attempt in range(1, MAX_ATTEMPTS + 1):
            print(f"  attempt {attempt}: sendkey login + mount + setup...")

            send_hmp(monitor, 'sendkey ctrl-c')
            time.sleep(0.5)
            send_line(monitor, '')
            time.sleep(0.5)

            send_line(monitor, 'root')
            time.sleep(5)

            send_line(monitor, 'mount_cd9660 /dev/cd0 /mnt')
            time.sleep(2)

            send_line(monitor, 'sh /mnt/setup.sh')

            print(f"    waiting up to {SSH_CHECK_TIMEOUT}s for SSH...")
            deadline = time.time() + SSH_CHECK_TIMEOUT
            while time.time() < deadline:
                time.sleep(SSH_CHECK_INTERVAL)
                if ssh_reachable():
                    print(f"  SSH reachable after attempt {attempt}")
                    return 0

            print("    SSH not reachable")

            if artifacts_dir:
                screendump(monitor,
                           os.path.join(artifacts_dir, 'last-console.ppm'))

        print("ERROR: VM setup failed — SSH never became reachable",
              file=sys.stderr)
        if artifacts_dir:
            last = os.path.join(artifacts_dir, 'last-console.ppm')
            if os.path.exists(last):
                sz = os.path.getsize(last)
                print(f"  diagnostic screendump saved: {last} ({sz} bytes)",
                      file=sys.stderr)
        return 1
    finally:
        monitor.close()


if __name__ == "__main__":
    sys.exit(main())
