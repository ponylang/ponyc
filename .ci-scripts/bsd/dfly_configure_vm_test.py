#!/usr/bin/env python3
"""Tests for dfly_configure_vm.

Guards the KEYMAP de-escaping (backslash, backtick, dollar were shell-escaped in
the original heredoc), send_line key mapping, the socket-path contract with
dragonfly-provision.bash (DFLY_MONITOR_SOCK), the sendkey + SSH bootstrap flow,
screendump-based boot detection, and the PPM pixel hash helper.  No VM required.
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
    """Monitor socket that writes fake PPM files on screendump commands.

    Writes a different image for the first N screendumps (simulating boot),
    then a stable image for the rest (simulating the login prompt).
    """

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


def run_main(env, monitor_cls=None, ssh_succeed_on_attempt=1):
    """Run main() with stubbed sockets and SSH check.

    ssh_succeed_on_attempt: SSH becomes reachable on this sendkey attempt
    number (1-based).  Set to 0 for "never reachable".
    """
    saved_env = dict(os.environ)
    saved_socket = d.socket.socket
    saved_send_line = d.send_line
    saved_ssh = d.ssh_reachable
    saved_bios_wait = d.BIOS_WAIT
    saved_boot_timeout = d.BOOT_TIMEOUT
    saved_stable = d.STABLE_SECONDS
    saved_interval = d.SCREENDUMP_INTERVAL
    saved_ssh_timeout = d.SSH_CHECK_TIMEOUT
    saved_ssh_interval = d.SSH_CHECK_INTERVAL
    saved_max_attempts = d.MAX_ATTEMPTS

    d.BIOS_WAIT = 0
    d.BOOT_TIMEOUT = 5
    d.STABLE_SECONDS = 0
    d.SCREENDUMP_INTERVAL = 0
    d.SSH_CHECK_TIMEOUT = 0.01
    d.SSH_CHECK_INTERVAL = 0

    monitor = (monitor_cls() if monitor_cls else
               ScreendumpMonitorSock(unstable_count=0))

    def fake_socket(*_a, **_k):
        return monitor

    typed = []

    def capturing_send_line(sock, text):
        typed.append(text)
        saved_send_line(sock, text)

    def fake_ssh_reachable():
        if ssh_succeed_on_attempt == 0:
            return False
        # Count attempts by how many times 'setup.sh' has been typed
        current = sum(1 for t in typed if 'setup.sh' in t)
        return current >= ssh_succeed_on_attempt

    if ssh_succeed_on_attempt == 0:
        d.MAX_ATTEMPTS = 2

    os.environ.clear()
    os.environ.update(env)
    d.socket.socket = fake_socket
    d.send_line = capturing_send_line
    d.ssh_reachable = fake_ssh_reachable
    try:
        rc = d.main()
    finally:
        d.send_line = saved_send_line
        d.socket.socket = saved_socket
        d.ssh_reachable = saved_ssh
        d.BIOS_WAIT = saved_bios_wait
        d.BOOT_TIMEOUT = saved_boot_timeout
        d.STABLE_SECONDS = saved_stable
        d.SCREENDUMP_INTERVAL = saved_interval
        d.SSH_CHECK_TIMEOUT = saved_ssh_timeout
        d.SSH_CHECK_INTERVAL = saved_ssh_interval
        d.MAX_ATTEMPTS = saved_max_attempts
        os.environ.clear()
        os.environ.update(saved_env)

    return rc, monitor, typed


def main():
    failures = []

    def check(name, cond):
        if not cond:
            failures.append(name)

    # -- KEYMAP: chars that were backslash-escaped in the original heredoc --
    check("backslash maps to 'backslash'", d.KEYMAP['\\'] == 'backslash')
    check("backtick maps to 'grave_accent'", d.KEYMAP['`'] == 'grave_accent')
    check("dollar maps to 'shift-4'", d.KEYMAP['$'] == 'shift-4')

    # -- send_line key mapping --
    check("send_line appends ret", keys_for('') == ['ret'])
    check("'$' -> shift-4, ret", keys_for('$') == ['shift-4', 'ret'])
    check("backslash -> backslash, ret", keys_for('\\') == ['backslash', 'ret'])
    check("backtick -> grave_accent, ret", keys_for('`') == ['grave_accent', 'ret'])
    check("'aB' -> a, shift-b, ret", keys_for('aB') == ['a', 'shift-b', 'ret'])
    check("'7' -> 7, ret", keys_for('7') == ['7', 'ret'])
    check("tab is skipped", keys_for('\t') == ['ret'])

    # -- _ppm_pixel_hash --
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

    # -- wait_for_boot with screendumps --
    with tempfile.TemporaryDirectory() as tmpdir:
        saved_bios = d.BIOS_WAIT
        saved_timeout = d.BOOT_TIMEOUT
        saved_stable = d.STABLE_SECONDS
        saved_interval = d.SCREENDUMP_INTERVAL
        d.BIOS_WAIT = 0
        d.BOOT_TIMEOUT = 5
        d.STABLE_SECONDS = 0
        d.SCREENDUMP_INTERVAL = 0
        try:
            mon = ScreendumpMonitorSock(unstable_count=1)
            result = d.wait_for_boot(mon, tmpdir)
            check("wait_for_boot: returns True on stable screen", result)
            last = os.path.join(tmpdir, 'last-console.ppm')
            check("wait_for_boot: saves last-console.ppm",
                  os.path.exists(last))
        finally:
            d.BIOS_WAIT = saved_bios
            d.BOOT_TIMEOUT = saved_timeout
            d.STABLE_SECONDS = saved_stable
            d.SCREENDUMP_INTERVAL = saved_interval

    # -- wait_for_boot with no artifacts_dir --
    saved_bios = d.BIOS_WAIT
    saved_timeout = d.BOOT_TIMEOUT
    saved_stable = d.STABLE_SECONDS
    saved_interval = d.SCREENDUMP_INTERVAL
    d.BIOS_WAIT = 0
    d.BOOT_TIMEOUT = 0.01
    d.STABLE_SECONDS = 0
    d.SCREENDUMP_INTERVAL = 0
    try:
        mon_no_dir = ScreendumpMonitorSock(unstable_count=100)
        result_no_dir = d.wait_for_boot(mon_no_dir, "")
        check("wait_for_boot: returns False on timeout (no dir)",
              not result_no_dir)
    finally:
        d.BIOS_WAIT = saved_bios
        d.BOOT_TIMEOUT = saved_timeout
        d.STABLE_SECONDS = saved_stable
        d.SCREENDUMP_INTERVAL = saved_interval

    # -- full flow: SSH reachable on first attempt --
    with tempfile.TemporaryDirectory() as tmpdir:
        env = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc, monitor, typed = run_main(env)

        check("main returns 0 on success", rc == 0)

        sendkeys = [c.removeprefix('sendkey ')
                    for c in monitor.sent if c.startswith('sendkey ')]

        check(
            "screendump commands are sent",
            any("screendump" in c for c in monitor.sent),
        )
        check(
            "console is reset (ctrl-c) before login",
            sendkeys[0] == 'ctrl-c',
        )
        check(
            "root login is typed",
            'root' in typed,
        )
        check(
            "mount command is typed",
            any('mount_cd9660' in line for line in typed),
        )
        check(
            "setup script is typed",
            any('setup.sh' in line for line in typed),
        )

    # -- socket path defaults --
    env_default = {}
    _rc_d, mon_d, _typed_d = run_main(env_default)
    check(
        "monitor socket defaults to dfly-monitor.sock",
        mon_d.connected_path == "dfly-monitor.sock",
    )

    # -- socket path overrides --
    env_override = {
        "DFLY_MONITOR_SOCK": "/tmp/vm/mon.sock",
    }
    _rc2, mon2, _typed2 = run_main(env_override)
    check(
        "DFLY_MONITOR_SOCK override is honored",
        mon2.connected_path == "/tmp/vm/mon.sock",
    )

    # -- retry: SSH not reachable on first attempt, succeeds on second --
    with tempfile.TemporaryDirectory() as tmpdir:
        env_retry = {"DFLY_ARTIFACTS_DIR": tmpdir}
        rc_retry, _mon_r, typed_r = run_main(
            env_retry, ssh_succeed_on_attempt=2,
        )
        check("retry: main returns 0", rc_retry == 0)
        root_count = sum(1 for t in typed_r if t == 'root')
        check("retry: typed root at least twice", root_count >= 2)

    # -- failure: SSH never reachable --
    rc_fail, _mon_f, _typed_f = run_main(
        {}, ssh_succeed_on_attempt=0,
    )
    check("failure: main returns 1", rc_fail == 1)

    # -- ssh_reachable with real sockets --
    check("ssh_reachable: returns False on unlistened port",
          not d.ssh_reachable())

    total = 27
    if failures:
        print(f"dfly_configure_vm_test: FAIL ({len(failures)}): "
              f"{', '.join(failures)}")
        return 1
    print(f"dfly_configure_vm_test: ok ({total} checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
