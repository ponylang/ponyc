---
name: create-freebsd-dev-env
description: Load when you need to reproduce or validate a FreeBSD-specific ponyc issue on a local VM, or to stand up a local FreeBSD development VM that matches ponyc tier-3 CI. Covers the cloud-init seed, the sudo-free QEMU/KVM boot, the one-time expect/su root bootstrap, the clang build env, and the gotchas (freebsd-user login, no bash in base, growfs auto-resize, detached builds, per-block shells) that make hand-rolling one error-prone.
disable-model-invocation: false
---

# Create a FreeBSD dev/test VM for ponyc

Stand up a local FreeBSD VM that matches the ponyc tier-3 CI job, so you can build
and test ponyc on FreeBSD without GitHub Actions.

FreeBSD's tier-3 job exercises paths the clang-on-Linux jobs never do: the embedded-LLD
`--static` link recipe, the native sanitizer link path, the dtrace/illumos provider
path, and FreeBSD's base libc++/libunwind. So this VM is the authoritative local
environment for **FreeBSD-platform** ponyc issues. (It is clang-based — FreeBSD's base
compiler is clang, not gcc. For **gcc-specific** issues use the DragonFly skill instead.)

Run the repo-relative commands (`rsync` of the source) from your ponyc checkout root.
The VM itself lives in a separate directory (`$VMDIR`).

**Each fenced `sh` block below is self-contained.** It re-sets `VMDIR`/`FREEBSD_VERSION`
(and, where needed, the `SSH` command) at its top, because an agent runs each block as a
fresh shell — environment variables and the working directory do NOT carry from one
block to the next. Do not "optimize" by setting them once and dropping them from later
blocks: an empty `$SSH` would make a `$SSH /bin/sh <<'EOF' ... EOF` heredoc run **on your
host** (silently) instead of in the VM. Keep every block whole. Pick your `VMDIR`/
`FREEBSD_VERSION` once and use the same values in every block.

## Gotchas that will bite you (read first)

FreeBSD is much simpler to bring up than DragonFly (cloud-init injects the ssh key, so
there is no VGA-console automation), but these still matter:

- **Per-block fresh shells.** See above: re-establish `VMDIR`/`FREEBSD_VERSION`/`SSH` in
  every block; never let `$SSH` be empty in front of a `/bin/sh` heredoc.
- **You log in as `freebsd`, not `root`.** The cloud-init seed installs your key for the
  `freebsd` user and sets root's password to `ciroot`. Build and test run **as
  `freebsd`** (no root needed). Root is only needed twice during setup — the one-time
  `expect`/`su` bootstrap below installs the packages and configures passwordless `doas`,
  after which the rare root operation (e.g. loading the dtrace kernel module) goes through
  `doas`.
- **Cloud-init means no console driving.** Unlike the DragonFly skill there is no QEMU
  `sendkey`/screendump/monitor dance — just boot and wait for ssh. But the **first** boot
  runs `nuageinit` (key install) and `growfs` (root-fs auto-resize), so ssh is not
  instant; poll for it (Step 4).
- **The root fs auto-grows; there is no growfs command to run.** `qemu-img resize ... 60G`
  enlarges the disk and the image's first boot grows the root fs to fill it. You do not
  run `growfs` yourself — the wait-for-ssh loop is what accounts for it.
- **FreeBSD's base system has no `bash`.** In-VM scripts must be POSIX `sh` (the two CI
  smoke scripts already are). Don't reach for bashisms in a `$SSH /bin/sh` heredoc.
- **Detach long in-VM builds.** `make libs` (LLVM) takes hours; run it
  `nohup … > /…/x.log 2>&1 &` and poll the log, so an ssh drop doesn't kill it.
- **The one-time root bootstrap needs `expect`.** FreeBSD's `nuageinit` supports only
  `ssh_authorized_keys` + `chpasswd` (not cloud-init's `write_files`/`runcmd`), so the
  package install and `doas.conf` setup are driven through `su -m root` with `expect`,
  exactly as CI does it. After that, `doas` is on disk and persists across reboots.

## Step 0 — verify prerequisites (do NOT assume they're installed)

This skill needs, on the host:

- an existing ponyc checkout (you run the `rsync` from it)
- hardware-accelerated virtualization for QEMU (KVM on Linux, HVF on macOS)
- enough free disk for the VM's sparse qcow2 disk (provisioned at 60 GB nominal; it grows
  only as used — `make libs` plus a debug build take the qcow2 to ~12 GB, and a release
  build on top adds more), ~1 GB for the compressed image, and network access to
  download.freebsd.org
- `qemu-system-x86_64` and `qemu-img`
- `cloud-localds` (from cloud-image-utils) or `genisoimage` to build the cloud-init
  seed image
- `expect` (drives the one-time `su` root bootstrap)
- `xz`, `rsync`, `git`, `curl`, `python3`
- an OpenSSH client (`ssh`, `scp`, `ssh-keygen`)

Note this list is shorter than the DragonFly skill's: FreeBSD needs no `bsdtar`/`bunzip2`
(no ISO header mining) and no PPM-to-PNG converter (no console screendumps).

Check that every required tool is present. Do not assume the environment has them, and do
not install anything yourself — installing them is the user's call (it needs privilege,
and the package names vary by OS). If anything is missing, **stop, tell the user which
tools/capabilities are missing, and ask them to install them with their platform's
package manager**, then re-run the check before proceeding.

```sh
missing=""
for t in qemu-system-x86_64 qemu-img expect xz rsync git ssh scp ssh-keygen curl python3; do
  command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
done
command -v cloud-localds >/dev/null 2>&1 || command -v genisoimage >/dev/null 2>&1 \
  || missing="$missing seed-builder(cloud-localds|genisoimage)"
# Hardware acceleration is host-specific — check the right accelerator for this OS.
case "$(uname -s)" in
  Linux)  [ -w /dev/kvm ] || missing="$missing kvm(/dev/kvm-not-writable)" ;;
  Darwin) qemu-system-x86_64 -accel help 2>&1 | grep -q hvf || missing="$missing hvf-accelerator" ;;
  *)      echo "NOTE: confirm this host has a QEMU hardware accelerator and adjust the boot accel= below" ;;
esac
[ -n "$missing" ] && echo "MISSING:$missing" || echo "all prerequisites present"
```

If the output is anything but `all prerequisites present`, name the missing
tools/capabilities to the user and ask them to install them however their OS does — do
NOT guess a distro package name. (`cloud-localds` ships in `cloud-image-utils` on
Debian/Ubuntu; if it is unavailable, the cidata seed can also be built with
`genisoimage -output seed.img -volid cidata -joliet -rock user-data meta-data`, which is
what the OpenBSD CI path uses.) A missing hardware accelerator — no writable `/dev/kvm`
on Linux, no HVF on macOS — is a host/BIOS/virtualization fix only the user can make; the
VM is unusably slow without it, so don't fall back to TCG emulation.

This flow is verified on Linux+KVM through `make libs` and a debug build. The guest side
is OS-agnostic, so macOS goes through the same steps with the HVF accelerator
(`accel=kvm:hvf` in Step 3 selects it).

## Setup

Pick a persistent VM directory and a FreeBSD version that **matches ponyc CI** (the tier-3
`freebsd` job in `.github/workflows/ponyc-tier3.yml` runs a matrix — currently 14.3 and
15.1; the image URL is in `.ci-scripts/bsd/freebsd-provision.bash`). Use the same two
values in every block below.

### 1. Download the image (one time; keep the `.xz` to avoid re-downloading)

The BASIC-CLOUDINIT image boots straight to a cloud-init-configured system — no installer,
and it ships the full base system including `/usr/include`.

```sh
VMDIR=~/vms/freebsd-14.3; FREEBSD_VERSION=14.3
mkdir -p "$VMDIR" && cd "$VMDIR"
base="https://download.freebsd.org/releases/VM-IMAGES/${FREEBSD_VERSION}-RELEASE/amd64/Latest"
curl -fL --retry 3 -o freebsd.qcow2.xz \
  "$base/FreeBSD-${FREEBSD_VERSION}-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz"
xz -dk freebsd.qcow2.xz                 # -k keeps the .xz original
qemu-img resize freebsd.qcow2 60G       # first boot grows the root fs to fill this
```

### 2. Key + cloud-init seed

`nuageinit` reads a cidata seed image at boot: it installs your ssh public key for the
`freebsd` user and sets root's password (so the Step 5 `su` bootstrap can reach root).

```sh
VMDIR=~/vms/freebsd-14.3; cd "$VMDIR"
test -f vm_key || ssh-keygen -t ed25519 -f vm_key -N "" -q
cat > user-data <<USERDATA
#cloud-config
ssh_authorized_keys:
  - $(cat vm_key.pub)
chpasswd:
  expire: false
  list:
    - root:ciroot
USERDATA
cat > meta-data <<'METADATA'
instance-id: freebsd-dev
local-hostname: freebsd-dev
METADATA
cloud-localds seed.img user-data meta-data
```

### 3. Boot (daemonized, persistent)

```sh
VMDIR=~/vms/freebsd-14.3; FREEBSD_VERSION=14.3; cd "$VMDIR"
qemu-system-x86_64 \
  -name freebsd-${FREEBSD_VERSION} \
  -machine pc,accel=kvm:hvf -cpu host -smp 8 -m 12G \
  -drive file=freebsd.qcow2,format=qcow2,if=virtio \
  -drive file=seed.img,format=raw,if=virtio \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none -pidfile freebsd.pid -daemonize
```

(`accel=kvm:hvf` picks KVM on Linux or HVF on macOS and errors if neither is available —
it never silently falls back to slow TCG. `-smp`/`-m` are speed knobs; CI uses 4 CPUs /
12G. `hostfwd 2222->22` is the ssh port — to run two FreeBSD VMs at once (e.g. both CI
matrix versions), give each its own `$VMDIR` and a distinct hostfwd port, and pass that
port to every ssh/scp/rsync `-p`. The seed drive stays attached — `nuageinit` skips it on
later boots once the instance-id is seen. This boot command is adapted from
`.ci-scripts/bsd/freebsd-provision.bash`: it adds `-pidfile`/`-smp 8` and drops the CI
host-bootstrap; CI has no rng or monitor device for FreeBSD and neither does this.)

### 4. Wait for ssh (first boot runs nuageinit + growfs)

```sh
VMDIR=~/vms/freebsd-14.3; FREEBSD_VERSION=14.3; cd "$VMDIR"
up=""
for i in $(seq 1 150); do   # ~5 min, matching CI's timeout
  if ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=2 -i vm_key -p 2222 \
       freebsd@localhost true 2>/dev/null; then up=1; echo "SSH up"; break; fi
  sleep 2
done
[ -n "$up" ] || echo "SSH NEVER CAME UP — see recovery below."
```

If ssh never comes up, this VM has no VGA console to look at (it booted headless), so
diagnose in this order:

- **Is the process alive?** `pgrep -af "qemu-system-x86_64 -name freebsd-${FREEBSD_VERSION}"`.
  If it's gone, the boot died early (usually a bad/incomplete image from Step 1) — re-check
  the Step 1 download/resize and re-boot.
- **Watch the boot.** The image logs to the serial console, so re-boot with a serial log
  added — `kill "$(cat freebsd.pid)"`, then re-run the Step 3 boot command with
  `-serial file:$VMDIR/console.log` appended — and read `console.log` to see how far it got.
- **ssh actively *refuses* the key (vs. just timing out)?** That means `nuageinit` never
  installed it — the seed didn't apply. Confirm `seed.img` is attached as a drive in the
  boot command (Step 3) and was built in Step 2, then re-boot.

### 5. One-time root bootstrap (install deps + configure doas) via expect

FreeBSD's `nuageinit` can't run `write_files`/`runcmd`, so — exactly as CI does — drive
`su -m root` with `expect` to install the build dependencies and grant the `freebsd` user
passwordless `doas`. This runs once; `doas.conf` persists on disk for the VM's life. (The
expect block is adapted from the one embedded in `freebsd-provision.bash`.)

```sh
VMDIR=~/vms/freebsd-14.3; cd "$VMDIR"
expect <<'EXPECT'
set timeout 600
spawn ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i vm_key -p 2222 -t freebsd@localhost su -m root
expect {
  "Password:" { send "ciroot\r" }
  timeout { puts "Timeout waiting for password prompt"; exit 1 }
}
expect {
  "su: Sorry" { puts "su failed - wrong password or nuageinit didn't set it"; exit 1 }
  "#" {}
  timeout { puts "Timeout waiting for root shell"; exit 1 }
}
send "pkg install -y cmake gmake libunwind git python3 rsync doas\r"
expect {
  "#" {}
  timeout { puts "Timeout during pkg install"; exit 1 }
}
send "echo 'permit nopass freebsd' > /usr/local/etc/doas.conf\r"
expect {
  "#" {}
  timeout { puts "Timeout writing doas.conf"; exit 1 }
}
send "exit\r"
expect eof
EXPECT
```

If `expect` prints one of its `Timeout …` messages, it's safe to re-run this block — the
`pkg install -y` and the clobbering `>` write to `doas.conf` are both idempotent. The one
exception is `su failed`: that means the seed never set root's password, so root was never
reachable. Re-running won't fix it — that's a boot/seed problem, so work through the Step 4
recovery (is the VM up? did `seed.img` apply?) first.

### 6. Rsync the ponyc checkout in (exclude the host `build/`)

Run from your ponyc checkout. Exclude the top-level `build/` (host artifacts; the VM
builds its own). Keep `.git` (CMake runs `git rev-parse`). The vendored LLVM submodule
under `lib/llvm/src` IS transferred — the VM needs it for `make libs`.

```sh
VMDIR=~/vms/freebsd-14.3
rsync -az --exclude='/build' \
  -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222" \
  "$(git rev-parse --show-toplevel)/" freebsd@localhost:/home/freebsd/ponyc/
```

## Using the VM

The build uses FreeBSD's default clang — no `CC`/`CXX`/`LD_LIBRARY_PATH` exports (that is
the DragonFly-only gcc13 dance). `make libs` builds the vendored LLVM and is the multi-hour
long pole — run it detached, then poll the log until it ends with `libs DONE rc=0` (a
nonzero rc means it failed — read the log above the marker):

```sh
VMDIR=~/vms/freebsd-14.3
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 freebsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
cat > /home/freebsd/run-libs.sh <<'S'
#!/bin/sh
cd /home/freebsd/ponyc && gmake libs llvm_tools=false build_flags=-j8
echo "libs DONE rc=$?"
S
chmod +x /home/freebsd/run-libs.sh
nohup /home/freebsd/run-libs.sh > /home/freebsd/libs-build.log 2>&1 &
echo "libs building pid $!"
EOF
# poll until done: $SSH /bin/sh -c 'tail -3 /home/freebsd/libs-build.log'  -> wait for "libs DONE rc=0"
```

Once `libs` is built (it persists in the VM), build and test with the same shell. Do not
run this block until the poll shows `libs DONE rc=0` — `gmake build` against a half-built
`build/libs` fails:

```sh
VMDIR=~/vms/freebsd-14.3
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 freebsd@localhost"
$SSH /bin/sh <<'EOF'
set -e
cd /home/freebsd/ponyc
gmake configure config=debug
gmake build config=debug
gmake test-ci-core config=debug
EOF
```

To iterate on a fix: edit on the host, re-run Step 6's `rsync` (it's incremental — only
changed files are sent), then re-run the build block above (long rebuilds should also be
detached + polled). For exact CI parity, the tier-3 `freebsd` job
(`.github/workflows/ponyc-tier3.yml`) runs more than this — a `--static` embedded-LLD link
smoke, the self-hosted tool tests (`test-pony-doc`/`test-pony-lint`/`test-pony-lsp`), a
release build + `test-ci-core`, and two extra smokes:
`.ci-scripts/freebsd-sanitizer-smoke.sh` and `.ci-scripts/freebsd-dtrace-smoke.sh` (the
dtrace one needs the `doas` configured in Step 5). Consult that job when validating a
CI-matching issue.

## Lifecycle

```sh
VMDIR=~/vms/freebsd-14.3; FREEBSD_VERSION=14.3; cd "$VMDIR"
pgrep -af "qemu-system-x86_64 -name freebsd-${FREEBSD_VERSION}"        # status
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i vm_key -p 2222 freebsd@localhost   # interactive shell
# stop: CI stops this VM by killing the process; do the same. Disks persist for next time.
# The pgrep/pkill are scoped to this version so a second FreeBSD VM (e.g. the
# other CI matrix version) isn't matched; the pidfile path is per-$VMDIR anyway.
kill "$(cat freebsd.pid)" 2>/dev/null || pkill -f "qemu-system-x86_64 -name freebsd-${FREEBSD_VERSION}"
```

To restart later, re-run the step-3 boot command (the disk and installed deps persist; you
do not redo setup). On a reboot, ssh-as-`freebsd` works immediately (the key was written
to disk on first boot) and `doas` still works (`doas.conf` persisted) — there is no
re-bootstrap, unlike the DragonFly VM.

## How this relates to CI (don't re-add the CI-only steps)

This mirrors `.ci-scripts/bsd/freebsd-provision.bash` (the source of truth — keep this
skill in sync if it changes). The local setup deliberately differs from CI, and each
difference is safe:

- Skips CI host-bootstrap (free disk space, `apt-get` qemu, make `/dev/kvm` writable) —
  your host already has qemu and a usable hardware accelerator (Step 0 verified it).
- Persistent VM instead of built-fresh-and-killed per run.
- ssh adds `-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR` to quiet the host-key
  warning: a persistent VM reused on `localhost:2222` collides with the stale
  `known_hosts` entry from a prior VM. CI's ephemeral runners never reuse the port, so
  `freebsd-provision.bash` doesn't bother.
- `-smp 8` for build speed (CI uses 4), and a `-pidfile` for the lifecycle commands.
- No GHCR libs cache (that's token-gated CI plumbing) — you just `make libs` once.

The DragonFly and OpenBSD CI VMs follow the same shape
(`.ci-scripts/bsd/{dragonfly,openbsd}-provision.bash`) and each has its own skill:
`create-dragonfly-dev-env` (gcc-only, and it drives a VGA console) and
`create-openbsd-dev-env` (also cloud-init, but with static-PIE static-link checks, a raised
`datasize` limit, and the source under a `/build` disk). This skill is FreeBSD-only; load a
sibling if you need one of those.
