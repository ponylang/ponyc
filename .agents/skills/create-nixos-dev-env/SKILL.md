# Create a NixOS dev/test VM for ponyc

Stand up a local NixOS VM that matches the ponyc tier-3 NixOS CI job, so you can build
and test ponyc with the same GCC and glibc versions CI uses.

The NixOS tier-3 job exercises a gcc-on-Linux path that the main clang-on-Linux CI never
touches: Nix's own glibc, Nix's GCC (not the host's), and the Nix store dynamic linker.
This VM is the authoritative local environment for **NixOS-platform** ponyc issues. (For
clang-specific or BSD issues, use the FreeBSD, DragonFly, or OpenBSD skills instead.)

The CI runs `nix develop` on an Ubuntu runner; this VM runs NixOS itself with the same
`nix develop` shell from the ponyc flake. The flake pins `nixos-26.05`, so the toolchain
(GCC version, glibc version) matches CI exactly.

Run the repo-relative commands (`rsync` of the source) from your ponyc checkout root.
The VM itself lives in a separate directory (`$VMDIR`).

**Each fenced `sh` block below is self-contained.** It re-sets `VMDIR` (and, where
needed, the `SSH` command) at its top, because an agent runs each block as a fresh
shell — environment variables and the working directory do NOT carry from one block to
the next. Do not "optimize" by setting them once and dropping them from later blocks: an
empty `$SSH` would make a `$SSH /bin/sh <<'EOF' ... EOF` heredoc run **on your host**
(silently) instead of in the VM. Keep every block whole.

## Gotchas that will bite you (read first)

- **Per-block fresh shells.** See above: re-establish `VMDIR`/`SSH` in every block; never
  let `$SSH` be empty in front of a heredoc.
- **Two-phase setup.** First boot is from the NixOS ISO (live environment); you install
  NixOS to the disk via SSH, then reboot from the installed disk. The lifecycle restart
  command boots from disk, not the ISO.
- **The NixOS ISO auto-logs in as `nixos`, not `root`.** The `nixos` user has passwordless
  sudo. The console setup script uses `sudo` for all privileged commands. Root has no
  password and no authorized_keys until the seed ISO's setup script copies the key.
- **Port 2222 may be in use.** Check whether another VM or service is listening on port
  2222 before booting. If it is, use a different port (e.g. 2223) in every `hostfwd` and
  `-p` argument throughout. The SSH port must be consistent across all steps.
- **`nix develop` downloads on first use.** The first `nix develop --no-update-lock-file`
  in the VM downloads the entire toolchain (~1-2 GB) from the Nix binary cache. This is a
  one-time cost; subsequent invocations are instant.
- **Use `x86-64-debug`/`x86-64-release` presets.** Always pass
  `-DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++` to cmake configure commands, exactly
  as CI does. The standard `debug`/`release` presets default to clang, which won't work.
- **Detach long in-VM builds.** The libs build (LLVM) takes hours; run it detached and
  poll the log, so an ssh drop doesn't kill it.
- **Don't re-run Step 6 after installation.** `parted`/`mkfs` erase the disk. After
  installation and reboot, the disk is your persistent NixOS system.

## Step 0 — verify prerequisites (do NOT assume they're installed)

This skill needs, on the host:

- an existing ponyc checkout (you run the `rsync` from it)
- hardware-accelerated virtualization for QEMU (KVM on Linux, HVF on macOS)
- enough free disk for the VM's sparse qcow2 disk (provisioned at 60 GB nominal; it grows
  only as used), ~1 GB for the ISO, and network access to channels.nixos.org
- `qemu-system-x86_64` and `qemu-img`
- `genisoimage` (or `mkisofs`) for the seed ISO
- `rsync`, `git`, `curl`, `python3`
- an OpenSSH client (`ssh`, `scp`, `ssh-keygen`)

```sh
missing=""
for t in qemu-system-x86_64 qemu-img genisoimage rsync git ssh scp ssh-keygen curl python3; do
  command -v "$t" >/dev/null 2>&1 || missing="$missing $t"
done
case "$(uname -s)" in
  Linux)  [ -w /dev/kvm ] || missing="$missing kvm(/dev/kvm-not-writable)" ;;
  Darwin) qemu-system-x86_64 -accel help 2>&1 | grep -q hvf || missing="$missing hvf-accelerator" ;;
  *)      echo "NOTE: confirm this host has a QEMU hardware accelerator and adjust the boot accel= below" ;;
esac
[ -n "$missing" ] && echo "MISSING:$missing" || echo "all prerequisites present"
```

If anything is missing, tell the user and ask them to install it. A missing hardware
accelerator is a host/BIOS/virtualization fix only the user can make; don't fall back to
TCG emulation.

## Setup

### 1. Download the NixOS minimal ISO (one time)

The ISO version must match the flake's nixpkgs pin — currently `nixos-26.05`. Check
`flake.nix` if unsure.

```sh
VMDIR=~/vms/nixos-26.05
mkdir -p "$VMDIR" && cd "$VMDIR"
curl -fL --retry 3 -o nixos.iso \
  "https://channels.nixos.org/nixos-26.05/latest-nixos-minimal-x86_64-linux.iso"
```

### 2. Create the disk + SSH key + seed ISO

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
qemu-img create -f qcow2 nixos.qcow2 60G
test -f vm_key || ssh-keygen -t ed25519 -f vm_key -N "" -q
cat > setup.sh <<'SETUP'
#!/bin/bash
set -e
mkdir -p /root/.ssh
cp /tmp/seed/authorized_keys /root/.ssh/authorized_keys
chmod 700 /root/.ssh
chmod 600 /root/.ssh/authorized_keys
echo "SSH key installed"
SETUP
cp vm_key.pub authorized_keys
genisoimage -output seed.iso -volid CIDATA -joliet -rock setup.sh authorized_keys
```

### 3. Boot from the ISO (first boot only)

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
qemu-system-x86_64 \
  -name nixos-dev \
  -machine pc,accel=kvm:hvf -cpu host -smp 8 -m 12G \
  -drive file=nixos.qcow2,format=qcow2,if=virtio \
  -drive file=nixos.iso,media=cdrom,readonly=on \
  -drive file=seed.iso,media=cdrom,readonly=on \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -monitor unix:nixos-monitor.sock,server,nowait \
  -serial unix:nixos-serial.sock,server,nowait \
  -display none -pidfile nixos.pid -daemonize
```

(`accel=kvm:hvf` picks KVM on Linux or HVF on macOS and errors if neither is available —
it never silently falls back to slow TCG. `-smp`/`-m` are speed knobs; CI uses 4 CPUs /
12G. `hostfwd 2222->22` is the ssh port — to run this alongside a BSD VM, give each a
distinct port and pass that port to every ssh/rsync command. The `pgrep -f`/`pkill -f`
commands in Lifecycle spell the pattern `[q]emu-system-x86_64` so they can't match the
shell running them.)

### 4. Run the console setup script

The NixOS ISO starts sshd at boot but root has no authorized_keys. This script logs in
via QEMU sendkey, mounts the seed ISO, and copies the key. Copy the script from the skill
directory and run it:

```sh
repo_root=$(git rev-parse --show-toplevel)
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
cp "$repo_root/.claude/skills/create-nixos-dev-env/nixos_configure_vm.py" "$VMDIR/"
python3 nixos_configure_vm.py
```

If SSH doesn't come up, the script saves a VGA screendump to `nixos-console.ppm`. Convert
it to PNG for viewing (`magick nixos-console.ppm nixos-console.png` or
`ffmpeg -y -loglevel error -i nixos-console.ppm nixos-console.png`).

### 5. Wait for SSH (belt-and-suspenders)

Step 4 already verifies SSH. This confirms it:

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR \
  -i vm_key -p 2222 root@localhost uname -a
```

### 6. Install NixOS to the disk (one time, via SSH)

This partitions the disk, writes a NixOS configuration, and runs `nixos-install`. The
install downloads packages from the binary cache (~5-15 min depending on network speed).

```sh
VMDIR=~/vms/nixos-26.05
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
PUBKEY=$(cat "$VMDIR/vm_key.pub")
$SSH /bin/sh <<NIXINSTALL
set -e

echo "=== Partitioning ==="
parted /dev/vda -- mklabel msdos
parted /dev/vda -- mkpart primary ext4 1MiB 100%
mkfs.ext4 -L nixos /dev/vda1

echo "=== Mounting ==="
mount /dev/vda1 /mnt

echo "=== Generating hardware config ==="
nixos-generate-config --root /mnt

echo "=== Writing configuration.nix ==="
cat > /mnt/etc/nixos/configuration.nix <<'NIXCFG'
{ config, pkgs, ... }:
{
  imports = [ ./hardware-configuration.nix ];

  boot.loader.grub.enable = true;
  boot.loader.grub.device = "/dev/vda";
  boot.kernelParams = [ "console=ttyS0,115200n8" "console=tty0" ];

  networking.hostName = "nixos-dev";

  services.openssh = {
    enable = true;
    settings.PermitRootLogin = "prohibit-password";
  };

  users.users.root.openssh.authorizedKeys.keys = [
    "PUBKEY_PLACEHOLDER"
  ];

  environment.systemPackages = with pkgs; [ git rsync ];

  nix.settings.experimental-features = [ "nix-command" "flakes" ];

  system.stateVersion = "26.05";
}
NIXCFG
sed -i "s|PUBKEY_PLACEHOLDER|${PUBKEY}|" /mnt/etc/nixos/configuration.nix

echo "=== Running nixos-install ==="
nixos-install --no-root-passwd

echo "=== Installation complete ==="
NIXINSTALL
```

### 7. Reboot from the installed disk

Stop the VM, then re-launch without the ISO drives:

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
kill "$(cat nixos.pid)" 2>/dev/null || pkill -f '[q]emu-system-x86_64 -name nixos-dev'
sleep 2
qemu-system-x86_64 \
  -name nixos-dev \
  -machine pc,accel=kvm:hvf -cpu host -smp 8 -m 12G \
  -drive file=nixos.qcow2,format=qcow2,if=virtio \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -serial unix:nixos-serial.sock,server,nowait \
  -display none -pidfile nixos.pid -daemonize
```

### 8. Wait for SSH on the installed system

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
up=""
for i in $(seq 1 90); do
  if ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -o ConnectTimeout=2 -i vm_key -p 2222 \
       root@localhost true 2>/dev/null; then up=1; echo "SSH up"; break; fi
  sleep 2
done
[ -n "$up" ] || echo "SSH NEVER CAME UP — check serial console: socat - UNIX-CONNECT:$VMDIR/nixos-serial.sock"
```

### 9. Rsync the ponyc checkout in

Run from your ponyc checkout. Exclude the top-level `build/` (host artifacts; the VM
builds its own). Keep `.git` (CMake runs `git rev-parse`). The vendored LLVM submodule
under `lib/llvm/src` IS transferred — the VM needs it for the libs build.

```sh
VMDIR=~/vms/nixos-26.05
rsync -az --exclude='/build' \
  -e "ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222" \
  "$(git rev-parse --show-toplevel)/" root@localhost:/root/ponyc/
```

After the first rsync, tell git the directory is safe (libgit2 inside `nix develop`
rejects repos owned by a different user):

```sh
VMDIR=~/vms/nixos-26.05
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH 'git config --global --add safe.directory /root/ponyc'
```

## Using the VM

All build commands run inside `nix develop`. The first invocation downloads the toolchain
from the Nix binary cache (~1-2 GB, one-time). The devShell provides GCC, cmake, and all
build dependencies — no manual package installation needed.

### Build the vendored LLVM libraries (one time)

This is the multi-hour long pole — run it detached:

```sh
VMDIR=~/vms/nixos-26.05
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
set -e
cat > /root/run-libs.sh <<'S'
#!/bin/sh
cd /root/ponyc
nix develop --no-update-lock-file --command \
  bash -c 'cmake -DJOBS=4 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake'
echo "libs DONE rc=$?"
S
chmod +x /root/run-libs.sh
nohup /root/run-libs.sh > /root/libs-build.log 2>&1 &
echo "libs building pid $!"
EOF
# poll until done: $SSH /bin/sh -c 'tail -3 /root/libs-build.log'  -> wait for "libs DONE rc=0"
```

### Build and test ponyc

Do not run this until `libs DONE rc=0`:

```sh
VMDIR=~/vms/nixos-26.05
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
set -e
cd /root/ponyc
nix develop --no-update-lock-file --command bash <<'NIX'
set -e
cmake --preset x86-64-debug \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
cmake --build --preset x86-64-debug
PONY_DEBUG=1 ctest --preset x86-64-debug -L ci-core --output-on-failure
NIX
EOF
```

### Run individual tests

```sh
VMDIR=~/vms/nixos-26.05
SSH="ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i $VMDIR/vm_key -p 2222 root@localhost"
$SSH /bin/sh <<'EOF'
cd /root/ponyc
nix develop --no-update-lock-file --command bash -c \
  'ctest --preset x86-64-debug -R libponyrt.tests --output-on-failure'
EOF
```

### Iterate on a fix

Edit on the host, re-run Step 9's `rsync` (incremental), then rebuild and re-test.

## Lifecycle

```sh
VMDIR=~/vms/nixos-26.05; cd "$VMDIR"
pgrep -af '[q]emu-system-x86_64 -name nixos-dev'                     # status
ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o LogLevel=ERROR -i vm_key -p 2222 root@localhost   # interactive shell
kill "$(cat nixos.pid)" 2>/dev/null || pkill -f '[q]emu-system-x86_64 -name nixos-dev'   # stop
```

To restart later, re-run the Step 7 boot command (without the ISO drives). The disk and
Nix store persist — no re-setup needed. SSH works immediately on reboot.

## How this relates to CI (don't re-add the CI-only steps)

This mirrors the `nixos-linux` job in `.github/workflows/ponyc-tier3.yml` and
`.ci-scripts/nix/nix-build-and-test.sh` (the sources of truth — keep this skill in sync
if they change). The local setup deliberately differs from CI:

- Full NixOS VM instead of `nix develop` on Ubuntu — same toolchain, different host.
- Persistent VM instead of built-fresh per run.
- Skips the libs cache restore/push (GHCR token-gated CI plumbing) — LLVM is built once
  locally.
- Skips the link-smoke test (the `readelf` PT_INTERP check in `nix-build-and-test.sh`)
  — that validates installed-package behavior, not the build/test cycle.
- ssh adds `-o UserKnownHostsFile=/dev/null -o LogLevel=ERROR` to quiet host-key
  warnings from a persistent VM reused on `localhost:2222`.
- `-cpu host` for native speed (CI uses the runner's default).
- `-smp 8` for build speed (CI uses the runner's default, typically 4).
- `-name`/`-pidfile` for lifecycle commands (CI's ephemeral runner doesn't need them).
- `rsync --exclude='/build'` to skip the host's build artifacts.

The FreeBSD, DragonFly, and OpenBSD skills cover their respective platforms. This skill
is NixOS-only; load a sibling if you need one of those.
