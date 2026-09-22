#!/usr/bin/env bash
#
# Provision a DragonFly BSD 6.4.2 VM on the GitHub Actions host (qemu), install
# the ponyc build dependencies, and rsync the checkout into it. Shared by the
# libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# ../libs-cache/README.md for how the two call it. Reads
# GITHUB_WORKSPACE from the environment; leaves a booted VM at
# 'ssh -i vm_key -p 2222 root@localhost' with the source under /build/ponyc.
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

echo "::group::Download DragonFly BSD image"
curl -L -o "$VM_ARTIFACTS/dfly.img.bz2" \
  "https://mirror-master.dragonflybsd.org/iso-images/dfly-x86_64-6.4.2_REL.img.bz2"
bunzip2 "$VM_ARTIFACTS/dfly.img.bz2"
qemu-img convert -f raw -O qcow2 \
  "$VM_ARTIFACTS/dfly.img" "$VM_ARTIFACTS/dfly.qcow2"
rm "$VM_ARTIFACTS/dfly.img"
qemu-img resize "$VM_ARTIFACTS/dfly.qcow2" 60G
# Second disk for build workspace (root partition is only ~1.8GB)
qemu-img create -f qcow2 "$VM_ARTIFACTS/dfly-data.qcow2" 50G
echo "::endgroup::"

echo "::group::Download DragonFly BSD ISO"
# The raw image lacks /usr/include (system headers). Extract
# them from the ISO which contains the full base system.
curl -L -o "$VM_ARTIFACTS/dfly.iso.bz2" \
  "https://mirror-master.dragonflybsd.org/iso-images/dfly-x86_64-6.4.2_REL.iso.bz2"
bunzip2 "$VM_ARTIFACTS/dfly.iso.bz2"
mkdir -p "$VM_ARTIFACTS/dfly-iso"
sudo mount -o loop,ro "$VM_ARTIFACTS/dfly.iso" "$VM_ARTIFACTS/dfly-iso"
tar cf "$VM_ARTIFACTS/dfly-include.tar" -C "$VM_ARTIFACTS/dfly-iso/usr" include
sudo umount "$VM_ARTIFACTS/dfly-iso"
rm "$VM_ARTIFACTS/dfly.iso"
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -t ed25519 -f vm_key -N ""

# Create a seed ISO with the SSH key and a setup script.  The VM mounts this
# as a CD-ROM, so the sendkey bootstrap only has to type three short commands
# (login, mount, run script) instead of piping dozens of characters through
# the fragile VGA sendkey path.
cat > "$VM_ARTIFACTS/setup.sh" <<'SETUP'
#!/bin/sh
set -e
if pgrep -x sshd >/dev/null 2>&1; then
  exit 0
fi
dhclient vtnet0
echo "PermitRootLogin yes" >> /etc/ssh/sshd_config
echo "PermitEmptyPasswords yes" >> /etc/ssh/sshd_config
mkdir -p /root/.ssh && chmod 700 /root/.ssh
cp /mnt/authorized_keys /root/.ssh/authorized_keys
chmod 600 /root/.ssh/authorized_keys
ssh-keygen -A
/usr/sbin/sshd
SETUP
cp vm_key.pub "$VM_ARTIFACTS/authorized_keys"
genisoimage -output "$VM_ARTIFACTS/seed.iso" -volid CIDATA -joliet -rock \
  "$VM_ARTIFACTS/setup.sh" "$VM_ARTIFACTS/authorized_keys"
echo "::endgroup::"

echo "::group::Boot DragonFly BSD VM"
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 12G \
  -drive file="$VM_ARTIFACTS/dfly.qcow2",format=qcow2,if=virtio \
  -drive file="$VM_ARTIFACTS/dfly-data.qcow2",format=qcow2,if=virtio \
  -drive file="$VM_ARTIFACTS/seed.iso",media=cdrom \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -object rng-random,id=rng0,filename=/dev/urandom \
  -device virtio-rng-pci,rng=rng0 \
  -monitor unix:"$VM_ARTIFACTS/dfly-monitor.sock",server,nowait \
  -serial unix:"$VM_ARTIFACTS/dfly-serial.sock",server,nowait \
  -display none \
  -daemonize
echo "::endgroup::"

echo "::group::Configure and wait for VM"
# dfly_configure_vm.py detects when boot finishes (VGA screendump stability),
# types login + mount + setup via sendkey, then verifies SSH is reachable.
# The setup script lives on the seed ISO created above.
DFLY_MONITOR_SOCK="$VM_ARTIFACTS/dfly-monitor.sock" \
  DFLY_ARTIFACTS_DIR="$VM_ARTIFACTS" \
  python3 .ci-scripts/bsd/dfly_configure_vm.py
echo "SSH available"
echo "::endgroup::"

echo "::group::Mount build disk"
# The root partition is too small for the build, so it uses the second disk
# (vbd1). newfs runs here over ssh, once, rather than being blind-typed into the
# console on each configure attempt, where a retype could newfs an already-mounted
# disk.
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 root@localhost /bin/sh <<'EOF'
set -e
newfs /dev/vbd1
mkdir -p /build
mount /dev/vbd1 /build
EOF
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
  "$VM_ARTIFACTS/dfly-include.tar" root@localhost:/build/
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

echo "::group::List files in VM"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 root@localhost /bin/sh <<'EOF'
ls -la /build/ponyc/
EOF
echo "::endgroup::"