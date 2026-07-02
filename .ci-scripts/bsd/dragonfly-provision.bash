#!/usr/bin/env bash
#
# Provision a DragonFly BSD 6.4.2 VM on the GitHub Actions host (qemu), install
# the ponyc build dependencies, and rsync the checkout into it. Shared by the
# libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# .known-couplings/ghcr-libs-cache.md for how the two call it. Reads
# GITHUB_WORKSPACE from the environment; leaves a booted VM at
# 'ssh -i vm_key -p 2222 root@localhost' with the source under /build/ponyc.
set -euo pipefail

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

echo "::group::Download DragonFly BSD image"
curl -L -o dfly.img.bz2 \
  "https://mirror-master.dragonflybsd.org/iso-images/dfly-x86_64-6.4.2_REL.img.bz2"
bunzip2 dfly.img.bz2
qemu-img convert -f raw -O qcow2 dfly.img dfly.qcow2
rm dfly.img
qemu-img resize dfly.qcow2 60G
# Second disk for build workspace (root partition is only ~1.8GB)
qemu-img create -f qcow2 dfly-data.qcow2 50G
echo "::endgroup::"

echo "::group::Download DragonFly BSD ISO"
# The raw image lacks /usr/include (system headers). Extract
# them from the ISO which contains the full base system.
curl -L -o dfly.iso.bz2 \
  "https://mirror-master.dragonflybsd.org/iso-images/dfly-x86_64-6.4.2_REL.iso.bz2"
bunzip2 dfly.iso.bz2
mkdir -p dfly-iso
sudo mount -o loop,ro dfly.iso dfly-iso
tar cf dfly-include.tar -C dfly-iso/usr include
sudo umount dfly-iso
rm dfly.iso
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -t ed25519 -f vm_key -N ""
echo "::endgroup::"

echo "::group::Boot DragonFly BSD VM"
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 12G \
  -drive file=dfly.qcow2,format=qcow2,if=virtio \
  -drive file=dfly-data.qcow2,format=qcow2,if=virtio \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -object rng-random,id=rng0,filename=/dev/urandom \
  -device virtio-rng-pci,rng=rng0 \
  -monitor unix:dfly-monitor.sock,server,nowait \
  -display none \
  -daemonize
echo "::endgroup::"

echo "::group::Configure VM"
# Declare then export PUB_KEY separately (SC2155) so dfly_configure_vm.py can
# read the VM ssh public key from the environment.
PUB_KEY="$(cat vm_key.pub)"
export PUB_KEY

# Wait for the VM to boot
sleep 90

# DragonFly BSD raw images boot to a login prompt with root having no password
# and no cloud-init support; dfly_configure_vm.py types the setup commands into
# the VGA console via the QEMU monitor, reading the key from PUB_KEY.
python3 .ci-scripts/bsd/dfly_configure_vm.py
echo "::endgroup::"

echo "::group::Wait for VM"
timeout 300 bash -c '
  while ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 -i vm_key -p 2222 root@localhost true 2>/dev/null; do
    sleep 2
  done
'
echo "SSH available"
echo "::endgroup::"

echo "::group::Install build dependencies"
# Move /usr/local to /build disk before installing packages.
# The root partition is only ~1.8GB and gcc13 alone needs 418MB.
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 root@localhost /bin/sh <<'EOF'
set -e
cpdup /usr/local /build/usr_local
rm -rf /usr/local
ln -s /build/usr_local /usr/local
EOF

# Install system headers from ISO (raw image ships without /usr/include)
scp -o StrictHostKeyChecking=no -i vm_key -P 2222 \
  dfly-include.tar root@localhost:/build/
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 root@localhost /bin/sh <<'EOF'
set -e
tar xf /build/dfly-include.tar -C /build
ln -s /build/include /usr/include
rm /build/dfly-include.tar
EOF

# Install packages (now goes to /build disk via symlink)
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 root@localhost /bin/sh <<'EOF'
set -e
pkg install -y cmake gmake git python3 cxx_atomics rsync gcc13 ca_root_nss
pkg clean -ay
git config --global --add safe.directory /build/ponyc
EOF
echo "::endgroup::"

echo "::group::Copy source to VM"
rsync -az -e "ssh -o StrictHostKeyChecking=no -i vm_key -p 2222" \
  "$GITHUB_WORKSPACE/" root@localhost:/build/ponyc/
echo "::endgroup::"
