# The libs cache

ponyc vendors LLVM, LLD, and clang, and building them from source takes hours.
CI caches the built artifact — `build/libs` plus
`lib/llvm/src/compiler-rt/lib/builtins` — as a GHCR OCI artifact rather than in
the GitHub Actions cache.

None of that caching lives in the build. `cmake -P lib/build-libs.cmake` knows
only how to *build* libs. Every bit of cache logic lives in the scripts in this
directory, which sequence the cache primitives around that build command.

There are two caches: a **main cache** (`ponyc-libs-cache/`) that only the
warmer writes, and a **branch cache** (`ponyc-branch-libs-cache/`) that PR and
branch builds write so a build isn't repeated and can later be promoted into the
main cache.

## The scripts

Thin entry points:

- `oci_libs_cache.py` — push/pull/exists primitives for the main cache (registry
  v2 API).
- `branch_libs_cache.py` — the same primitives for the branch cache.
- `promote_libs_cache.py` — copies a branch-cache artifact into the main cache at
  the same tag (`registry.copy`).
- `resolve_libs_cache.py` — the consumer and warmer orchestration the workflows
  call.
- `pr_libs_cache.py` — the PR-job orchestration.
- `prune_libs_cache.py`, `prune_branch_libs_cache.py`, `clear_libs_cache.py` —
  retention.

Shared support library, imported by the above so there is one definition rather
than copies to keep in sync:

- `registry.py` — the registry-v2 client (`request`, auth, blobs, manifest,
  archive, `copy`, `derive_platform`, `IMAGE_RE`, `repo_root`).
- `ghpackages.py` — GitHub Packages REST plumbing (`gh_request`, `paginate`,
  `encode`).
- `cache_arch.py` — arch normalization.
- `common.py` — `die`/`info`, the orchestrator helpers, and the shared entry-script
  path constants (`MAIN_CACHE`, `BRANCH_CACHE`, `PROMOTE`).

So `oci_libs_cache.py` and `branch_libs_cache.py` are little more than a
`NAMESPACE`, a `cache_package`, and a `main` that calls `registry.dispatch`;
`promote_libs_cache.py` resolves both namespaces and calls `registry.copy`. A
change to builder-image naming is a one-place edit in `registry.py`.

## Package naming and cache identity

The full package name is assembled in one place, `cache_package()`, as
`ponyc-libs-cache/<platform>-<arch>`, and the tag is the `hashFiles` content
hash.

The `ponyc-libs-cache/` path namespace keeps these packages apart from the
distributable containers (which use `nightly/` and `releases/`) and fences the
globs that retention and clear match on.

`<platform>` is either the builder image's name and date, derived by
`derive_platform` for container jobs that pass `--image`, or a literal label
passed with `--platform` (a bare label like `x86-macos-15-intel`; the script adds
the namespace and the arch, so workflows pass the label alone).

`<arch>` comes from `platform.machine()` on the build machine, normalized to one
canonical spelling per ISA by `cache_arch.canonical` (`amd64` becomes `x86_64`,
`aarch64` becomes `arm64`). An unrecognized arch is a hard error rather than a
silent passthrough — add it to `ARCH_ALIASES` deliberately. Normalization is
essential twice over:

1. The alpine and ubuntu26.04 builder images are multi-arch, built on both the
   x86-64 and the arm64 warmer jobs. Without the arch component the two would
   push to the same `package:tag` and clobber each other, and an arm64 consumer
   would then pull x86-64 libs.
2. The same ISA is spelled differently across operating systems — Linux says
   `x86_64` where FreeBSD and OpenBSD say `amd64`. The BSD warmer's host-side
   existence check runs on the Linux runner while the push happens inside the VM,
   so the two must canonicalize to the same name or the host could never see what
   the VM wrote.

Changing an ISA's canonical spelling — or any part of the package-name shape —
renames the package. The old-named main-cache artifacts are then orphaned, and
`prune` (which keeps N *per package*) never reclaims a package that stopped
receiving versions. Run `clear-libs-cache.yml` once to delete the strays. The
branch cache heals itself, because its retention is age-based; the main cache
does not.

## The main cache

`update-lib-cache.yml`, the warmer, is the **only writer of the main cache**, and
it runs on push to main. On a miss it first tries to promote a matching
branch-cache artifact with `promote_libs_cache.py` — a registry copy that reuses
a build some PR or ad-hoc dispatch already made — and cold-builds only when there
is none.

Every other workflow pulls or builds. So the warmer's jobs must cover **every
platform and label any consumer pulls**: a consumer whose builder image or runner
label the warmer doesn't build gets a miss on every run. When you add a libs
consumer, or a new platform to an existing one, add the matching platform to the
warmer, in the right stage.

### The warmer's three stages

A cold push — an LLVM-input change, when every platform cold-builds at once —
would otherwise saturate the org-wide runner pool with rarely-used builds ahead
of the most-used caches. So the build-outs run in three sequential stages:

1. **PR platforms** (`x86_64-linux-pr`, which is ubuntu26.04 x86-64;
   `arm64-macos`; `x86_64-windows`) — what `pr.yml` pulls. No gate; starts
   immediately.
2. **Release and nightly platforms** (`x86_64-linux-release`, `arm64-linux`,
   `x86_64-macos`, `arm64-windows`) — the rest of what `release.yml` and
   `nightlies.yml` pull.
3. **Everything else** (`x86_64-linux-other`, which is fedora plus the cross
   images; `freebsd`; `openbsd`; `dragonflybsd`) — tier2, tier3, and weekly-only
   platforms.

Those last seven builds — the four `x86_64-linux-other` matrix legs and the three
BSD VMs — each take two to three hours, so stage 3 is capped at two running at
once: `x86_64-linux-other` carries `max-parallel: 2`, the BSD jobs `needs:`
`x86_64-linux-other` so the VMs never overlap the container legs, and
`dragonflybsd` also `needs:` `freebsd`. Removing any one of those re-opens the
stage to more than two.

Each later stage `needs:` only the prior stage's *fast* jobs, Linux and macOS,
and never Windows — a Windows build kicks off in its stage but must never gate
the next one, because it is the slowest. Stage jobs carry `if: ${{ !cancelled()
}}`, so a prior-stage build *failure* delays the later stages without cancelling
them.

A new platform goes in the earliest stage that pulls it. The x86-64 Linux builds
are split across three jobs (`-pr`, `-release`, `-other`) because `needs:` is
job-level rather than matrix-entry-level and those builder images span all three
stages; keep each image in exactly one of the three.

### Consumers that never cold-build

The stress-test workflows (`stress-test-*.yml`), `ponyc-tier2.yml`, and
`ponyc-tier3.yml` choose their mode by whether the code under test is main. The
step's branch check is `github.event_name == 'workflow_dispatch' && <ref> != 'main'`,
where `<ref>` is the checkout ref: `inputs.ref` for tier2 and tier3,
`github.event.inputs.sha` for stress. Both default to `main`, and a scheduled run
has neither, so it counts as on-main.

**On main** — a scheduled run, or a dispatch left on `main` — they pass
`--require-cache-hit --skip-on-miss` and never cold-build. A miss writes the
`.libs-cache-miss` marker and exits 0; every build and run step gates on
`if: hashFiles('.libs-cache-miss') == ''`, so the job goes green with those steps
skipped and the `Send alert on failure` Zulip step (gated on `failure() &&
github.event_name == 'schedule'` for the stress jobs) stays silent. This is
deliberate: the warmer owns main, and the continuous stress loop runs at
staggered times that overlap an empty or refilling cache, so a scheduled miss is
expected rather than a coverage bug.

**Off main** — a manual `workflow_dispatch` of a branch — they run in consumer
mode with `--branch-cache -- cmake … -P lib/build-libs.cmake`. That pulls main,
then the branch cache, and on a total miss builds LLVM and pushes the branch
cache, exactly as a PR does, so a manual test of an LLVM change isn't blocked and
a re-dispatch of the branch reuses the build. This is why these workflows hold
`packages: write` rather than `packages: read`.

The mode is spelled with two environment variables at the bash and sh sites:
`LIBS_MODE` for the pre-`--image` flags, and `LIBS_BUILD` for the trailing
`-- cmake … -P lib/build-libs.cmake`, which is present only off main so that on
main it adds no stray arguments. PowerShell cannot word-split a variable's value,
so the Windows sites spell the two modes out under a literal `if
($env:LIBS_OFF_MAIN -eq 'true')`. The marker is written to `GITHUB_WORKSPACE` —
falling back to the working directory, which is the workspace mount for the
arm64-linux docker-in-docker jobs — so the host-side `hashFiles` gate sees it.

The accepted tradeoff: a permanent gap in the warmer's coverage is no longer
surfaced loudly by these jobs on main, since a scheduled miss just skips. It
still surfaces via the non-stress consumers that pull the same platforms. tier2's
and tier3's `Send alert on failure` are left as a plain `failure()` rather than
gated on `schedule` like the stress one — an on-main skip never fails, so it
stays silent, but a manual-run build or test failure still alerts.

### Builder images and the BSD VMs

The container-platform name is derived from the builder image reference by
`IMAGE_RE` (via `derive_platform`) in `registry.py`. The contract is
`ghcr.io/ponylang/ponyc-ci-<name>:<YYYYMMDD>`; a builder image whose name or tag
breaks that format fails the step loudly. Update `IMAGE_RE` if the naming
convention changes.

The BSD VMs have no builder image, so they use explicit per-version labels
(`freebsd-15.1`, `openbsd-7.9`, `dragonfly-6.4.2`, and so on). The warmer boots
those VMs to build and push; the `ponyc-tier3.yml` BSD jobs pull. Each threads
`GITHUB_TOKEN` into the VM over ssh, like every other consumer.

VM provisioning is shared, not duplicated: both workflows call
`.ci-scripts/bsd/{freebsd,openbsd,dragonfly}-provision.bash` from a single
`Provision VM` step. Those scripts free disk, install QEMU, download the image,
boot the VM, install dependencies, and rsync the checkout in. DragonFly's script
shells out to `.ci-scripts/bsd/dfly_configure_vm.py`, the QEMU `sendkey` console
automation, which reads the ssh public key from `PUB_KEY`. Change VM setup in the
script, not in two copies of YAML. `freebsd-provision.bash` takes
`FREEBSD_VERSION` and installs `doas` plus a `doas.conf` unconditionally — tier3's
dtrace smoke test needs it, and it is harmless to the warmer.

The in-VM libs handling is shared per platform the same way the provisioning is.
Both workflows call `.ci-scripts/bsd/{freebsd,openbsd,dragonfly}-libs-cache.bash <operation>`
for the restore and build/push steps, rather than each job embedding
its own inline `ssh` remote-exec. Each script holds its platform's ssh options, VM
user, repo directory, build environment, compiler flags, and build-tree cleanup.
The `<operation>` — one of `restore`, `build-push-branch`, `build-push-main` —
selects the branch-cache restore, the branch-cache build and push (tier3, off
main), or the main-cache build and push (the warmer).

Both sets of scripts are file-based, so Super-Linter's shellcheck covers them,
which it could not do for the embedded `run:` blocks they replaced.
`dfly_configure_vm_test.py` guards the `KEYMAP` de-escaping and the
`DFLY_MONITOR_SOCK` monitor-socket path contract with `dragonfly-provision.bash`.

The two callers differ only in how they invoke the script.

The **warmer** runs a host-side `oci_libs_cache.py exists` check, then a
host-side promote step (`branch_libs_cache.py exists`, then
`promote_libs_cache.py`, with no VM boot) that reuses a branch artifact if one
exists. It gates the `Provision VM` step and the terminal build-and-push step on
`if: steps.check.outputs.hit != 'true' && steps.promote.outputs.promoted !=
'true'`, so a main hit or a successful promote skips the QEMU boot entirely. A
promote failure is non-fatal: `promoted=false` falls through to the VM build. The
job still *succeeds* with those steps skipped rather than failed, which keeps
`prune`'s `needs:` satisfied.

**tier3** also gates the QEMU boot on a host-side cache check, and like its
container legs it splits on main versus off main. The host-side `Check libs cache`
step (`id: check`) sets a `hit` output and, on main only, writes the
`.libs-cache-miss` marker on a miss, so `Provision VM` and everything after it
skip and a scheduled miss skips the VM entirely instead of cold-building. Off
main it never writes the marker: a main-cache hit, or a branch-cache hit, sets
`hit=true`; a total off-main miss sets `hit=false`. The in-VM steps then gate on
that output. `Restore libs` (`hit == 'true'`) pulls with `--require-cache-hit
--branch-cache` and does not build. `Build libs` (`hit == 'false'`) builds LLVM in
the VM and pushes the branch cache with `branch_libs_cache.py push`, mirroring the
warmer's in-VM build but writing the branch cache rather than main. So off main,
tier3 does capture BSD builds to the branch cache, and a re-dispatch of the same
branch reuses them. This is why tier3 holds `packages: write`.

### Retention and clearing

The warmer's `prune` job runs `prune_libs_cache.py --keep 2`, keeping the two
newest versions **per package**. The platform lives in the package name rather
than the tag, so keep-N counts per platform; moving the platform into the tag
would let keep-N delete live artifacts belonging to other platforms.

`clear-libs-cache.yml` and `clear_libs_cache.py` are the escape hatch. They
whole-package-delete every `ponyc-libs-cache/*` package (REST API, with `/`
encoded as `%2F`) and re-dispatch the warmer. Because the tag is a content hash,
there is no "touch to expire" — deletion is the only way to invalidate. The clear
workflow also needs `actions: write` for the re-warm dispatch.

### Why deletion needs two tokens

Each token can only do the half its scope allows. The org-level
`PONYLANG_MAIN_READ_PACKAGE_TOKEN` (a classic PAT with `read:packages`) is the
only one that can **enumerate** the org's packages — the repo-scoped
`GITHUB_TOKEN` gets a 400 on the org package-list endpoint. But only
`GITHUB_TOKEN` (with `packages: write`) can **delete** these packages, because
they are repo-scoped, so the org PAT gets a 404 on the delete regardless of its
scopes.

So `clear_libs_cache.py` and `prune_libs_cache.py` list with
`PONYLANG_MAIN_READ_PACKAGE_TOKEN` and delete with `GITHUB_TOKEN`, and both
workflows pass both secrets. This split is also why retention is a custom script
rather than `snok/container-retention-policy`, which takes one token and cannot
enumerate-and-delete here.

## The branch cache

The branch cache is a separate cache with its own namespace,
`ponyc-branch-libs-cache/<platform>-<arch>`, its own push/pull script
(`branch_libs_cache.py`), and its own retention (`prune_branch_libs_cache.py`).

It is tag-addressable: the package name is just the platform and arch, and the
version is the same `hashFiles` content hash, so a branch package is **exactly
the main cache's name under a different namespace prefix**. There is no `-pr<N>`
component. That earlier partitioning is gone because it did no correctness work,
and dropping it buys free cross-PR and cross-tier dedup — two builds that share a
tag share identical content. It also means the warmer can construct the name by
hand and find a promotable artifact with a single `exists` HEAD request, with no
enumeration.

The cache exists so that a build which changes an LLVM-determining input — a
non-fork PR, or an ad-hoc `workflow_dispatch` of weekly on a branch — builds LLVM
once and reuses it on later runs instead of cold-building every time, and so that
the warmer can promote that build into the main cache after merge rather than
rebuilding.

**Writers**: PR jobs (`pr_libs_cache.py`, non-fork only) and the weekly consumer
(`resolve_libs_cache.py --branch-cache`) always push it. tier2, tier3, and stress
push it too, but only off main — a manual `workflow_dispatch` of a branch. Those
workflows have no `pull_request` trigger, so the pusher is always
repo-authorized.

The warmer **reads** the branch cache, in order to promote, but never pushes a
branch package. Promotion writes only the main cache, which keeps the warmer the
main cache's sole writer. The branch scripts do not import the main cache's
scripts, and the main `ponyc-libs-cache` is always the source of truth.

### pr_libs_cache.py

`pr_libs_cache.py` owns the PR-job flow, and `resolve_libs_cache.py`'s
`--branch-cache` consumer mode does the same for the tier jobs. Both just sequence
the existing cache primitives around the build command the workflow hands them
after `--`. There are two modes:

- **Consumer mode** (the default): check main, then (with `--branch-cache`) check
  the branch via `pull`, which downloads the blob on a hit; on a miss, run the
  build; then (with `--branch-cache`) push the branch cache on a best-effort
  basis. A push failure logs a warning and degrades to "rebuild next run" rather
  than failing the job.
- **Ensure mode** (`--ensure`): the same sequence, but it checks via the `exists`
  subcommand — no blob download, since the job only needs to know *whether* to
  build — and a branch push failure hard-fails the job, so a registry write
  problem surfaces here instead of leaving each consumer to cold-build. `--ensure`
  requires `--branch-cache`.

The `exists` subcommand lives in both `oci_libs_cache.py` and
`branch_libs_cache.py`. It checks the manifest only, with no blob download, and
exits 0 when present and 1 when absent. Any HTTP or network error routes through
`die` to exit 1, so it fails safe to "build".

A main-cache hit short-circuits before any push, so the warmer stays the only
pusher to `ponyc-libs-cache`.

### How pr.yml drives it

One workflow drives the branch cache: `pr.yml`, the merged `PR` workflow that
replaced `pr-ponyc.yml`, `pr-pony-compiler.yml`, and `pr-tools.yml`. The merge is
what makes cross-workflow dedup possible. The three suites used to fire as three
concurrent workflows, each cold-building the same shared LLVM platform on a cache
miss, and `needs:` is intra-workflow only. Now, for each platform shared by two or
more suites (ubuntu glibc, macOS, Windows), a `maybe-build-<plat>` job runs
`pr_libs_cache.py --ensure` once, and the consumer jobs `needs:` it and pull.

Consumer gating is `!cancelled() && needs.changes.outputs.<suite> == 'true' && needs.maybe-build-<plat>.result != 'failure'`.
The `!cancelled()` lets a consumer
run when its maybe-build was *skipped*, which is the fork path — the consumer then
pulls or builds without `--branch-cache`. The `result != 'failure'` skips the
consumer when the shared build genuinely failed, so an LLVM build failure is
reported once rather than three times.

### Fork safety

`--branch-cache` is gated to non-fork in the workflow expression itself. A
consumer's `LIBS_BRANCH_CACHE` environment variable is
`${{ head.repo.full_name == github.repository && '--branch-cache' || '' }}` — the
flag for a non-fork, empty for a fork, which suppresses the branch pull and push.
On the bash sites the `run:` line references it **unquoted**
(`$LIBS_BRANCH_CACHE`), so a fork's empty value disappears rather than being
passed as a stray empty argument. Quoting it would break forks: argparse would
see an empty positional and exit 2.

PowerShell does not drop an empty variable that way — it passes `''` as a real
argument — so the Windows site builds the flag as an array instead:
`$bc = if ($env:LIBS_BRANCH_CACHE) { @($env:LIBS_BRANCH_CACHE) } else { @() }`,
and an empty array contributes nothing.

The maybe-build jobs' `if:` already requires non-fork, so they pass
`--branch-cache` literally. `--branch-cache` is a boolean, and it is the only
non-fork signal the script reads; it carries no PR number, because the cache is
tag-addressable.

`pr.yml` carries `permissions: packages: write` for the non-fork branch push. A
fork's `pull_request` token is read-only regardless. **Never** switch this to
`pull_request_target` — that would hand fork code a write token.

### Branch-cache retention

Retention is age-based, and deliberately different from the main cache's keep-N.
`prune-branch-libs-cache.yml` (a daily `schedule` plus `workflow_dispatch`) runs
`prune_branch_libs_cache.py`, which deletes branch-cache artifacts older than two
weeks and drops a package once all of its versions are stale — a platform that
stopped receiving builds, such as a retired builder image. The per-platform
packages are long-lived now, not per-PR, and keep-N would never delete an idle
package; hence a separate script.

It uses the same two-token split as the main retention: enumerate with
`PONYLANG_MAIN_READ_PACKAGE_TOKEN`, delete with `GITHUB_TOKEN`. It enumerates and
deletes only within `ponyc-branch-libs-cache/`, so it cannot touch the main cache.
Likewise the main cache's `prune_libs_cache.py` filters on `ponyc-libs-cache/` and
never sees branch packages, so the two prunes cannot cross.

There is no clear or escape-hatch workflow for the branch cache. The age-based
prune is the only reclaim.
