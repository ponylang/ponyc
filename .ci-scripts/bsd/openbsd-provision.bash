#!/usr/bin/env bash
#
# Provision an OpenBSD 7.9 VM on the GitHub Actions host (qemu), install the
# ponyc build dependencies, and rsync the checkout into it. Shared by the
# libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# .known-couplings/ghcr-libs-cache.md for how the two call it. Reads
# GITHUB_WORKSPACE from the environment; leaves a booted VM at
# 'ssh -i vm_key -p 2222 openbsd@localhost' with the source under /build/ponyc.
set -euo pipefail

# VM disk images and other scratch artifacts go here, outside the checkout. The
# "Copy source to VM" rsync below copies all of "$GITHUB_WORKSPACE/" into the
# guest, so anything left in the checkout gets copied in too; keeping the big
# qcow2 images out of it is the point (issue #5709). vm_key stays in the
# checkout so the later `-i vm_key` ssh steps still find it there.
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
sudo apt-get install -y -q qemu-utils qemu-system-x86 genisoimage
sudo chmod 666 /dev/kvm
echo "::endgroup::"

echo "::group::Download OpenBSD image"
curl -L -o "$VM_ARTIFACTS/openbsd.qcow2" \
  "https://github.com/hcartiaux/openbsd-cloud-image/releases/download/v7.9_2026-06-03-20-46/openbsd-generic.qcow2"
# Build-workspace disk. OpenBSD's default disklabel confines the
# build to /home (~10.8G), which the LLVM 22 build overflows, so
# build on a dedicated disk instead, mirroring the DragonFly job.
qemu-img create -f qcow2 "$VM_ARTIFACTS/openbsd-data.qcow2" 50G
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -t ed25519 -f vm_key -N ""
PUB_KEY=$(cat vm_key.pub)

cat > "$VM_ARTIFACTS/user-data" <<USERDATA
#cloud-config
users:
- name: openbsd
  groups: wheel
  ssh_authorized_keys:
  - ${PUB_KEY}
write_files:
- path: /etc/doas.conf
  content: |
    permit nopass keepenv :wheel
  owner: root:wheel
  permissions: '0600'
USERDATA
cat > "$VM_ARTIFACTS/meta-data" <<METADATA
instance-id: openbsd-ci
local-hostname: openbsd-ci
METADATA
genisoimage -output "$VM_ARTIFACTS/seed.iso" -volid cidata -joliet -rock \
  "$VM_ARTIFACTS/user-data" "$VM_ARTIFACTS/meta-data"
echo "::endgroup::"

echo "::group::Boot OpenBSD VM"
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 6G \
  -drive file="$VM_ARTIFACTS/openbsd.qcow2",format=qcow2,if=virtio \
  -drive file="$VM_ARTIFACTS/openbsd-data.qcow2",format=qcow2,if=virtio \
  -drive file="$VM_ARTIFACTS/seed.iso",media=cdrom \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none \
  -daemonize
echo "::endgroup::"

echo "::group::Wait for VM"
timeout 300 bash -c '
  while ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 -i vm_key -p 2222 openbsd@localhost true 2>/dev/null; do
    sleep 2
  done
'
echo "SSH available"
echo "::endgroup::"

echo "::group::Set up build disk"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 openbsd@localhost <<'EOF'
set -e
# The image disk confines builds to /home (~10.8G). Build on the
# dedicated second disk (sd1) instead: one 4.2BSD partition spanning
# the disk, newfs'd and mounted at /build.
printf 'a a\n\n\n\nw\nq\n' | doas disklabel -E sd1
doas newfs /dev/rsd1a
doas mkdir -p /build
doas mount /dev/sd1a /build
doas chown openbsd:wheel /build
EOF
echo "::endgroup::"

echo "::group::Install build dependencies"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 openbsd@localhost \
  "doas pkg_add -u && doas pkg_add cmake gmake git python%3 rsync--"
echo "::endgroup::"

echo "::group::Raise datasize limit"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 openbsd@localhost \
  "doas sed -i 's/datasize-max=1536M/datasize-max=4096M/' /etc/login.conf && doas sed -i 's/datasize-cur=1536M/datasize-cur=4096M/' /etc/login.conf"
echo "::endgroup::"

echo "::group::Copy source to VM"
rsync -az -e "ssh -o StrictHostKeyChecking=no -i vm_key -p 2222" \
  "$GITHUB_WORKSPACE/" openbsd@localhost:/build/ponyc/
echo "::endgroup::"

echo "::group::List files in VM"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 openbsd@localhost <<'EOF'
ls -la /build/ponyc/
EOF
echo "::endgroup::"