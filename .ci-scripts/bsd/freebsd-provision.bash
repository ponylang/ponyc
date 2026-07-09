#!/usr/bin/env bash
#
# Provision a FreeBSD VM on the GitHub Actions host (qemu), install the ponyc
# build dependencies, and rsync the checkout into it. Shared by the libs-cache
# warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# ../libs-cache/README.md for how the two call it. Reads
# FREEBSD_VERSION (e.g. 15.1) and GITHUB_WORKSPACE from the environment; leaves
# a booted VM at 'ssh -i vm_key -p 2222 freebsd@localhost' with the source under
# /home/freebsd/ponyc.
set -euo pipefail

: "${FREEBSD_VERSION:?set FREEBSD_VERSION, e.g. 15.1}"

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
sudo apt-get install -y -q qemu-utils qemu-system-x86 cloud-image-utils expect
sudo chmod 666 /dev/kvm
echo "::endgroup::"

echo "::group::Download FreeBSD image"
curl -L -o "$VM_ARTIFACTS/freebsd.qcow2.xz" \
  "https://download.freebsd.org/releases/VM-IMAGES/${FREEBSD_VERSION}-RELEASE/amd64/Latest/FreeBSD-${FREEBSD_VERSION}-RELEASE-amd64-BASIC-CLOUDINIT-ufs.qcow2.xz"
xz -d "$VM_ARTIFACTS/freebsd.qcow2.xz"
qemu-img resize "$VM_ARTIFACTS/freebsd.qcow2" 60G
echo "::endgroup::"

echo "::group::Prepare VM access"
ssh-keygen -t ed25519 -f vm_key -N ""
PUB_KEY=$(cat vm_key.pub)

# nuageinit seed: SSH key for freebsd, set root password for su access
cat > "$VM_ARTIFACTS/user-data" <<USERDATA
#cloud-config
ssh_authorized_keys:
  - ${PUB_KEY}
chpasswd:
  expire: false
  list:
    - root:ciroot
USERDATA
cat > "$VM_ARTIFACTS/meta-data" <<METADATA
instance-id: freebsd-ci
local-hostname: freebsd-ci
METADATA
cloud-localds "$VM_ARTIFACTS/seed.img" \
  "$VM_ARTIFACTS/user-data" "$VM_ARTIFACTS/meta-data"
echo "::endgroup::"

echo "::group::Boot FreeBSD VM"
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 12G \
  -drive file="$VM_ARTIFACTS/freebsd.qcow2",format=qcow2,if=virtio \
  -drive file="$VM_ARTIFACTS/seed.img",format=raw,if=virtio \
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
send "pkg install -y cmake gmake libunwind git python3 rsync doas valgrind\r"
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

echo "::group::List files in VM"
ssh -o StrictHostKeyChecking=no -i vm_key -p 2222 freebsd@localhost <<'EOF'
ls -la /home/freebsd/ponyc/
EOF
echo "::endgroup::"