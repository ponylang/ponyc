#!/usr/bin/env bash
#
# Provision a Haiku VM on the GitHub Actions host (qemu), install
# the ponyc build dependencies, and rsync the checkout into it. Shared by the
# libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# ../libs-cache/README.md for how the two call it. Reads HAIKU_NIGHTLY_VERSION,
# GITHUB_WORKSPACE and RUNNER_TEMP from the environment;
# leaves a booted VM at 'ssh -i haiku_vm_key -p 2222 user@localhost'
# with the source under /Data/ponyc.
set -euo pipefail

: "${HAIKU_NIGHTLY_VERSION:?set HAIKU_NIGHTLY_VERSION, e.g. hrev59860}"
: "${GITHUB_WORKSPACE:?set GITHUB_WORKSPACE, e.g., ./ponyc/}"

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# VM disk images and other scratch artifacts go here, outside the checkout. The
# "Copy source to VM" rsync below copies all of "$GITHUB_WORKSPACE/" into the
# guest, so anything left in the checkout gets copied in too; keeping the big
# qcow2 images out of it is the point (issue #5709). vm_key stays in the
# checkout so the later `-i vm_key` ssh steps still find it there.
VM_ARTIFACTS="${RUNNER_TEMP:-$(dirname "$GITHUB_WORKSPACE")}/vm-artifacts"
mkdir -p "$VM_ARTIFACTS"

# Dependencies we need for ponyc building and testing, that are not installed
# by default. These should be kept up-to-date with info from BUILD.md, plus `rsync`.
HAIKU_PACKAGES="cmake python3.14 libexecinfo_devel rsync"

# Run this only on Ubuntu/Debian CI
if command -v apt-get >/dev/null 2>&1 ; then
  echo "::group::Free disk space"
  sudo rm -rf /usr/share/dotnet
  sudo rm -rf /usr/local/lib/android
  sudo rm -rf /opt/hostedtoolcache
  df -h /
  echo "::endgroup::"
fi

# Run this only on Ubuntu/Debian CI
if command -v apt-get >/dev/null 2>&1 ; then
  echo "::group::Install QEMU"
  sudo apt-get update -q
  sudo apt-get install -y -q qemu-utils qemu-system-x86 genisoimage
  sudo chmod 666 /dev/kvm
  echo "::endgroup::"
fi

export HAIKU_INSTALL_SYSTEM=no
echo "::group::Create virtual disks"
if [ ! -f "$VM_ARTIFACTS/haiku_system.qcow2" ] ; then
  # First disk is for root/os partition
  qemu-img create -f qcow2 "$VM_ARTIFACTS/haiku_system.qcow2" 6G
  export HAIKU_INSTALL_SYSTEM=yes
else
  echo "Skipping qemu-img create: $VM_ARTIFACTS/haiku_system.qcow2 already exists."
fi
# Second disk is for sources, building and testing data
qemu-img create -f qcow2 "$VM_ARTIFACTS/haiku_data.qcow2" 50G
echo "::endgroup::"

if [ "$HAIKU_INSTALL_SYSTEM" == "yes" ] && [ ! -f "$VM_ARTIFACTS/haiku.iso" ] ; then
  echo "::group::Download Haiku OS nightly ISO"
  curl -L -o "$VM_ARTIFACTS/haiku.zip" "https://haiku-nightly.cdn.haiku-os.org/x86_64/haiku-master-$HAIKU_NIGHTLY_VERSION-x86_64-anyboot.zip"
  unzip -d "$VM_ARTIFACTS" "$VM_ARTIFACTS/haiku.zip"
  rm "$VM_ARTIFACTS/haiku.zip"
  rm "$VM_ARTIFACTS/ReadMe.md"
  mv "$VM_ARTIFACTS/"haiku-master-"$HAIKU_NIGHTLY_VERSION"*.iso "$VM_ARTIFACTS/haiku.iso"
  echo "::endgroup::"
else
  echo "Skipping Haiku ISO: system already installed or ISO already exists."
fi

echo "::group::Prepare VM access"
if [ ! -f haiku_vm_key ] ; then
  ssh-keygen -t ed25519 -f haiku_vm_key -N ""
else
  echo "Skipping ssh-keygen: haiku_vm_key already exists."
fi
echo "::endgroup::"

echo "::group::Prepare VM scripts"
if [ "$HAIKU_INSTALL_SYSTEM" == "yes" ] ; then
  # Create customization script, so it's run by haiku-guest-installer.bash
  # on Haiku guest's side. This allows us to have a fully prepared system
  # saved in qcow, that we can cache (along with haiku_vm_key files).
  cat > "$VM_ARTIFACTS/haiku-guest-customize.bash" <<CUSTOMIZE
#!/bin/env bash
set -e
echo "::guest::Configure git"
git config --global --add safe.directory /Data/ponyc

# Sometimes pkgman fails with:
# "*** Failed to download package X: Interrupted system call"
# In such cases, wait a bit and try again once more.
echo "::guest::Install dependencies"
pkgman install -y $HAIKU_PACKAGES || (sleep 3 && pkgman install -y $HAIKU_PACKAGES)

# Haiku's python package does not set up default python3 command,
# so we need to create one so our ci-scripts can run the same as on other platforms.
echo "::guest::Link python3 to python3.14"
ln -s /bin/python3.14 /boot/system/non-packaged/bin/python3

# Haiku has ulimit -n set to 512 by default, which is fine for running all tests
# (at the moment of writing this line). But when user connects to Haiku through
# SSH, ulimit -n is set to 256, and full-programs-tests fail at some point.
# Set ulimit to some higher, safer value, for every SSH session.
echo "::guest::Force SSH session environment setup"
cat >/boot/home/config/settings/ssh/entry <<'EOF'
#!/bin/bash
# Set ulimit -n to value high enough for tests to not fail.
ulimit -Sn 1024
# Either run command or start shell
if [[ \$SSH_ORIGINAL_COMMAND ]]; then
    eval "\$SSH_ORIGINAL_COMMAND"
else
    exec \$SHELL
fi
EOF
chmod +x /boot/home/config/settings/ssh/entry
cat >>/boot/system/settings/ssh/sshd_config <<'EOF'
# Whenever "user" connects, run entry script.
Match User user
  ForceCommand /bin/bash -c "/boot/home/config/settings/ssh/entry"
EOF
CUSTOMIZE
  # Create ISO with stuff we want to pass into the VM. This way we won't have to
  # "type" everything through QEmu monitor's console in haiku_configure_vm.py.
  genisoimage -output "$VM_ARTIFACTS/haiku_seed.iso" -volid cidata -joliet -rock \
    haiku_vm_key.pub \
    $SCRIPT_DIR/haiku-guest-installer.bash \
    "$VM_ARTIFACTS/haiku-guest-customize.bash"
else
  echo "Skipping seed ISO: system already installed."
fi
echo "::endgroup::"

echo "::group::Boot Haiku VM"
export HAIKU_MONITOR_SOCKET="$VM_ARTIFACTS/haiku_monitor.sock"
export HAIKU_SERIAL_SOCKET="$VM_ARTIFACTS/haiku_serial.sock"

CDROM1=
CDROM2=
if [ "$HAIKU_INSTALL_SYSTEM" == "yes" ] ; then
  CDROM1="-boot once=d -cdrom $VM_ARTIFACTS/haiku.iso"
  CDROM2="-drive file=$VM_ARTIFACTS/haiku_seed.iso,media=cdrom"
else
  echo "Skipping CDROM: system already installed."
fi
# Haiku disabled virtio disk driver because it could corrupt data
# in some cases, which means we have to use much slower ide now.
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 4 \
  -m 8G \
  -snapshot \
  $CDROM1 \
  -drive file="$VM_ARTIFACTS/haiku_system.qcow2",format=qcow2,media=disk,if=ide,id=system \
  -drive file="$VM_ARTIFACTS/haiku_data.qcow2",format=qcow2,media=disk,if=ide,id=data \
  $CDROM2 \
  -netdev user,id=net0,hostfwd=tcp:127.0.0.1:2222-:22 \
  -device e1000,netdev=net0 \
  -object rng-random,id=rng0,filename=/dev/urandom \
  -device virtio-rng-pci,rng=rng0 \
  -monitor unix:"$HAIKU_MONITOR_SOCKET",server,nowait \
  -serial unix:"$HAIKU_SERIAL_SOCKET",server,nowait \
  -display none \
  -daemonize
echo "::endgroup::"

echo "::group::Configure VM"
if [ "$HAIKU_INSTALL_SYSTEM" == "yes" ] ; then
  # No need to wait for VM here.
  # haiku_configure.py will read serial output from sock and wait until
  # known output line shows up.

  # Haiku ISO images boot to an UI prompt with root/user having no password
  # and no cloud-init support; haiku_configure_vm.py types the setup commands
  # into the Terminal application via the QEMU monitor socket and checks state
  # needed to continue configuration steps via serial socket (both sockets are
  # created by qemu above, under VM_ARTIFACTS).
  python3 "$SCRIPT_DIR/haiku_configure_vm.py"
else
  echo "Skipping: system already installed."
fi
echo "::endgroup::"

echo "::group::Wait for VM"
timeout 300 bash -c '
  while ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 -i haiku_vm_key -p 2222 user@localhost true 2>/dev/null; do
    sleep 2
  done
'
echo "SSH available"
echo "::endgroup::"

echo "::group::Copy source to VM"
rsync -avz -e "ssh -o StrictHostKeyChecking=no -i haiku_vm_key -p 2222" \
  --exclude "$GITHUB_WORKSPACE/haiku_vm_key*" \
  --chown=user:root \
  "$GITHUB_WORKSPACE/" user@localhost:/Data/ponyc/
echo "::endgroup::"

echo "::group::List ulimits"
ssh -o StrictHostKeyChecking=no -i haiku_vm_key -p 2222 user@localhost /bin/sh <<'EOF'
ulimit -a
EOF
echo "::endgroup::"

echo "::group::List files in VM"
ssh -o StrictHostKeyChecking=no -i haiku_vm_key -p 2222 user@localhost /bin/sh <<'EOF'
ls -la /Data/ponyc/
EOF
echo "::endgroup::"
