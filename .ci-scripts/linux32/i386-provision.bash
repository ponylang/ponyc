#!/usr/bin/env bash
#
# Provision a 32-bit x86 Alpine Linux VM on the GitHub Actions host (qemu), install
# the ponyc build dependencies, and rsync the checkout into it. Reads
# GITHUB_WORKSPACE from the environment; leaves a booted VM reachable as
# `ssh ponyc-i386-vm` (a host alias this script adds to ~/.ssh/config) with the
# source under /build/ponyc.
#
# The VM boots Alpine's 32-bit netboot kernel directly (no installer, no disk
# image): the root filesystem is a tmpfs, and builds go on a separate data disk
# mounted at /build, which also carries swap.
set -euo pipefail

# shellcheck source-path=SCRIPTDIR source=i386-alpine.env disable=SC1091
. "$(dirname "$0")/i386-alpine.env"
NETBOOT="alpine-netboot-${ALPINE_RELEASE}-x86.tar.gz"
NETBOOT_URL="https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}/releases/x86/${NETBOOT}"
REPO_URL="https://dl-cdn.alpinelinux.org/alpine/${ALPINE_BRANCH}"
HTTP_PORT=8099

# VM disk images, the ssh key, and other scratch artifacts go here, outside the
# checkout. The "Copy source to VM" rsync below copies all of "$GITHUB_WORKSPACE/"
# into the guest, so anything left in the checkout gets copied in too
# (issue #5709).
VM_ARTIFACTS="${RUNNER_TEMP:-$(dirname "$GITHUB_WORKSPACE")}/vm-artifacts"
mkdir -p "$VM_ARTIFACTS"

echo "::group::Free disk space"
sudo rm -rf /usr/share/dotnet
sudo rm -rf /usr/local/lib/android
sudo rm -rf /opt/hostedtoolcache
df -h /
echo "::endgroup::"

echo "::group::Install QEMU"
sudo apt-get update -q
sudo apt-get install -y -q qemu-utils qemu-system-x86
sudo chmod 666 /dev/kvm
echo "::endgroup::"

echo "::group::Download Alpine netboot"
curl -fsSL -o "$VM_ARTIFACTS/$NETBOOT" "$NETBOOT_URL"
echo "${NETBOOT_SHA256}  $VM_ARTIFACTS/$NETBOOT" | sha256sum -c -
mkdir -p "$VM_ARTIFACTS/netboot"
tar -xzf "$VM_ARTIFACTS/$NETBOOT" -C "$VM_ARTIFACTS/netboot"
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -q -t ed25519 -f "$VM_ARTIFACTS/vm_key" -N ""
# ssh takes the first value it finds for each option, so the alias goes at the
# top of ~/.ssh/config, ahead of any Host * block already there.
mkdir -p ~/.ssh
touch ~/.ssh/config
{ cat <<SSHCONFIG
Host ponyc-i386-vm
  HostName 127.0.0.1
  Port 2222
  User pony
  IdentityFile $VM_ARTIFACTS/vm_key
  StrictHostKeyChecking no
  UserKnownHostsFile /dev/null
  LogLevel ERROR
  ServerAliveInterval 60

SSHCONFIG
  cat ~/.ssh/config
} > "$VM_ARTIFACTS/ssh_config"
cp "$VM_ARTIFACTS/ssh_config" ~/.ssh/config
# Alpine's initramfs fetches the modloop and root's authorized key over http at
# boot (the modloop= and ssh_key= arguments below). Serve them from a directory
# holding only those two files, so the http server never hands out the private
# key. The guest sees the host as 10.0.2.2.
mkdir -p "$VM_ARTIFACTS/http"
cp "$VM_ARTIFACTS/vm_key.pub" "$VM_ARTIFACTS/http/authorized_keys"
cp "$VM_ARTIFACTS/netboot/boot/modloop-lts" "$VM_ARTIFACTS/http/"
python3 -m http.server "$HTTP_PORT" --bind 127.0.0.1 \
  --directory "$VM_ARTIFACTS/http" > "$VM_ARTIFACTS/http.log" 2>&1 &
http_pid=$!
qemu-img create -q -f qcow2 "$VM_ARTIFACTS/data.qcow2" 120G
echo "::endgroup::"

# VM_CONSOLE is the guest's serial console log.
if [ -n "${GITHUB_ENV:-}" ]; then
  echo "VM_CONSOLE=$VM_ARTIFACTS/console.log" >> "$GITHUB_ENV"
fi

echo "::group::Boot Alpine VM"
# coreduo is a 32-bit CPU with SSE2. ponyc unconditionally enables SSE2 for
# 32-bit x86 targets, so the VM needs SSE2 to execute the generated code. KVM
# otherwise reports
# the host's vendor, and LLVM names the same model differently on AMD, so the
# vendor is fixed. One socket per vCPU: with a single multi-core socket this CPU
# model makes the guest report every vCPU as a hyperthread of one core, and the
# Pony runtime counts physical cores. The 32-bit kernel can't use RAM above
# 4 GB, and from 3584M up QEMU's pc machine maps everything past 3 GB above
# 4 GB, so the VM gets 3500M, all of it below 4 GB.
qemu-system-i386 \
  -machine pc,accel=kvm \
  -cpu coreduo,vendor=GenuineIntel \
  -smp 4,sockets=4,cores=1,threads=1 \
  -m 3500M \
  -kernel "$VM_ARTIFACTS/netboot/boot/vmlinuz-lts" \
  -initrd "$VM_ARTIFACTS/netboot/boot/initramfs-lts" \
  -append "console=ttyS0 ip=dhcp alpine_repo=${REPO_URL}/main modloop=http://10.0.2.2:${HTTP_PORT}/modloop-lts ssh_key=http://10.0.2.2:${HTTP_PORT}/authorized_keys" \
  -drive file="$VM_ARTIFACTS/data.qcow2",format=qcow2,if=virtio \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none \
  -serial file:"$VM_ARTIFACTS/console.log" \
  -daemonize
echo "::endgroup::"

echo "::group::Wait for VM"
if ! timeout 300 bash -c "
  while ! ssh -l root -o ConnectTimeout=2 ponyc-i386-vm true 2>/dev/null; do
    sleep 2
  done
"; then
  echo "VM did not come up; its console log is $VM_ARTIFACTS/console.log."
  exit 1
fi
kill "$http_pid"
echo "SSH available"
echo "::endgroup::"

echo "::group::Install build dependencies"
# The netboot system leaves the loopback interface down; the runtime and stdlib
# tests bind to 127.0.0.1.
# shellcheck disable=SC2087
ssh -l root ponyc-i386-vm /bin/sh <<ROOT
set -e
ip link set lo up
printf '%s\n' '${REPO_URL}/main' '${REPO_URL}/community' > /etc/apk/repositories
apk update -q
apk add -q build-base clang clang-dev coreutils linux-headers cmake git \
  zlib-dev bash python3 libressl libressl-dev rsync e2fsprogs ca-certificates
modprobe ext4
mkfs.ext4 -q /dev/vda
mkdir -p /build
mount /dev/vda /build
fallocate -l 8G /build/swap
chmod 600 /build/swap
mkswap /build/swap > /dev/null
swapon /build/swap
adduser -D -s /bin/sh pony
# adduser -D locks the account (password field '!'), and sshd refuses key
# logins to locked accounts. '*' allows no password but isn't locked.
sed -i 's/^pony:!:/pony:*:/' /etc/shadow
mkdir -p /home/pony/.ssh
cp /root/.ssh/authorized_keys /home/pony/.ssh/authorized_keys
chown -R pony:pony /home/pony/.ssh
chown pony:pony /build
uname -m
free -m
# The packages aren't pinned; record the toolchain this run got.
clang --version | head -n 1
apk list -I musl libressl cmake 2>/dev/null
ROOT
echo "::endgroup::"

echo "::group::Copy source to VM"
rsync -az "$GITHUB_WORKSPACE/" ponyc-i386-vm:/build/ponyc/
echo "::endgroup::"
