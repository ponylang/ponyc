---
name: create-dragonfly-dev-env
description: Load when you need to reproduce or validate a gcc-specific or DragonFly-specific ponyc issue on a local VM, or to stand up a local DragonFly BSD development VM that matches ponyc tier-3 CI. Covers the prerequisite check, the sudo-free QEMU/KVM setup, the reused console automation, the in-VM gcc13 build env, and the gotchas (csh, screendump paths, boot timing, detached builds, per-block shells) that make hand-rolling one error-prone.
disable-model-invocation: false
---

# Create a DragonFly BSD dev/test VM for ponyc

Stand up a local DragonFly BSD VM that matches the ponyc tier-3 CI job, so you can
build and test ponyc on DragonFly without GitHub Actions.

DragonFly only builds ponyc with gcc (the `gcc13` package), so it is the authoritative
local environment for **gcc-specific** ponyc issues — the ones clang-based CI never
sees. The setup is fiddly and timing-sensitive; the steps below are the known-good
sequence. Follow them in order.

Run the repo-relative commands (`.ci-scripts/bsd/...`, `rsync` of the source) from your
ponyc checkout root. The VM itself lives in a separate directory (`$VMDIR`).

**Each fenced `sh` block below is self-contained.** It re-sets `VMDIR`/`DFLY_VER` (and,
where needed, the `SSH` command) at its top, because an agent runs each block as a fresh
shell — environment variables and the working directory do NOT carry from one block to
the next. Do not "optimize" by setting them once and dropping them from later blocks: an
empty `$SSH` would make a `$SSH /bin/sh <<'EOF' ... EOF` heredoc run **on your host**
(silently, exit 0) — and Step 7 contains `rm -rf /usr/local`. Keep every block whole.
Pick your `VMDIR`/`DFLY_VER` once and use the same values in every block.

## Gotchas that will bite you (read first)

Each of these cost real debugging. Internalize them before you start — they recur at
several steps:

- **Per-block fresh shells.** See above: re-establish `VMDIR`/`DFLY_VER`/`SSH` in every
  block; never let `$SSH` be empty in front of a `/bin/sh` heredoc.
- **Root's login shell is csh, not sh.** Any remote command with `$(...)`, `export`, or
  a here-string MUST go through an explicit `/bin/sh`:
  `ssh ... root@localhost /bin/sh <<'EOF' ... EOF`. A bare
  `ssh ... root@localhost 'cmd with $(...)'` fails with `Illegal variable name.`
- **Don't blind-`sleep` for boot.** Verify the VM reached the `login:` prompt by taking
  a console screendump over the QEMU monitor and looking at it; re-screendump until it's
  there. Driving the console early types the whole setup into a half-booted VM and it's
  silently lost.
- **A daemonized QEMU `chdir`s to `/`.** The monitor `screendump` (and any relative path
  the daemon writes) needs an **absolute** path, or it fails `Permission denied`.
- **No `sudo` needed (and often unavailable).** CI loop-mounts the ISO with `sudo` to get
  `/usr/include`; locally, extract it with `bsdtar` instead — same result, no privilege.
- **Detach long in-VM builds.** `make libs` (LLVM) takes hours; run it
  `nohup … > /build/x.log 2>&1 &` and poll the log, so an ssh drop doesn't kill it.
- **Reuse the CI console script verbatim.** `.ci-scripts/bsd/dfly_configure_vm.py` types
  the setup commands into the VGA console via QEMU `sendkey`; the timing/KEYMAP is fiddly
  and already correct. Copy it, don't reimplement console typing.

## Step 0 — verify prerequisites (do NOT assume they're installed)

This skill needs, on the host:

- an existing ponyc checkout (you run the repo-relative commands from it)
- hardware-accelerated virtualization for QEMU (KVM on Linux, HVF on macOS)
- enough free disk for the VM's sparse qcow2 disks (provisioned at 60 GB + 50 GB nominal;
  they grow only as used, and `make libs` plus a build fill a large chunk of the data
  disk), ~2 GB for the image/ISO files, and network access to mirror-master.dragonflybsd.org
- `qemu-system-x86_64` and `qemu-img`
- `bsdtar` (a libarchive tar that reads ISO9660; the default `tar` on macOS, FreeBSD, and DragonFly)
- `bunzip2`, `rsync`, `git`, `curl`, `python3`
- an OpenSSH client (`ssh`, `scp`, `ssh-keygen`)
- a PPM-to-PNG converter: any one of magick/convert, ffmpeg, or pnmtopng

Check that every required tool is present. Do not assume the environment has them, and do
not install anything yourself — installing them is the user's call (it needs privilege,
and the package names vary by OS, so naming a specific package would be wrong on half the
platforms this runs on). If anything is missing, **stop, tell the user which
tools/capabilities are missing, and ask them to install them with their platform's
package manager**, then re-run the check before proceeding.

```sh
missing=""
for t in qemu-system-x86_64 qemu-img bsdtar bunzip2 rsync git ssh scp ssh-keygen curl python3; do
  command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
done
command -v magick >/dev/null 2>&1 || command -v convert >/dev/null 2>&1 \
  || command -v ffmpeg >/dev/null 2>&1 || command -v pnmtopng >/dev/null 2>&1 \
  || missing="$missing ppm-converter(magick|ffmpeg|pnmtopng)"
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
NOT guess a distro package name. (`bsdtar` is the libarchive tar; on macOS/FreeBSD/
DragonFly the system `tar` is already bsdtar, elsewhere it comes from libarchive.) A
missing hardware accelerator — no writable `/dev/kvm` on Linux, no HVF on macOS — is a
host/BIOS/virtualization fix only the user can make; the VM is unusably slow without it,
so don't fall back to TCG emulation.

This flow is verified on Linux+KVM. The guest side is OS-agnostic, so macOS goes through
the same steps with the HVF accelerator (`accel=kvm:hvf` in Step 4 selects it).

## Setup

Pick a persistent VM directory and a DragonFly version that **matches ponyc CI** (check
the image URL in `.ci-scripts/bsd/dragonfly-provision.bash`; currently 6.4.2). Use the
same two values in every block below.

### 1. Download the image + ISO (one time; keep the `.bz2` to avoid re-downloading)

The raw `.img` boots; the `.iso` is only mined for system headers (the raw image ships
without `/usr/include`).

```sh
VMDIR=~/vms/dragonfly-6.4.2; DFLY_VER=6.4.2
mkdir -p "$VMDIR" && cd "$VMDIR"
base="https://mirror-master.dragonflybsd.org/iso-images"
curl -fL --retry 3 -o dfly.img.bz2 "$base/dfly-x86_64-${DFLY_VER}_REL.img.bz2"
curl -fL --retry 3 -o dfly.iso.bz2 "$base/dfly-x86_64-${DFLY_VER}_REL.iso.bz2"
```

### 2. Build the disks

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
bunzip2 -k -f dfly.img.bz2 dfly.iso.bz2          # -k keeps the .bz2 originals
qemu-img convert -f raw -O qcow2 dfly.img dfly.qcow2
qemu-img resize dfly.qcow2 60G
qemu-img create -f qcow2 dfly-data.qcow2 50G     # build workspace; root part is ~1.8G
rm -f dfly.img                                    # keep dfly.qcow2 + the .bz2
```

### 3. Extract system headers from the ISO — no sudo (replaces CI's `mount -o loop`)

`bsdtar` reads ISO9660 directly. The tar must have `include/` as its top entry so the
in-VM extraction matches CI. (Both the extract and the repack use `bsdtar`, so no
separate `tar` is needed on the host.)

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
rm -rf iso-stage && mkdir iso-stage
bsdtar -xf dfly.iso -C iso-stage usr/include
bsdtar -cf dfly-include.tar -C iso-stage/usr include
rm -rf iso-stage
```

### 4. Key + boot (daemonized, persistent)

```sh
VMDIR=~/vms/dragonfly-6.4.2; DFLY_VER=6.4.2; cd "$VMDIR"
test -f vm_key || ssh-keygen -t ed25519 -f vm_key -N "" -q
qemu-system-x86_64 \
  -name dragonfly-${DFLY_VER} \
  -machine pc,accel=kvm:hvf -cpu host -smp 8 -m 12G \
  -drive file=dfly.qcow2,format=qcow2,if=virtio \
  -drive file=dfly-data.qcow2,format=qcow2,if=virtio \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -object rng-random,id=rng0,filename=/dev/urandom \
  -device virtio-rng-pci,rng=rng0 \
  -monitor unix:dfly-monitor.sock,server,nowait \
  -display none -pidfile dfly.pid -daemonize
```

(`accel=kvm:hvf` picks KVM on Linux or HVF on macOS and errors if neither is available —
it never silently falls back to slow TCG. `-smp`/`-m` are speed knobs; CI uses 4 CPUs /
12G. `hostfwd 2222->22` is the ssh port.)

### 5. Confirm the login prompt (re-screendump until ready), then drive the console

Save the monitor helper and screendump the console. The daemonized QEMU writes to an
**absolute** path. Boot takes ~80s; **if the image isn't at the `login:` prompt yet,
wait ~20s and screendump again — repeat until you see `login:`.** Do not run the console
script before then (it blind-types into the console and a half-booted VM loses it).

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
cat > monitor_cmd.py <<'PY'
import socket, sys, time
s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); s.connect('dfly-monitor.sock')
time.sleep(0.4); s.recv(65536)
s.sendall((sys.argv[1] + '\n').encode()); time.sleep(0.5); s.settimeout(1.0)
try:
    while True:
        d = s.recv(65536)
        if not d: break
        sys.stdout.write(d.decode(errors='replace'))
except socket.timeout: pass
s.close()
PY
to_png() {  # convert PPM->PNG with whichever converter is installed
  if   command -v magick   >/dev/null 2>&1; then magick "$1" "$2"
  elif command -v convert  >/dev/null 2>&1; then convert "$1" "$2"
  elif command -v ffmpeg   >/dev/null 2>&1; then ffmpeg -y -loglevel error -i "$1" "$2"
  else pnmtopng "$1" > "$2"; fi
}
python3 monitor_cmd.py "screendump $VMDIR/console1.ppm"   # absolute path is required
to_png "$VMDIR/console1.ppm" "$VMDIR/console1.png"
# open console1.png. If it is NOT yet at "login:", wait ~20s and re-run THIS WHOLE block
# (a fresh shell won't have $VMDIR or to_png). Only continue once you see "login:".
```

Then run the CI console script verbatim — it lives in the repo at
`.ci-scripts/bsd/dfly_configure_vm.py`. Copy it into `$VMDIR` (it connects to the
`dfly-monitor.sock` in its working directory) and run it there. It logs in, brings up
networking, configures sshd, installs your key, and formats/mounts the build disk:

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
cp "$(git rev-parse --show-toplevel)/.ci-scripts/bsd/dfly_configure_vm.py" "$VMDIR/"
export PUB_KEY="$(cat vm_key.pub)"
python3 dfly_configure_vm.py
```

### 6. Wait for ssh

If ssh never comes up within the budget, the console script likely ran before the VM was
ready (Step 5) — re-screendump to check the console state, and re-run Step 5 if it's not
logged in.

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
up=""
for i in $(seq 1 150); do   # ~5 min, matching CI's timeout
  if ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=2 -i vm_key -p 2222 \
       root@localhost true 2>/dev/null; then up=1; echo "SSH up"; break; fi
  sleep 2
done
[ -n "$up" ] || echo "SSH NEVER CAME UP — check the console screendump (Step 5) and rerun if needed"
```

From here, **always** drive the VM through `/bin/sh` (root's shell is csh — see Gotchas),
and define `$SSH` in the same block you use it (it does not carry between blocks):

```sh
VMDIR=~/vms/dragonfly-6.4.2
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
uname -a
EOF
```

### 7. Move `/usr/local` to the build disk, install headers, install deps

Root is only ~1.8G; `gcc13` alone is ~418M, so package installs must land on `/build`.

```sh
VMDIR=~/vms/dragonfly-6.4.2
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
set -e
cpdup /usr/local /build/usr_local
rm -rf /usr/local
ln -s /build/usr_local /usr/local
EOF

scp -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i "$VMDIR/vm_key" -P 2222 "$VMDIR/dfly-include.tar" root@localhost:/build/
$SSH /bin/sh <<'EOF'
set -e
tar xf /build/dfly-include.tar -C /build
ln -s /build/include /usr/include
rm /build/dfly-include.tar
EOF

$SSH /bin/sh <<'EOF'
set -e
pkg install -y cmake gmake git python3 cxx_atomics rsync gcc13 ca_root_nss
pkg clean -ay
git config --global --add safe.directory /build/ponyc
EOF
```

### 8. Rsync the ponyc checkout in (exclude the host `build/`)

Run from your ponyc checkout. Exclude the top-level `build/` (host artifacts; the VM
builds its own). Keep `.git` (CMake runs `git rev-parse`). The vendored LLVM submodule
under `lib/llvm/src` IS transferred — the VM needs it for `make libs`.

```sh
VMDIR=~/vms/dragonfly-6.4.2
rsync -az --exclude='/build' \
  -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222" \
  "$(git rev-parse --show-toplevel)/" root@localhost:/build/ponyc/
```

## Using the VM

Always set the gcc13 build env first (matches the tier-3 `dragonflybsd` job). `make libs`
builds the vendored LLVM and is the multi-hour long pole — run it detached, then poll the
log until it ends with `libs DONE rc=0` (a nonzero rc means it failed — read the log
above the marker):

```sh
VMDIR=~/vms/dragonfly-6.4.2
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
set -e
cat > /build/run-libs.sh <<'S'
#!/bin/sh
export CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13
export LD_LIBRARY_PATH=/usr/local/lib/gcc13
export SSL_CERT_FILE=/usr/local/share/certs/ca-root-nss.crt
cd /build/ponyc && gmake libs llvm_tools=false build_flags=-j8
echo "libs DONE rc=$?"
S
chmod +x /build/run-libs.sh
nohup /build/run-libs.sh > /build/libs-build.log 2>&1 &
echo "libs building pid $!"
EOF
# poll until done: $SSH /bin/sh -c 'tail -3 /build/libs-build.log'  -> wait for "libs DONE rc=0"
```

Once `libs` is built (it persists in the VM), build and test with the same env:

```sh
VMDIR=~/vms/dragonfly-6.4.2
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
set -e
export CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13
export LD_LIBRARY_PATH=/usr/local/lib/gcc13
export SSL_CERT_FILE=/usr/local/share/certs/ca-root-nss.crt
cd /build/ponyc
gmake configure config=debug
gmake build config=debug
gmake test-ci-core config=debug
EOF
```

To iterate on a fix: edit on the host, re-run Step 8's `rsync` (it's incremental — only
changed files are sent), then re-run the build block above (long rebuilds should also be
detached + polled). For exact CI parity, the tier-3 `dragonflybsd` job
(`.github/workflows/ponyc-tier3.yml`) runs more than this — the reject-unsupported-builds
check, a `--static` link smoke, and the coverage smoke — so consult it when validating a
CI-matching issue.

## Lifecycle

```sh
VMDIR=~/vms/dragonfly-6.4.2; cd "$VMDIR"
pgrep -af "qemu-system-x86_64 -name dragonfly"        # status
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i vm_key -p 2222 root@localhost   # interactive shell
# stop: DragonFly's default install has no ACPI shutdown handler, so system_powerdown
# usually does nothing — kill the process instead. Disks persist for next time.
kill "$(cat dfly.pid)" 2>/dev/null || pkill -f "qemu-system-x86_64 -name dragonfly"
```

To restart later, re-run the step-4 boot command (the disks and installed deps persist;
you do not redo setup). Note: this VM's sshd start and `/build` mount were done once by
`dfly_configure_vm.py` and are not persisted across a guest reboot — for a long-lived
VM, make `sshd_enable` permanent in `/etc/rc.conf` and add the `/build` mount to
`/etc/fstab`.

## How this relates to CI (don't re-add the CI-only steps)

This mirrors `.ci-scripts/bsd/dragonfly-provision.bash` (the source of truth — keep this
skill in sync if it changes) and **reuses `.ci-scripts/bsd/dfly_configure_vm.py`
unchanged**. The local setup deliberately differs from CI, and each difference is safe:

- Skips CI host-bootstrap (free disk space, install qemu, make `/dev/kvm` writable) —
  your host already has qemu and a usable hardware accelerator (Step 0 verified it).
- `bsdtar` instead of `sudo mount -o loop` for the ISO headers — no privilege needed.
- Persistent VM instead of built-fresh-and-killed per run.
- ssh adds `-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR` to quiet the host-key
  warning: a persistent VM reused on `localhost:2222` collides with the stale
  `known_hosts` entry from a prior VM. CI's ephemeral runners never reuse the port, so
  `dragonfly-provision.bash` doesn't bother.
- `-smp 8` for build speed (CI uses 4).
- Screendump-verified boot instead of a blind `sleep 90`.
- No GHCR libs cache (that's token-gated CI plumbing) — you just `make libs` once.

The FreeBSD and OpenBSD CI VMs follow the same shape
(`.ci-scripts/bsd/{freebsd,openbsd}-provision.bash`); FreeBSD takes a `FREEBSD_VERSION`,
and OpenBSD differs in its static-link checks and rejects `use=coverage`. This skill is
DragonFly-only; adapt from those scripts if you need a sibling.
