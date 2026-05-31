#!/bin/sh
# POST_BUILD step for libponyrt on FreeBSD when building with use=dtrace.
# Invoked from src/libponyrt/CMakeLists.txt; see the comment there for the full
# rationale. In short: FreeBSD's illumos-derived `dtrace -G` rewrites probe-site
# objects in place and emits a separate DOF object. So we extract libponyrt.a's
# freshly-compiled objects, run `dtrace -G` over them, re-archive libponyrt.a
# from the patched objects, and put the DOF object in its own libdtrace_probes.a
# (linked whole-archive by genexe.cc). Working on extracted copies keeps CMake's
# tracked objects pristine, so every rebuild redoes this from clean objects.
#
#   $1 = libponyrt.a            (rewritten in place from the patched objects)
#   $2 = libdtrace_probes.a     ((re)created with the DOF object)
#   $3 = dtrace_probes.d        (the provider script)
#   $4 = ar                     (the archiver CMake uses; a single executable
#                                path — CMAKE_AR is never a multi-word command)
set -eu

archive="$1"
dtrace_lib="$2"
dscript="$3"
ar="$4"

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Extract the freshly-compiled (pristine) runtime objects.
(cd "$tmp" && "$ar" x "$archive")

# Snapshot the object list BEFORE dtrace -G creates dtrace_probes.o, so the DOF
# object is not mistaken for a runtime object below.
set -- "$tmp"/*.o

# `ar x` flattens members to their basenames. If two runtime sources ever share
# a basename (e.g. a future ds/list.c next to the existing one), extraction
# would silently collapse them and we'd re-archive an incomplete libponyrt.a.
# Fail loudly instead: the extracted object count must equal the member count.
members=$("$ar" t "$archive" | wc -l | tr -d ' ')
if [ "$#" -ne "$members" ]; then
  echo "dtrace postbuild: extracted $# objects but archive has $members members;" \
       "runtime source basenames must be unique (see _c_src in CMakeLists.txt)" >&2
  exit 1
fi

# Rewrite the probe sites in place and emit the DOF object.
dtrace -G -s "$dscript" -o "$tmp/dtrace_probes.o" "$@"

# Re-archive both libs by writing a sibling file and renaming into place, so an
# interrupted run can't leave a truncated archive that a timestamp check would
# then treat as up to date. The .new files sit next to their targets, so the
# rename is a same-filesystem atomic replace. libponyrt.a holds the patched
# runtime objects; libdtrace_probes.a holds only the DOF object.
"$ar" qcs "${archive}.new" "$@"
mv -f "${archive}.new" "$archive"
"$ar" qcs "${dtrace_lib}.new" "$tmp/dtrace_probes.o"
mv -f "${dtrace_lib}.new" "$dtrace_lib"
