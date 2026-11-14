#!/usr/bin/env bash
#
# Restore the prebuilt vendored LLVM libraries from a cache, or build and push
# them. Run from the checkout root inside the committed Nix development shell.
#
# Usage: nix-libs-cache.bash <operation>
#   restore            pull the libs cache
#   build-push-branch  build LLVM and push the branch cache
#   build-push-main    build LLVM and push the main cache
#
# Requires GITHUB_TOKEN, GITHUB_REPOSITORY, and LIBS_TAG in the environment.
# GITHUB_ACTOR is optional.
set -euo pipefail

operation=${1:?usage: nix-libs-cache.bash <operation>}

: "${GITHUB_TOKEN:?set GITHUB_TOKEN}"
: "${GITHUB_REPOSITORY:?set GITHUB_REPOSITORY}"
: "${LIBS_TAG:?set LIBS_TAG}"

case "$operation" in
  restore)
    python3 -B .ci-scripts/libs-cache/resolve_libs_cache.py \
      --require-cache-hit --branch-cache --platform nixos-nix --tag "$LIBS_TAG"
    ;;
  build-push-branch)
    cmake -DJOBS=4 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
      -P lib/build-libs.cmake
    python3 -B .ci-scripts/libs-cache/branch_libs_cache.py push \
      --platform nixos-nix --tag "$LIBS_TAG" \
      || echo "::warning::nixos branch libs cache push failed, will rebuild next run"
    ;;
  build-push-main)
    cmake -DJOBS=4 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
      -P lib/build-libs.cmake
    python3 -B .ci-scripts/libs-cache/oci_libs_cache.py push \
      --platform nixos-nix --tag "$LIBS_TAG"
    ;;
  *)
    echo "unknown operation: $operation" >&2
    exit 1
    ;;
esac
