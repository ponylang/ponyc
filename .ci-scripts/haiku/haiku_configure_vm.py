#!/usr/bin/env python3
"""Drive the Haiku VM's over the QEMU monitor. Peek into what's happening
inside Haiku VM through a serial socket.

Haiku ISO images boot to a passwordless root/user UI with no cloud-init, so the
setup commands (login, network, sshd, ssh key, build disk) are typed in with QEMU
monitor `sendkey`. Runs on the GitHub Actions host after the VM boots; mounts 
seed cd-rom and runs haiku-guest-installer.bash.

Reads HAIKU_MONITOR_SOCKET, HAIKU_SERIAL_SOCKET, and IS_NIGHTLY from the environment.
Called by haiku-provision.bash.
"""
import os
import socket
import time
import sys

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


def send_hmp(monitor, cmd):
    monitor.sendall((cmd + '\n').encode())
    time.sleep(0.1)
    monitor.settimeout(0.3)
    try:
        data=monitor.recv(4096)
        # if len(data):
        #     print(data.decode(encoding='utf-8', errors='backslashreplace'), end='', flush=True)
    except socket.timeout:
        pass
    monitor.settimeout(None)


def send_line(monitor, text):
    for ch in text + '\n':
        if ch in KEYMAP:
            key = KEYMAP[ch]
        elif ch.isalpha():
            key = f'shift-{ch.lower()}' if ch.isupper() else ch
        elif ch.isdigit():
            key = ch
        else:
            continue
        send_hmp(monitor, f'sendkey {key}')
        time.sleep(0.03)

def wait_for_serial_output(serial, buf, search):
    while True:
        data = serial.recv(2048)
        if not data:
            return False
        buf.extend(data)
        print(data.decode(encoding='utf-8', errors='backslashreplace'), end='', flush=True)
        (before,sep,after) = buf.rpartition(search)
        if sep:
            buf.clear()
            buf.extend(after)
            return True
        if buf.rfind(b'\r\n[RESULT]:FAIL') >= 0:
            return False

def wait_for_result(serial, buf):
    if not wait_for_serial_output(serial, buf, b'\r\n[RESULT]:'):
        return False
    success = b'OK\r\n'
    while len(buf) < len(success):
        data = serial.recv(2048)
        if not data:
            return False
        buf.extend(data)
        print(data.decode(encoding='utf-8', errors='backslashreplace'), end='', flush=True)
    return buf.startswith(success)

def wait_until_haiku_is_ready(serial, buf):
    # Wait for known text output from serial socket
    # This will let us know that Haiku VM is ready
    return wait_for_serial_output(serial, buf, b'\r\nps2: keyboard found')

def haiku_open_terminal(monitor):
    # Press ALT+Meta+T to open Terminal
    send_hmp(monitor, f'sendkey alt-meta_l-t')
    time.sleep(5)

def haiku_install(monitor, serial, buf):
    # Press enter and login as root (no password)
    send_line(monitor, '')
    time.sleep(5)

    haiku_open_terminal(monitor)

    # Get our "seed" data
    send_line(monitor, 'mkdir /seed 2>&1 >/dev/dprintf && echo "[RESULT]:OK" >/dev/dprintf || echo "[RESULT]:FAIL" >/dev/dprintf')
    if not wait_for_result(serial, buf):
        sys.exit('[host] Failed to prepare mountpoint for seed CD-Rom')

    send_line(monitor, 'mount /dev/disk/atapi/1/slave/raw /seed 2>&1 >/dev/dprintf && echo "[RESULT]:OK" >/dev/dprintf || echo "[RESULT]:FAIL" >/dev/dprintf')
    if not wait_for_result(serial, buf):
        sys.exit('[host] Failed to mount seed CD-Rom')

    send_line(monitor, 'bash /seed/haiku-guest-installer.bash')
    if not wait_for_serial_output(serial, buf, b'\r\n::guest::Run Installer'):
        sys.exit('[host] Failed to run haiku-guest-installer.bash inside Haiku VM')

    if not wait_for_serial_output(serial, buf, b'\r\n::guest::Installer is ready'):
        sys.exit('[host] Failed to run haiku-guest-installer.bash inside Haiku VM')

    # Press enter to continue
    send_line(monitor, '')
    time.sleep(2)
    # Press TAB twice to jump to target disk selection
    send_hmp(monitor, f'sendkey tab')
    time.sleep(0.3)
    send_hmp(monitor, f'sendkey tab')
    time.sleep(0.3)

    # Press DOWN arrow twice to select first disk (first arrow pops-up menu)
    send_hmp(monitor, f'sendkey down')
    time.sleep(0.3)
    send_hmp(monitor, f'sendkey down')
    time.sleep(0.3)

    # Press enter to accept selection
    send_line(monitor, '')

    # Press enter to start installation process
    send_line(monitor, '')

    if not wait_for_serial_output(serial, buf, b'\r\nWriting boot code to "/dev/disk/ata/0/master/raw"'):
        sys.exit('[host] Installer failed before writing boot code to disk')

    if not wait_for_serial_output(serial, buf, b' MB written ('):
        sys.exit('[host] Installer failed before writing summary')

    # Wait a bit more, just to be on the safe-side
    time.sleep(1)

    # Press enter to close Installer
    # We should be back in the Terminal window after that.
    send_line(monitor, '')

    if not wait_for_serial_output(serial, buf, b'\r\n::guest::Reboot'):
        sys.exit('[host] Installer failed before first reboot')

    # Should output '[RESULT]:OK' after our customization script is run
    # and it's about to reboot again.
    if not wait_for_result(serial, buf):
        sys.exit('[host] Failed to setup system')

    if os.environ.get('IS_NIGHTLY', 'yes') == 'yes':
        if not wait_until_haiku_is_ready(serial, buf):
            sys.exit('[host] Failed to boot into Haiku OS ready for use')
    else:
        # In case of LTS releases, serial output is disabled, but there's:
        # 'vesa: vesa_uninit()' before last reboot, so we have to use that
        # and then wait additional time, crossing fingers that's enough.
        if not wait_for_serial_output(serial, buf, b'vesa: vesa_uninit()'):
            sys.exit('[host] Installation process failed to call reboot')
        time.sleep(30)

    # Commit changes to disk
    send_hmp(monitor, f'commit system')
    time.sleep(0.3)

def main():
    monitor_sock_path = os.environ.get('HAIKU_MONITOR_SOCKET', 'haiku_monitor.sock')
    serial_sock_path = os.environ.get('HAIKU_SERIAL_SOCKET', 'haiku_serial.sock')

    serial = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    serial.connect(serial_sock_path)
    buf = bytearray()

    monitor = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    monitor.connect(monitor_sock_path)

    # Wait for known text output from serial socket
    # This will let us know that Haiku VM is ready
    if not wait_until_haiku_is_ready(serial, buf):
        sys.exit('[host] Failure before Haiku VM became ready for keyboard input')

    haiku_install(monitor, serial, buf)

    serial.close()
    monitor.close()


if __name__ == "__main__":
    main()
