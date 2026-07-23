#!/usr/bin/env python3
"""Tests for dfly_configure_vm: KEYMAP de-escaping, send_line key mapping, the
monitor-socket path selection, and the console reset.

The KEYMAP checks guard the heredoc-to-standalone-.py extraction: the entries for
backslash, backtick and dollar were shell-escaped in the original embedded
heredoc, so a botched de-escape would silently corrupt them. The monitor-socket
checks guard the contract with dragonfly-provision.bash, which creates the socket
under VM_ARTIFACTS (outside the checkout, so it isn't rsynced into the guest) and
passes its path via DFLY_MONITOR_SOCK; hardcoding the path back would break
DragonFly provisioning. The console-reset check guards that main() sends ctrl-c
before any login keystroke, so a re-run after a dropped keystroke starts at a fresh
prompt rather than adding to a half-typed line. No VM required.
"""
import os
import sys

import dfly_configure_vm as d


def _noop(*_args, **_kwargs):
    return None


# the module's per-keystroke sleeps would make the test crawl; stub them out
d.time.sleep = _noop


class FakeSock:
    def __init__(self):
        self.keys = []

    def sendall(self, data):
        self.keys.append(data.decode().strip())

    def settimeout(self, _):
        pass

    def recv(self, _):
        return b''


def keys_for(text):
    sock = FakeSock()
    d.send_line(sock, text)
    return [k.removeprefix('sendkey ') for k in sock.keys]


class ConnectSock:
    """A socket stand-in that records the path main() connects to."""

    connected_path = None

    def connect(self, path):
        ConnectSock.connected_path = path

    def sendall(self, _):
        pass

    def settimeout(self, _):
        pass

    def recv(self, _):
        return b''

    def close(self):
        pass


def monitor_path_for(env):
    """Run main() with a stubbed socket and env; return the connected path."""
    ConnectSock.connected_path = None
    saved_env = dict(os.environ)
    saved_socket = d.socket.socket
    os.environ.clear()
    os.environ.update(env)
    d.socket.socket = lambda *_a, **_k: ConnectSock()
    try:
        d.main()
    finally:
        d.socket.socket = saved_socket
        os.environ.clear()
        os.environ.update(saved_env)
    return ConnectSock.connected_path


class RecordingSock:
    """A socket stand-in that records every command main() sends."""

    def __init__(self):
        self.sent = []

    def connect(self, _):
        pass

    def sendall(self, data):
        self.sent.append(data.decode().strip())

    def settimeout(self, _):
        pass

    def recv(self, _):
        return b''

    def close(self):
        pass


def capture_main(env):
    """Run main() with a recording socket and send_line captured; return
    (sendkeys, typed_lines) -- the ordered sendkey args and the text of each line
    main() types into the console."""
    saved_env = dict(os.environ)
    saved_socket = d.socket.socket
    saved_send_line = d.send_line
    rec = RecordingSock()
    typed = []

    def capturing_send_line(sock, text):
        typed.append(text)
        saved_send_line(sock, text)

    os.environ.clear()
    os.environ.update(env)
    d.socket.socket = lambda *_a, **_k: rec
    d.send_line = capturing_send_line
    try:
        d.main()
    finally:
        d.send_line = saved_send_line
        d.socket.socket = saved_socket
        os.environ.clear()
        os.environ.update(saved_env)
    sendkeys = [c.removeprefix('sendkey ') for c in rec.sent if c.startswith('sendkey ')]
    return sendkeys, typed


def main():
    failures = []

    def check(name, cond):
        if not cond:
            failures.append(name)

    # KEYMAP: the three chars that were backslash-escaped in the heredoc.
    check("backslash maps to 'backslash'", d.KEYMAP['\\'] == 'backslash')
    check("backtick maps to 'grave_accent'", d.KEYMAP['`'] == 'grave_accent')
    check("dollar maps to 'shift-4'", d.KEYMAP['$'] == 'shift-4')

    # send_line appends a trailing newline, which maps to 'ret'.
    check("send_line appends ret", keys_for('') == ['ret'])
    # punctuation routes through KEYMAP (the escaped trio in context).
    check("'$' -> shift-4, ret", keys_for('$') == ['shift-4', 'ret'])
    check("backslash -> backslash, ret", keys_for('\\') == ['backslash', 'ret'])
    check("backtick -> grave_accent, ret", keys_for('`') == ['grave_accent', 'ret'])
    # letters: lower verbatim, upper as shift-<lower>.
    check("'aB' -> a, shift-b, ret", keys_for('aB') == ['a', 'shift-b', 'ret'])
    # digits pass through.
    check("'7' -> 7, ret", keys_for('7') == ['7', 'ret'])
    # unmapped chars (e.g. tab) are skipped.
    check("tab is skipped", keys_for('\t') == ['ret'])

    # monitor socket: DFLY_MONITOR_SOCK selects the path main() connects to,
    # falling back to the cwd-relative default when it is unset.
    check(
        "DFLY_MONITOR_SOCK is honored",
        monitor_path_for({"PUB_KEY": "k", "DFLY_MONITOR_SOCK": "/tmp/vm/mon.sock"})
        == "/tmp/vm/mon.sock",
    )
    check(
        "monitor socket defaults to dfly-monitor.sock",
        monitor_path_for({"PUB_KEY": "k"}) == "dfly-monitor.sock",
    )

    # main() clears the console before it logs in, so a re-run after a dropped
    # keystroke starts at a fresh prompt; newfs/mount are no longer typed (the
    # provision script mounts the build disk over ssh once ssh is up); and the
    # bring-up still starts sshd.
    sendkeys, typed = capture_main({"PUB_KEY": "k"})
    check("console is reset (ctrl-c) before any login keystroke", sendkeys[0] == "ctrl-c")
    check(
        "newfs/mount are not typed into the console",
        not any("newfs" in line or "mount" in line for line in typed),
    )
    check("bring-up starts sshd", any("/usr/sbin/sshd" in line for line in typed))

    if failures:
        print(f"dfly_configure_vm_test: FAIL ({len(failures)}): {', '.join(failures)}")
        return 1
    print("dfly_configure_vm_test: ok (15 checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
