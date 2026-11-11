#!/usr/bin/env bash
#
# Libs-cache handling for the i386 VM: look up its cache packages from the host,
# restore the prebuilt vendored LLVM ("libs") into the VM or build it there and
# push it, and check the VM's CPU and toolchain. Run from the checkout root.
#
# Usage: i386-libs-cache.bash <operation>
#   Host-side, before or without the VM. exists-main and exists-branch exit 0
#   if present, 1 on a miss, 3 on an error; promote exits 0 or 3. A missing or
#   unknown operation, an unreadable i386-alpine.env, and a missing LIBS_TAG
#   also exit 3, so none of them reads as a miss:
#     exists-main        is the libs package in the main cache?
#     exists-branch      is it in the branch cache?
#     promote            copy it from the branch cache into the main cache
#   In the VM booted by i386-provision.bash (`ssh ponyc-i386-vm`):
#     restore            pull the libs (main cache, then branch cache) into the VM
#     build-push-branch  build LLVM in the VM, push the branch cache
#     build-push-main    build LLVM in the VM, push the main cache
#     check-32bit        check the CPU has SSE2 and the kernel, clang, and LLVM's
#                        default triple are 32-bit x86
#
# The cache operations read LIBS_TAG and GITHUB_TOKEN from the environment, and
# GITHUB_REPOSITORY and GITHUB_ACTOR from the runner; the VM ones forward them
# into the VM.
set -euo pipefail

if [ $# -lt 1 ]; then
  echo "usage: i386-libs-cache.bash <operation>" >&2
  exit 3
fi
operation=$1
case "$operation" in
  exists-main|exists-branch|promote|check-32bit) ;;
  restore|build-push-branch|build-push-main) ;;
  *)
    echo "unknown operation: $operation" >&2
    exit 3
    ;;
esac

env_file="$(dirname "$0")/i386-alpine.env"
if [ ! -r "$env_file" ]; then
  echo "can't read $env_file" >&2
  exit 3
fi
# shellcheck source-path=SCRIPTDIR source=i386-alpine.env disable=SC1091
. "$env_file"
if [ -z "${ALPINE_BRANCH:-}" ]; then
  echo "ALPINE_BRANCH isn't set in $env_file" >&2
  exit 3
fi
LIBS_PLATFORM="alpine${ALPINE_BRANCH#v}"
# The VM's arch in libs-cache spelling. The cache scripts in the VM name packages
# with their own machine's arch; the x86_64 host names the same packages with
# --arch "$LIBS_ARCH".
LIBS_ARCH=i686

cache=.ci-scripts/libs-cache

# Run inside the VM. LLVM's default triple becomes ponyc's default target, so it
# must name 32-bit x86.
triple_check="llvm_triple=\$(sed -n 's/^#define LLVM_DEFAULT_TARGET_TRIPLE \"\(.*\)\"\$/\\1/p' build/libs/include/llvm/Config/llvm-config.h)
case \"\$llvm_triple\" in
  i[3-6]86-*) ;;
  *) echo \"LLVM's default triple is '\$llvm_triple', not 32-bit x86\" >&2; exit 1 ;;
esac"

# A missing LIBS_TAG is an error, not a miss, so exit 3 rather than the shell's 1.
require_libs_tag() {
  if [ -z "${LIBS_TAG:-}" ]; then
    echo "set LIBS_TAG" >&2
    exit 3
  fi
}

case "$operation" in
  exists-main)
    require_libs_tag
    exec python3 "$cache/oci_libs_cache.py" exists --platform "$LIBS_PLATFORM" --arch "$LIBS_ARCH" --tag "$LIBS_TAG"
    ;;
  exists-branch)
    require_libs_tag
    exec python3 "$cache/branch_libs_cache.py" exists --platform "$LIBS_PLATFORM" --arch "$LIBS_ARCH" --tag "$LIBS_TAG"
    ;;
  promote)
    require_libs_tag
    exec python3 "$cache/promote_libs_cache.py" --platform "$LIBS_PLATFORM" --arch "$LIBS_ARCH" --tag "$LIBS_TAG"
    ;;
  check-32bit)
    # Unquoted delimiter so ${triple_check} is inserted; \$ expands in the VM.
    # shellcheck disable=SC2087
    exec ssh ponyc-i386-vm /bin/sh <<CHECK
set -e
cd /build/ponyc
grep -e vendor_id -e 'model name' /proc/cpuinfo | head -n 2
if ! grep -qw sse2 /proc/cpuinfo; then
  echo "the VM's CPU has no SSE2" >&2
  exit 1
fi
echo "uname -m: \$(uname -m); clang: \$(clang -dumpmachine)"
case "\$(uname -m)" in
  i[3-6]86) ;;
  *) echo "the kernel reports \$(uname -m)" >&2; exit 1 ;;
esac
case "\$(clang -dumpmachine)" in
  i[3-6]86-*) ;;
  *) echo "clang targets \$(clang -dumpmachine)" >&2; exit 1 ;;
esac
${triple_check}
echo "LLVM's default triple: \$llvm_triple"
CHECK
    ;;
esac

: "${GITHUB_TOKEN:?set GITHUB_TOKEN}"
: "${GITHUB_REPOSITORY:?set GITHUB_REPOSITORY}"
: "${LIBS_TAG:?set LIBS_TAG}"
# GITHUB_ACTOR is only the GHCR basic-auth username; the in-VM cache scripts fall
# back to the repo owner when it is unset, so it is optional.
GITHUB_ACTOR=${GITHUB_ACTOR:-}

build="cmake -DJOBS=4 -P lib/build-libs.cmake
${triple_check}"

case "$operation" in
  restore)
    remote_body="python3 $cache/resolve_libs_cache.py --require-cache-hit --branch-cache --platform '${LIBS_PLATFORM}' --tag '${LIBS_TAG}'"
    ;;
  build-push-branch)
    remote_body="${build}
python3 $cache/branch_libs_cache.py push --platform '${LIBS_PLATFORM}' --tag '${LIBS_TAG}' || echo '::warning::i386 branch libs cache push failed, will rebuild next run'"
    ;;
  build-push-main)
    remote_body="${build}
python3 $cache/oci_libs_cache.py push --platform '${LIBS_PLATFORM}' --tag '${LIBS_TAG}'"
    ;;
esac

# The delimiter is unquoted on purpose: an unescaped $ expands on the runner,
# where these values are set, and an escaped \$ expands in the VM. The single
# quotes around each value protect it in the VM's shell, and the token travels
# on stdin, never on the ssh command line.
# shellcheck disable=SC2087
ssh ponyc-i386-vm /bin/sh <<REMOTE
set -e
cd /build/ponyc
# Refuse if this machine's cache arch isn't LIBS_ARCH.
arch=\$(cd $cache && python3 -c 'import cache_arch; print(cache_arch.package_arch())')
if [ "\$arch" != '${LIBS_ARCH}' ]; then
  echo "VM's cache arch is \$arch but the host names it ${LIBS_ARCH}" >&2
  exit 1
fi
export GITHUB_TOKEN='${GITHUB_TOKEN}' GITHUB_REPOSITORY='${GITHUB_REPOSITORY}' GITHUB_ACTOR='${GITHUB_ACTOR}'
${remote_body}
REMOTE
