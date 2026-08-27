#!/bin/env bash
#
# Run this inside Haiku VM guest to install and configure Haiku system.
# haiku_configure_vm.py calls this from a host machine, through haiku-monitor.sock.
# When run from Installtion CD, our tiny custom cd-rom data with haiku_vm_key.pub
# and haiku-guest-installer.bash should be already mounted at /seed.
set -euo pipefail

exec >/dev/dprintf
exec 2>&1
sleep .1

success() {
  echo "[RESULT]:OK"
}

failure() {
  echo "[RESULT]:FAIL on line $BASH_LINENO of $0"
  exit 1
}

trap 'failure "LINENO" "BASH_LINENO"' ERR

if test -f /boot/home/Desktop/Installer ; then
  # First boot is from Haiku installation CD
  echo "::guest::Format disks/partitions"
  mkfs -t bfs /dev/disk/ata/0/master/raw -q Haiku
  mkfs -t bfs /dev/disk/ata/0/slave/raw -o "block_size 4096; noindex" -q Data

  echo "::guest::Run Installer"
  # Make sure job control is enabled
  set -m
  Installer &

  # Wait for Installer's window to show up and become focused
  timeout 60 bash -c 'while [[ $(hey -o Installer GET Active of Window [0]) != "true" ]]; do
    echo "::guest::Waiting for Installer..."
    sleep 1;
  done'
  echo "::guest::Installer is ready"
  fg %Installer || true

  # Make sure system was installed.
  # Since we named target partition "Haiku", it should be mounted as "Haiku1".
  test -d /Haiku1/home/config

  echo "::guest::Setup SSH login"
  mkdir -p /Haiku1/home/config/settings/ssh
  cat /seed/haiku_vm_key.pub >>/Haiku1/home/config/settings/ssh/authorized_keys
  chmod 600 /Haiku1/home/config/settings/ssh/authorized_keys

  echo "::guest::Setup first-boot configuration script"
  cp /seed/haiku-guest-installer.bash /Haiku1/home/config/settings/boot/launch/ci-first-launch.sh
  chmod +x /Haiku1/home/config/settings/boot/launch/ci-first-launch.sh

  if test -f /seed/haiku-guest-customize.bash ; then
    echo "::guest::Setup customization script"
    cp /seed/haiku-guest-customize.bash /Haiku1/home/Desktop/ci-first-launch-customize.sh
    chmod +x /Haiku1/home/Desktop/ci-first-launch-customize.sh
  fi

  echo "::guest::Reboot into newly installed Haiku OS"
  shutdown -r -s
  exit
elif test -f /boot/home/config/settings/boot/launch/ci-first-launch.sh ; then
  # We're at first login stage (first boot after installation).
  # Move ourselves to be called on every launch
  mv /boot/home/config/settings/boot/launch/ci-first-launch.sh /boot/home/config/settings/boot/launch/ci-launch.sh
  # Haiku generates SSHD keys on first boot, but it seems to happen
  # after net_server, and SSHD as its sub-process, are already running.
  # SSHD does not seem to be restarted, and restarting net_server seems
  # to sometimes work (SSHD accepts our haiku_vm_key login), but fails
  # at other times. It might be that our restart happens too soon.
  # We could observe SSHD key files and then restart, but... that's a lot
  # of additional code. It's simpler to just reboot one more time.
  trap 'shutdown -r -s' EXIT
else
  # Regular launch
  # Reformat data disk
  unmount /Data || true
  mkfs -t bfs /dev/disk/ata/0/slave/raw -o "block_size 4096; noindex" -q Data
fi

# Mount /Data, it will be automounted after reboot.
echo "::guest::Mount /Data"
mkdir -p /Data
mount /dev/disk/ata/0/slave/raw /Data

# Run customization script, if one exists.
if [ -f /boot/home/Desktop/ci-first-launch-customize.sh ] ; then
  echo "::guest::Customize installation"
  bash /boot/home/Desktop/ci-first-launch-customize.sh
  rm /boot/home/Desktop/ci-first-launch-customize.sh
fi

sync

# Just to be on the safe-side. Not needed when run locally, but CI can be slow.
sleep 2

# Let haiku_configure_vm.py observer know we're done and OK.
success
