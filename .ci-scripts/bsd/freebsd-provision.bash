#!/usr/bin/env bash
#
# Provision a FreeBSD VM on the GitHub Actions host (qemu), install the ponyc
# build dependencies, and rsync the checkout into it. Shared by the libs-cache
# warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# .github/workflows/AGENTS.md for how the two call it. Reads FREEBSD_VERSION
# (e.g. 14.3) and GITHUB_WORKSPACE from the environment; leaves a booted VM at
# 'ssh -i vm_key -p 2222 freebsd@localhost' with the source under
# /home/freebsd/ponyc.
set -euo pipefail

: "${FREEBSD_VERSION:?set FREEBSD_VERSION, e.g. 14.3}"

echo "::group::Free disk space"
sudo rm -rf /usr/share/dotnet
sudo rm -rf /usr/local/lib/android
sudo rm -rf /opt/hostedtoolcache
df -h /
echo "::endgroup::"

echo "::group::Install QEMU"
sudo apt-get update -q
sudo apt-get install -y -q qemu-utils qemu-system-x86 cloud-image-utils expect
sudo chmod 666 /dev/kvm
echo "::endgroup::"

echo "::group::Download FreeBSD image"
curl -L -o freebsd.qcow2.xz \
  "https://download.freebsd.org/releases/VM-IMAGES/${FREEBSD_VERSION}-RELEASE/amd64/Latest/FreeBSD-${FREEBSD_VERSION}-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz"
xz -d freebsd.qcow2.xz
qemu-img resize freebsd.qcow2 60G
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -t ed25519 -f vm_key -N ""
PUB_KEY=$(cat vm_key.pub)

# nuageinit seed: SSH key for freebsd, set root password for su access
cat > user-data <<USERDATA
#cloud-config
ssh_authorized_keys:
  - ${PUB_KEY}
chpasswd:
  expire: false
  list:
    - root:ciroot
USERDATA
cat > meta-data <<METADATA
instance-id: freebsd-ci
local-hostname: freebsd-ci
METADATA
cloud-localds seed.img user-data meta-data
echo "::endgroup::"

echo "::group::Boot FreeBSD VM"
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 12G \
  -drive file=freebsd.qcow2,format=qcow2,if=virtio \
  -drive file=seed.img,format=raw,if=virtio \
  -netdev user,id=net0,hostfwd=tcp::2222-:22 \
  -device virtio-net-pci,netdev=net0 \
  -display none \
  -daemonize
echo "::endgroup::"

echo "::group::Wait for VM"
timeout 300 bash -c '
  while ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 -i vm_key -p 2222 freebsd@localhost true 2>/dev/null; do
    sleep 2
  done
'
echo "SSH available"
echo "::endgroup::"

echo "::group::Install build dependencies"
expect <<'EXPECT'
set timeout 600
spawn ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 -t freebsd@localhost su -m root
expect {
  "Password:" { send "ciroot\r" }
  timeout { puts "Timeout waiting for password prompt"; exit 1 }
}
expect {
  "#" {}
  "su: Sorry" { puts "su failed - wrong password or nuageinit didn't set it"; exit 1 }
  timeout { puts "Timeout waiting for root shell"; exit 1 }
}
send "pkg install -y cmake gmake libunwind git python3 rsync doas\r"
expect {
  "#" {}
  timeout { puts "Timeout during pkg install"; exit 1 }
}
# Let the freebsd user load the dtrace kernel module and run dtrace
# non-interactively for the use=dtrace probe-firing smoke test.
send "echo 'permit nopass freebsd' > /usr/local/etc/doas.conf\r"
expect {
  "#" {}
  timeout { puts "Timeout writing doas.conf"; exit 1 }
}
send "exit\r"
expect eof
EXPECT
echo "::endgroup::"

echo "::group::Copy source to VM"
rsync -az -e "ssh -o StrictHostKeyChecking=no -i vm_key -p 2222" \
  "$GITHUB_WORKSPACE/" freebsd@localhost:/home/freebsd/ponyc/
echo "::endgroup::"
