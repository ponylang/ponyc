#!/bin/bash

# Manually kick off the release work that is normally triggered by Cloudsmith.
#
# When a release package finishes syncing to the ponylang/releases Cloudsmith
# repository, Cloudsmith fires a `cloudsmith-package-synchronised`
# repository_dispatch event at ponylang/ponyc. The release image build is the
# only consumer of those events, gated on the alpine3.24 package names (the
# Alpine version must match the Dockerfile base; see
# .known-couplings/docker-image-base-vs-trigger-package.md):
#
#   ponyc-x86-64-unknown-linux-alpine3.24.tar.gz
#   ponyc-arm64-unknown-linux-alpine3.24.tar.gz
#       -> Build Release Image. Both arches must be sent: the multi-arch
#          `:release` image ORs the two (so the duplicate run collapses under
#          `concurrency: cancel-in-progress`), but the versioned per-arch images
#          are gated individually -- amd64 on the x86-64 name, arm64 on the
#          arm64 name -- and each feeds the multiplatform merge. Sending only
#          one arch silently skips the other's versioned image. The build
#          cascades to the playground update, stdlib-builder + docs, the release
#          announcement, and a ponyc-release-image-pushed fan-out to the
#          documentation/shared-docker repos.
#
# Unlike the nightly cascade there are no macOS/Windows Cloudsmith forwards for
# releases -- those gates are nightlies-only -- so only the two alpine events
# are needed here.
#
# Should the Cloudsmith side ever fail to send those events, run this by hand
# to trigger the release work that would otherwise be skipped.
#
# Run this only once the release's alpine packages have actually synced to
# Cloudsmith (i.e. the release is published) -- the image build pulls the
# current release via ponyup and only tags it with <version>, so triggering
# early or with a version that isn't the published release mis-tags the image.
#
# The two dispatches are not atomic, and the first one alone fires the full
# outward-facing cascade -- including the public release announcement and the
# documentation fan-out. So if the second dispatch fails, the versioned
# `:<version>` image manifest is left single-arch with the announcement already
# sent. Re-run to finish the manifest; re-running re-enters the cascade (and may
# repeat the announcement).
#
# Usage: trigger-release-tasks.bash <github-token> <version>
#   <github-token>  a token with write access to ponylang/ponyc.
#   <version>       the release version, as found in the VERSION file
#                   (semver X.Y.Z, e.g. 0.63.4).

set -e

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <github-token> <version>" >&2
  exit 1
fi

token="$1"
version="$2"
repo="ponylang/ponyc"

if ! [[ "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "error: version must be a release semver (X.Y.Z), as found in the" \
    "VERSION file; got '${version}'." >&2
  exit 1
fi

# Send a single cloudsmith-package-synchronised event for one package name.
dispatch() {
  name="$1"

  # -S so transport failures (DNS, timeout) print despite -s; the
  # `|| status="000"` keeps set -e from aborting before the check below, so a
  # failed curl is reported by the else branch rather than dying silently.
  status=$(curl -sSL -o /dev/null -w '%{http_code}' -X POST \
    -H "Accept: application/vnd.github+json" \
    -H "Authorization: Bearer ${token}" \
    -H "X-GitHub-Api-Version: 2022-11-28" \
    "https://api.github.com/repos/${repo}/dispatches" \
    -d "{\"event_type\":\"cloudsmith-package-synchronised\",\"client_payload\":{\"data\":{\"repository\":\"releases\",\"name\":\"${name}\",\"version\":\"${version}\"}}}") || status="000"

  if [ "$status" = "204" ]; then
    echo "Dispatched ${name} (HTTP 204)."
  else
    echo "Dispatch failed for ${name} (HTTP ${status})." >&2
    exit 1
  fi
}

dispatch "ponyc-x86-64-unknown-linux-alpine3.24.tar.gz"
dispatch "ponyc-arm64-unknown-linux-alpine3.24.tar.gz"

echo "All release tasks triggered for version ${version}."
