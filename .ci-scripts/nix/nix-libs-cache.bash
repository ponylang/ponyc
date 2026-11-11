#!/usr/bin/env bash
#
# Run the in-container libs-cache handling for the NixOS tier-3 job: either
# restore the prebuilt vendored LLVM ("libs") from a cache, or build it and push
# it. Shared by the libs-cache warmer (update-lib-cache.yml) and ponyc-tier3.yml.
# Expects a running Docker container named ponyc-nix with the checkout at /work
# and nix-shell providing the build dependencies. Run from the checkout root.
#
# Usage: nix-libs-cache.bash <operation>
#   restore            pull the libs cache (tier3, on a cache hit)
#   build-push-branch  build LLVM, push the branch cache (tier3, on a miss)
#   build-push-main    build LLVM, push the main cache (the warmer)
#
# Reads GITHUB_TOKEN, GITHUB_REPOSITORY, GITHUB_ACTOR, and LIBS_TAG from the
# environment and forwards them into the container.
set -euo pipefail

operation=${1:?usage: nix-libs-cache.bash <operation>}

: "${GITHUB_TOKEN:?set GITHUB_TOKEN}"
: "${GITHUB_REPOSITORY:?set GITHUB_REPOSITORY}"
: "${LIBS_TAG:?set LIBS_TAG}"
GITHUB_ACTOR=${GITHUB_ACTOR:-}

case "$operation" in
  restore)
    remote_body="python3 .ci-scripts/libs-cache/resolve_libs_cache.py --require-cache-hit --branch-cache --platform nixos-nix --tag '${LIBS_TAG}'"
    ;;
  build-push-branch)
    remote_body="cmake -DJOBS=4 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake
python3 .ci-scripts/libs-cache/branch_libs_cache.py push --platform nixos-nix --tag '${LIBS_TAG}' || echo '::warning::nixos branch libs cache push failed, will rebuild next run'"
    ;;
  build-push-main)
    remote_body="cmake -DJOBS=4 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -P lib/build-libs.cmake
python3 .ci-scripts/libs-cache/oci_libs_cache.py push --platform nixos-nix --tag '${LIBS_TAG}'"
    ;;
  *)
    echo "unknown operation: $operation" >&2
    exit 1
    ;;
esac

docker exec \
  -e GITHUB_TOKEN="$GITHUB_TOKEN" \
  -e GITHUB_REPOSITORY="$GITHUB_REPOSITORY" \
  -e GITHUB_ACTOR="$GITHUB_ACTOR" \
  ponyc-nix \
  nix-shell -p cmake clang gcc python3 zlib openssl libxml2 ncurses coreutils findutils binutils git file --run "
set -e
cd /work
${remote_body}
"
