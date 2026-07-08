#!/usr/bin/env bash
#
# Run the in-VM libs-cache handling for the DragonFly BSD tier-3 job over ssh:
# either restore the prebuilt vendored LLVM ("libs") from a cache, or build it
# and push it. Shared by the libs-cache warmer (update-lib-cache.yml) and
# ponyc-tier3.yml; see .known-couplings/ghcr-libs-cache.md for how the two call
# it. Expects a VM already booted by dragonfly-provision.bash, reachable at
# 'ssh -i vm_key -p 2222 root@localhost' with the checkout under /build/ponyc.
# Run from the checkout root (where the provision step left vm_key).
#
# Usage: dragonfly-libs-cache.bash <operation>
#   restore            pull the branch cache into the VM (tier3, on a cache hit)
#   build-push-branch  build LLVM in the VM, push the branch cache (tier3, on a miss)
#   build-push-main    build LLVM in the VM, push the main cache (the warmer)
#
# Reads GITHUB_TOKEN, GITHUB_REPOSITORY, GITHUB_ACTOR, and LIBS_TAG from the
# environment and forwards them into the VM. The workflow step sets GITHUB_TOKEN
# and LIBS_TAG; the runner provides GITHUB_REPOSITORY and GITHUB_ACTOR.
set -euo pipefail

operation=${1:?usage: dragonfly-libs-cache.bash <operation>}

: "${GITHUB_TOKEN:?set GITHUB_TOKEN}"
: "${GITHUB_REPOSITORY:?set GITHUB_REPOSITORY}"
: "${LIBS_TAG:?set LIBS_TAG}"
# GITHUB_ACTOR is only the GHCR basic-auth username; the in-VM cache scripts fall
# back to the repo owner when it is unset, so it is optional.
GITHUB_ACTOR=${GITHUB_ACTOR:-}

# The command(s) to run in the VM. build-push-branch warns and continues on a
# push failure (tier3 rebuilds next run); build-push-main and restore are fatal.
# DragonFly builds with pkg gcc13 (not the base clang), so the libs build names
# it on CC/CXX. It builds on a dedicated /build disk, so the build ops free the
# intermediate LLVM tree once the libs are built: tier3's follow-on test build
# needs the room; harmless on the warmer, whose VM is discarded next.
case "$operation" in
  restore)
    remote_body="python3 .ci-scripts/libs-cache/resolve_libs_cache.py --require-cache-hit --branch-cache --platform dragonfly-6.4.2 --tag '${LIBS_TAG}'"
    ;;
  build-push-branch)
    remote_body="cmake -DTOOLS=false -DJOBS=4 -DCMAKE_C_COMPILER=/usr/local/bin/gcc13 -DCMAKE_CXX_COMPILER=/usr/local/bin/g++13 -P lib/build-libs.cmake
rm -rf build/build_libs
python3 .ci-scripts/libs-cache/branch_libs_cache.py push --platform dragonfly-6.4.2 --tag '${LIBS_TAG}' || echo '::warning::dragonfly branch libs cache push failed, will rebuild next run'"
    ;;
  build-push-main)
    remote_body="cmake -DTOOLS=false -DJOBS=4 -DCMAKE_C_COMPILER=/usr/local/bin/gcc13 -DCMAKE_CXX_COMPILER=/usr/local/bin/g++13 -P lib/build-libs.cmake
rm -rf build/build_libs
python3 .ci-scripts/libs-cache/oci_libs_cache.py push --platform dragonfly-6.4.2 --tag '${LIBS_TAG}'"
    ;;
  *)
    echo "unknown operation: $operation" >&2
    exit 1
    ;;
esac

# The delimiter is unquoted on purpose: the values below must expand on the
# runner, since they are not set inside the VM. The single quotes around each
# value protect it in the VM's shell, and the token travels on stdin, never in
# the VM's process arguments. DragonFly's root login shell is csh, so pipe an
# explicit /bin/sh; the exported CC/CXX plus gcc13's runtime libs and CA bundle
# are what the libs build and the push scripts run under.
# shellcheck disable=SC2087
ssh -o StrictHostKeyChecking=no -o ServerAliveInterval=60 -i vm_key -p 2222 root@localhost /bin/sh <<REMOTE
set -e
cd /build/ponyc
export GITHUB_TOKEN='${GITHUB_TOKEN}' GITHUB_REPOSITORY='${GITHUB_REPOSITORY}' GITHUB_ACTOR='${GITHUB_ACTOR}'
export CC=/usr/local/bin/gcc13 CXX=/usr/local/bin/g++13 LD_LIBRARY_PATH=/usr/local/lib/gcc13 SSL_CERT_FILE=/usr/local/share/certs/ca-root-nss.crt
${remote_body}
REMOTE
