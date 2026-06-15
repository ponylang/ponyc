#!/usr/bin/env python3
"""Tests for dfly_configure_vm: KEYMAP de-escaping and send_line key mapping.

These guard the heredoc-to-standalone-.py extraction. The KEYMAP entries for
backslash, backtick and dollar were shell-escaped in the original embedded
heredoc, so a botched de-escape would silently corrupt them. No VM required.
"""
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

    if failures:
        print(f"dfly_configure_vm_test: FAIL ({len(failures)}): {', '.join(failures)}")
        return 1
    print("dfly_configure_vm_test: ok (10 checks)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
