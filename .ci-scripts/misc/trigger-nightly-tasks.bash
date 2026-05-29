#!/bin/bash

# Manually kick off the nightly work that is normally triggered by Cloudsmith.
#
# When a nightly package finishes syncing to the ponylang/nightlies Cloudsmith
# repository, Cloudsmith fires a `cloudsmith-package-synchronised`
# repository_dispatch event at ponylang/ponyc. Different package names gate
# different downstream jobs:
#
#   ponyc-x86-64-unknown-linux-alpine3.23.tar.gz
#   ponyc-arm64-unknown-linux-alpine3.23.tar.gz
#       -> Build Nightly Image (image -> stdlib-builder -> docs test -> prune
#          -> notify downstream). The gate ORs the two alpine arches and the
#          build is multi-arch, so in the real cascade both packages sync and
#          fire the build; `concurrency: cancel-in-progress` collapses the
#          duplicate. We send both names to faithfully replay that.
#   ponyc-x86-64-apple-darwin.tar.gz
#       -> macOS x86-64 nightly-released (corral, ponyup)
#   ponyc-arm64-apple-darwin.tar.gz
#       -> macOS arm64 nightly-released (appdirs, corral, lori, ponyup)
#   ponyc-x86-64-pc-windows-msvc.zip
#       -> Windows nightly-released (appdirs, corral, http, lori, ponyup,
#          regex, ssl)
#
# Dispatching these events reproduces the full cascade; the downstream
# `ponyc-*-nightly-released` and `ponyc-nightly-image-pushed` events are
# forwarded by ponyc's own workflows once these fire.
#
# Should the Cloudsmith side ever fail to send those events, run this by hand
# to trigger the nightly work that would otherwise be skipped.
#
# Usage: trigger-nightly-tasks.bash <github-token> <version>
#   <github-token>  a token with write access to ponylang/ponyc.
#   <version>       the nightly version stamp, as produced by the nightly
#                   build (`date +%Y%m%d`, e.g. 20260529).

set -e

if [ "$#" -ne 2 ]; then
  echo "usage: $0 <github-token> <version>" >&2
  exit 1
fi

token="$1"
version="$2"
repo="ponylang/ponyc"

if ! [[ "${version}" =~ ^[0-9]{8}$ ]]; then
  echo "error: version must be an 8-digit date stamp (YYYYMMDD)," \
    "as produced by the nightly build; got '${version}'." >&2
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
    -d "{\"event_type\":\"cloudsmith-package-synchronised\",\"client_payload\":{\"data\":{\"repository\":\"nightlies\",\"name\":\"${name}\",\"version\":\"${version}\"}}}") || status="000"

  if [ "$status" = "204" ]; then
    echo "Dispatched ${name} (HTTP 204)."
  else
    echo "Dispatch failed for ${name} (HTTP ${status})." >&2
    exit 1
  fi
}

dispatch "ponyc-x86-64-unknown-linux-alpine3.23.tar.gz"
dispatch "ponyc-arm64-unknown-linux-alpine3.23.tar.gz"
dispatch "ponyc-x86-64-apple-darwin.tar.gz"
dispatch "ponyc-arm64-apple-darwin.tar.gz"
dispatch "ponyc-x86-64-pc-windows-msvc.zip"

echo "All nightly tasks triggered for version ${version}."
