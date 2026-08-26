#!/usr/bin/env python3
"""Drive the DragonFly BSD VM setup via QEMU serial console.

DragonFly raw images boot to a passwordless root login with no cloud-init.
Typing every command blind via VGA sendkey fails when boot is slow enough that
keystrokes arrive before the login prompt.  This script uses a two-phase
approach:

  Phase 1 (sendkey bootstrap): After waiting for the boot loader to pass, type
  the root login and a command that starts a /bin/sh on the serial port, all via
  QEMU sendkey into the VGA console.  A retry loop handles the variable boot
  time — each attempt clears the console, logs in, and starts the serial shell,
  checking for a shell prompt on the serial socket before moving on.

  Phase 2 (serial setup): Run all setup commands (network, sshd, ssh key)
  through the serial console with prompt detection, so each command is confirmed
  before the next is sent.

Reads PUB_KEY, DFLY_MONITOR_SOCK, and DFLY_SERIAL_SOCK from the environment.
Called by dragonfly-provision.bash.
"""
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

BOOT_LOADER_WAIT = 30
BOOT_TIMEOUT = 300
COMMAND_TIMEOUT = 60
MAX_BOOTSTRAP_ATTEMPTS = 20
SERIAL_DEVICE = '/dev/cuaa0'


def send_hmp(sock, cmd):
    sock.sendall((cmd + '\n').encode())
    time.sleep(0.1)
    sock.settimeout(0.3)
    try:
        sock.recv(4096)
    except socket.timeout:
        pass
    sock.settimeout(None)


def send_line(sock, text):
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
        time.sleep(0.03)


def serial_recv(sock, timeout=3):
    sock.settimeout(timeout)
    data = b''
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    sock.settimeout(None)
    return data.decode(errors='replace')


def serial_drain(sock):
    sock.settimeout(0.3)
    try:
        while sock.recv(4096):
            pass
    except socket.timeout:
        pass
    sock.settimeout(None)


def serial_cmd(sock, cmd, timeout=COMMAND_TIMEOUT):
    """Send a command over serial and wait for the next shell prompt.

    Returns the output on success, raises RuntimeError on timeout.
    """
    sock.sendall((cmd + '\n').encode())
    output = ''
    deadline = time.time() + timeout
    while time.time() < deadline:
        remaining = deadline - time.time()
        chunk = serial_recv(sock, timeout=min(2, max(0.1, remaining)))
        output += chunk
        # The serial shell is /bin/sh running as root, whose default prompt
        # ends with '#'.  None of the commands this script runs produce
        # output ending with '#'.
        if output.rstrip().endswith('#'):
            return output
    raise RuntimeError(f"serial_cmd timed out after {timeout}s: {cmd!r}")


def bootstrap_serial_shell(monitor, serial):
    """Type login + serial shell start via sendkey; retry until serial responds.

    Returns True once a shell prompt appears on the serial socket.
    """
    time.sleep(BOOT_LOADER_WAIT)

    deadline = time.time() + BOOT_TIMEOUT
    for attempt in range(1, MAX_BOOTSTRAP_ATTEMPTS + 1):
        if time.time() > deadline:
            break

        print(f"  attempt {attempt}: sendkey login + serial shell...")

        send_hmp(monitor, 'sendkey ctrl-c')
        time.sleep(0.3)

        send_line(monitor, '')
        time.sleep(0.5)
        send_line(monitor, 'root')
        time.sleep(2)

        # Kill stale serial shells, then start a fresh one.
        # Root's login shell is csh, so wrap sh-specific redirects in /bin/sh -c.
        send_line(monitor, 'pkill -f cuaa0')
        time.sleep(1)
        cmd = f"/bin/sh -c '/bin/sh <{SERIAL_DEVICE} >{SERIAL_DEVICE} 2>&1 &'"
        send_line(monitor, cmd)
        time.sleep(2)

        serial.sendall(b'\n')
        response = serial_recv(serial, timeout=3)
        if '#' in response or '$' in response:
            print(f"  serial shell up after {attempt} attempt(s)")
            return True

        print(f"  no serial prompt (got: {response!r})")
        time.sleep(10)

    return False


def main():
    pub_key = os.environ["PUB_KEY"]
    monitor_sock = os.environ.get("DFLY_MONITOR_SOCK", "dfly-monitor.sock")
    serial_sock = os.environ.get("DFLY_SERIAL_SOCK", "dfly-serial.sock")

    monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    monitor.connect(monitor_sock)
    time.sleep(0.5)
    monitor.recv(4096)

    serial = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    serial.connect(serial_sock)
    serial_drain(serial)

    try:
        print("Bootstrapping serial shell via sendkey...")
        if not bootstrap_serial_shell(monitor, serial):
            print("ERROR: serial shell never came up", file=sys.stderr)
            return 1

        serial_drain(serial)

        print("Configuring VM via serial console...")

        serial_cmd(serial, 'dhclient vtnet0', timeout=COMMAND_TIMEOUT)

        serial_cmd(serial,
                   'echo "PermitRootLogin yes" >> /etc/ssh/sshd_config')
        serial_cmd(serial,
                   'echo "PermitEmptyPasswords yes" >> /etc/ssh/sshd_config')

        serial_cmd(serial, 'mkdir -p /root/.ssh && chmod 700 /root/.ssh')
        serial_cmd(serial,
                   f'echo "{pub_key}" > /root/.ssh/authorized_keys')
        serial_cmd(serial, 'chmod 600 /root/.ssh/authorized_keys')

        serial_cmd(serial, 'ssh-keygen -A', timeout=COMMAND_TIMEOUT)
        serial_cmd(serial, '/usr/sbin/sshd')

        print("VM configured successfully")
        return 0
    except RuntimeError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        monitor.close()
        serial.close()


if __name__ == "__main__":
    sys.exit(main())
