---
name: create-openbsd-dev-env
description: Load when you need to reproduce or validate an OpenBSD-specific ponyc issue on a local VM, or to stand up a local OpenBSD development VM that matches ponyc tier-3 CI. Covers the cloud-init seed (cdrom), the sudo-free QEMU/KVM boot, the doas-via-cloud-init setup (no expect bootstrap), the dedicated /build data disk, the datasize limit, the clang build env, and the gotchas (openbsd-user login, disklabel build disk, raised datasize, static-PIE static linking, pkg_add naming, detached builds, per-block shells) that make hand-rolling one error-prone.
disable-model-invocation: false
---

# Create an OpenBSD dev/test VM for ponyc

Stand up a local OpenBSD VM that matches the ponyc tier-3 CI job, so you can build
and test ponyc on OpenBSD without GitHub Actions.

OpenBSD's tier-3 job exercises paths the Linux jobs never do: the static-PIE `--static`
embedded-LLD link recipe, OpenBSD's base clang/libc++, and the per-process `datasize`
limit that the LLVM build pushes against. It also asserts the `use=` build options that
can't work there are rejected. So this VM is the authoritative local environment for
**OpenBSD-platform** ponyc issues. (It is clang-based — OpenBSD's base compiler is clang,
not gcc. For **gcc-specific** issues use the DragonFly skill instead.)

Run the repo-relative commands (`rsync` of the source) from your ponyc checkout root. The
VM itself lives in a separate directory (`$VMDIR`).

**Each fenced `sh` block below is self-contained.** It re-sets `VMDIR`/`OPENBSD_VERSION`
(and, where needed, the `SSH` command) at its top, because an agent runs each block as a
fresh shell — environment variables and the working directory do NOT carry from one block
to the next. Do not "optimize" by setting them once and dropping them from later blocks:
an empty `$SSH` would make a `$SSH /bin/sh <<'EOF' ... EOF` heredoc run **on your host**
(silently) instead of in the VM — and Step 5 contains `newfs` and Step 6 edits
`/etc/login.conf`. Keep every block whole. Pick your `VMDIR`/`OPENBSD_VERSION` once and use
the same values in every block.

## Gotchas that will bite you (read first)

OpenBSD is a hybrid of the other two BSD skills: like FreeBSD it boots from a cloud-init
seed and is driven entirely over ssh (no VGA-console automation), but like DragonFly the
build lives on a dedicated data disk. These matter:

- **Per-block fresh shells.** See above: re-establish `VMDIR`/`OPENBSD_VERSION`/`SSH` in
  every block; never let `$SSH` be empty in front of a `/bin/sh` heredoc.
- **You log in as `openbsd`, not `root`.** The cloud-init seed creates the `openbsd` user
  in the `wheel` group and writes `/etc/doas.conf` (`permit nopass keepenv :wheel`). Build
  and test run **as `openbsd`**; the few root operations (disk setup, package install,
  editing `login.conf`) go through `doas`, which is configured before first login and
  persists on disk. There is **no** `expect`/`su` bootstrap — OpenBSD's cloud-init runs
  `write_files`, so `doas` is in place from the first boot (simpler than FreeBSD).
- **`doas` is in the base system.** Nothing to install; cloud-init just drops
  `/etc/doas.conf`. Keep its `0600 root:wheel` permissions exactly — `doas` refuses to run
  with a group- or world-writable config.
- **Cloud-init means no console driving.** Unlike the DragonFly skill there is no QEMU
  `sendkey`/screendump/monitor dance — just boot and wait for ssh. But the **first** boot
  runs cloud-init (user + key + `doas.conf`), so ssh is not instant; poll for it (Step 4).
- **Build on the `/build` data disk, not `/home`.** OpenBSD's default disklabel confines
  writable space to `/home` (~10.8G), which the LLVM 22 build overflows. A second qcow2 is
  partitioned, `newfs`'d, and mounted at `/build`, and the source plus the build live
  there — mirroring the DragonFly job. The disklabel setup is **one-time and not
  idempotent** (`newfs` reformats), so run Step 5 exactly once.
- **The `/build` mount is not persisted across reboot.** `disklabel`/`newfs`/`mount` are
  manual one-time steps with no `/etc/fstab` entry; after a guest reboot, remount
  `/dev/sd1a` at `/build`. `/etc/login.conf` and `/etc/doas.conf` are on the root disk and
  **do** persist.
- **Raise the `datasize` limit.** OpenBSD caps a process's data segment in
  `/etc/login.conf` (1536M on the stock 7.9 image); the LLVM build needs more, so Step 6
  raises `datasize-max`/`-cur` to at least 4096M. Without it the libs build dies with
  allocation failures partway through. The edit is on the root disk and persists.
- **OpenBSD's base system has no `bash`.** `/bin/sh` is a POSIX `ksh`; in-VM scripts must
  be POSIX `sh`. Don't reach for bashisms in a `$SSH /bin/sh` heredoc.
- **Two `pkg_add` names look like typos but aren't:** `python%3` (the `%3` selects the 3.x
  branch) and `rsync--` (the trailing `--` picks the no-flavor build). Copy them verbatim.
- **Detach long in-VM builds.** The libs build (LLVM) takes hours; run it
  `nohup … > /build/x.log 2>&1 &` and poll the log, so an ssh drop doesn't kill it.

## Step 0 — verify prerequisites (do NOT assume they're installed)

This skill needs, on the host:

- an existing ponyc checkout (you run the `rsync` from it)
- hardware-accelerated virtualization for QEMU (KVM on Linux, HVF on macOS)
- enough free disk for the VM's two sparse qcow2 disks (the shipped root image plus a 50 GB
  nominal data disk; they grow only as used — the libs build plus a debug build take the data
  disk to several GB), network access to github.com (the image is a GitHub release asset)
- `qemu-system-x86_64` and `qemu-img`
- `genisoimage` (builds the cidata seed CD-ROM; `cloud-localds` can also produce an
  equivalent cidata image)
- `rsync`, `git`, `curl`
- an OpenSSH client (`ssh`, `scp`, `ssh-keygen`)

Note this is the shortest prerequisite list of the three BSD skills: OpenBSD needs no
`expect` (cloud-init configures `doas` directly), no `xz` (the image ships as a plain
qcow2 — no decompress, no root-fs resize), and no `bsdtar`/PPM-converter (it is headless
with no ISO header mining and no console screendumps).

Check that every required tool is present. Do not assume the environment has them, and do
not install anything yourself — installing them is the user's call (it needs privilege, and
the package names vary by OS). If anything is missing, **stop, tell the user which
tools/capabilities are missing, and ask them to install them with their platform's package
manager**, then re-run the check before proceeding.

```sh
missing=""
for t in qemu-system-x86_64 qemu-img rsync git ssh scp ssh-keygen curl; do
  command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
done
command -v genisoimage >/dev/null 2>&1 || command -v cloud-localds >/dev/null 2>&1 \
  || missing="$missing seed-builder(genisoimage|cloud-localds)"
# Hardware acceleration is host-specific — check the right accelerator for this OS.
case "$(uname -s)" in
  Linux)  [ -w /dev/kvm ] || missing="$missing kvm(/dev/kvm-not-writable)" ;;
  Darwin) qemu-system-x86_64 -accel help 2>&1 | grep -q hvf || missing="$missing hvf-accelerator" ;;
  *)      echo "NOTE: confirm this host has a QEMU hardware accelerator and adjust the boot accel= below" ;;
esac
[ -n "$missing" ] && echo "MISSING:$missing" || echo "all prerequisites present"
```

If the output is anything but `all prerequisites present`, name the missing
tools/capabilities to the user and ask them to install them however their OS does — do NOT
guess a distro package name. (`genisoimage` ships in `genisoimage` or `cdrkit` on
Debian/Ubuntu; CI uses `genisoimage`. If only `cloud-localds` is available it can build the
same cidata image — attach it as the cdrom in Step 3.) A missing hardware accelerator — no
writable `/dev/kvm` on Linux, no HVF on macOS — is a host/BIOS/virtualization fix only the
user can make; the VM is unusably slow without it, so don't fall back to TCG emulation.

This flow is verified on Linux+KVM through VM bring-up, the `/build` disk plus
`doas`/`datasize` setup, dependency install, the source rsync, and the unsupported-build
rejection smoke (`.ci-scripts/openbsd-reject-unsupported-builds.sh`, which runs `cmake …
-DPONY_USES=…` and so needs no LLVM). The LLVM build (`cmake -P lib/build-libs.cmake`), the subsequent
`cmake --preset debug`/build — which require the built LLVM (CMake's `find_package(LLVM)`
fails without it) — and the test suites were **not** run locally; those steps mirror
`.ci-scripts/bsd/openbsd-provision.bash` and the tier-3 `openbsd` job, which CI exercises
end-to-end and keeps green. The guest side is OS-agnostic, so macOS goes through the same
steps with the HVF accelerator (`accel=kvm:hvf` in Step 3 selects it).

## Setup

Pick a persistent VM directory and an OpenBSD version that **matches ponyc CI** (the tier-3
`openbsd` job in `.github/workflows/ponyc-tier3.yml` runs OpenBSD 7.9; the exact image URL,
including the dated build tag, is in `.ci-scripts/bsd/openbsd-provision.bash`). Use the same
two values in every block below.

### 1. Download the image + create the build disk (one time)

OpenBSD ships no official cloud image, so CI uses the community
`hcartiaux/openbsd-cloud-image` GitHub release — a plain qcow2 that boots straight to a
cloud-init-configured system. Pin the **full dated release tag** that
`openbsd-provision.bash` references (a bare `v7.9` is not a valid asset path). The root
image is left at its shipped size; the build does not live on it.

```sh
VMDIR=~/vms/openbsd-7.9; OPENBSD_VERSION=7.9
mkdir -p "$VMDIR" && cd "$VMDIR"
curl -fL --retry 3 -o openbsd.qcow2 \
  "https://github.com/hcartiaux/openbsd-cloud-image/releases/download/v7.9_2026-06-03-20-46/openbsd-generic.qcow2"
# Build-workspace disk. OpenBSD's default disklabel confines builds to /home
# (~10.8G), which the LLVM 22 build overflows, so build on a dedicated disk.
qemu-img create -f qcow2 openbsd-data.qcow2 50G
```

### 2. Key + cloud-init seed

OpenBSD's cloud-init reads a cidata seed at boot: it creates the `openbsd` user (in
`wheel`) with your ssh public key, and writes `/etc/doas.conf` so the `openbsd` user gets
passwordless `doas`. Build the seed as an ISO and attach it as a CD-ROM (Step 3).

```sh
VMDIR=~/vms/openbsd-7.9; cd "$VMDIR"
test -f vm_key || ssh-keygen -t ed25519 -f vm_key -N "" -q
cat > user-data <<USERDATA
#cloud-config
users:
- name: openbsd
  groups: wheel
  ssh_authorized_keys:
  - $(cat vm_key.pub)
write_files:
- path: /etc/doas.conf
  content: |
    permit nopass keepenv :wheel
  owner: root:wheel
  permissions: '0600'
USERDATA
cat > meta-data <<'METADATA'
instance-id: openbsd-dev
local-hostname: openbsd-dev
METADATA
genisoimage -output seed.iso -volid cidata -joliet -rock user-data meta-data
```

### 3. Boot (daemonized, persistent)

```sh
VMDIR=~/vms/openbsd-7.9; OPENBSD_VERSION=7.9; cd "$VMDIR"
qemu-system-x86_64 \
  -name openbsd-${OPENBSD_VERSION} \
  -machine pc,accel=kvm:hvf -cpu host -smp 8 -m 8G \
  -drive file=openbsd.qcow2,format=qcow2,if=virtio \
  -drive file=openbsd-data.qcow2,format=qcow2,if=virtio \
  -drive file=seed.iso,media=cdrom \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none -pidfile openbsd.pid -daemonize
```

(`accel=kvm:hvf` picks KVM on Linux or HVF on macOS and errors if neither is available — it
never silently falls back to slow TCG; CI uses bare `accel=kvm` because it always runs on
Linux. `-smp`/`-m` are speed knobs; CI uses 4 CPUs / 6G. The build is capped by the 4096M
`datasize` limit per process, not by total RAM. `hostfwd 2222->22` is the ssh port — to run
two OpenBSD VMs at once, give each its own `$VMDIR` and a distinct hostfwd port, and pass
that port to every ssh/scp/rsync `-p`. The seed is attached as a **cdrom** — that is how
OpenBSD's cloud-init finds the cidata volume; do not copy FreeBSD's raw-virtio-disk seed
form here. This boot command is adapted from `.ci-scripts/bsd/openbsd-provision.bash`: it
adds `-pidfile`/`-smp 8`, drops the CI host-bootstrap, and like CI uses no rng or monitor
device.)

### 4. Wait for ssh (first boot runs cloud-init)

```sh
VMDIR=~/vms/openbsd-7.9; OPENBSD_VERSION=7.9; cd "$VMDIR"
up=""
for i in $(seq 1 150); do   # ~5 min, matching CI's timeout
  if ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=2 -i vm_key -p 2222 \
       openbsd@localhost true 2>/dev/null; then up=1; echo "SSH up"; break; fi
  sleep 2
done
[ -n "$up" ] || echo "SSH NEVER CAME UP — see recovery below."
```

If ssh never comes up, this VM has no VGA console to look at (it booted headless), so
diagnose in this order:

- **Is the process alive?** `pgrep -af "qemu-system-x86_64 -name openbsd-${OPENBSD_VERSION}"`.
  If it's gone, the boot died early (usually a bad/incomplete image from Step 1) — re-check
  the Step 1 download and re-boot.
- **Watch the boot.** The image logs to the serial console, so re-boot with a serial log
  added — `kill "$(cat openbsd.pid)"`, then re-run the Step 3 boot command with
  `-serial file:$VMDIR/console.log` appended — and read `console.log` to see how far it got.
- **ssh actively *refuses* the key (vs. just timing out)?** That means cloud-init never
  applied the seed. Confirm `seed.iso` is attached as a **cdrom** drive in the boot command
  (Step 3) and was built in Step 2, then re-boot.

### 5. One-time build-disk setup (disklabel, newfs, mount)

OpenBSD sees the second virtio disk as `sd1`. Give it a single 4.2BSD partition spanning
the disk, `newfs` it, mount it at `/build`, and `chown` it to `openbsd` so the later rsync
and build run without root. The `printf` drives the interactive `disklabel -E` editor (`a a`
add partition, then default offset/size/type, `w` write, `q` quit) — it is fiddly and copied
verbatim from CI. **Run this once:** `newfs` reformats, so re-running destroys whatever is
on `/build`.

```sh
VMDIR=~/vms/openbsd-7.9
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 openbsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
printf 'a a\n\n\n\nw\nq\n' | doas disklabel -E sd1
doas newfs /dev/rsd1a
doas mkdir -p /build
doas mount /dev/sd1a /build
doas chown openbsd:wheel /build
df -h /build
EOF
```

If the `doas` commands here fail (permission denied, or `doas` prompts for a password),
cloud-init's `write_files` never created `/etc/doas.conf` — the seed didn't apply. Don't
retype the disk commands; nothing here works until `doas` does, so work through the Step 4
recovery (is the VM up? is `seed.iso` attached as a **cdrom**?) first.

### 6. Install deps + raise the datasize limit

`doas pkg_add` installs the build tools (the `python%3`/`rsync--` spellings are correct —
see Gotchas). Then raise the per-process `datasize` ceiling for the `openbsd` user — and
treat this as intent, not a fixed substitution. OpenBSD's `/etc/login.conf` caps each
process's data segment in the `default` login class (the class the `openbsd` user falls
under), and the LLVM build needs roughly 4 GB, so both `datasize-cur` and `datasize-max`
must end up at least `4096M` (what CI sets). On the stock OpenBSD 7.9 image both default to
`1536M`, so the two `sed`s below do it. But the goal is the `4096M` ceiling, not the literal
`1536M`: if a future image ships a different default the `sed`s match nothing, so read the
current values (the trailing `grep` prints them) and, if either is below `4096M`, raise it
to `4096M` directly — adjust the `sed` pattern or edit `/etc/login.conf`. Confirm both read
`4096M` before continuing; too low and the libs build dies with allocation failures partway
through, far from this step. The edit persists on the root disk and re-running is a no-op.

```sh
VMDIR=~/vms/openbsd-7.9
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 openbsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
doas pkg_add -u && doas pkg_add cmake gmake git python%3 rsync--
# Raise the openbsd user's datasize cap to 4096M. The stock 7.9 image
# defaults both to 1536M; if a future image differs, the sed matches
# nothing — read the grep below and set both to 4096M however fits.
doas sed -i 's/datasize-max=1536M/datasize-max=4096M/' /etc/login.conf
doas sed -i 's/datasize-cur=1536M/datasize-cur=4096M/' /etc/login.conf
grep -E 'datasize-(cur|max)=' /etc/login.conf   # confirm both now read 4096M
EOF
```

### 7. Rsync the ponyc checkout in (exclude the host `build/`)

Run from your ponyc checkout. Exclude the top-level `build/` (host artifacts; the VM builds
its own — this `--exclude` is a deliberate divergence from CI, whose fresh checkout has none
to skip). Keep `.git` (CMake runs `git rev-parse`). The vendored LLVM submodule under
`lib/llvm/src` IS transferred — the VM needs it for the libs build.

```sh
VMDIR=~/vms/openbsd-7.9
rsync -az --exclude='/build' \
  -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222" \
  "$(git rev-parse --show-toplevel)/" openbsd@localhost:/build/ponyc/
```

## Using the VM

The build uses OpenBSD's base clang — no `CC`/`CXX`/`LD_LIBRARY_PATH` exports (that is the
DragonFly-only gcc13 dance). `cmake -P lib/build-libs.cmake` builds the vendored LLVM and is the multi-hour long
pole — run it detached, then poll the log until it ends with `libs DONE rc=0` (a nonzero rc
means it failed — read the log above the marker). Keep `-DJOBS=4` (what CI uses):
each parallel compile is bounded by the 4096M `datasize` cap, so higher parallelism is the
first thing to suspect on a libs out-of-memory.

```sh
VMDIR=~/vms/openbsd-7.9
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 openbsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
cat > /build/run-libs.sh <<'S'
#!/bin/sh
cd /build/ponyc && cmake -DJOBS=4 -P lib/build-libs.cmake
echo "libs DONE rc=$?"
S
chmod +x /build/run-libs.sh
nohup /build/run-libs.sh > /build/libs-build.log 2>&1 &
echo "libs building pid $!"
EOF
# poll until done: $SSH /bin/sh -c 'tail -3 /build/libs-build.log'  -> wait for "libs DONE rc=0"
```

Once `libs` is built (it persists in the VM), build and test with the same shell. Do not run
this block until the poll shows `libs DONE rc=0` — even `cmake --preset debug` needs
the built LLVM (CMake's `find_package(LLVM)` fails without it), so it cannot run before
the libs build:

```sh
VMDIR=~/vms/openbsd-7.9
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 openbsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
cd /build/ponyc
cmake --preset debug
cmake --build --preset debug
ctest --preset debug -L ci-core
EOF
```

To iterate on a fix: edit on the host, re-run Step 7's `rsync` (it's incremental — only
changed files are sent), then re-run the build block above (long rebuilds should also be
detached + polled). For exact CI parity, the tier-3 `openbsd` job
(`.github/workflows/ponyc-tier3.yml`) runs more than this. In order: an
unsupported-build-rejection smoke that runs **before** the libs build (it needs no LLVM —
`.ci-scripts/openbsd-reject-unsupported-builds.sh` asserts `use=address_sanitizer`,
`thread_sanitizer`, `undefined_behavior_sanitizer`, `coverage`, `valgrind`, and `dtrace` are
each rejected with "not supported on OpenBSD"); then debug configure/build; a `--static`
embedded-LLD link smoke (OpenBSD static is static-PIE, so the binary's `file` output reads
"shared object" — the smoke instead asserts `readelf -d` shows **no** `(NEEDED)` entries);
the debug `ci-core` suite; the self-hosted tool tests (`pony-doc-tests`/`pony-lint-tests`/
`pony-lsp-tests`); and a release build + the `ci-core` suite. Consult that job when validating a
CI-matching issue.

## Lifecycle

```sh
VMDIR=~/vms/openbsd-7.9; OPENBSD_VERSION=7.9; cd "$VMDIR"
pgrep -af "qemu-system-x86_64 -name openbsd-${OPENBSD_VERSION}"        # status
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i vm_key -p 2222 openbsd@localhost   # interactive shell
# stop: kill the process. Disks persist for next time. The pgrep/pkill are scoped to this
# version so a second OpenBSD VM isn't matched; the pidfile path is per-$VMDIR anyway.
kill "$(cat openbsd.pid)" 2>/dev/null || pkill -f "qemu-system-x86_64 -name openbsd-${OPENBSD_VERSION}"
```

To restart later, re-run the step-3 boot command (the disks and installed deps persist; you
do not redo setup). On a reboot, ssh-as-`openbsd` works immediately and `doas` still works
(`/etc/doas.conf` persisted), and the raised `datasize` limit is still in `/etc/login.conf`
— but the `/build` data disk is **not** auto-mounted (there is no `/etc/fstab` entry), so
remount it before building: `doas mount /dev/sd1a /build`. The source under `/build/ponyc`
is preserved on the data disk across the remount. For a long-lived VM, add the mount to
`/etc/fstab` so it survives reboots.

## How this relates to CI (don't re-add the CI-only steps)

This mirrors `.ci-scripts/bsd/openbsd-provision.bash` (the source of truth — keep this skill
in sync if it changes). The local setup deliberately differs from CI, and each difference is
safe:

- Skips CI host-bootstrap (free disk space, `apt-get` qemu, make `/dev/kvm` writable) — your
  host already has qemu and a usable hardware accelerator (Step 0 verified it).
- Persistent VM instead of built-fresh-and-killed per run.
- ssh adds `-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR` to quiet the host-key
  warning: a persistent VM reused on `localhost:2222` collides with the stale `known_hosts`
  entry from a prior VM. CI's ephemeral runners never reuse the port, so
  `openbsd-provision.bash` doesn't bother.
- `-smp 8` / `-m 8G` for build speed (CI uses 4 / 6G), and a `-pidfile` for the lifecycle
  commands.
- `rsync --exclude='/build'` to skip the host's build artifacts (CI's fresh checkout has
  none).
- No GHCR libs cache (that's token-gated CI plumbing) — you just run `cmake -P lib/build-libs.cmake` once.

The FreeBSD and DragonFly CI VMs follow the same shape
(`.ci-scripts/bsd/{freebsd,dragonfly}-provision.bash`) and each has its own skill:
`create-freebsd-dev-env` (also cloud-init, but base libc++/dtrace, an `expect`/`su` root
bootstrap, and the source under `/home/freebsd` rather than a `/build` disk) and
`create-dragonfly-dev-env` (gcc-only, and it drives a VGA console). This skill is
OpenBSD-only; load a sibling if you need one of those.
