---
name: create-i386-dev-env
description: Load when you need to reproduce or validate an i386 (32-bit x86) ponyc issue on a local VM, or to stand up a local 32-bit x86 Linux development VM that matches the ponyc tier-3 i386 CI job. Covers the Alpine netboot boot (no installer, tmpfs root), the boot-time http server for the modloop and ssh key, the per-boot root setup, the /build data disk with swap, the CPU and memory flags, and the gotchas (nothing outside /build survives a reboot, busybox adduser locks the account, loopback down after boot, coreduo with one socket per vCPU, the 3500M RAM ceiling, the ~3 GB address space, an `openssl` command that doesn't match the LibreSSL libraries the build uses, per-package stdlib tests, detached builds, per-block shells) that make hand-rolling one error-prone.
disable-model-invocation: false
---

# Create an i386 dev/test VM for ponyc

Stand up a local 32-bit x86 Alpine Linux VM that matches the ponyc tier-3 `i386-linux` CI
job, so you can build and test ponyc on i386 without GitHub Actions.

The i386 job exercises paths the 64-bit jobs never do: ILP32 (32-bit pointers and
`USize`), a per-process address space of about 3 GB, musl on i386, the x87 register used
to return floating-point values on i386, and aligned SSE2 loads that fault on misaligned
addresses. So this VM is the authoritative local environment for **i386** ponyc issues.

It is a VM and not a `--platform linux/386` container on purpose. A container shares the
host's 64-bit kernel: `uname -m` reports `x86_64`, so the vendored LLVM defaults to an
x86_64 target, and a 32-bit process gets 4 GB of address space instead of a real i386
kernel's 3 GB. The VM boots Alpine's 32-bit kernel, so `uname -m`, LLVM's default target
and the address space all match a real i386 machine. (For 32-bit ARM use `32-bit-support-validation` instead.)

The VM lives in its own directory (`$VMDIR`). The blocks that read CI files or copy the
source set `PONYC` to the path of your ponyc checkout; set it to yours.

**Each fenced `sh` block below is self-contained.** It re-sets `VMDIR`, `SSH_PORT`,
`HTTP_PORT`, `PONYC` and the `SSH`/`SSH_ROOT` commands at its top where it uses them,
because an agent runs each block as a fresh shell: environment variables and the working
directory do NOT carry from one block to the next. Do not "optimize" by setting them once
and dropping them from later blocks. An empty `$SSH_ROOT` would make a
`$SSH_ROOT /bin/sh <<'EOF' ... EOF` heredoc run **on your host** instead of in the VM, and
Step 5 contains `mkfs.ext4` and edits `/etc/shadow`. Keep every block whole. Pick your
values once and use the same ones in every block.

## Gotchas (read first)

This VM differs from the BSD dev VMs in one big way: it has no installed root disk. It
boots Alpine's netboot kernel and initramfs directly, and its root filesystem is a tmpfs.

- **Per-block fresh shells.** See above: re-establish `VMDIR`/`SSH_PORT`/`HTTP_PORT`/
  `PONYC`/`SSH`/`SSH_ROOT` in every block; never let `$SSH` or `$SSH_ROOT` be empty in
  front of a `/bin/sh` heredoc.
- **Nothing outside `/build` survives a reboot.** The root filesystem is a tmpfs, so the
  packages, the `pony` user, `/etc/apk/repositories`, the loopback interface, the `/build`
  mount and the swap are all gone after a guest reboot. Only the data disk mounted at
  `/build` persists, and that is where the source, LLVM and the builds live. After every
  boot, run the root setup again (Step 5); it skips the format and swap-file creation when
  the disk already has them. sshd's host keys are also generated fresh each boot, which is
  why every ssh command here turns off host-key checking.
- **The tmpfs root is small.** It is half the guest's RAM (about 1.7G with `-m 3500M`), and
  the packages fill most of it (about 600M free after setup). Keep large output (logs, scratch builds)
  under `/build`.
- **The guest needs the network every boot.** It fetches its packages from
  `dl-cdn.alpinelinux.org` on every boot, because nothing installed survives.
- **The boot needs an http server on the host.** Alpine's initramfs fetches the kernel
  modules (`modloop-lts`) and root's `authorized_keys` over http while it boots (the
  `modloop=` and `ssh_key=` kernel arguments). The guest reaches the host as `10.0.2.2`.
  Step 3 serves a directory holding only those two files, so the server never hands out
  the private key, and Step 4 stops it once ssh is up. A stale server left on the same port
  from another VM serves a different key, and ssh then refuses yours; Step 3 exits
  without starting anything if the port is taken.
- **Root for setup, `pony` for build and test.** Root's key comes from the boot, so the
  setup logs in as root. The build and the tests run as the `pony` user, like CI. Busybox
  `adduser -D` locks the new account (password field `!`), and sshd refuses key logins to
  locked accounts, so Step 5 changes the field to `*` (no password, not locked).
- **The loopback interface is down after a netboot.** The runtime and stdlib tests bind
  to `127.0.0.1`, so Step 5 runs `ip link set lo up` each boot.
- **CPU: `-cpu coreduo,vendor=GenuineIntel` and one socket per vCPU.** coreduo is a
  32-bit CPU with SSE2. ponyc generates code for the host CPU's features, and without SSE2
  that code does F64 math on x87, whose wider registers give results that differ from SSE2
  machines (a `pentium3` CPU fails json tests this way). KVM otherwise reports the host's
  vendor, and LLVM names the same CPU model differently on AMD, so the vendor is fixed.
  `-smp N,sockets=N,cores=1,threads=1` gives each vCPU its own socket: with one multi-core
  socket, this CPU model makes the guest report every vCPU as a hyperthread of one core,
  the Pony runtime counts physical cores, so it counts one.
- **`-m 3500M` is the most RAM the guest can use.** The 32-bit kernel can't use RAM above
  4 GB, and from 3584M up QEMU's `pc` machine maps everything past 3 GB above 4 GB. Don't
  raise it.
- **About 3 GB of address space per process.** That is the i386 kernel's user-space limit,
  and it is the reason for the 8G swap file on `/build` and for the per-package stdlib
  tests below.
- **The monolithic `stdlib` test is excluded.** Linking every stdlib package into one test
  binary exhausts the 3 GB address space during LLVM code generation. CI runs `ctest` with
  `-E '^stdlib$'` and then `.ci-scripts/i386-stdlib-perpkg.sh`, which compiles and runs
  each package's tests separately. Do the same.
- **Don't derive the SSL flag from `openssl version`.** The build links against LibreSSL
  (`libressl-dev`), but the `openssl` command doesn't describe it: on a fresh boot of this
  setup `openssl` is OpenSSL 3 (the `openssl` package is installed next to LibreSSL), and a
  VM set up with a different package history may have no `openssl` command at all
  (LibreSSL's own command is `libressl`). The per-package script reads the flag CMake chose
  (`PONY_SSL_FLAG` in the preset's `CMakeCache.txt`). For a manual per-module compile,
  take the flag from the same place:
  `sed -n 's/^PONY_SSL_FLAG:[A-Z]*=//p' build/build_debug/CMakeCache.txt`.
- **Alpine's sshd accepts no environment variables.** `ssh -o SendEnv` and `SetEnv` do
  nothing. Pass a value into a guest script as a positional argument
  (`$SSH_ROOT /bin/sh -s -- "$VALUE" <<'EOF'`), as Step 5 does.
- **Guest kernel messages go to the serial console log.** OOM kills and panics show up in
  `$VMDIR/console.log`, not in the ssh output. Read it when a build or test dies without
  an error.
- **Detach long in-VM builds.** The libs build (LLVM) takes hours; run it
  `nohup … > /build/x.log 2>&1 &` and poll the log, so an ssh drop doesn't kill it.

## Known i386 failures

These are current failures, not accepted limitations; they need to be fixed. Before
treating one of them as expected, run `gh issue view N` for its issue. If the issue is
closed and the failure shows up, it is a regression, not the known failure.

- **`buffered` segfaults in the per-package stdlib run** (#6440). It fails in all four
  runtime and codegen combinations, so `.ci-scripts/i386-stdlib-perpkg.sh` reports one
  package failed and exits nonzero.
- **The `byte_order` signaling-NaN round-trip test fails intermittently** (#6439). It has
  been seen under debug codegen. The bug itself also reaches release builds where the
  calls aren't inlined.

## Step 0: verify prerequisites (do NOT assume they're installed)

This skill needs, on the host:

- an existing ponyc checkout (Steps 1, 3, 5 and 6 read from it)
- Linux with KVM (a writable `/dev/kvm`); the blocks use Linux-only `ss` and `accel=kvm`
- enough free disk for the sparse 60G qcow2 data disk and the netboot tarball (the disk
  file grows as it's used and never shrinks; after LLVM, debug and release builds and the
  full test matrix it was about 40G), and network access to `dl-cdn.alpinelinux.org`
- `qemu-system-i386` and `qemu-img`
- `curl`, `sha256sum`, `tar`, `cmp`
- `python3` (serves the boot files over http)
- `rsync`, `git`
- an OpenSSH client (`ssh`, `ssh-keygen`)
- `pgrep`, `pkill`, `ss`

Check that every required tool is present. Do not assume the environment has them, and do
not install anything yourself: installing them is the user's call (it needs privilege, and
the package names vary by OS). If anything is missing, **stop, tell the user which
tools/capabilities are missing, and ask them to install them with their platform's package
manager**, then re-run the check before proceeding.

```sh
missing=""
for t in qemu-system-i386 qemu-img curl sha256sum tar cmp python3 rsync git ssh ssh-keygen pgrep pkill ss; do
  command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
done
case "$(uname -s)" in
  Linux) [ -w /dev/kvm ] || missing="$missing kvm(/dev/kvm-not-writable)" ;;
  *)     missing="$missing linux-host(the blocks use Linux-only ss and accel=kvm)" ;;
esac
df -h "${VMDIR:-$HOME}" | tail -n 1   # free space where the VM directory will live
[ -n "$missing" ] && echo "MISSING:$missing" || echo "all prerequisites present"
```

If the output is anything but `all prerequisites present`, name the missing
tools/capabilities to the user and ask them to install them however their OS does; do NOT
guess a distro package name. A missing or unwritable `/dev/kvm` is a host, BIOS or
virtualization fix only the user can make; the VM is unusably slow without it, so don't
fall back to TCG emulation. On a host that isn't Linux, stop and tell the user: the blocks
use Linux-only `ss` and `accel=kvm`, and the skill is verified only on Linux with KVM. If
the `df` line shows less than about 40G free, tell the user before going on.

This flow is verified on Linux with KVM on an Intel host, in a fresh VM directory, through
Step 0 to Step 6, the 32-bit check, starting the detached libs build (stopped early, not
run to completion), the graceful stop while that build was still writing to `/build`, and a
restart (Steps 3 to 5 and the 32-bit check) that kept `/build`'s contents without
reformatting it. The LLVM triple check and the hello-world build and run were run against
an earlier VM that already had LLVM and a debug ponyc built. That earlier VM is also where
the full build and test matrix ran: debug and release builds, `ci-core` with debug codegen
on both runtimes, `full-programs` with release codegen on both, the per-package stdlib
tests in all four combinations, and the three tool test suites, with the results in
"Known i386 failures". It used the same Alpine 3.24.4 netboot, package list and default
`libs` preset as here, but `-cpu coreduo` without the vendor pin, `-m 4G` (the guest sees
about 3 GB), and a 79G data disk.

## Setup

Pick a persistent VM directory and two host ports: `SSH_PORT` (forwarded to the guest's
sshd for as long as the VM runs) and `HTTP_PORT` (the boot-file server, only during boot).
The defaults below are 2232 and 8099. To run two i386 VMs, give each its own `VMDIR`,
`SSH_PORT` and `HTTP_PORT`, and don't boot two of them at the same time on the same
`HTTP_PORT`. The Alpine release, its download branch and the tarball's sha256 come from
`.ci-scripts/linux32/i386-alpine.env`, the same pins CI uses.

### 1. Download the netboot tarball and create the data disk

The tarball holds the 32-bit kernel (`vmlinuz-lts`), initramfs (`initramfs-lts`) and
kernel modules (`modloop-lts`). The data disk is created only if it doesn't exist yet,
because it holds everything that persists. Run this once, and again (with Step 2) whenever
`.ci-scripts/linux32/i386-alpine.env` changes in the checkout: Steps 3 and 5 read the
Alpine branch from that file on every boot, so a stale kernel and `modloop-lts` would boot
against a newer package repository.

```sh
VMDIR=~/vms/i386-alpine; PONYC=~/code/ponyc
set -e
. "$PONYC/.ci-scripts/linux32/i386-alpine.env"
NETBOOT="alpine-netboot-${ALPINE_RELEASE}-x86.tar.gz"
mkdir -p "$VMDIR/netboot" && cd "$VMDIR"
curl -fL --retry 3 -o "$NETBOOT" \
  "https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}/releases/x86/${NETBOOT}"
echo "${NETBOOT_SHA256}  $NETBOOT" | sha256sum -c -
tar -xzf "$NETBOOT" -C netboot
test -f data.qcow2 || qemu-img create -q -f qcow2 data.qcow2 60G
ls netboot/boot/vmlinuz-lts netboot/boot/initramfs-lts netboot/boot/modloop-lts
```

### 2. ssh key and the http directory

The http directory holds only the public key (as `authorized_keys`) and `modloop-lts`.

```sh
VMDIR=~/vms/i386-alpine
set -e
cd "$VMDIR"
test -f vm_key || ssh-keygen -q -t ed25519 -f vm_key -N ""
mkdir -p http
cp vm_key.pub http/authorized_keys
cp netboot/boot/modloop-lts http/
```

### 3. Start the http server and boot (daemonized)

Run this for every boot. It exits without starting anything if either port is already in
use. Otherwise it starts the
http server in the background (its pid goes in `http.pid`), checks that the server returns
your public key, and then boots QEMU.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232; HTTP_PORT=8099; PONYC=~/code/ponyc
set -e
cd "$VMDIR"
. "$PONYC/.ci-scripts/linux32/i386-alpine.env"
REPO_URL="https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}"
for p in "$SSH_PORT" "$HTTP_PORT"; do
  if ss -ltn "sport = :$p" | grep -q LISTEN; then
    echo "port $p is already in use; stop what holds it or pick another port"; exit 1
  fi
done
nohup python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 \
  --directory "$VMDIR/http" > "$VMDIR/http.log" 2>&1 &
echo $! > "$VMDIR/http.pid"
served=""
for i in $(seq 1 10); do
  if curl -fs "http://127.0.0.1:$HTTP_PORT/authorized_keys" | cmp -s - vm_key.pub; then
    served=1; break
  fi
  sleep 1
done
if [ -z "$served" ]; then
  echo "the http server is not serving vm_key.pub; see $VMDIR/http.log"
  kill "$(cat "$VMDIR/http.pid")"; exit 1
fi
qemu-system-i386 \
  -name "i386-alpine-$SSH_PORT" \
  -machine pc,accel=kvm \
  -cpu coreduo,vendor=GenuineIntel \
  -smp 8,sockets=8,cores=1,threads=1 \
  -m 3500M \
  -kernel netboot/boot/vmlinuz-lts \
  -initrd netboot/boot/initramfs-lts \
  -append "console=ttyS0 ip=dhcp alpine_repo=${REPO_URL}/main modloop=http://10.0.2.2:${HTTP_PORT}/modloop-lts ssh_key=http://10.0.2.2:${HTTP_PORT}/authorized_keys" \
  -drive file=data.qcow2,format=qcow2,if=virtio \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:${SSH_PORT}-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none \
  -serial file:"$VMDIR/console.log" \
  -pidfile "$VMDIR/qemu.pid" \
  -daemonize || { kill "$(cat "$VMDIR/http.pid")"; exit 1; }
echo "VM booting"
```

(`-cpu`, `-m` and the socket layout are explained in Gotchas. `-smp 8` only affects speed;
CI uses 4. The Pony runtime starts one scheduler thread per core it counts, so when you are
reproducing a CI failure that may depend on timing, use `-smp 4,sockets=4,cores=1,threads=1`
like CI. Keep `sockets` equal to the vCPU count whatever number you pick. The QEMU name
ends in the ssh port so the lifecycle commands match only this VM. Those commands
spell the pattern `[q]emu-system-i386`, so `pgrep -f`/`pkill -f` can't match the shell
running them, whose own command line contains the pattern text. This boot command is
adapted from `.ci-scripts/linux32/i386-provision.bash`.)

### 4. Wait for ssh, then stop the http server

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
cd "$VMDIR"
SSH_ROOT="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -o ConnectTimeout=2 -i $VMDIR/vm_key -p $SSH_PORT root@127.0.0.1"
up=""
for i in $(seq 1 150); do   # ~5 min
  if $SSH_ROOT true 2>/dev/null; then up=1; echo "SSH up"; break; fi
  sleep 2
done
if [ -n "$up" ]; then
  kill "$(cat http.pid)" && rm http.pid && echo "http server stopped"
else
  echo "SSH NEVER CAME UP; see recovery below."
fi
```

If ssh never comes up, diagnose in this order:

- **Is the process alive?** `pgrep -af "[q]emu-system-i386 -name i386-alpine-$SSH_PORT "`.
  If it's gone, QEMU exited during boot; read `$VMDIR/console.log` for why.
- **Read the boot.** `$VMDIR/console.log` has the guest's serial console from the start
  of this boot. It shows whether the kernel started, whether DHCP worked, and whether the
  initramfs fetched `modloop-lts` and the ssh key. `$VMDIR/http.log` shows which files the
  guest requested from the http server.
- **ssh actively *refuses* the key (vs. just timing out)?** The initramfs didn't install
  your key. Either the http server wasn't running during the boot, the `ssh_key=` URL's
  port didn't match `HTTP_PORT`, or a stale server on `HTTP_PORT` served a different key.
- **Before retrying,** stop the http server (`kill "$(cat "$VMDIR/http.pid")"`) and the VM
  (the stop command in Lifecycle), then re-run Step 3.

### 5. Root setup (every boot)

This installs the same packages CI installs, formats the data disk on first use, mounts it
at `/build`, enables the swap file, creates the `pony` user with your key, and prints the
toolchain this boot got. The packages are not pinned and are reinstalled on every boot, so
the printed clang, musl, LibreSSL and cmake versions are the record of what you built
with. The LLVM build on `/build` stays as it was.

Two guards make it safe after a reboot: `mkfs.ext4` runs only when `blkid` doesn't report
an ext4 filesystem on `/dev/vda` (busybox `blkid` exits 0 even for a missing device, so the
guard checks its output), and the swap file is created only if `/build/swap` doesn't
exist. **Run this once per boot.** Within one boot it is not repeatable: `adduser`,
`mount` and `swapon` fail the second time. If it stops partway, read the error; the
simplest recovery is to stop the VM (Lifecycle) and boot again, since the tmpfs root
starts clean. To tell whether a VM that is already running has had Step 5 this boot, run
`$SSH_ROOT 'mountpoint -q /build && id pony'` (with `SSH_ROOT` set as below); if it
succeeds, skip Step 5.

The Alpine repository URL goes to the guest as a positional argument, because Alpine's
sshd accepts no environment variables and the heredoc is quoted so that nothing in it
expands on the host.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232; PONYC=~/code/ponyc
. "$PONYC/.ci-scripts/linux32/i386-alpine.env"
REPO_URL="https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}"
SSH_ROOT="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT root@127.0.0.1"
$SSH_ROOT /bin/sh -s -- "$REPO_URL" <<'EOF'
set -e
REPO_URL=$1
[ -n "$REPO_URL" ] || { echo "no repository URL given" >&2; exit 1; }
ip link set lo up
printf '%s\n' "$REPO_URL/main" "$REPO_URL/community" > /etc/apk/repositories
apk update -q
apk add -q build-base clang clang-dev coreutils linux-headers cmake git \
  zlib-dev bash python3 libressl libressl-dev rsync e2fsprogs ca-certificates
modprobe ext4
blkid /dev/vda | grep -q 'TYPE="ext4"' || mkfs.ext4 -q /dev/vda
mkdir -p /build
mount /dev/vda /build
if [ ! -f /build/swap ]; then
  fallocate -l 8G /build/swap
  chmod 600 /build/swap
  mkswap /build/swap > /dev/null
fi
swapon /build/swap
adduser -D -u 1000 -s /bin/sh pony
# adduser -D locks the account (password field '!'), and sshd refuses key
# logins to locked accounts. '*' allows no password but isn't locked.
sed -i 's/^pony:!:/pony:*:/' /etc/shadow
mkdir -p /home/pony/.ssh
cp /root/.ssh/authorized_keys /home/pony/.ssh/authorized_keys
chown -R pony:pony /home/pony/.ssh
chown pony:pony /build
uname -m
free -m
clang --version | head -n 1
apk list -I musl libressl cmake 2>/dev/null
EOF
```

The fixed uid 1000 keeps the files on `/build` owned by `pony` across boots, since the
user is recreated each time.

Then check, as `pony`, that the VM is 32-bit x86 with SSE2. This also confirms that the
`pony` login works. These are the CPU, kernel and clang checks from the `check-32bit`
operation of `.ci-scripts/linux32/i386-libs-cache.bash`; the LLVM check comes after the
libs build.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT pony@127.0.0.1"
$SSH /bin/sh <<'EOF'
set -e
grep -e vendor_id -e 'model name' /proc/cpuinfo | head -n 2
if ! grep -qw sse2 /proc/cpuinfo; then
  echo "the VM's CPU has no SSE2" >&2
  exit 1
fi
echo "uname -m: $(uname -m); clang: $(clang -dumpmachine)"
case "$(uname -m)" in
  i[3-6]86) ;;
  *) echo "the kernel reports $(uname -m)" >&2; exit 1 ;;
esac
case "$(clang -dumpmachine)" in
  i[3-6]86-*) ;;
  *) echo "clang targets $(clang -dumpmachine)" >&2; exit 1 ;;
esac
echo "32-bit checks passed"
EOF
```

### 6. Rsync the ponyc checkout in (exclude the host `build/`)

Exclude the top-level `build/` (host artifacts; the VM builds its own). The anchored
`/build` pattern matters: an unanchored `build/` would also drop directories named `build`
inside the LLVM submodule, leaving it with missing files. Keep `.git`: `lib/CMakeLists.txt`
runs `git submodule` and `git apply` (for the LLVM patches) in the VM. The vendored LLVM
submodule under `lib/llvm/src` IS transferred; the VM needs it for the libs build.
`--delete` removes files from the VM's copy that you deleted or renamed on the host (Pony
compiles every `.pony` file in a package directory, so a stale one changes the build). It
doesn't touch the excluded `/build`.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232; PONYC=~/code/ponyc
rsync -az --delete --exclude='/build' \
  -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT" \
  "$PONYC/" pony@127.0.0.1:/build/ponyc/
```

## Using the VM

The build uses Alpine's clang and the default `libs` preset, as CI does; there are no
`CC`/`CXX` exports. `cmake -P lib/build-libs.cmake` builds the vendored LLVM and is the
step that takes hours. Run it detached, then poll the log until it ends with
`libs DONE rc=0` (a nonzero rc means it failed; read the log above the marker, and
`$VMDIR/console.log` for OOM kills). Keep `-DJOBS=4` (what CI uses): each compile job
is its own process and they share the 3500M of RAM and the swap, so higher parallelism is
the first thing to suspect on a libs out-of-memory. If the build is interrupted (a VM
stop, an ssh drop that the detached run should survive anyway), re-running this block
resumes it. Re-run it too, after Step 6's `rsync`, whenever the LLVM submodule,
`lib/CMakePresets.json`, `CMakeLists.txt`, `lib/CMakeLists.txt`, `lib/build-libs.cmake` or
`lib/llvm/patches/` changes; those are the files CI hashes to decide when to rebuild it.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT pony@127.0.0.1"
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
# poll until done: $SSH tail -3 /build/libs-build.log  -> wait for "libs DONE rc=0"
```

When the libs are built, check that LLVM's default target triple is 32-bit x86. It becomes
ponyc's default target, so an `x86_64` triple here means every program ponyc builds is
64-bit. The check is the one `check-32bit` runs in CI.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT pony@127.0.0.1"
$SSH /bin/sh <<'EOF'
set -e
cd /build/ponyc
llvm_triple=$(sed -n 's/^#define LLVM_DEFAULT_TARGET_TRIPLE "\(.*\)"$/\1/p' build/libs/include/llvm/Config/llvm-config.h)
echo "LLVM's default triple: $llvm_triple"
case "$llvm_triple" in
  i[3-6]86-*) ;;
  *) echo "LLVM's default triple is '$llvm_triple', not 32-bit x86" >&2; exit 1 ;;
esac
EOF
```

Then build ponyc and the three tool test binaries, and check that the new ponyc builds a
32-bit i386 program that runs. The libs build persists on `/build`, so you don't rebuild
it after a reboot. Do not run this block until the poll shows `libs DONE rc=0`; even
`cmake --preset debug` needs the built LLVM.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT pony@127.0.0.1"
$SSH /bin/sh <<'EOF'
set -e
cd /build/ponyc
cmake --preset debug
cmake --build --preset debug
cmake --build --preset debug --target pony-doc-tests pony-lint-tests pony-lsp-tests
mkdir -p /tmp/hello
printf 'actor Main\n  new create(env: Env) =>\n    env.out.print("hello from i386")\n' > /tmp/hello/main.pony
PONYPATH="$PWD/packages" ./build/debug/ponyc -o /tmp/hello /tmp/hello
# ELF header: class 1 is 32-bit, machine 3 is i386.
test "$(od -An -tx1 -j4 -N1 /tmp/hello/hello | tr -d ' ')" = 01
test "$(od -An -tx1 -j18 -N2 /tmp/hello/hello | tr -d ' ')" = 0300
# LLVM's default triple ends in gnu; the program only runs if ponyc
# linked it against musl's loader.
/tmp/hello/hello
EOF
```

Run the core tests with debug codegen, the way CI's first test step does, and then the
per-package stdlib tests. The block has no `set -e`, so the per-package run still happens
when `ctest` reports a failure. Expect the per-package script to exit nonzero while the
`buffered` failure in "Known i386 failures" is open. These runs are long; for a run you
can't afford to lose to an ssh drop, put the commands in a script and detach it the same
way as the libs build.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i $VMDIR/vm_key -p $SSH_PORT pony@127.0.0.1"
$SSH /bin/sh <<'EOF'
cd /build/ponyc || exit 1
PONY_DEBUG=1 ctest --preset debug -L ci-core -E '^stdlib$'
PONY_DEBUG=1 sh .ci-scripts/i386-stdlib-perpkg.sh debug
EOF
```

To iterate on a fix: edit on the host, re-run Step 6's `rsync` (it's incremental), then
re-run the build block above. For exact CI parity, the tier-3 `i386-linux` job
(`.github/workflows/ponyc-tier3.yml`) runs more than this. After the debug build and the
hello-world check, in order:

- `PONY_DEBUG=1 ctest --preset debug -L ci-core -E '^stdlib$'` and
  `PONY_DEBUG=1 sh .ci-scripts/i386-stdlib-perpkg.sh debug` (debug runtime, debug codegen)
- `ctest --preset debug -R "^full-programs$"` and
  `sh .ci-scripts/i386-stdlib-perpkg.sh debug` (debug runtime, release codegen)
- `cmake --preset release` and `cmake --build --preset release`
- `PONY_DEBUG=1 ctest --preset release -L ci-core -E '^stdlib$'` and
  `PONY_DEBUG=1 sh .ci-scripts/i386-stdlib-perpkg.sh release` (release runtime, debug
  codegen)
- `ctest --preset release -R "^full-programs$"` and
  `sh .ci-scripts/i386-stdlib-perpkg.sh release` (release runtime, release codegen)
- `ctest --preset debug -R pony-doc-tests`, `ctest --preset debug -R pony-lint-tests`
  and `ctest --preset debug -R pony-lsp-tests`

Run them from `/build/ponyc` as `pony`. Consult that job when validating a CI-matching
issue.

## Lifecycle

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
cd "$VMDIR"
pgrep -af "[q]emu-system-i386 -name i386-alpine-$SSH_PORT "        # status
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ServerAliveInterval=60 -i vm_key -p "$SSH_PORT" pony@127.0.0.1   # interactive shell
```

Stop the VM gracefully, so the data disk is unmounted cleanly. The commands are joined
with `;`, so a failed `umount` (it fails while a detached build is still writing to
`/build`) still reaches `poweroff`. If QEMU hasn't exited after about a minute, the block
kills it.

```sh
VMDIR=~/vms/i386-alpine; SSH_PORT=2232
cd "$VMDIR"
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=5 -i vm_key -p "$SSH_PORT" root@127.0.0.1 \
  'sync; swapoff /build/swap; umount /build; poweroff'
for i in $(seq 1 30); do
  kill -0 "$(cat qemu.pid 2>/dev/null)" 2>/dev/null || break
  sleep 2
done
if [ -f qemu.pid ] && kill -0 "$(cat qemu.pid)" 2>/dev/null; then
  kill "$(cat qemu.pid)"
else
  pkill -f "[q]emu-system-i386 -name i386-alpine-$SSH_PORT " || true
fi
pgrep -af "[q]emu-system-i386 -name i386-alpine-$SSH_PORT " || echo "VM stopped"
```

To restart later, re-run Steps 3, 4 and 5, then the 32-bit check after Step 5. Steps 1 and
2 don't need repeating unless `i386-alpine.env` changed (see Step 1). Step 5 skips the format and the swap-file creation because the
disk already has both, and it reinstalls the packages and recreates the `pony` user that
the reboot erased. The source, the LLVM build and the ponyc builds under `/build/ponyc`
are still there; re-run Step 6's `rsync` if the source changed on the host.

## How this relates to CI (don't re-add the CI-only steps)

This mirrors `.ci-scripts/linux32/i386-provision.bash` (the source of truth; keep this
skill in sync if it changes) and reads its Alpine pins from
`.ci-scripts/linux32/i386-alpine.env`. The local setup deliberately differs from CI, and
each difference is safe:

- Skips CI host-bootstrap (free disk space, `apt-get` qemu, make `/dev/kvm` writable);
  your host already has qemu and KVM (Step 0 checks for both).
- Persistent VM instead of built-fresh-and-killed per run: the data disk is kept, so
  `mkfs.ext4` and the swap-file creation are guarded, and the restart path re-runs the
  root setup. The `pony` user gets a fixed uid so `/build` ownership stays right.
- Graceful stop (`sync`, `swapoff`, `umount /build`, `poweroff`) instead of CI's `pkill`,
  because the data disk persists here and CI's is thrown away.
- Step 5 passes the repository URL as a positional argument into a quoted heredoc. CI
  uses an unquoted heredoc and lets the host expand it; quoting keeps everything else in
  the block from expanding on the host by accident.
- No `ponyc-i386-vm` host alias in `~/.ssh/config`. CI writes one; this skill passes the
  same options as plain `ssh` flags, so it doesn't change the user's ssh config. The
  options add `-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR` like CI, because the
  guest's host keys change every boot.
- Configurable ports (default ssh 2232, http 8099; CI uses 2222 and 8099), a `-name` that
  ends in the ssh port, `-pidfile`, and an http server started with `nohup` and stopped
  through `http.pid`.
- `-smp 8,sockets=8,cores=1,threads=1` for build speed (CI uses 4); the same `-m 3500M`.
- `rsync --delete --exclude='/build'` to skip the host's build artifacts (CI's fresh
  checkout has none) and to drop files deleted on the host since the last sync.
- No GHCR libs cache (that's token-gated CI plumbing); you run
  `cmake -P lib/build-libs.cmake` once.
- The `check-32bit` checks from `.ci-scripts/linux32/i386-libs-cache.bash` are inlined
  (after Step 5, and after the libs build) instead of run through that script, which needs
  the CI ssh alias.

The BSD tier-3 VMs have their own skills (`create-freebsd-dev-env`,
`create-openbsd-dev-env`, `create-dragonfly-dev-env`), and 32-bit ARM is covered by
`32-bit-support-validation`. This skill is i386-only; load one of those if you need a
different platform.
