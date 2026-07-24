#!/usr/bin/env bash
#
# Run the in-VM libs-cache handling for the Haiku tier-3 job over ssh: either
# restore the prebuilt vendored LLVM ("libs") from a cache, or build it and push
# it. Shared by the libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml;
# see ../libs-cache/README.md for how the two call it.
# Expects a VM already booted by haiku-provision.bash, reachable at
# 'ssh -i haiku_vm_key -p 2222 user@localhost' with the checkout under /Data/ponyc.
# Run from the checkout root (where the provision step left haiku_vm_key).
#
# Usage: haiku-libs-cache.bash <operation>
#   restore            pull the branch cache into the VM (tier3, on a cache hit)
#   build-push-branch  build LLVM in the VM, push the branch cache (tier3, on a miss)
#   build-push-main    build LLVM in the VM, push the main cache (the warmer)
#
# Reads GITHUB_TOKEN, GITHUB_REPOSITORY, GITHUB_ACTOR, and LIBS_TAG from the
# environment and forwards them into the VM. The workflow step sets GITHUB_TOKEN
# and LIBS_TAG; the runner provides GITHUB_REPOSITORY and GITHUB_ACTOR.
set -euo pipefail

operation=${1:?usage: haiku-libs-cache.bash <operation>}

: "${GITHUB_TOKEN:?set GITHUB_TOKEN}"
: "${GITHUB_REPOSITORY:?set GITHUB_REPOSITORY}"
: "${LIBS_TAG:?set LIBS_TAG}"
# GITHUB_ACTOR is only the GHCR basic-auth username; the in-VM cache scripts fall
# back to the repo owner when it is unset, so it is optional.
GITHUB_ACTOR=${GITHUB_ACTOR:-}

# The command(s) to run in the VM. build-push-branch warns and continues on a
# push failure (tier3 rebuilds next run); build-push-main and restore are fatal.
# Haiku builds on a dedicated /Data disk (its default disklabel confines root),
# so the build ops free the intermediate LLVM tree once the libs
# are built: tier3's follow-on test build needs the room; harmless on the warmer,
# whose VM is discarded next.
case "$operation" in
  restore)
    remote_body="python3 .ci-scripts/libs-cache/resolve_libs_cache.py --require-cache-hit --branch-cache --platform haiku --tag '${LIBS_TAG}'"
    ;;
  build-push-branch)
    remote_body="cmake -DJOBS=4 -DPRESET=libs-haiku-x86-64 -P lib/build-libs.cmake
rm -rf build/build_libs
python3 .ci-scripts/libs-cache/branch_libs_cache.py push --platform haiku --tag '${LIBS_TAG}' || echo '::warning::haiku branch libs cache push failed, will rebuild next run'"
    ;;
  build-push-main)
    remote_body="cmake -DJOBS=4 -DPRESET=libs-haiku-x86-64 -P lib/build-libs.cmake
rm -rf build/build_libs
python3 .ci-scripts/libs-cache/oci_libs_cache.py push --platform haiku --tag '${LIBS_TAG}'"
    ;;
  *)
    echo "unknown operation: $operation" >&2
    exit 1
    ;;
esac

# The delimiter is unquoted on purpose: the values below must expand on the
# runner, since they are not set inside the VM. The single quotes around each
# value protect it in the VM's shell, and the token travels on stdin, never in
# the VM's process arguments.
# shellcheck disable=SC2087
ssh -o StrictHostKeyChecking=no -o ServerAliveInterval=60 -i haiku_vm_key -p 2222 user@localhost /bin/sh <<REMOTE
set -e
cd /Data/ponyc
export GITHUB_TOKEN='${GITHUB_TOKEN}' GITHUB_REPOSITORY='${GITHUB_REPOSITORY}' GITHUB_ACTOR='${GITHUB_ACTOR}'
${remote_body}
REMOTE
