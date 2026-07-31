---
name: upgrade-llvm
description: Load when upgrading the vendored LLVM submodule in ponyc. Covers the per-version commit strategy, submodule mechanics, patch handling, hash updates, and common API migration patterns.
disable-model-invocation: false
---

# Upgrading the vendored LLVM in ponyc

ponyc vendors LLVM as a submodule at `lib/llvm/src`, pinned by hash in
`lib/CMakeLists.txt` and patched from `lib/llvm/patches/`. Upgrading it means
moving the pin, re-checking the patches, fixing whatever LLVM removed or
changed under `src/libponyc/codegen/`, and rebuilding LLVM from source.

Run every command from the repo root.

## Gotchas that will bite you (read first)

- **The submodule is a shallow clone.** `lib/CMakeLists.txt` clones it with
  `--depth 1` and fetches no tags. A plain `git fetch origin tag llvmorg-XX.Y.Z`
  deepens it to the whole of llvm-project's history up to that tag. Pass
  `--depth 1`.
- **A patch that fails the forward check has not necessarily been upstreamed.**
  The forward check also fails when the patch is already applied to the tree,
  and when `git apply` cannot read the tree at all. Work through step 4 before
  you delete anything.
- **Don't `rm -rf build`.** That directory holds every configured build tree,
  not just LLVM's. The vendored LLVM builds in `build/build_libs` and installs
  to `build/libs`; remove only those two.
- **Stage the submodule pointer before you build.** `cmake -P
  lib/build-libs.cmake` runs `git submodule update --init`, which resets
  `lib/llvm/src` to the recorded pointer. A pointer change that exists only in
  the working tree is lost.
- **ponyc still builds when an intrinsic's signature changes.** The failure
  comes later, when ponyc compiles a Pony program, so run the tests and not
  just the build.

## Strategy

Upgrade one major version at a time, and give each version its own commit. One
PR can carry more than one version. Separate commits show which LLVM version
each source change came from, and let you bisect the branch when something
breaks. ponylang repos squash-merge, so those commits become one commit on
`main` — you can only bisect them while the branch is open.

## Steps

Steps 1, 8, and 9 run once for the PR. Steps 2 through 7 repeat for each major
version you move through.

### 1. Branch

Branch off `main`.

### 2. Update the submodule

```bash
# Discard any patches applied to the tree
git -C lib/llvm/src checkout -- .
# Fetch and check out the new tag, keeping the clone shallow
git -C lib/llvm/src fetch --depth 1 origin tag llvmorg-XX.Y.Z
git -C lib/llvm/src checkout llvmorg-XX.Y.Z
```

`git checkout -- .` throws away every modification to a tracked file in the
submodule, including a patch you rebased by hand. Save that work outside the
tree first.

### 3. Update `LLVM_DESIRED_HASH`

Get the new commit hash:

```bash
git -C lib/llvm/src rev-parse HEAD
```

Set `LLVM_DESIRED_HASH` in `lib/CMakeLists.txt` to this value.

### 4. Check the patches

The build applies every `.diff` in `lib/llvm/patches/`, so check each one.
Run the same check the build runs, with the same flags:

```bash
git -C lib/llvm/src apply --check -p 1 --ignore-whitespace --whitespace=nowarn \
  ../patches/<patch>.diff
```

If that fails, run the reverse check before concluding anything:

```bash
git -C lib/llvm/src apply --reverse --check -p 1 --ignore-whitespace \
  --whitespace=nowarn ../patches/<patch>.diff
```

- Forward check passes: the patch still applies. Keep it.
- Forward fails, reverse passes: the change is already in the tree. On a tree
  you reset in step 2, LLVM upstreamed it, so delete the patch file. On a tree
  you did not reset, an earlier build applied it — reset and check again.
- Both fail: either LLVM moved the code the patch touches, or `git apply` could
  not read the tree. In the second case `git apply` reports `fatal: not a git
  repository`, which comes from a dangling submodule gitlink — a `.git` file
  pointing at a `.git/modules/...` target that isn't there, left behind when
  the tree was copied or rsynced without the parent's `.git/modules`.
  Otherwise, rebase the patch onto the new source rather than deleting it.

### 5. Commit the submodule pointer

```bash
git add lib/llvm/src lib/CMakeLists.txt
git commit -m "Upgrade LLVM AA.B.C -> XX.Y.Z"
```

`cmake -P lib/build-libs.cmake` resets the submodule to the pointer recorded in
the index, so stage this before step 6 or the new pointer is lost. Amend this
commit as the next two steps produce changes.

### 6. Build, and fix what breaks

```bash
rm -rf build/libs build/build_libs         # Drop the old LLVM; the next command rebuilds it
cmake -DJOBS=<n> -P lib/build-libs.cmake   # Build LLVM; see BUILD.md for picking <n>
cmake --preset release                     # Configure the ponyc build
cmake --build --preset release             # Build ponyc
```

`JOBS` defaults to 4. BUILD.md says to pass your own core count and keep it
modest, because LLVM is memory-hungry. On Windows use the Windows presets
(BUILD.md).

Two kinds of failure come out of this step.

**A patch-hash mismatch**, if you added, removed, or edited a patch file in
step 4. The lib configure step prints both hashes and stops:

```
Patch hash actual '<actual>' does not match desired '<desired>'
```

Copy `<actual>` into `PATCHES_DESIRED_HASH` in `lib/CMakeLists.txt`. The hash
is a SHA256 over the seed string `needed_if_no_patches` and the SHA256 of each
patch file's text with newlines squashed to spaces, which is awkward to
reproduce in a shell — let CMake compute it. If no patches remain at all, the
value is `SHA256("needed_if_no_patches")` =
`3e16c097794cb669a8f6a0bd7600b440205ac5c29a6135750c2e83263eb16a95`.

**Compile errors**, from LLVM APIs that were removed or changed. See "Common
migration patterns" below. Fix them, amend the commit, and build again.

### 7. Run the tests

```bash
ctest --preset release -L ci-core
```

The `ci-core` label covers `libponyc.tests`, where the codegen assertions live,
along with the full-program, stdlib, and examples tests. Running only
`full-programs-release` skips the codegen gtests.

This is narrower than what the PR will run: CI runs `ci-core` in both debug and
release on Linux, macOS, and Windows, and separately builds and runs
`pony-compiler-tests`. A failure that only shows up in one config, or on one
platform, will not appear here.

To reproduce a tier-3 BSD failure, the `create-dragonfly-dev-env`,
`create-freebsd-dev-env`, and `create-openbsd-dev-env` skills stand up a local
VM matching that CI job.

### 8. Add a release note

An LLVM upgrade is user-facing. Add `.release-notes/update-llvm-to-XX.md`:

```markdown
## Update to LLVM XX.Y.Z

We've updated the LLVM version used to build Pony from AA.B.C to XX.Y.Z.
```

Do not edit `next-release.md`; CI aggregates the individual files.

### 9. Push and open the PR

Title the PR `Update to LLVM XX.Y.Z`, matching the release note. Before merging,
a Pony team member adds a `changelog - changed` label, which puts a line built
from the PR title into that CHANGELOG section. That is a separate mechanism from
the release note file, which reaches `next-release.md` on its own.

## Common migration patterns

### `LLVMConst*` to `LLVMBuild*` (constant expression removal)

LLVM removed a number of constant expression functions. The `LLVMBuild*`
equivalents constant-fold when given constant operands, so they compute the
same thing; they take a builder and a name as well:

```c
// Before:
return LLVMConstShl(l_value, r_value);
// After:
return LLVMBuildShl(c->builder, l_value, r_value, "");
```

`LLVMConstICmp`, `LLVMConstFCmp`, `LLVMConstShl`, and `LLVMConstMul` are gone
from LLVM 22.1.6's `Core.h`. `LLVMConstAdd` and `LLVMConstSub` are still there
and carry no deprecation, but ponyc calls none of the six.

### Deprecated to newer API variants

On the clang and gcc builds ponyc's C files compile with `-Werror`, so a
deprecated LLVM C function fails the build if its declaration carries
`LLVM_ATTRIBUTE_C_DEPRECATED`, which produces a compiler warning. The MSVC build
passes `/wd4996` alongside `/WX`, so the same call compiles clean on Windows.

A function marked deprecated only in a doc comment produces no warning
anywhere. The headers use two spellings for that, `@deprecated` and a plain
`Deprecated: use X instead`, so grep for both when you want to know what ponyc
calls that upstream has marked. The list below is a starting point, not a
complete one.

`LLVMGetMDKindID` carried the attribute rather than a doc comment, so it did
fail the build. That migration is done; `src/` now calls
`LLVMGetMDKindIDInContext`. `LLVMBuildNUWNeg` also carries the attribute — its
replacement is `LLVMBuildNeg` followed by `LLVMSetNUW`, which ponyc wraps as
`LLVMSetNoUnsignedWrap` in `host.cc`. `LLVMBuildNSWNeg` is not deprecated.

### A new link-time dependency

An LLVM upgrade can make ponyc need a system library it did not need before,
and the need can be Windows-only, so a local Linux build will not show it. The
LLVM 21 upgrade (`22e84c4a1`) added `ntdll.lib` to the Windows link line in
`src/libponyc/platform/vcvars.c` and to three `CMakeLists.txt` files.

### Intrinsic signature changes

LLVM can change an intrinsic's signature in a major version. These do not show
up as compile errors; ponyc builds clean. They surface as a module verification
failure when ponyc compiles a Pony program (`Incorrect number of arguments
passed to called function!`), which is why step 7 runs the tests.

- **LLVM 22**: `llvm.lifetime.start` and `llvm.lifetime.end` dropped their
  leading `i64 size` argument and now take a single pointer. ponyc emits these
  in `gencall_lifetime_start` and `gencall_lifetime_end` (`gencall.c`). Drop
  the size operand, drop the `type` parameter the size was computed from, and
  call with one argument:

```c
// Before (2 args: size, ptr):
LLVMValueRef args[2];
args[0] = LLVMConstInt(c->i64, size, false);
args[1] = ptr;
LLVMBuildCall2(c->builder, func_type, func, args, 2, "");
// After (1 arg: ptr):
LLVMBuildCall2(c->builder, func_type, func, &ptr, 1, "");
```

`CodegenTest.LifetimeIntrinsicsHaveSinglePointerArgument` in
`test/libponyc/codegen.cc` asserts the emitted argument count. When a later
LLVM changes another intrinsic's signature, assert its argument count the same
way.

### C++ API changes

ponyc also embeds LLD (`genexe.cc`) and clang's frontend (`gencshim.cc`). Those
C++ APIs carry no stability guarantee and break more often than LLVM's C API.

- **LLVM 19**: `DIBuilder::insertDeclare` returns `DbgInstPtr` (PointerUnion)
  instead of `Instruction*`. If callers don't use the return value, change the
  wrapper's return type to `void`.
- **LLVM 20**: `Intrinsic::getDeclaration` to
  `Intrinsic::getOrInsertDeclaration`.
- **LLVM 20**: Optimizer extension point callbacks
  (`registerOptimizerEarlyEPCallback`, `registerOptimizerLastEPCallback`) now
  take a `ThinOrFullLTOPhase` parameter in the lambda.
- **LLVM 21**: `Attribute::NoCapture` to
  `Attribute::getWithCaptureInfo(ctx, CaptureInfo::none())`.
- **LLVM 21**: `LintPass()` requires a `bool AbortOnError` parameter:
  `LintPass(false)`.
- **LLVM 22**: `createTargetMachine` takes a `const Triple&` instead of a
  `StringRef`; wrap with `Triple(opt->triple)`. ponyc made this change under
  LLVM 21, in `3eae0a119`, while the `StringRef` overload was still there and
  deprecated.

## APIs to check on every upgrade

APIs ponyc calls that a future LLVM may break:

| API | Location | Likely replacement |
|-----|----------|-------------------|
| `LLVMArrayType` | codegen.c, gendesc.c, genident.c | `LLVMArrayType2` (`uint64_t` count) |
| `LLVMConstArray` | gendesc.c, genident.c | `LLVMConstArray2` (`uint64_t` count) |
| `LLVMConstStringInContext` | codegen.c | `LLVMConstStringInContext2` (`size_t` length) |
| `LLVMConstNeg` | genoperator.c | `LLVMBuildNeg` |
| `LLVMConstNot` | genoperator.c | `LLVMBuildNot` |
| `LLVMConstXor` | genoperator.c | `LLVMBuildXor` |
| `LLVMConstTrunc` | genreference.c | `LLVMBuildTrunc` |

The first three carry an `@deprecated` doc comment; the other four are not
deprecated. The build fails once one of them gains the deprecation attribute or
is removed. Re-check this table against the new headers as part of the upgrade.

## Files typically modified

- `lib/CMakeLists.txt` — `LLVM_DESIRED_HASH`, `PATCHES_DESIRED_HASH`
- `lib/llvm/patches/` — patch files, added or removed
- `.release-notes/` — the release note
- `src/libponyc/codegen/genoperator.c` — constant expression replacements
- `src/libponyc/codegen/genreference.c` — constant expression replacements
- `src/libponyc/codegen/gendebug.cc`, `gendebug.h` — debug info API changes
- `src/libponyc/codegen/host.cc` — target machine, intrinsics
- `src/libponyc/codegen/genopt.cc` — optimizer pass pipeline
- `src/libponyc/codegen/gendesc.c` — array and descriptor construction
- `src/libponyc/codegen/genident.c` — array construction
- `src/libponyc/codegen/codegen.c` — core codegen types and helpers
- `src/libponyc/codegen/gencontrol.c` — metadata APIs
- `src/libponyc/codegen/gencall.c`, `gencall.h` — intrinsic emission
  (lifetime markers, memcpy and memmove)
- `src/libponyc/codegen/genexe.cc` — embedded LLD
- `src/libponyc/codegen/gencshim.cc` — embedded clang frontend
- `src/libponyc/platform/vcvars.c`, `src/ponyc/CMakeLists.txt`,
  `test/libponyc/CMakeLists.txt`, `benchmark/libponyc/CMakeLists.txt` — link
  dependencies
- `test/libponyc/codegen.cc` — codegen assertions

A bigger LLVM can also push a CI job past its disk limit. During the LLVM 22
upgrade we moved the OpenBSD tier-3 job onto a dedicated 50 GB data disk; the
script that creates it is in `.ci-scripts/bsd/`.
