#!/usr/bin/env python3
"""Drive the DragonFly BSD VM's VGA console over the QEMU monitor.

DragonFly raw images boot to a passwordless root login with no cloud-init, so the
setup commands (login, network, sshd, ssh key, build disk) are typed in with QEMU
monitor `sendkey`. Runs on the GitHub Actions host after the VM boots; reads the
public key to install from the PUB_KEY environment variable and the QEMU monitor
socket path from DFLY_MONITOR_SOCK (defaulting to dfly-monitor.sock in the
current directory). Called by dragonfly-provision.bash.
"""
import os
import socket
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


def main():
    pub_key = os.environ["PUB_KEY"]
    monitor_sock = os.environ.get("DFLY_MONITOR_SOCK", "dfly-monitor.sock")

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(monitor_sock)
    time.sleep(0.5)
    sock.recv(4096)

    # Press enter and login as root (no password)
    send_line(sock, '')
    time.sleep(2)
    send_line(sock, 'root')
    time.sleep(3)

    # Configure network
    send_line(sock, 'dhclient vtnet0')
    time.sleep(15)

    # Configure sshd
    send_line(sock, 'echo "PermitRootLogin yes" >> /etc/ssh/sshd_config')
    time.sleep(0.5)
    send_line(sock, 'echo "PermitEmptyPasswords yes" >> /etc/ssh/sshd_config')
    time.sleep(0.5)

    # Install SSH key
    send_line(sock, 'mkdir -p /root/.ssh && chmod 700 /root/.ssh')
    time.sleep(0.5)
    send_line(sock, f'echo "{pub_key}" > /root/.ssh/authorized_keys')
    time.sleep(0.5)
    send_line(sock, 'chmod 600 /root/.ssh/authorized_keys')
    time.sleep(0.5)

    # Generate host keys (virtio-rng provides entropy) and start sshd
    send_line(sock, 'ssh-keygen -A')
    time.sleep(10)
    send_line(sock, '/usr/sbin/sshd')
    time.sleep(3)

    # Format and mount build disk
    send_line(sock, 'newfs /dev/vbd1')
    time.sleep(10)
    send_line(sock, 'mkdir -p /build && mount /dev/vbd1 /build')
    time.sleep(2)

    sock.close()


if __name__ == "__main__":
    main()
