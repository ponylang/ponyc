#!/usr/bin/env python3
"""Drive the DragonFly BSD VM setup via serial console or VGA sendkey.

The provision script creates a seed ISO containing a setup script and the SSH
public key, attached as a CD-ROM.  This script tries two approaches in order:

  1. Serial console (preferred).  Intercepts the bootloader via VGA sendkey
     (just Escape + one ``set`` command at a clean prompt) to redirect the
     console to serial.  All subsequent interaction — login, mount, setup —
     happens over the serial socket where prompts and errors are visible.

  2. VGA sendkey (fallback).  If serial output never appears, falls back to
     the original approach: detect boot completion via screendump stability,
     then blind-type the commands via QEMU sendkey.

Reads DFLY_MONITOR_SOCK and (optionally) DFLY_SERIAL_SOCK and
DFLY_ARTIFACTS_DIR from the environment.  Called by dragonfly-provision.bash.
"""
import hashlib
import os
import socket
import sys
import time

# ---------------------------------------------------------------------------
# QEMU sendkey helpers
# ---------------------------------------------------------------------------

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

LOADER_WAIT = 2
SERIAL_DETECT_TIMEOUT = 5
SERIAL_BOOT_DETECT_TIMEOUT = 15
SERIAL_BOOT_TIMEOUT = 300
SERIAL_CMD_TIMEOUT = 60
POST_BOOT_DELAY = 15


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


# ---------------------------------------------------------------------------
# Screendump helpers
# ---------------------------------------------------------------------------

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


def wait_for_boot(monitor, artifacts_dir, skip_bios_wait=False):
    """Wait until the VGA console stabilizes, indicating boot is complete.

    Takes periodic screendumps and compares their hashes.  Returns True once
    the screen has been identical for STABLE_SECONDS, or False on timeout.
    Saves the last screendump as 'last-console.ppm' in artifacts_dir.
    """
    if not skip_bios_wait:
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


# ---------------------------------------------------------------------------
# Serial console helpers
# ---------------------------------------------------------------------------

def _log_serial(data):
    """Log serial output to stdout."""
    text = data.decode('ascii', errors='replace')
    for line in text.splitlines(True):
        sys.stdout.write(f"    serial> {line}")
    if text and not text.endswith('\n'):
        sys.stdout.write('\n')
    sys.stdout.flush()


def _create_serial_socket(path):
    """Connect to the QEMU serial Unix socket.  Returns socket or None."""
    try:
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(path)
        return s
    except OSError as e:
        print(f"  could not connect to serial socket {path}: {e}")
        return None


def _serial_read(serial, timeout):
    """Read whatever is available on serial within *timeout* seconds."""
    buf = b''
    deadline = time.time() + timeout
    serial.settimeout(2.0)
    while time.time() < deadline:
        try:
            chunk = serial.recv(4096)
            if chunk:
                buf += chunk
                _log_serial(chunk)
            else:
                break
        except socket.timeout:
            if buf:
                break
    serial.settimeout(None)
    return buf


def serial_read_until(serial, pattern, timeout):
    """Read from serial until *pattern* is found or *timeout* expires."""
    buf = b''
    deadline = time.time() + timeout
    serial.settimeout(2.0)
    while time.time() < deadline:
        try:
            chunk = serial.recv(4096)
            if chunk:
                buf += chunk
                _log_serial(chunk)
                if pattern in buf:
                    serial.settimeout(None)
                    return buf, True
            else:
                break
        except socket.timeout:
            pass
    serial.settimeout(None)
    return buf, False


def serial_send(serial, text):
    """Send *text* to the VM via the serial socket."""
    serial.sendall(text.encode())


# ---------------------------------------------------------------------------
# Serial console setup
# ---------------------------------------------------------------------------

def try_serial_setup(monitor, serial, artifacts_dir):
    """Try to configure the VM entirely via serial console.

    Intercepts the bootloader to redirect the console to serial, then
    boots, logs in, mounts the seed ISO, and runs the setup script —
    all over the serial socket.

    Returns True if SSH becomes reachable, False to fall back to sendkey.
    """
    # -- Bootloader interception ------------------------------------------
    time.sleep(BIOS_WAIT)
    print("  interrupting bootloader...")
    send_hmp(monitor, 'sendkey esc')
    time.sleep(2)
    send_hmp(monitor, 'sendkey esc')
    time.sleep(LOADER_WAIT)

    print("  enabling serial console at loader prompt...")
    send_line(monitor, 'set console=comconsole')
    time.sleep(LOADER_WAIT)

    # -- Detect whether the console switched to serial --------------------
    print("  checking for serial output...")
    initial = _serial_read(serial, timeout=SERIAL_DETECT_TIMEOUT)

    if not initial:
        # Console may not have switched.  Send ``boot`` via VGA sendkey in
        # case the loader is still waiting on VGA (otherwise the VM never
        # boots and we hang).
        print("  no serial output — sending boot via VGA sendkey")
        send_line(monitor, 'boot')
        # Give the kernel time to start; check whether serial wakes up.
        boot_data = _serial_read(serial, timeout=SERIAL_BOOT_DETECT_TIMEOUT)
        if not boot_data:
            print("  still no serial output — serial console not available")
            return False
        print("  serial output detected after VGA boot")
        initial = boot_data

    print("  serial console active")

    # -- Boot (if still at the loader prompt) -----------------------------
    if b'login:' not in initial:
        serial_send(serial, '\r')
        time.sleep(0.5)
        serial_send(serial, 'boot\r')
        print(f"  waiting up to {SERIAL_BOOT_TIMEOUT}s for login prompt "
              "on serial...")
        buf, found = serial_read_until(
            serial, b'login:', SERIAL_BOOT_TIMEOUT)
        if not found:
            print("  login prompt not found on serial")
            return False
    else:
        print("  login prompt already on serial")

    # -- Login as root ----------------------------------------------------
    print("  logging in as root via serial...")
    serial_send(serial, 'root\r')
    buf, found = serial_read_until(serial, b'# ', SERIAL_CMD_TIMEOUT)
    if not found:
        print("  shell prompt not found after login")
        return False

    # -- Mount seed ISO ---------------------------------------------------
    print("  mounting seed ISO...")
    serial_send(serial, 'mount_cd9660 /dev/cd0 /mnt\r')
    buf, found = serial_read_until(serial, b'# ', SERIAL_CMD_TIMEOUT)
    if not found:
        print("  shell prompt not found after mount")
        return False

    # -- Run setup script -------------------------------------------------
    print("  running setup script...")
    serial_send(serial, 'sh /mnt/setup.sh\r')
    buf, found = serial_read_until(serial, b'# ', SERIAL_CMD_TIMEOUT * 2)
    if not found:
        print("  shell prompt not found after setup script")
        return False

    # -- Verify SSH -------------------------------------------------------
    print(f"  waiting up to {SSH_CHECK_TIMEOUT}s for SSH...")
    deadline = time.time() + SSH_CHECK_TIMEOUT
    while time.time() < deadline:
        time.sleep(SSH_CHECK_INTERVAL)
        if ssh_reachable():
            print("  SSH reachable (serial setup)")
            return True

    print("  SSH not reachable after serial setup")
    return False


# ---------------------------------------------------------------------------
# VGA sendkey setup (fallback)
# ---------------------------------------------------------------------------

def sendkey_setup(monitor, artifacts_dir, skip_bios_wait=False):
    """Set up the VM via VGA sendkey.  Returns 0 on success, 1 on failure."""
    print("Waiting for boot to finish (screendump stability)...")
    boot_ok = wait_for_boot(
        monitor, artifacts_dir, skip_bios_wait=skip_bios_wait)
    if not boot_ok:
        print("  WARNING: boot did not stabilize within timeout; "
              "trying anyway")

    time.sleep(POST_BOOT_DELAY)

    for attempt in range(1, MAX_ATTEMPTS + 1):
        print(f"  attempt {attempt}: sendkey login + mount + setup...")

        send_hmp(monitor, 'sendkey ctrl-c')
        time.sleep(0.5)
        send_line(monitor, '')
        time.sleep(0.5)

        send_line(monitor, 'root')
        time.sleep(8)

        send_line(monitor, 'mount_cd9660 /dev/cd0 /mnt')
        time.sleep(5)

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
                       os.path.join(artifacts_dir, f'attempt-{attempt}.ppm'))

    print("ERROR: VM setup failed — SSH never became reachable",
          file=sys.stderr)
    if artifacts_dir:
        last = os.path.join(artifacts_dir, 'last-console.ppm')
        screendump(monitor, last)
        if os.path.exists(last):
            sz = os.path.getsize(last)
            print(f"  diagnostic screendump saved: {last} ({sz} bytes)",
                  file=sys.stderr)
    return 1


# ---------------------------------------------------------------------------
# SSH check
# ---------------------------------------------------------------------------

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


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    monitor_sock_path = os.environ.get(
        "DFLY_MONITOR_SOCK", "dfly-monitor.sock")
    artifacts_dir = os.environ.get("DFLY_ARTIFACTS_DIR", "")

    monitor_dir = os.path.dirname(monitor_sock_path)
    serial_sock_path = os.environ.get(
        "DFLY_SERIAL_SOCK",
        os.path.join(monitor_dir, "dfly-serial.sock")
        if monitor_dir else "dfly-serial.sock")

    monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    monitor.connect(monitor_sock_path)
    time.sleep(0.5)
    monitor.recv(4096)

    serial = None
    bios_waited = False
    try:
        # Phase 1: try serial console
        serial = _create_serial_socket(serial_sock_path)
        if serial:
            print("Serial socket connected — trying serial console setup")
            bios_waited = True
            try:
                if try_serial_setup(monitor, serial, artifacts_dir):
                    return 0
            except OSError as e:
                print(f"  serial setup error: {e}")
            serial.close()
            serial = None
            print("Serial approach did not succeed — "
                  "falling back to VGA sendkey")
        else:
            print("Serial socket not available — using VGA sendkey")

        # Phase 2: fallback to VGA sendkey
        return sendkey_setup(
            monitor, artifacts_dir, skip_bios_wait=bios_waited)
    finally:
        if serial:
            serial.close()
        monitor.close()


if __name__ == "__main__":
    sys.exit(main())
