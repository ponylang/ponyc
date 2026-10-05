## Fix compilation failure when LLVM detects a CPU name invalid for the target

When running ponyc under QEMU with `-cpu host`, the emulated CPUID could map to a CPU name that only exists for a narrower target — for example, `athlon-xp` (32-bit only) on an x86-64 host — causing compilation to fail. ponyc now validates the detected CPU name against the compile target and falls back to the target's baseline when it is not recognized.
