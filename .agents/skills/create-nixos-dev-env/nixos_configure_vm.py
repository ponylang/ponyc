#!/usr/bin/env python3
"""
Bootstrap a freshly booted NixOS live ISO in QEMU.

The NixOS minimal ISO starts sshd at boot with PermitRootLogin yes, but root
has no password and no authorized_keys. This script logs in via QEMU monitor
sendkey, mounts a seed ISO containing setup.sh and authorized_keys, runs the
setup script, and waits for SSH to become reachable.

Run from the VM directory (where the QEMU sockets live).
"""
import os
import socket
import subprocess
import sys
import time

MONITOR_SOCK = "nixos-monitor.sock"
SSH_PORT = int(os.environ.get("NIXOS_SSH_PORT", "2222"))
SSH_KEY = os.environ.get("NIXOS_SSH_KEY", "vm_key")
BOOT_TIMEOUT = 150
SSH_TIMEOUT = 60


def monitor_cmd(sock, cmd):
    sock.sendall((cmd + "\n").encode())
    time.sleep(0.2)
    try:
        return sock.recv(65536).decode(errors="replace")
    except socket.timeout:
        return ""


def sendkeys(sock, text, delay=0.08):
    keymap = {
        "\n": "ret", " ": "spc", "\t": "tab",
        "-": "minus", "=": "equal", ".": "dot",
        "/": "slash", "\\": "backslash",
        "'": "apostrophe", ",": "comma", ";": "semicolon",
        "[": "bracket_left", "]": "bracket_right",
        "`": "grave_accent",
        "!": "shift-1", "@": "shift-2", "#": "shift-3",
        "$": "shift-4", "%": "shift-5", "^": "shift-6",
        "&": "shift-7", "*": "shift-8", "(": "shift-9",
        ")": "shift-0", "_": "shift-minus", "+": "shift-equal",
        "{": "shift-bracket_left", "}": "shift-bracket_right",
        "|": "shift-backslash", ":": "shift-semicolon",
        '"': "shift-apostrophe", "<": "shift-comma",
        ">": "shift-dot", "?": "shift-slash",
        "~": "shift-grave_accent",
    }
    for ch in text:
        if ch in keymap:
            key = keymap[ch]
        elif ch.isalpha():
            key = f"shift-{ch.lower()}" if ch.isupper() else ch
        elif ch.isdigit():
            key = ch
        else:
            continue
        monitor_cmd(sock, f"sendkey {key}")
        time.sleep(delay)


def ssh_up(port, key):
    try:
        r = subprocess.run(
            ["ssh", "-o", "StrictHostKeyChecking=no",
             "-o", "UserKnownHostsFile=/dev/null",
             "-o", "LogLevel=ERROR",
             "-o", "ConnectTimeout=2",
             "-i", key,
             "-p", str(port), "root@localhost", "true"],
            capture_output=True, timeout=5,
        )
        return r.returncode == 0
    except (subprocess.TimeoutExpired, OSError):
        return False


def wait_for_ssh(port, key, timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        if ssh_up(port, key):
            return True
        time.sleep(2)
    return False


def screendump(sock, path):
    monitor_cmd(sock, f"screendump {os.path.abspath(path)}")


def send_bootstrap(sock):
    """Mount the seed ISO and run setup.sh with sudo.

    The NixOS minimal ISO auto-logs in as the nixos user on tty1.
    The nixos user has passwordless sudo, so prefix privileged commands
    with sudo.
    """
    sendkeys(sock, "sudo mkdir -p /tmp/seed && sudo mount -L CIDATA /tmp/seed && sudo bash /tmp/seed/setup.sh\n")
    time.sleep(5)


def main():
    if ssh_up(SSH_PORT, SSH_KEY):
        print("SSH already up — system is running.")
        return 0

    print("Connecting to QEMU monitor...")
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(2.0)
    sock.connect(MONITOR_SOCK)
    time.sleep(0.5)
    try:
        sock.recv(65536)
    except socket.timeout:
        pass

    print(f"Waiting up to {BOOT_TIMEOUT}s for NixOS to boot...")
    start = time.time()
    while time.time() - start < BOOT_TIMEOUT:
        time.sleep(10)
        elapsed = int(time.time() - start)
        print(f"  ...{elapsed}s")
        if ssh_up(SSH_PORT, SSH_KEY):
            print("SSH already up — system is running.")
            sock.close()
            return 0

    print("Sending bootstrap commands...")
    send_bootstrap(sock)

    print(f"Waiting up to {SSH_TIMEOUT}s for SSH on port {SSH_PORT}...")
    if wait_for_ssh(SSH_PORT, SSH_KEY, SSH_TIMEOUT):
        print("SSH is up!")
        sock.close()
        return 0

    print("SSH not up — retrying bootstrap...")
    send_bootstrap(sock)
    if wait_for_ssh(SSH_PORT, SSH_KEY, SSH_TIMEOUT):
        print("SSH is up!")
        sock.close()
        return 0

    print("SSH did not come up. Saving diagnostic screendump...")
    screendump(sock, "nixos-console.ppm")
    print(f"Screendump: {os.path.abspath('nixos-console.ppm')}")
    sock.close()
    return 1


if __name__ == "__main__":
    sys.exit(main())
