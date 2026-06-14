#!/usr/bin/env python3
"""Canonical architecture naming for libs-cache package names.

`platform.machine()` spells the same ISA differently across operating systems:
Linux/macOS/DragonFly say `x86_64` while FreeBSD/OpenBSD/Windows say `amd64`;
Linux says `aarch64` while macOS/Windows say `arm64`. For naming a libs-cache
package these are equivalent -- the artifact is the same -- so the cache scripts
must collapse each ISA to one canonical spelling. Otherwise the same artifact
built on two operating systems (e.g. the host-side existence check on a Linux
runner vs. the FreeBSD VM that actually pushes it) would land in two different
packages, and the host could never see what the VM wrote.

Both `oci_libs_cache.py` and `branch_libs_cache.py` import this so there is a
single definition rather than two copies to keep in sync.

Stdlib only so no pip install is required on any CI runner.
"""

# The known equivalence classes. An ISA not listed here is a hard error
# (`canonical` raises) rather than a silent passthrough, so a new platform can
# never quietly create a non-canonical package name -- add it here on purpose.
ARCH_ALIASES = {
    'x86_64': 'x86_64',
    'amd64': 'x86_64',
    'aarch64': 'arm64',
    'arm64': 'arm64',
}


def canonical(machine):
    """Map a `platform.machine()` value to one canonical spelling per ISA.

    Raises ValueError on an unrecognized architecture; callers convert that to
    their own `die` so the failure carries the script's usual formatting.
    """
    arch = ARCH_ALIASES.get(machine.lower())
    if arch is None:
        raise ValueError(f"Unrecognized architecture '{machine}'. Add it to "
                         "ARCH_ALIASES in cache_arch.py.")
    return arch
