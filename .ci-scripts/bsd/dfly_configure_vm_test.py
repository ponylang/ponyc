#!/usr/bin/env python3
"""Tests for dfly_configure_vm.

Guards the KEYMAP, send_line key mapping, the socket-path contract with
dragonfly-provision.bash, the sendkey + SSH bootstrap flow, screendump-based
boot detection, PPM pixel hash, the serial console setup path, and the
fallback from serial to VGA sendkey.  No VM required.
"""
import os
import socket
import sys
import tempfile

import dfly_configure_vm as d


def _noop(*_args, **_kwargs):
    return None


d.time.sleep = _noop


class FakeSock:
    def __init__(self):
        self.keys = []

    def sendall(self, data):
        self.keys.append(data.decode().strip())

    def settimeout(self, _):
        pass

    def recv(self, _):
        return b'(qemu) '


def keys_for(text):
    sock = FakeSock()
    d.send_line(sock, text)
    return [k.removeprefix('sendkey ') for k in sock.keys]


class MonitorSock:
    """Records HMP commands sent to the monitor socket."""

    def __init__(self):
        self.sent = []
        self._first_recv = True

    def connect(self, path):
        self.connected_path = path

    def sendall(self, data):
        self.sent.append(data.decode().strip())

    def settimeout(self, _):
        pass

    def recv(self, _):
        if self._first_recv:
            self._first_recv = False
            return b'(qemu) '
        raise socket.timeout

    def close(self):
        pass


class ScreendumpMonitorSock(MonitorSock):
    """Monitor socket that writes fake PPM files on screendump commands."""

    def __init__(self, unstable_count=2):
        super().__init__()
        self._dump_count = 0
        self._unstable_count = unstable_count

    def sendall(self, data):
        text = data.decode().strip()
        self.sent.append(text)
        if text.startswith('screendump '):
            path = text.split(' ', 1)[1]
            self._dump_count += 1
            if self._dump_count <= self._unstable_count:
                pixel = bytes([self._dump_count] * 30)
            else:
                pixel = bytes([0xFF] * 30)
            ppm = b'P6\n10 1\n255\n' + pixel
            try:
                with open(path, 'wb') as f:
                    f.write(ppm)
            except OSError:
                pass


class FakeSerialSock:
    """Mock serial socket that returns scripted responses.

    *responses* maps a sent-data substring to the bytes that should appear
    in the receive buffer after that send.  *initial* is data already in
    the buffer when the first recv is called (e.g. the loader prompt that
    appears after the VGA ``set console=comconsole`` command).
    """

    def __init__(self, responses=None, initial=b''):
        self._responses = responses or {}
        self._buffer = bytearray(initial)
        self.sent = []
        self._connected = False

    def connect(self, path):
        self._connected = True
        self.connected_path = path

    def settimeout(self, _):
        pass

    def recv(self, n):
        if self._buffer:
            data = bytes(self._buffer[:n])
            del self._buffer[:n]
            return data
        raise socket.timeout

    def sendall(self, data):
        self.sent.append(data)
        for pattern, response in self._responses.items():
            if pattern in data:
                self._buffer.extend(response)

    def close(self):
        pass


def _patch_constants():
    """Zero out all timeouts for fast tests.  Returns a restore callable."""
    saved = {
        'BIOS_WAIT': d.BIOS_WAIT,
        'BOOT_TIMEOUT': d.BOOT_TIMEOUT,
        'STABLE_SECONDS': d.STABLE_SECONDS,
        'SCREENDUMP_INTERVAL': d.SCREENDUMP_INTERVAL,
        'SSH_CHECK_TIMEOUT': d.SSH_CHECK_TIMEOUT,
        'SSH_CHECK_INTERVAL': d.SSH_CHECK_INTERVAL,
        'MAX_ATTEMPTS': d.MAX_ATTEMPTS,
        'LOADER_WAIT': d.LOADER_WAIT,
        'SERIAL_DETECT_TIMEOUT': d.SERIAL_DETECT_TIMEOUT,
        'SERIAL_BOOT_DETECT_TIMEOUT': d.SERIAL_BOOT_DETECT_TIMEOUT,
        'SERIAL_BOOT_TIMEOUT': d.SERIAL_BOOT_TIMEOUT,
        'SERIAL_CMD_TIMEOUT': d.SERIAL_CMD_TIMEOUT,
        'POST_BOOT_DELAY': d.POST_BOOT_DELAY,
    }
    d.BIOS_WAIT = 0
    d.BOOT_TIMEOUT = 5
    d.STABLE_SECONDS = 0
    d.SCREENDUMP_INTERVAL = 0
    d.SSH_CHECK_TIMEOUT = 0.01
    d.SSH_CHECK_INTERVAL = 0
    d.MAX_ATTEMPTS = 5
    d.LOADER_WAIT = 0
    d.SERIAL_DETECT_TIMEOUT = 0.01
    d.SERIAL_BOOT_DETECT_TIMEOUT = 0.01
    d.SERIAL_BOOT_TIMEOUT = 0.01
    d.SERIAL_CMD_TIMEOUT = 0.01
    d.POST_BOOT_DELAY = 0

    def restore():
        for k, v in saved.items():
            setattr(d, k, v)
    return restore


def run_main(env, monitor_cls=None, ssh_succeed_on_attempt=1,
             serial_sock=None, serial_available=False):
    """Run main() with stubbed sockets and SSH check.

    ssh_succeed_on_attempt: SSH becomes reachable on this sendkey attempt
    number (1-based).  Set to 0 for "never reachable".

    serial_sock: a FakeSerialSock (or None).  When *serial_available* is
    True and *serial_sock* is None, a default no-output serial is created.
    """
    saved_env = dict(os.environ)
    saved_socket = d.socket.socket
    saved_send_line = d.send_line
    saved_ssh = d.ssh_reachable
    saved_create_serial = d._create_serial_socket

    restore = _patch_constants()

    monitor = (monitor_cls() if monitor_cls else
               ScreendumpMonitorSock(unstable_count=0))

    if serial_available and serial_sock is None:
        serial_sock = FakeSerialSock()

    def fake_socket(*_a, **_k):
        return monitor

    typed = []

    def capturing_send_line(sock, text):
        typed.append(text)
        saved_send_line(sock, text)

    def fake_ssh_reachable():
        if ssh_succeed_on_attempt == 0:
            return False
        if ssh_succeed_on_attempt == 1:
            return True
        attempt_count = sum(1 for t in typed if t == 'root')
        return attempt_count >= ssh_succeed_on_attempt

    def fake_create_serial(path):
        if serial_sock is not None:
            serial_sock.connect(path)
            return serial_sock
        return None

    if ssh_succeed_on_attempt == 0:
        d.MAX_ATTEMPTS = 2

    os.environ.clear()
    os.environ.update(env)
    d.socket.socket = fake_socket
    d.send_line = capturing_send_line
    d.ssh_reachable = fake_ssh_reachable
    d._create_serial_socket = fake_create_serial
    try:
        rc = d.main()
    finally:
        d.send_line = saved_send_line
        d.socket.socket = saved_socket
        d.ssh_reachable = saved_ssh
        d._create_serial_socket = saved_create_serial
        restore()
        os.environ.clear()
        os.environ.update(saved_env)

    return rc, monitor, typed


def main():
    failures = []

    def check(name, cond):
        if not cond:
            failures.append(name)

    # ---- KEYMAP: chars that were backslash-escaped in the original heredoc
    check("backslash maps to 'backslash'", d.KEYMAP['\\'] == 'backslash')
    check("backtick maps to 'grave_accent'", d.KEYMAP['`'] == 'grave_accent')
    check("dollar maps to 'shift-4'", d.KEYMAP['$'] == 'shift-4')

    # ---- send_line key mapping
    check("send_line appends ret", keys_for('') == ['ret'])
    check("'$' -> shift-4, ret", keys_for('$') == ['shift-4', 'ret'])
    check("backslash -> backslash, ret", keys_for('\\') == ['backslash', 'ret'])
    check("backtick -> grave_accent, ret",
          keys_for('`') == ['grave_accent', 'ret'])
    check("'aB' -> a, shift-b, ret", keys_for('aB') == ['a', 'shift-b', 'ret'])
    check("'7' -> 7, ret", keys_for('7') == ['7', 'ret'])
    check("tab is skipped", keys_for('\t') == ['ret'])

    # ---- _ppm_pixel_hash
    ppm = b'P6\n10 1\n255\n' + bytes(30)
    h1 = d._ppm_pixel_hash(ppm)
    h2 = d._ppm_pixel_hash(ppm)
    check("ppm_pixel_hash: same data same hash", h1 == h2)
    ppm2 = b'P6\n10 1\n255\n' + bytes([1] * 30)
    h3 = d._ppm_pixel_hash(ppm2)
    check("ppm_pixel_hash: different pixels different hash", h1 != h3)
    ppm_alt_hdr = b'P6\n5 2\n255\n' + bytes(30)
    check("ppm_pixel_hash: ignores header",
          d._ppm_pixel_hash(ppm) == d._ppm_pixel_hash(ppm_alt_hdr))
    check("ppm_pixel_hash: handles None", d._ppm_pixel_hash(None) is not None)
    check("ppm_pixel_hash: handles non-PPM",
          d._ppm_pixel_hash(b'not a ppm') is not None)

    # ---- wait_for_boot with screendumps
    with tempfile.TemporaryDirectory() as tmpdir:
        restore = _patch_constants()
        try:
            mon = ScreendumpMonitorSock(unstable_count=1)
            result = d.wait_for_boot(mon, tmpdir)
            check("wait_for_boot: returns True on stable screen", result)
            last = os.path.join(tmpdir, 'last-console.ppm')
            check("wait_for_boot: saves last-console.ppm",
                  os.path.exists(last))
        finally:
            restore()

    # ---- wait_for_boot with no artifacts_dir
    restore = _patch_constants()
    d.BOOT_TIMEOUT = 0.01
    try:
        mon_no_dir = ScreendumpMonitorSock(unstable_count=100)
        result_no_dir = d.wait_for_boot(mon_no_dir, "")
        check("wait_for_boot: returns False on timeout (no dir)",
              not result_no_dir)
    finally:
        restore()

    # ---- wait_for_boot with skip_bios_wait
    with tempfile.TemporaryDirectory() as tmpdir:
        restore = _patch_constants()
        try:
            mon_skip = ScreendumpMonitorSock(unstable_count=0)
            result_skip = d.wait_for_boot(
                mon_skip, tmpdir, skip_bios_wait=True)
            check("wait_for_boot: skip_bios_wait works", result_skip)
        finally:
            restore()

    # ---- serial: _serial_read with data
    serial_r = FakeSerialSock(initial=b'hello world')
    restore = _patch_constants()
    try:
        data = d._serial_read(serial_r, timeout=0.01)
        check("_serial_read: returns available data", data == b'hello world')
    finally:
        restore()

    # ---- serial: _serial_read with no data
    serial_empty = FakeSerialSock()
    restore = _patch_constants()
    try:
        data = d._serial_read(serial_empty, timeout=0.01)
        check("_serial_read: returns empty on timeout", data == b'')
    finally:
        restore()

    # ---- serial: serial_read_until finds pattern
    serial_pat = FakeSerialSock(initial=b'booting...\r\nlogin: ')
    restore = _patch_constants()
    try:
        buf, found = d.serial_read_until(serial_pat, b'login:', 0.01)
        check("serial_read_until: finds pattern", found)
        check("serial_read_until: returns buffer", b'login:' in buf)
    finally:
        restore()

    # ---- serial: serial_read_until timeout
    serial_nopat = FakeSerialSock(initial=b'no match here')
    restore = _patch_constants()
    try:
        buf, found = d.serial_read_until(serial_nopat, b'login:', 0.01)
        check("serial_read_until: returns False on timeout", not found)
    finally:
        restore()

    # ---- serial: serial_send
    serial_s = FakeSerialSock()
    d.serial_send(serial_s, 'boot\r')
    check("serial_send: data is sent", serial_s.sent == [b'boot\r'])

    # ---- full flow: serial setup succeeds
    serial_ok = FakeSerialSock(
        initial=b'\r\nOK ',
        responses={
            b'boot\r': b'Booting...\r\nlogin: ',
            b'root\r': b'root\r\nLast login: Thu Jan 1\r\n# ',
            b'mount_cd9660 /dev/cd0 /mnt\r': b'# ',
            b'sh /mnt/setup.sh\r': b'Starting sshd.\r\n# ',
        },
    )
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc, monitor, typed = run_main(
            env, serial_sock=serial_ok, serial_available=True)
        check("serial success: main returns 0", rc == 0)
        check("serial success: boot sent via serial",
              b'boot\r' in serial_ok.sent)
        check("serial success: root sent via serial",
              b'root\r' in serial_ok.sent)
        check("serial success: mount sent via serial",
              b'mount_cd9660 /dev/cd0 /mnt\r' in serial_ok.sent)
        check("serial success: setup sent via serial",
              b'sh /mnt/setup.sh\r' in serial_ok.sent)
        # VGA sendkey should NOT have typed login commands
        check("serial success: no VGA root login",
              'root' not in typed)
        check("serial success: comconsole set via VGA",
              any('set console=comconsole' in t for t in typed))

    # ---- full flow: serial not available, falls back to sendkey
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc, monitor, typed = run_main(env, serial_available=False)
        check("no serial: main returns 0", rc == 0)
        check("no serial: root typed via sendkey", 'root' in typed)
        check("no serial: mount typed via sendkey",
              any('mount_cd9660' in t for t in typed))
        check("no serial: setup typed via sendkey",
              any('setup.sh' in t for t in typed))

    # ---- full flow: serial available but no output (console switch failed)
    serial_silent = FakeSerialSock()
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc, monitor, typed = run_main(
            env, serial_sock=serial_silent, serial_available=True)
        check("silent serial: main returns 0 (sendkey fallback)", rc == 0)
        check("silent serial: root typed via sendkey", 'root' in typed)

    # ---- full flow: serial OSError falls back to sendkey
    serial_err = FakeSerialSock(initial=b'\r\nOK ')
    _err_orig = serial_err.sendall

    def _err_sendall(data):
        if b'boot' in data:
            raise BrokenPipeError("serial disconnected")
        _err_orig(data)

    serial_err.sendall = _err_sendall
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_err, _mon_err, typed_err = run_main(
            env, serial_sock=serial_err, serial_available=True)
        check("serial OSError: main returns 0 (sendkey fallback)",
              rc_err == 0)
        check("serial OSError: root typed via sendkey",
              'root' in typed_err)

    # ---- full flow: serial with login prompt already present
    serial_login = FakeSerialSock(
        initial=b'\r\nlogin: ',
        responses={
            b'root\r': b'root\r\n# ',
            b'mount_cd9660 /dev/cd0 /mnt\r': b'# ',
            b'sh /mnt/setup.sh\r': b'Starting sshd.\r\n# ',
        },
    )
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_login, _mon_login, typed_login = run_main(
            env, serial_sock=serial_login, serial_available=True)
        check("serial login present: main returns 0", rc_login == 0)
        check("serial login present: boot not sent via serial",
              b'boot\r' not in serial_login.sent)

    # ---- full flow: sendkey retry (SSH reachable on attempt 2)
    with tempfile.TemporaryDirectory() as tmpdir:
        env_retry = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_retry, _mon_r, typed_r = run_main(
            env_retry, ssh_succeed_on_attempt=2)
        check("retry: main returns 0", rc_retry == 0)
        check("retry: root typed at least twice",
              sum(1 for t in typed_r if t == 'root') >= 2)

    # ---- full flow: complete failure (SSH never reachable)
    rc_fail, _mon_f, _typed_f = run_main(
        {}, ssh_succeed_on_attempt=0)
    check("failure: main returns 1", rc_fail == 1)

    # ---- socket path defaults
    env_default = {}
    _rc_d, mon_d, _typed_d = run_main(env_default)
    check(
        "monitor socket defaults to dfly-monitor.sock",
        mon_d.connected_path == "dfly-monitor.sock",
    )

    # ---- socket path overrides
    env_override = {"DFLY_MONITOR_SOCK": "/tmp/vm/mon.sock"}
    _rc2, mon2, _typed2 = run_main(env_override)
    check(
        "DFLY_MONITOR_SOCK override is honored",
        mon2.connected_path == "/tmp/vm/mon.sock",
    )

    # ---- serial socket path derivation
    serial_path_check = FakeSerialSock()
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {
            "DFLY_MONITOR_SOCK": os.path.join(tmpdir, "dfly-monitor.sock"),
            "DFLY_ARTIFACTS_DIR": tmpdir,
        }
        _rc3, _mon3, _typed3 = run_main(
            env, serial_sock=serial_path_check, serial_available=True)
        expected_serial = os.path.join(tmpdir, "dfly-serial.sock")
        check("serial socket path derived from monitor path",
              serial_path_check.connected_path == expected_serial)

    # ---- serial socket path override
    serial_path_override = FakeSerialSock()
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {
            "DFLY_MONITOR_SOCK": os.path.join(tmpdir, "dfly-monitor.sock"),
            "DFLY_SERIAL_SOCK": "/custom/serial.sock",
            "DFLY_ARTIFACTS_DIR": tmpdir,
        }
        _rc4, _mon4, _typed4 = run_main(
            env, serial_sock=serial_path_override, serial_available=True)
        check("DFLY_SERIAL_SOCK override is honored",
              serial_path_override.connected_path == "/custom/serial.sock")

    # ---- _looks_like_text
    check("_looks_like_text: empty is False",
          not d._looks_like_text(b''))
    check("_looks_like_text: readable text is True",
          d._looks_like_text(b'login: '))
    check("_looks_like_text: boot messages are True",
          d._looks_like_text(b'Booting...\r\nkernel text\r\n'))
    check("_looks_like_text: all-printable short string is True",
          d._looks_like_text(b'XMMNNOO'))
    check("_looks_like_text: high bytes are False",
          not d._looks_like_text(bytes(range(0x80, 0x90))))
    check("_looks_like_text: mixed mostly-printable is True",
          d._looks_like_text(b'hello world\x00'))
    check("_looks_like_text: exactly 60% printable is True",
          d._looks_like_text(b'helloo\x80\x81\x82\x83'))
    check("_looks_like_text: below 60% printable is False",
          not d._looks_like_text(b'hello\x80\x81\x82\x83\x84'))

    # ---- full flow: garbled serial falls back to sendkey
    serial_garble = FakeSerialSock(initial=b'\x80\x81\x82\x83\x84')
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_garble, _mon_garble, typed_garble = run_main(
            env, serial_sock=serial_garble, serial_available=True)
        check("garbled serial: main returns 0 (sendkey fallback)",
              rc_garble == 0)
        check("garbled serial: root typed via sendkey",
              'root' in typed_garble)
        check("garbled serial: screendump saved at transition",
              os.path.exists(os.path.join(tmpdir, 'serial-fallback.ppm')))

    # ---- full flow: garbled serial on both reads falls back to sendkey
    class AlwaysGarbledSerial(FakeSerialSock):
        def recv(self, n):
            return b'\x80\x81\x82\x83\x84'

    serial_garble2 = AlwaysGarbledSerial()
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_g2, _mon_g2, typed_g2 = run_main(
            env, serial_sock=serial_garble2, serial_available=True)
        check("garbled boot_data: main returns 0 (sendkey fallback)",
              rc_g2 == 0)
        check("garbled boot_data: root typed via sendkey",
              'root' in typed_g2)

    # ---- ssh_reachable with real sockets
    check("ssh_reachable: returns False on unlistened port",
          not d.ssh_reachable())

    # ---- summary
    total = 63
    if failures:
        print(f"dfly_configure_vm_test: FAIL ({len(failures)}): "
              f"{', '.join(failures)}")
        return 1
    print(f"dfly_configure_vm_test: ok ({total} checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
