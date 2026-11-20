import re
import struct
import subprocess


def output(*args):
    return subprocess.check_output(args, text=True)


def records(exe):
    nm = output('nm', '-n', exe)
    names = {name: int(addr, 16) for addr, name in
             re.findall(r'^([0-9a-f]+) [A-Za-z] (\S+)$', nm, re.M)}
    functions_by_address = {int(addr, 16): name for addr, name in
                            re.findall(r'^([0-9a-f]+) [tT] (\S+)$', nm, re.M)}
    begin, end = names['pony_maps_begin'], names['pony_maps_end']
    segments = output('readelf', '-Wl', exe)
    relro = next(line.split() for line in segments.splitlines() if 'GNU_RELRO' in line)
    assert int(relro[2], 16) <= begin <= end <= int(relro[2], 16) + int(relro[5], 16)
    assert 'TEXTREL' not in output('readelf', '-Wd', exe)
    if begin == end:
        return []
    section, = re.findall(r'\] \.data\.rel\.ro\s+PROGBITS\s+([0-9a-f]+)\s+([0-9a-f]+)',
                          output('readelf', '-WS', exe))
    offset = begin - int(section[0], 16) + int(section[1], 16)
    with exe.open('rb') as f:
        f.seek(offset)
        raw = f.read(end - begin)
    version, functions, constants, total = struct.unpack_from('<4I', raw)
    assert version == 3 and constants == 0
    reloc = {int(a, 16): int(b, 16) for a, b in re.findall(
        r'^([0-9a-f]+)\s+\w+\s+R_X86_64_RELATIVE\s+([0-9a-f]+)$',
        output('readelf', '-Wr', exe), re.M)}
    rows = []
    pos = 16 + functions * 24
    for index in range(functions):
        off = 16 + index * 24
        _, size, count = struct.unpack_from('<QQQ', raw, off)
        addr = reloc[begin + off]
        for _ in range(count):
            role, delta, flags, n = struct.unpack_from('<QIHH', raw, pos)
            assert flags == 0 and n == (0 if role == 4 else 1)
            reg = loc = 0
            if n:
                kind, _, width, reg, _, loc = struct.unpack_from('<BBHHHi', raw, pos + 16)
                assert (kind, width) == (2, 8)
                assert reg == 6 and -size <= loc <= -8 or reg == 7 and 0 <= loc <= size-8
            rows.append((functions_by_address[addr], role, addr + delta, reg, loc))
            pos = (pos + 16 + n * 12 + 7) & ~7
            assert struct.unpack_from('<HH', raw, pos) == (0, 0)
            pos += 8
    assert len(rows) == total and pos == len(raw)
    return rows


def inspect(exe, kind):
    rows = records(exe)
    if kind == 'check':
        assert not rows
        return []
    by_function = {}
    for name, role, pc, reg, slot in rows:
        by_function.setdefault(name, []).append((role, pc, reg, slot))
    published = []
    for name, sites in by_function.items():
        asm = output('objdump', '-d', '--no-show-raw-insn', '--disassemble='+name, exe)
        (exe.parent/(name+'.asm')).write_text(asm)
        insns = [(int(addr, 16), inst.strip()) for addr, inst in
                 re.findall(r'^\s*([0-9a-f]+):\s+([^\n]+)', asm, re.M)]
        addrs = [a for a, _ in insns]
        for role, pc, reg, slot in sites:
            if role == 1:
                continue
            i = addrs.index(pc)
            call = re.fullmatch(r'call\s+[a-f0-9]+ <([^>]+)>', insns[i-1][1])
            if call is None:
                raise ValueError((name, role, hex(pc), 'map not at direct return PC'))
            callee = call[1]
            if role == 4:
                assert callee == 'ponyint_mapped_collect'
            else:
                expected = 'Probe_ref_middle_o' if role == 3 else 'Probe_ref_phase_o'
                assert callee == expected
                pre, = (at for mark, at, base, loc in sites if mark == 1 and (base, loc) == (reg, slot))
                j = addrs.index(pre)
                assert j < i
                register = 'rbp' if reg == 6 else 'rsp'
                regslot = f'{hex(slot)}(%{register})'
                assert re.fullmatch(r'mov\s+%\w+,' + re.escape(regslot), insns[j-1][1])
                for _, text in insns[j:i-1]:
                    if text == 'vzeroupper':
                        continue
                    move = re.fullmatch(r'mov\s+%\w+,%(\w+)', text)
                    if not move or move[1] in ('rsp', 'esp', 'sp', 'spl', 'rbp', 'ebp', 'bp', 'bpl'):
                        raise ValueError(('unexpected instruction between root store and call', text))
            published.append(dict(function=name, role=role, pc=hex(pc), register=reg, slot=slot, callee=callee))
    return published
