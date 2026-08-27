#!/usr/bin/env bash
#
# Provision a Haiku VM on the GitHub Actions host (qemu), install
# the ponyc build dependencies, and rsync the checkout into it. Shared by the
# libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml; see
# ../libs-cache/README.md for how the two call it. Reads HAIKU_VERSION,
# GITHUB_WORKSPACE and RUNNER_TEMP from the environment;
# leaves a booted VM at 'ssh -i haiku_vm_key -p 2222 user@localhost'
# with the source under /Data/ponyc.
set -euo pipefail

: "${HAIKU_VERSION:?set HAIKU_VERSION, e.g. hrev60028 for nightly, r1beta6 for LTS}"
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

# Nightly and LTS releases are hosted and served differently, so we have to handle
# them differently too.
export IS_NIGHTLY=yes
echo "::group::Recognize Haiku release type"
case $HAIKU_VERSION in
  hrev*)
    export IS_NIGHTLY=yes
    echo "Setting up Haiku nightly"
    ;;
  *)
    export IS_NIGHTLY=no
    echo "Setting up Haiku LTS"
    ;;
esac
echo "::endgroup::"

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
  # Unfortunately for us, releases do not output anything to serial console by default,
  # which makes configuring VM so much more complicated and fragile for us.
  # Enforcing serial output from ISO boot is not achievable in a sane manner, so we cheat:
  # we find nightly image, boot into it, change repositories to the requested version,
  # sync update everything and hope for the best ;P.
  HAIKU_NIGHTLY_VERSION=

  if [ "$IS_NIGHTLY" == "yes" ] ; then
    HAIKU_NIGHTLY_VERSION=$HAIKU_VERSION
  else
    # We could download nightly closest to the release, but CDN keeps only latest X nightlies.
    # We'll risk it and just downgrade from latest nightly.
    echo "::group::Download list of nightlies and select latest one"
    curl -L -o "$VM_ARTIFACTS/nightlies.json" "https://eu.hpkg.haiku-os.org/haiku/master/x86_64"
    # List of versions is served as text/plain, but is formatted like a JSON array:
    # ["v1", "v2"]
    LATEST=$(cat "$VM_ARTIFACTS/nightlies.json" | tr -d ,\"\[\] | tr ' ' '\n' | sort -V | tail -n 1)
    HAIKU_NIGHTLY_VERSION=$(echo $LATEST | cut -d _ -f 2)
    echo "::endgroup::"
  fi

  echo "::group::Download Haiku OS $HAIKU_NIGHTLY_VERSION nightly ISO"
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

# Whenever application crashes, Haiku's debug_server asks user what to do.
# While in CI, we want it to just save report by default, without waiting
# for any user input (and hanging forever on the failed test for example).
# That allows us to store generated ~/Desktop/*.report files as CI artifacts.
echo "::guest::Make debugger default to saving report files"
mkdir -p /boot/home/config/settings/system/debug_server
echo "default_action report" >>/boot/home/config/settings/system/debug_server/settings

# Haiku's virtual memory might cause problems, so disable it
echo "::guest::Disable Virtual Memory"
mkdir -p /boot/home/config/settings/kernel/drivers/
sed -i "s/vm on/vm off/" /boot/home/config/settings/kernel/drivers/virtual_memory || echo "vm off" >/boot/home/config/settings/kernel/drivers/virtual_memory
mimeset -f /boot/home/config/settings/kernel/drivers/virtual_memory
CUSTOMIZE

  if [ "$IS_NIGHTLY" == "no" ] ; then
    cat >> "$VM_ARTIFACTS/haiku-guest-customize.bash" <<CUSTOMIZE
# Downgrade to requested release version by switching repositories and running full-sync
echo "::guest::Switch repositories to $HAIKU_VERSION"
echo "cfgversion=2" >/boot/system/settings/package-repositories/Haiku
echo "baseurl=https://eu.hpkg.haiku-os.org/haiku/$HAIKU_VERSION/x86_64/current" >>/boot/system/settings/package-repositories/Haiku
echo "identifier=tag:haiku-os.org,2001:repositories/haiku/$HAIKU_VERSION/x86_64" >>/boot/system/settings/package-repositories/Haiku
echo "url=tag:haiku-os.org,2001:repositories/haiku/$HAIKU_VERSION/x86_64" >>/boot/system/settings/package-repositories/Haiku
echo "priority=1" >>/boot/system/settings/package-repositories/Haiku

echo "cfgversion=2" >/boot/system/settings/package-repositories/HaikuPorts
echo "baseurl=https://eu.hpkg.haiku-os.org/haikuports/master/x86_64/current" >>/boot/system/settings/package-repositories/HaikuPorts
echo "identifier=tag:haiku-os.org,2001:repositories/haikuports/master/x86_64" >>/boot/system/settings/package-repositories/HaikuPorts
echo "url=tag:haiku-os.org,2001:repositories/haikuports/master/x86_64" >>/boot/system/settings/package-repositories/HaikuPorts
echo "priority=1" >>/boot/system/settings/package-repositories/HaikuPorts

echo "::guest::Synchronize packages"
pkgman full-sync -y
CUSTOMIZE
  fi

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

BOOT=
CDROM1=
CDROM2=
if [ "$HAIKU_INSTALL_SYSTEM" == "yes" ] ; then
  BOOT="-boot once=d"
  CDROM1="-cdrom $VM_ARTIFACTS/haiku.iso"
  CDROM2="-drive file=$VM_ARTIFACTS/haiku_seed.iso,media=cdrom"
else
  echo "Skipping CDROM: system already installed."
fi

# Haiku disabled virtio disk driver because it could corrupt data
# in some cases, which means we have to use much slower ide now.
qemu-system-x86_64 \
  -machine pc,accel=kvm \
  -cpu host \
  -smp 2 \
  -m 8G \
  -snapshot \
  $BOOT \
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
# We check not only for SSH connection, but also for /Data being mounted
# (in which case there's a `trash` directory there too).
timeout 300 bash -c '
  while ! ssh -o StrictHostKeyChecking=no -o ConnectTimeout=2 -i haiku_vm_key -p 2222 user@localhost test -d /Data/trash 2>/dev/null; do
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

echo "::group::System info"
ssh -o StrictHostKeyChecking=no -i haiku_vm_key -p 2222 user@localhost /bin/sh <<'EOF'
set -x
ulimit -a || true
vmstat || true
EOF
echo "::endgroup::"

echo "::group::List files in VM"
ssh -o StrictHostKeyChecking=no -i haiku_vm_key -p 2222 user@localhost /bin/sh <<'EOF'
ls -la /Data/ponyc/
EOF
echo "::endgroup::"
